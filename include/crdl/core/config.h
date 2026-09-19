#pragma once

#include "types.h"
#include <filesystem>

namespace crdl {

class Config {
public:
    // Authentication
    std::string username;
    std::string password;

    // Download settings
    Quality quality{Quality::P1080};
    std::vector<std::string> audio_languages{"ja-JP"};
    std::string release_group{"CRDL"};
    std::filesystem::path output_dir{"downloads"};

    // Paths
    std::filesystem::path config_dir;
    std::filesystem::path json_dir;
    std::filesystem::path widevine_dir;
    std::filesystem::path logs_dir;

    // Network settings
    int max_retries{3};
    int retry_delay_ms{2000};
    int connect_timeout_sec{30};

    // API settings
    std::string device_id;
    std::string device_type{"Xiaomi Redmi Note 7"};
    std::string device_name{"Redmi Note 7"};
    std::string user_agent{"Crunchyroll/ANDROIDTV/3.65.0_22347 (Android 12; en-US; SHIELD Android TV Build/SR1A.211012.001)"};

    // Logging
    bool enable_logging{true};
    bool verbose{false};

    Config();

    // Load from file
    static Result<Config> load_from_file(const std::filesystem::path& path);

    // Save to file
    Result<void> save_to_file(const std::filesystem::path& path) const;

    // Setup default directories
    void setup_directories();

    // Validate configuration
    Result<void> validate() const;

    // Get credentials file path
    std::filesystem::path credentials_path() const;

    // Get widevine device path
    std::filesystem::path widevine_device_path() const;

private:
    void generate_device_id();
    std::filesystem::path get_default_config_dir() const;
};

// Quality conversion
std::string quality_to_string(Quality q);
Quality string_to_quality(const std::string& s);

} // namespace crdl
