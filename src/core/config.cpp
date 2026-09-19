#include <crdl/core/config.h>
#include <crdl/utils/string_utils.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace crdl {

Config::Config() {
    generate_device_id();
    config_dir = get_default_config_dir();
    json_dir = config_dir / "json";
    widevine_dir = config_dir / "widevine";
    logs_dir = config_dir / "logs";
}

void Config::generate_device_id() {
    device_id = "2b58e6c0-14df-4c62-85ed-ca0076af08ea";
}

std::filesystem::path Config::get_default_config_dir() const {
#ifdef _WIN32
    // Use %USERPROFILE%\.config\crdl for Windows (consistent with user's request)
    char* userprofile = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&userprofile, &sz, "USERPROFILE") == 0 && userprofile != nullptr) {
        std::filesystem::path result = userprofile;
        free(userprofile);
        return result / ".config" / "crdl";
    }
    // Fallback to APPDATA
    char* appdata = nullptr;
    if (_dupenv_s(&appdata, &sz, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path result = appdata;
        free(appdata);
        return result / "crdl";
    }
    return std::filesystem::path(".") / "crdl";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    return std::filesystem::path(home) / ".config" / "crdl";
#endif
}

void Config::setup_directories() {
    std::filesystem::create_directories(config_dir);
    std::filesystem::create_directories(json_dir);
    std::filesystem::create_directories(widevine_dir);
    std::filesystem::create_directories(logs_dir);
    std::filesystem::create_directories(output_dir);
}

Result<void> Config::validate() const {
    if (username.empty() || password.empty()) {
        return Result<void>(ErrorCode::InvalidArgument, "Username and password required");
    }

    if (!std::filesystem::exists(widevine_device_path())) {
        return Result<void>(ErrorCode::FileNotFound,
            "Widevine device file not found at: " + widevine_device_path().string());
    }

    return Result<void>(ErrorCode::Success);
}

std::filesystem::path Config::credentials_path() const {
    return config_dir / "credentials.json";
}

std::filesystem::path Config::widevine_device_path() const {
    return widevine_dir / "device.wvd";
}

Result<Config> Config::load_from_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<Config>(ErrorCode::FileNotFound, "Config file not found");
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<Config>(ErrorCode::FileNotFound, "Cannot open config file");
    }

    try {
        json j;
        file >> j;

        Config config;
        if (j.contains("username")) config.username = j["username"];
        if (j.contains("password")) config.password = j["password"];
        if (j.contains("quality")) config.quality = string_to_quality(j["quality"]);
        if (j.contains("output_dir")) config.output_dir = j["output_dir"].get<std::string>();
        if (j.contains("release_group")) config.release_group = j["release_group"];

        if (j.contains("audio_languages")) {
            config.audio_languages.clear();
            for (const auto& lang : j["audio_languages"]) {
                config.audio_languages.push_back(lang);
            }
        }

        return Result<Config>(config);

    } catch (const std::exception& e) {
        return Result<Config>(ErrorCode::Unknown, std::string("JSON parse error: ") + e.what());
    }
}

Result<void> Config::save_to_file(const std::filesystem::path& path) const {
    try {
        json j;
        j["username"] = username;
        j["password"] = password;
        j["quality"] = quality_to_string(quality);
        j["output_dir"] = output_dir.string();
        j["release_group"] = release_group;
        j["audio_languages"] = audio_languages;

        std::ofstream file(path);
        if (!file.is_open()) {
            return Result<void>(ErrorCode::Unknown, "Cannot create config file");
        }

        file << j.dump(4);
        return Result<void>(ErrorCode::Success);

    } catch (const std::exception& e) {
        return Result<void>(ErrorCode::Unknown, std::string("Save error: ") + e.what());
    }
}

std::string quality_to_string(Quality q) {
    switch (q) {
        case Quality::P1080: return "1080p";
        case Quality::P720: return "720p";
        case Quality::Best: return "best";
        case Quality::Worst: return "worst";
        default: return "1080p";
    }
}

Quality string_to_quality(const std::string& s) {
    if (s == "1080p") return Quality::P1080;
    if (s == "720p") return Quality::P720;
    if (s == "best") return Quality::Best;
    if (s == "worst") return Quality::Worst;
    return Quality::P1080;
}

} // namespace crdl
