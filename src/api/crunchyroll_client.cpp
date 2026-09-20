#include <crdl/api/crunchyroll_client.h>
#include <crdl/api/auth_manager.h>
#include <crdl/media/downloader.h>
#include <crdl/media/muxer.h>
#include <crdl/drm/widevine_cdm.h>
#include <crdl/utils/logger.h>
#include <crdl/utils/string_utils.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <regex>

using json = nlohmann::json;

namespace crdl {

class CrunchyrollClient::Impl {
public:
    explicit Impl(const Config& config)
        : config_(config),
          auth_manager_(config),
          http_client_(),
          downloader_(config),
          muxer_(),
          widevine_(config.widevine_device_path()) {

        http_client_.set_user_agent(config.user_agent);
        http_client_.set_timeout(config.connect_timeout_sec);
    }

    Result<void> login() {
        return auth_manager_.login(config_.username, config_.password);
    }

    Result<Episode> get_episode(const std::string& episode_id) {
        auto token_check = auth_manager_.ensure_valid_token();
        if (!token_check) return Result<Episode>(token_check.error(), token_check.error_message());

        // Get CMS data first
        auto cms_result = get_cms_data();
        if (!cms_result) return Result<Episode>(cms_result.error(), cms_result.error_message());

        auto cms = cms_result.value();

        std::string url = "https://beta-api.crunchyroll.com/cms/v2" + cms["bucket"].get<std::string>()
                         + "/episodes/" + episode_id;

        http_client_.clear_headers();
        http_client_.add_header("Authorization", "Bearer " + auth_manager_.get_access_token());

        // Add query params
        url += "?Policy=" + string_utils::url_encode(cms["policy"]);
        url += "&Signature=" + string_utils::url_encode(cms["signature"]);
        url += "&Key-Pair-Id=" + string_utils::url_encode(cms["key_pair_id"]);
        url += "&locale=en-US";

        auto response = http_client_.get(url);
        if (!response || !response.value().is_success()) {
            return Result<Episode>(ErrorCode::NetworkError, "Failed to get episode");
        }

        try {
            json j = json::parse(response.value().body);

            Episode ep;
            ep.id = j.value("id", "");
            ep.title = j.value("title", "");
            ep.series_id = j.value("series_id", "");
            ep.season_id = j.value("season_id", "");
            ep.season_number = j.value("season_number", 1);
            ep.episode_number = j.value("episode_number", 1);

            // Parse versions
            if (j.contains("versions") && !j["versions"].is_null()) {
                for (const auto& v : j["versions"]) {
                    Version ver;
                    ver.guid = v.value("guid", "");
                    ver.audio_locale = v.value("audio_locale", "");
                    ver.is_original = v.value("original", false);
                    ep.versions.push_back(ver);
                }
            }

            return Result<Episode>(ep);

        } catch (const json::exception& e) {
            return Result<Episode>(ErrorCode::Unknown, std::string("Parse error: ") + e.what());
        }
    }

    Result<StreamInfo> get_streams(const std::string& episode_id, const std::string& guid) {
        LOG_INFO("get_streams called with episode_id={}, guid={}", episode_id, guid);

        auto token_check = auth_manager_.ensure_valid_token();
        if (!token_check) return Result<StreamInfo>(token_check.error(), token_check.error_message());

        auto cms_result = get_cms_data();
        if (!cms_result) return Result<StreamInfo>(cms_result.error(), cms_result.error_message());

        auto cms = cms_result.value();

        std::string url = "https://cr-play-service.prd.crunchyrollsvc.com/v3/" + guid + "/tv/android_tv/play";

        url += "?Policy=" + string_utils::url_encode(cms["policy"]);
        url += "&Signature=" + string_utils::url_encode(cms["signature"]);
        url += "&Key-Pair-Id=" + string_utils::url_encode(cms["key_pair_id"]);
        url += "&locale=en-US&queue=0";

        http_client_.clear_headers();
        http_client_.add_header("Authorization", "Bearer " + auth_manager_.get_access_token());
        http_client_.add_header("Accept-Encoding", "gzip");

        // Retry logic
        for (int retry = 0; retry < config_.max_retries; ++retry) {
            auto response = http_client_.get(url);

            if (!response) {
                if (retry < config_.max_retries - 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config_.retry_delay_ms));
                    continue;
                }
                return Result<StreamInfo>(ErrorCode::NetworkError, response.error_message());
            }

            if (response.value().status_code == 420) {
                // Too many active streams - clean up
                LOG_WARN("Too many active streams (420), attempting cleanup");
                try {
                    json err_json = json::parse(response.value().body);
                    if (err_json.contains("activeStreams")) {
                        int cleaned = 0;
                        for (const auto& stream : err_json["activeStreams"]) {
                            if (stream.contains("active") && stream["active"].get<bool>()) {
                                std::string stream_guid = stream["episodeIdentity"];
                                std::string stream_token = stream["token"];
                                delete_stream(stream_guid, stream_token);
                                cleaned++;
                            }
                        }
                        LOG_INFO("Cleaned up {} active stream(s)", cleaned);
                    }
                } catch (...) {
                    LOG_WARN("Failed to parse active streams for cleanup");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                continue;
            }

            if (response.value().status_code == 401 && retry < config_.max_retries - 1) {
                // Token expired
                auth_manager_.ensure_valid_token();
                http_client_.add_header("Authorization", "Bearer " + auth_manager_.get_access_token());
                continue;
            }

            if (!response.value().is_success()) {
                if (retry < config_.max_retries - 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config_.retry_delay_ms));
                    continue;
                }
                return Result<StreamInfo>(ErrorCode::StreamNotFound,
                    "HTTP " + std::to_string(response.value().status_code));
            }

            // Success - parse response
            try {
                json j = json::parse(response.value().body);

                StreamInfo info;
                info.mpd_url = j.value("url", "");
                info.video_token = j.value("token", "");

                // Get subtitles
                if (j.contains("subtitles") && !j["subtitles"].is_null()) {
                    for (auto it = j["subtitles"].begin(); it != j["subtitles"].end(); ++it) {
                        Subtitle sub;
                        sub.language = it.key();
                        sub.url = it.value().value("url", "");
                        sub.format = it.value().value("format", "ass");
                        info.subtitles[sub.language] = sub;
                    }
                }

                // Parse MPD for DRM info (pass auth headers)
                // Use guid directly as content_id - it's the stream/version guid, not episode_id
                auto drm_result = widevine_.extract_mpd_info(
                    info.mpd_url,
                    auth_manager_.get_access_token(),
                    guid,  // Use guid (stream version guid) as content_id
                    info.video_token
                );
                if (drm_result) {
                    auto drm_info = drm_result.value();
                    info.pssh = drm_info.pssh;
                    info.kid = drm_info.kid;
                    info.license_url = drm_info.license_url;
                }

                return Result<StreamInfo>(info);

            } catch (const json::exception& e) {
                return Result<StreamInfo>(ErrorCode::Unknown, std::string("Parse error: ") + e.what());
            }
        }

        return Result<StreamInfo>(ErrorCode::NetworkError, "Max retries exceeded");
    }

    Result<void> delete_stream(const std::string& guid, const std::string& token) {
        auto token_check = auth_manager_.ensure_valid_token();
        if (!token_check) return token_check;

        std::string url = "https://cr-play-service.prd.crunchyrollsvc.com/v1/token/" + guid + "/" + token;

        http_client_.clear_headers();
        http_client_.add_header("Authorization", "Bearer " + auth_manager_.get_access_token());

        auto response = http_client_.delete_request(url);

        if (!response) {
            return Result<void>(ErrorCode::NetworkError, response.error_message());
        }

        // 200, 204, 404 are all success (404 = already deleted)
        if (response.value().status_code == 200 ||
            response.value().status_code == 204 ||
            response.value().status_code == 404) {
            return Result<void>(ErrorCode::Success);
        }

        return Result<void>(ErrorCode::NetworkError,
            "Delete failed: " + std::to_string(response.value().status_code));
    }

    Result<void> download_episode(const std::string& episode_id,
                                  const std::vector<std::string>& audio_langs) {
        LOG_INFO("Starting download for episode: {}", episode_id);

        // Get episode info
        auto ep_result = get_episode(episode_id);
        if (!ep_result) return Result<void>(ep_result.error(), ep_result.error_message());

        auto episode = ep_result.value();
        auto metadata = episode.to_metadata();

        // Get primary audio version GUID (use episode_id if no versions)
        std::string stream_guid = episode_id;
        LOG_INFO("Episode has {} versions", episode.versions.size());
        if (!episode.versions.empty()) {
            // Find original version or use first one
            for (const auto& ver : episode.versions) {
                LOG_INFO("Version: guid={}, is_original={}", ver.guid, ver.is_original);
                if (ver.is_original) {
                    stream_guid = ver.guid;
                    break;
                }
            }
            if (stream_guid == episode_id) {
                stream_guid = episode.versions[0].guid;
            }
        }
        LOG_INFO("Using stream_guid: {}", stream_guid);

        // Download video
        LOG_INFO("Downloading video track");
        auto video_result = get_streams(stream_guid, stream_guid);
        if (!video_result) return Result<void>(video_result.error(), video_result.error_message());

        auto stream_info = video_result.value();

        // Get DRM keys BEFORE downloading
        std::vector<DRMKey> keys;
        if (!stream_info.pssh.empty()) {
            LOG_INFO("Stream is DRM protected, fetching decryption keys...");

            // Get cookies from HTTP session to pass to license request
            std::string cookies_json = http_client_.get_cookies_json();

            // Use stream_guid for license request
            auto keys_result = widevine_.get_license_keys(
                stream_info.license_url,
                stream_info.pssh,
                stream_info.video_token,
                stream_guid,
                auth_manager_.get_access_token(),
                cookies_json
            );
            if (keys_result) {
                keys = keys_result.value();
                LOG_INFO("DRM keys acquired, ready to download");
            } else {
                LOG_ERROR("Failed to get DRM keys: {}", keys_result.error_message());
                // Clean up active stream on DRM failure
                LOG_INFO("Cleaning up active stream after DRM key failure");
                delete_stream(stream_guid, stream_info.video_token);
                return Result<void>(ErrorCode::DRMError, "Could not acquire decryption keys");
            }
        } else {
            LOG_INFO("Stream is not DRM protected (no PSSH)");
        }

        auto video_file = downloader_.download_video(stream_info, metadata, keys, config_.quality, auth_manager_.get_access_token());
        if (!video_file) return Result<void>(video_file.error(), video_file.error_message());

        // Clean up video stream
        if (!stream_info.video_token.empty()) {
            delete_stream(stream_guid, stream_info.video_token);
        }

        // Download audio tracks
        std::vector<AudioTrack> audio_tracks;
        for (const auto& lang : audio_langs) {
            // Find version for this language
            std::string audio_guid;
            bool is_original = false;

            for (const auto& ver : episode.versions) {
                if (ver.audio_locale == lang) {
                    audio_guid = ver.guid;
                    is_original = ver.is_original;
                    break;
                }
            }

            if (audio_guid.empty()) {
                LOG_WARN("Audio language {} not found", lang);
                continue;
            }

            LOG_INFO("Downloading audio: {}", lang);
            auto audio_stream = get_streams(audio_guid, audio_guid);
            if (!audio_stream) continue;

            // Get audio-specific keys
            std::vector<DRMKey> audio_keys;
            if (!audio_stream.value().pssh.empty()) {
                LOG_INFO("Getting decryption keys for audio track: {}", lang);
                std::string cookies_json = http_client_.get_cookies_json();
                auto keys_result = widevine_.get_license_keys(
                    audio_stream.value().license_url,
                    audio_stream.value().pssh,
                    audio_stream.value().video_token,
                    audio_guid,
                    auth_manager_.get_access_token(),
                    cookies_json
                );
                if (keys_result) {
                    audio_keys = keys_result.value();
                    LOG_INFO("Got {} keys for audio track", audio_keys.size());
                }
            }

            auto audio_file = downloader_.download_audio(audio_stream.value(), metadata, audio_keys, lang, auth_manager_.get_access_token());
            if (audio_file) {
                AudioTrack track;
                track.file_path = audio_file.value();
                track.language_code = lang;
                track.is_default = is_original;
                audio_tracks.push_back(track);
            }

            if (!audio_stream.value().video_token.empty()) {
                delete_stream(audio_guid, audio_stream.value().video_token);
            }
        }

        // Download subtitles
        auto subtitle_files = downloader_.download_subtitles(
            stream_info.subtitles, metadata, config_.output_dir / "subtitles"
        );

        // Mux everything
        std::filesystem::path output_file = config_.output_dir /
            (metadata.to_filename(quality_to_string(config_.quality), config_.release_group) + ".mkv");

        std::vector<std::filesystem::path> subs;
        if (subtitle_files) subs = subtitle_files.value();

        auto mux_result = muxer_.mux_to_mkv(
            video_file.value(),
            audio_tracks,
            subs,
            std::filesystem::path(),
            output_file
        );

        if (!mux_result) return Result<void>(mux_result.error(), mux_result.error_message());

        LOG_INFO("Muxing completed successfully");

        // Clean up temporary files after successful mux
        LOG_INFO("Cleaning up temporary files...");
        try {
            // Remove video file
            if (std::filesystem::exists(video_file.value())) {
                std::filesystem::remove(video_file.value());
                LOG_DEBUG("Removed: {}", video_file.value().string());
            }

            // Remove audio files
            for (const auto& audio : audio_tracks) {
                if (std::filesystem::exists(audio.file_path)) {
                    std::filesystem::remove(audio.file_path);
                    LOG_DEBUG("Removed: {}", audio.file_path.string());
                }
            }

            // Remove subtitle files
            for (const auto& sub : subs) {
                if (std::filesystem::exists(sub)) {
                    std::filesystem::remove(sub);
                    LOG_DEBUG("Removed: {}", sub.string());
                }
            }

            // Remove subtitles directory if empty
            std::filesystem::path subs_dir = config_.output_dir / "subtitles";
            if (std::filesystem::exists(subs_dir) && std::filesystem::is_empty(subs_dir)) {
                std::filesystem::remove(subs_dir);
                LOG_DEBUG("Removed empty subtitles directory");
            }

            LOG_INFO("Cleanup completed");
        } catch (const std::exception& e) {
            LOG_WARN("Cleanup failed (non-fatal): {}", e.what());
        }

        LOG_INFO("Download complete: {}", output_file.string());
        return Result<void>(ErrorCode::Success);
    }

private:
    Result<json> get_cms_data() {
        auto token_check = auth_manager_.ensure_valid_token();
        if (!token_check) return Result<json>(token_check.error(), token_check.error_message());

        http_client_.clear_headers();
        http_client_.add_header("Authorization", "Bearer " + auth_manager_.get_access_token());

        auto response = http_client_.get("https://beta-api.crunchyroll.com/index/v2");
        if (!response || !response.value().is_success()) {
            return Result<json>(ErrorCode::NetworkError, "Failed to get CMS data");
        }

        try {
            json j = json::parse(response.value().body);
            return Result<json>(j["cms"]);
        } catch (const json::exception& e) {
            return Result<json>(ErrorCode::Unknown, "Parse error");
        }
    }

    const Config& config_;
    AuthManager auth_manager_;
    HttpClient http_client_;
    Downloader downloader_;
    Muxer muxer_;
    WidevineCDM widevine_;
};

// Public interface implementation
CrunchyrollClient::CrunchyrollClient(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

CrunchyrollClient::~CrunchyrollClient() = default;

Result<void> CrunchyrollClient::login() {
    return impl_->login();
}

Result<void> CrunchyrollClient::download_episode(const std::string& episode_id,
                                                 const std::vector<std::string>& audio_langs) {
    return impl_->download_episode(episode_id, audio_langs);
}

Result<Episode> CrunchyrollClient::get_episode(const std::string& episode_id) {
    return impl_->get_episode(episode_id);
}

Result<void> CrunchyrollClient::download_season(const std::string& season_id,
                                                const std::vector<std::string>& audio_langs) {
    return Result<void>(ErrorCode::Unknown, "download_season not implemented yet");
}

Result<void> CrunchyrollClient::download_series(const std::string& series_id,
                                                const std::vector<std::string>& audio_langs) {
    return Result<void>(ErrorCode::Unknown, "download_series not implemented yet");
}

EpisodeMetadata Episode::to_metadata() const {
    EpisodeMetadata meta;
    meta.id = id;
    meta.title = title;
    meta.series_id = series_id;
    meta.season_number = season_number;
    meta.episode_number = episode_number;
    return meta;
}

} // namespace crdl
