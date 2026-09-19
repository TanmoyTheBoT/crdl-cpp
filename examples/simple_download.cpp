#include <crdl/api/crunchyroll_client.h>
#include <crdl/core/config.h>
#include <crdl/utils/logger.h>
#include <iostream>

int main() {
    try {
        // Setup
        crdl::Config config;
        config.username = "your_email@example.com";
        config.password = "your_password";
        config.quality = crdl::Quality::P1080;
        config.audio_languages = {"ja-JP"};

        config.setup_directories();
        crdl::Logger::init((config.logs_dir / "example.log").string(), true);

        // Create client
        crdl::CrunchyrollClient client(config);

        // Login
        std::cout << "Logging in...\n";
        auto login_result = client.login();
        if (!login_result) {
            std::cerr << "Login failed: " << login_result.error_message() << "\n";
            return 1;
        }
        std::cout << "✓ Logged in\n";

        // Download episode
        std::string episode_id = "G63VW2VWY"; // Replace with actual ID
        std::cout << "Downloading episode " << episode_id << "...\n";

        auto download_result = client.download_episode(episode_id);
        if (!download_result) {
            std::cerr << "Download failed: " << download_result.error_message() << "\n";
            return 1;
        }

        std::cout << "✓ Download complete\n";

        crdl::Logger::shutdown();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
