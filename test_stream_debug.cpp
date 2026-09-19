#include <crdl/api/crunchyroll_client.h>
#include <crdl/core/config.h>
#include <crdl/utils/logger.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

int main() {
    crdl::Logger::init("test_stream");
    
    crdl::Config config;
    config.setup_directories();
    
    // Load credentials
    std::ifstream f(config.credentials_path());
    nlohmann::json cred_json;
    f >> cred_json;
    config.username = cred_json["username"];
    config.password = cred_json["password"];
    
    crdl::CrunchyrollClient client(config);
    auto login_result = client.login();
    if (!login_result) {
        std::cerr << "Login failed: " << login_result.error_message() << std::endl;
        return 1;
    }
    
    std::cout << "=== Testing Stream Retrieval ===" << std::endl;
    auto ep_result = client.get_episode("GE00376426JAJP");
    if (!ep_result) {
        std::cerr << "Get episode failed: " << ep_result.error_message() << std::endl;
        return 1;
    }
    
    auto episode = ep_result.value();
    std::cout << "Episode ID: " << episode.id << std::endl;
    std::cout << "Title: " << episode.title << std::endl;
    std::cout << "Versions: " << episode.versions.size() << std::endl;
    
    for (const auto& v : episode.versions) {
        std::cout << "  - " << v.audio_locale << ": " << v.guid 
                  << " (original=" << v.is_original << ")" << std::endl;
    }
    
    std::string stream_guid = episode.id;
    if (!episode.versions.empty()) {
        for (const auto& ver : episode.versions) {
            if (ver.is_original) {
                stream_guid = ver.guid;
                break;
            }
        }
        if (stream_guid == episode.id && !episode.versions.empty()) {
            stream_guid = episode.versions[0].guid;
        }
    }
    
    std::cout << "\nUsing stream GUID: " << stream_guid << std::endl;
    
    return 0;
}
