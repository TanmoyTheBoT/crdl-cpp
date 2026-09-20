#pragma once

#include "../core/types.h"
#include <vector>
#include <filesystem>

namespace crdl {

struct PSSSInfo {
    std::string pssh_base64;
    std::string kid_hex;
};

struct DRMKey {
    std::string kid;
    std::string key;
    std::string type;  // "CONTENT", "SIGNING", etc.
};

class WidevineCDM {
public:
    explicit WidevineCDM(const std::filesystem::path& device_path);
    ~WidevineCDM();

    // Parse PSSH box
    Result<PSSSInfo> parse_pssh(const std::string& pssh_base64);

    // Build PSSH from KID
    Result<std::string> build_pssh_from_kid(const std::string& kid_hex);

    // Get license keys
    Result<std::vector<DRMKey>> get_license_keys(
        const std::string& license_url,
        const std::string& pssh_base64,
        const std::string& video_token,
        const std::string& content_id,
        const std::string& bearer_token,
        const std::string& cookies,
        const std::filesystem::path& json_dir
    );

    // Extract MPD information
    Result<StreamInfo> extract_mpd_info(
        const std::string& mpd_url,
        const std::string& access_token = "",
        const std::string& content_id = "",
        const std::string& video_token = ""
    );

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace crdl
