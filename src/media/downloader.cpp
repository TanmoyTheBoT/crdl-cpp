#include <crdl/media/downloader.h>
#include <crdl/utils/logger.h>
#include <crdl/utils/string_utils.h>
#include <crdl/utils/http_client.h>
#include <crdl/drm/widevine_cdm.h>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace crdl {

class Downloader::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {}

    Result<std::filesystem::path> download_video(
        const StreamInfo& stream_info,
        const EpisodeMetadata& metadata,
        const std::vector<DRMKey>& keys,
        Quality quality,
        const std::string& access_token = "") {

        LOG_INFO("Downloading video track");

        std::filesystem::path output_dir = config_.output_dir;
        std::filesystem::create_directories(output_dir);

        std::string filename = metadata.to_filename(quality_to_string(quality), config_.release_group);
        std::string video_filename = filename + "_video";

        // Build N_m3u8DL-RE command
        std::vector<std::string> args = {
            "N_m3u8DL-RE",
            stream_info.mpd_url,
            "--save-name", video_filename,
            "--save-dir", output_dir.string(),
            "--binary-merge", "false",
            "--drop-audio", ".*",
            "--drop-subtitle", ".*"
        };

        // Add quality selection with fallback
        // For specific resolutions, add :for=best as fallback if res doesn't match
        switch (quality) {
            case Quality::P1080:
                args.push_back("-sv");
                args.push_back("res=1080*:for=best");  // fallback to best if no 1080p
                break;
            case Quality::P720:
                args.push_back("-sv");
                args.push_back("res=720*:for=best");   // fallback to best if no 720p
                break;
            case Quality::Best:
                args.push_back("-sv");
                args.push_back("for=best");
                break;
            case Quality::Worst:
                args.push_back("-sv");
                args.push_back("for=worst");
                break;
        }

        // Add ALL decryption keys (all are needed for different quality streams)
        // BUT skip SIGNING keys - only use CONTENT keys
        if (!keys.empty()) {
            int content_keys = 0;
            for (const auto& key : keys) {
                if (key.type == "SIGNING") {
                    LOG_DEBUG("Skipping SIGNING key: {}", key.kid);
                    continue;
                }
                args.push_back("--key");
                args.push_back(key.kid + ":" + key.key);
                LOG_INFO("Added key: {}", key.kid);
                content_keys++;
            }
            LOG_INFO("Added {} CONTENT key(s) to video download command", content_keys);
        } else {
            LOG_WARN("No DRM keys provided - download will fail if content is encrypted");
        }

        // Add required headers (must match Python implementation)
        args.push_back("--header");
        args.push_back("accept-language: en-US,en;q=0.9");

        // Critical: Authorization bearer token
        if (!access_token.empty()) {
            args.push_back("--header");
            args.push_back("authorization: Bearer " + access_token);
        }

        args.push_back("--header");
        args.push_back("content-type: application/octet-stream");

        args.push_back("--header");
        args.push_back("referer: https://static.crunchyroll.com/");

        args.push_back("--header");
        args.push_back("user-agent: " + config_.user_agent);

        // Critical: Content ID
        if (!metadata.id.empty()) {
            args.push_back("--header");
            args.push_back("x-cr-content-id: " + metadata.id);
        }

        // Add video token header if available
        if (!stream_info.video_token.empty()) {
            args.push_back("--header");
            args.push_back("x-cr-video-token: " + stream_info.video_token);
        }

        // Execute download
        LOG_DEBUG("N_m3u8DL-RE command: {}", [&]() {
            std::string cmd;
            for (const auto& arg : args) {
                if (!cmd.empty()) cmd += " ";
                cmd += "\"" + arg + "\"";
            }
            return cmd;
        }());
        auto result = execute_command(args);
        if (result != 0) {
            return Result<std::filesystem::path>(ErrorCode::DownloadFailed,
                "Video download failed with code: " + std::to_string(result));
        }

        // Find downloaded video file (N_m3u8DL-RE outputs to current directory or a subfolder)
        std::filesystem::path expected_file = output_dir / (video_filename + ".mp4");
        if (std::filesystem::exists(expected_file)) {
            LOG_INFO("Video downloaded: {}", expected_file.string());
            return Result<std::filesystem::path>(expected_file);
        }

        // Try .m4v extension
        expected_file = output_dir / (video_filename + ".m4v");
        if (std::filesystem::exists(expected_file)) {
            LOG_INFO("Video downloaded: {}", expected_file.string());
            return Result<std::filesystem::path>(expected_file);
        }

        // N_m3u8DL-RE sometimes creates a folder with segments - look for any .mp4/.m4v
        for (const auto& entry : std::filesystem::directory_iterator(output_dir)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                // Match files ending with _video.mp4 or _video.m4v
                if (fname.find("_video.mp4") != std::string::npos || fname.find("_video.m4v") != std::string::npos) {
                    LOG_INFO("Video downloaded: {}", entry.path().string());
                    return Result<std::filesystem::path>(entry.path());
                }
            }
        }

        LOG_ERROR("Expected file not found: {}", (output_dir / (video_filename + ".mp4")).string());
        return Result<std::filesystem::path>(ErrorCode::FileNotFound, "Video file not found after download");
    }

    Result<std::filesystem::path> download_audio(
        const StreamInfo& stream_info,
        const EpisodeMetadata& metadata,
        const std::vector<DRMKey>& keys,
        const std::string& audio_lang,
        const std::string& access_token = "") {

        LOG_INFO("Downloading audio: {}", audio_lang);

        std::filesystem::path output_dir = config_.output_dir;
        std::string filename = metadata.to_filename("", config_.release_group);
        std::string audio_filename = filename + "_audio_" + audio_lang;

        std::vector<std::string> args = {
            "N_m3u8DL-RE",
            stream_info.mpd_url,
            "--save-name", audio_filename,
            "--save-dir", output_dir.string(),
            "--binary-merge", "false",
            "--drop-video", ".*",
            "--select-audio", "best",
            "--drop-subtitle", ".*"
        };

        // Add ALL decryption keys (all are needed for different quality streams)
        if (!keys.empty()) {
            LOG_INFO("Adding {} DRM key(s) to audio download command", keys.size());
            for (const auto& key : keys) {
                args.push_back("--key");
                args.push_back(key.kid + ":" + key.key);
                LOG_INFO("Added key: {}", key.kid);
            }
        }

        // Add required headers
        args.push_back("--header");
        args.push_back("accept-language: en-US,en;q=0.9");

        if (!access_token.empty()) {
            args.push_back("--header");
            args.push_back("authorization: Bearer " + access_token);
        }

        args.push_back("--header");
        args.push_back("content-type: application/octet-stream");

        args.push_back("--header");
        args.push_back("referer: https://static.crunchyroll.com/");

        args.push_back("--header");
        args.push_back("user-agent: " + config_.user_agent);

        if (!metadata.id.empty()) {
            args.push_back("--header");
            args.push_back("x-cr-content-id: " + metadata.id);
        }

        if (!stream_info.video_token.empty()) {
            args.push_back("--header");
            args.push_back("x-cr-video-token: " + stream_info.video_token);
        }

        auto result = execute_command(args);
        if (result != 0) {
            return Result<std::filesystem::path>(ErrorCode::DownloadFailed,
                "Audio download failed");
        }

        // Find downloaded audio file - match _audio_<lang> prefix
        for (const auto& entry : std::filesystem::directory_iterator(output_dir)) {
            std::string fname = entry.path().filename().string();
            if (fname.find("_audio_" + audio_lang) != std::string::npos &&
                (fname.size() >= 4 && (fname.substr(fname.size() - 4) == ".m4a" || fname.substr(fname.size() - 4) == ".aac"))) {
                LOG_INFO("Audio downloaded: {}", entry.path().string());
                return Result<std::filesystem::path>(entry.path());
            }
        }

        return Result<std::filesystem::path>(ErrorCode::FileNotFound, "Audio file not found");
    }

    Result<std::vector<std::filesystem::path>> download_subtitles(
        const std::map<std::string, Subtitle>& subtitles,
        const EpisodeMetadata& metadata,
        const std::filesystem::path& output_dir) {

        std::filesystem::create_directories(output_dir);
        std::vector<std::filesystem::path> subtitle_files;

        HttpClient client;

        for (const auto& [lang, sub] : subtitles) {
            try {
                std::string filename = metadata.to_filename("", "") + "." + lang + "." + sub.format;
                std::filesystem::path file_path = output_dir / filename;

                LOG_INFO("Downloading subtitle: {}", lang);

                auto response = client.get(sub.url);
                if (response && response.value().is_success()) {
                    std::ofstream file(file_path, std::ios::binary);
                    file.write(response.value().body.data(), response.value().body.size());
                    file.close();

                    subtitle_files.push_back(file_path);
                    LOG_INFO("Subtitle saved: {}", file_path.string());
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to download subtitle {}: {}", lang, e.what());
            }
        }

        return Result<std::vector<std::filesystem::path>>(subtitle_files);
    }

    static Result<void> check_dependencies() {
        // Check N_m3u8DL-RE
        if (execute_command({"N_m3u8DL-RE", "--version"}) != 0) {
            return Result<void>(ErrorCode::FileNotFound,
                "N_m3u8DL-RE not found in PATH");
        }

        // Check mkvmerge
        if (execute_command({"mkvmerge", "--version"}) != 0) {
            return Result<void>(ErrorCode::FileNotFound,
                "mkvmerge not found in PATH");
        }

        return Result<void>(ErrorCode::Success);
    }

private:
    static int execute_command(const std::vector<std::string>& args) {
#ifdef _WIN32
        // Windows: Use CreateProcess
        std::string cmdline;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmdline += " ";
            // Quote arguments with spaces
            if (args[i].find(' ') != std::string::npos) {
                cmdline += "\"" + args[i] + "\"";
            } else {
                cmdline += args[i];
            }
        }

        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessA(nullptr, const_cast<char*>(cmdline.c_str()),
                           nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            return -1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exit_code);
#else
        // Linux/macOS: Use fork/exec
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            std::vector<char*> argv;
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            _exit(127); // exec failed
        } else if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
        return -1;
#endif
    }

    const Config& config_;
};

// Public interface
Downloader::Downloader(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

Downloader::~Downloader() = default;

Result<std::filesystem::path> Downloader::download_video(
    const StreamInfo& stream_info,
    const EpisodeMetadata& metadata,
    const std::vector<DRMKey>& keys,
    Quality quality,
    const std::string& access_token) {
    return impl_->download_video(stream_info, metadata, keys, quality, access_token);
}

Result<std::filesystem::path> Downloader::download_audio(
    const StreamInfo& stream_info,
    const EpisodeMetadata& metadata,
    const std::vector<DRMKey>& keys,
    const std::string& audio_lang,
    const std::string& access_token) {
    return impl_->download_audio(stream_info, metadata, keys, audio_lang, access_token);
}

Result<std::vector<std::filesystem::path>> Downloader::download_subtitles(
    const std::map<std::string, Subtitle>& subtitles,
    const EpisodeMetadata& metadata,
    const std::filesystem::path& output_dir) {
    return impl_->download_subtitles(subtitles, metadata, output_dir);
}

Result<void> Downloader::check_dependencies() {
    return Impl::check_dependencies();
}

} // namespace crdl
