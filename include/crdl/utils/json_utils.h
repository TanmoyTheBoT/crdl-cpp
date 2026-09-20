#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <fstream>

namespace crdl {

// Save JSON to file in config json_dir for debugging
inline void save_json(const std::filesystem::path& json_dir,
                     const std::string& filename,
                     const nlohmann::json& data) {
    try {
        std::filesystem::create_directories(json_dir);
        std::ofstream file(json_dir / filename);
        if (file.is_open()) {
            file << data.dump(2);
        }
    } catch (...) {
        // ponytail: silent fail, debug helper only
    }
}

} // namespace crdl
