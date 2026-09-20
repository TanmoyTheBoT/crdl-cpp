#include <crdl/drm/widevine_cdm.h>
#include <crdl/utils/logger.h>
#include <crdl/utils/string_utils.h>
#include <crdl/utils/http_client.h>
#include <widevine/cdm.h>
#include <widevine/device.h>
#include <widevine/pssh.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <fstream>
#include <iomanip>

using json = nlohmann::json;

namespace crdl {

class WidevineCDM::Impl {
public:
    explicit Impl(const std::filesystem::path& device_path)
        : device_path_(device_path) {

        if (!std::filesystem::exists(device_path)) {
            LOG_ERROR("Widevine device file not found: {}", device_path.string());
            LOG_ERROR("You need a Widevine device (.wvd) file to decrypt DRM content");
            LOG_ERROR("Place it at: {}", device_path.string());
        } else {
            LOG_INFO("Found Widevine device at: {}", device_path.string());
        }
    }

    Result<PSSSInfo> parse_pssh(const std::string& pssh_base64) {
        try {
            auto pssh_bytes = string_utils::base64_decode(pssh_base64);

            if (pssh_bytes.size() < 32) {
                return Result<PSSSInfo>(ErrorCode::DRMError, "PSSH too small");
            }

            // Check for 'pssh' magic
            if (pssh_bytes[4] != 'p' || pssh_bytes[5] != 's' ||
                pssh_bytes[6] != 's' || pssh_bytes[7] != 'h') {
                return Result<PSSSInfo>(ErrorCode::DRMError, "Invalid PSSH magic");
            }

            PSSSInfo info;
            info.pssh_base64 = pssh_base64;

            // Get version
            uint8_t version = pssh_bytes[8];

            if (version == 1) {
                // Version 1: KID count at offset 28, then KIDs
                uint32_t kid_count = (pssh_bytes[28] << 24) | (pssh_bytes[29] << 16) |
                                    (pssh_bytes[30] << 8) | pssh_bytes[31];

                if (kid_count > 0 && pssh_bytes.size() >= 48) {
                    // First KID at offset 32
                    std::stringstream ss;
                    ss << std::hex << std::setfill('0');
                    for (int i = 32; i < 48; ++i) {
                        ss << std::setw(2) << static_cast<int>(pssh_bytes[i]);
                    }
                    info.kid_hex = ss.str();
                }
            } else if (version == 0) {
                // Version 0: Parse data field for protobuf KID
                if (pssh_bytes.size() > 32) {
                    uint32_t data_size = (pssh_bytes[28] << 24) | (pssh_bytes[29] << 16) |
                                        (pssh_bytes[30] << 8) | pssh_bytes[31];

                    if (data_size > 0 && pssh_bytes.size() >= 32 + data_size) {
                        // Look for protobuf pattern: 0x12 0x10 (field tag for KID)
                        for (size_t i = 32; i < 32 + data_size - 18; ++i) {
                            if (pssh_bytes[i] == 0x12 && pssh_bytes[i + 1] == 0x10) {
                                // Found KID
                                std::stringstream ss;
                                ss << std::hex << std::setfill('0');
                                for (int j = 0; j < 16; ++j) {
                                    ss << std::setw(2) << static_cast<int>(pssh_bytes[i + 2 + j]);
                                }
                                info.kid_hex = ss.str();
                                break;
                            }
                        }
                    }
                }
            }

            return Result<PSSSInfo>(info);

        } catch (const std::exception& e) {
            return Result<PSSSInfo>(ErrorCode::DRMError,
                std::string("PSSH parse error: ") + e.what());
        }
    }

    Result<std::string> build_pssh_from_kid(const std::string& kid_hex) {
        try {
            std::vector<uint8_t> kid_bytes;
            for (size_t i = 0; i < kid_hex.length(); i += 2) {
                std::string byte_str = kid_hex.substr(i, 2);
                kid_bytes.push_back(static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16)));
            }

            if (kid_bytes.size() != 16) {
                return Result<std::string>(ErrorCode::DRMError, "Invalid KID length");
            }

            std::vector<uint8_t> pssh_box;

            // PSSH v1 header
            pssh_box.push_back(0x00); pssh_box.push_back(0x00);
            pssh_box.push_back(0x00); pssh_box.push_back(0x34); // Size: 52 bytes
            pssh_box.push_back('p'); pssh_box.push_back('s');
            pssh_box.push_back('s'); pssh_box.push_back('h');
            pssh_box.push_back(0x01); pssh_box.push_back(0x00);
            pssh_box.push_back(0x00); pssh_box.push_back(0x00); // Version 1

            // Widevine system ID: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed
            uint8_t system_id[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
                                  0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed};
            pssh_box.insert(pssh_box.end(), system_id, system_id + 16);

            // KID count (1)
            pssh_box.push_back(0x00); pssh_box.push_back(0x00);
            pssh_box.push_back(0x00); pssh_box.push_back(0x01);

            // KID
            pssh_box.insert(pssh_box.end(), kid_bytes.begin(), kid_bytes.end());

            // Data size (0)
            pssh_box.push_back(0x00); pssh_box.push_back(0x00);
            pssh_box.push_back(0x00); pssh_box.push_back(0x00);

            return Result<std::string>(string_utils::base64_encode(pssh_box));

        } catch (const std::exception& e) {
            return Result<std::string>(ErrorCode::DRMError,
                std::string("PSSH build error: ") + e.what());
        }
    }

    Result<StreamInfo> extract_mpd_info(
        const std::string& mpd_url,
        const std::string& access_token,
        const std::string& content_id,
        const std::string& video_token) {

        StreamInfo info;

        LOG_INFO("Downloading MPD to extract DRM info...");

        // Download MPD with auth headers
        HttpClient client;
        if (!access_token.empty()) {
            client.add_header("Authorization", "Bearer " + access_token);
        }
        if (!content_id.empty()) {
            client.add_header("X-Cr-Content-Id", content_id);
        }
        if (!video_token.empty()) {
            client.add_header("X-Cr-Video-Token", video_token);
        }

        auto response = client.get(mpd_url);
        if (!response || !response.value().is_success()) {
            LOG_ERROR("Failed to download MPD: HTTP {}", response ? response.value().status_code : 0);
            return Result<StreamInfo>(ErrorCode::NetworkError, "Failed to download MPD");
        }

        std::string mpd_content = response.value().body;
        LOG_DEBUG("MPD downloaded, size: {} bytes", mpd_content.size());

        // Extract PSSH - find all cenc:pssh tags and check which is Widevine
        std::regex pssh_regex(R"(<cenc:pssh[^>]*>([^<]+)</cenc:pssh>)");
        auto pssh_begin = std::sregex_iterator(mpd_content.begin(), mpd_content.end(), pssh_regex);
        auto pssh_end = std::sregex_iterator();

        for (std::sregex_iterator i = pssh_begin; i != pssh_end; ++i) {
            std::string pssh_candidate = (*i)[1].str();

            // Decode and check if it's Widevine (system ID: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed)
            try {
                auto pssh_bytes = string_utils::base64_decode(pssh_candidate);
                if (pssh_bytes.size() >= 32) {
                    // Check for Widevine system ID at offset 12-27
                    uint8_t widevine_id[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
                                           0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed};
                    bool is_widevine = true;
                    for (int j = 0; j < 16; ++j) {
                        if (pssh_bytes[12 + j] != widevine_id[j]) {
                            is_widevine = false;
                            break;
                        }
                    }
                    if (is_widevine) {
                        info.pssh = pssh_candidate;
                        LOG_INFO("Found Widevine PSSH in MPD");
                        break;
                    }
                }
            } catch (...) {
                continue;
            }
        }

        if (info.pssh.empty()) {
            LOG_WARN("No Widevine PSSH found in MPD");
        }

        // Extract KID from ContentProtection
        std::regex kid_regex("cenc:default_KID=\"([^\"]+)\"");
        std::smatch match;
        if (std::regex_search(mpd_content, match, kid_regex)) {
            std::string kid_with_dashes = match[1].str();
            // Remove dashes
            std::string kid;
            for (char c : kid_with_dashes) {
                if (c != '-') kid += c;
            }
            info.kid = kid;
            LOG_INFO("Found KID: {}", kid);

            // If no PSSH found, build from KID
            if (info.pssh.empty()) {
                LOG_INFO("No PSSH in MPD, building from KID...");
                auto pssh_result = build_pssh_from_kid(kid);
                if (pssh_result) {
                    info.pssh = pssh_result.value();
                    LOG_INFO("Built PSSH from KID successfully");
                }
            }
        } else {
            LOG_WARN("No KID found in MPD");
        }

        // License URL (use default Crunchyroll one)
        info.license_url = "https://cr-license-proxy.prd.crunchyrollsvc.com/v1/license/widevine";

        return Result<StreamInfo>(info);
    }

    Result<std::vector<DRMKey>> get_license_keys(
        const std::string& license_url,
        const std::string& pssh_base64,
        const std::string& video_token,
        const std::string& content_id,
        const std::string& bearer_token,
        const std::string& cookies = "") {

        LOG_INFO("=== Getting Widevine License Keys (Native C++) ===");
        LOG_INFO("PSSH: {}", pssh_base64.substr(0, 50) + "...");
        LOG_INFO("License URL: {}", license_url);
        LOG_INFO("Content ID: {}", content_id);

        try {
            // Load device
            auto device = widevine::Device::from_wvd(device_path_.string());

            // Create CDM
            widevine::CDM cdm(device);

            // Open session
            auto session_id = cdm.open_session();

            // RAII wrapper to ensure session cleanup
            struct SessionGuard {
                widevine::CDM& cdm;
                std::vector<uint8_t> session_id;
                bool active = true;

                SessionGuard(widevine::CDM& c, std::vector<uint8_t> sid)
                    : cdm(c), session_id(std::move(sid)) {}

                ~SessionGuard() {
                    if (active) {
                        try {
                            cdm.close_session(session_id);
                        } catch (...) {
                            // Suppress exceptions in destructor
                        }
                    }
                }

                void release() { active = false; }
            };

            SessionGuard guard(cdm, session_id);

            // Parse PSSH
            auto pssh = widevine::PSSH::from_base64(pssh_base64);

            // Generate challenge
            auto challenge = cdm.get_license_challenge(
                session_id,
                pssh,
                widevine::LicenseType::STREAMING,
                true
            );

            // Send to license server
            HttpClient client;
            client.add_header("Authorization", "Bearer " + bearer_token);
            client.add_header("User-Agent", "Crunchyroll/ANDROIDTV/3.70.0_22358 (Android 12; en-US; SHIELD Android TV Build/SR1A.220624.014)");
            client.add_header("Accept", "*/*");
            client.add_header("Accept-Encoding", "gzip");
            client.add_header("Connection", "Keep-Alive");
            client.add_header("X-Cr-Content-Id", content_id);
            client.add_header("X-Cr-Video-Token", video_token);

            LOG_INFO("License request headers:");
            LOG_INFO("  Authorization: Bearer {}...", bearer_token.substr(0, 20));
            LOG_INFO("  X-Cr-Content-Id: {}", content_id);
            LOG_INFO("  X-Cr-Video-Token: {}...", video_token.substr(0, 20));
            LOG_INFO("  Challenge size: {} bytes", challenge.size());

            // Use post_binary to send raw bytes with application/octet-stream
            auto response = client.post_binary(license_url, challenge);

            if (!response || !response.value().is_success()) {
                int status_code = response ? response.value().status_code : 0;
                std::string error_body = response ? response.value().body.substr(0, 200) : "no response";
                LOG_ERROR("License request failed: HTTP {}, body: {}", status_code, error_body);
                return Result<std::vector<DRMKey>>(
                    ErrorCode::DRMError,
                    "License request failed: HTTP " + std::to_string(status_code)
                );
            }

            // Parse license response - Crunchyroll returns JSON with base64-encoded license
            std::vector<uint8_t> license_data;
            try {
                json response_json = json::parse(response.value().body);
                if (response_json.contains("license")) {
                    // Decode base64 license
                    std::string license_b64 = response_json["license"];
                    auto decoded = string_utils::base64_decode(license_b64);
                    license_data = std::vector<uint8_t>(decoded.begin(), decoded.end());
                    LOG_INFO("Parsed JSON license response, decoded {} bytes", license_data.size());
                } else {
                    LOG_WARN("No 'license' field in JSON response, using raw body");
                    license_data = std::vector<uint8_t>(
                        response.value().body.begin(),
                        response.value().body.end()
                    );
                }
            } catch (const json::exception& e) {
                // Not JSON, use raw response
                LOG_INFO("License response is not JSON, using raw body");
                license_data = std::vector<uint8_t>(
                    response.value().body.begin(),
                    response.value().body.end()
                );
            }

            cdm.parse_license(session_id, license_data);

            // Extract keys
            auto widevine_keys = cdm.get_keys(session_id);

            // Convert to crdl DRMKey format
            std::vector<DRMKey> keys;
            for (const auto& wv_key : widevine_keys) {
                DRMKey key;
                key.kid = wv_key.kid_hex();
                key.key = wv_key.key_hex();
                key.type = wv_key.type;
                keys.push_back(key);

                LOG_INFO("Retrieved DRM Key:");
                LOG_INFO("  KID: {}", key.kid);
                LOG_INFO("  Key: {}", key.key);
                LOG_INFO("  Type: {}", key.type);
            }

            LOG_INFO("Successfully retrieved {} decryption key(s)", keys.size());
            return Result<std::vector<DRMKey>>(keys);

        } catch (const std::exception& e) {
            LOG_ERROR("Widevine error: {}", e.what());
            return Result<std::vector<DRMKey>>(
                ErrorCode::DRMError,
                std::string("Widevine error: ") + e.what());
        }
    }

private:
    std::filesystem::path device_path_;
};

// Public API implementation
WidevineCDM::WidevineCDM(const std::filesystem::path& device_path)
    : impl_(std::make_unique<Impl>(device_path)) {
}

WidevineCDM::~WidevineCDM() = default;

Result<PSSSInfo> WidevineCDM::parse_pssh(const std::string& pssh_base64) {
    return impl_->parse_pssh(pssh_base64);
}

Result<StreamInfo> WidevineCDM::extract_mpd_info(
    const std::string& mpd_url,
    const std::string& access_token,
    const std::string& content_id,
    const std::string& video_token) {
    return impl_->extract_mpd_info(mpd_url, access_token, content_id, video_token);
}

Result<std::vector<DRMKey>> WidevineCDM::get_license_keys(
    const std::string& license_url,
    const std::string& pssh_base64,
    const std::string& video_token,
    const std::string& content_id,
    const std::string& bearer_token,
    const std::string& cookies) {
    return impl_->get_license_keys(license_url, pssh_base64, video_token, content_id, bearer_token, cookies);
}

} // namespace crdl
