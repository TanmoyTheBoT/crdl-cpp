#include <crdl/api/crunchyroll_client.h>
#include <crdl/core/config.h>
#include <crdl/utils/logger.h>
#include <crdl/utils/string_utils.h>
#include <crdl/media/downloader.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

void print_usage(const char* program_name) {
    std::cout << "CRDL - Crunchyroll Downloader v1.0.0\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Authentication:\n";
    std::cout << "  -u, --username USERNAME    Crunchyroll username\n";
    std::cout << "  -p, --password PASSWORD    Crunchyroll password\n\n";
    std::cout << "Content Selection:\n";
    std::cout << "  -e, --episode ID           Download episode by ID\n";
    std::cout << "  -S, --season ID            Download season by ID\n";
    std::cout << "  -s, --series ID            Download series by ID\n\n";
    std::cout << "Options:\n";
    std::cout << "  -q, --quality QUALITY      Video quality (1080p, 720p, best, worst)\n";
    std::cout << "  -a, --audio LANGS          Audio languages (comma-separated, e.g., ja-JP,en-US)\n";
    std::cout << "  -o, --output DIR           Output directory\n";
    std::cout << "  -v, --verbose              Enable verbose logging\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "  --version                  Show version information\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -e EPISODE_ID -u user@email.com -p password\n";
    std::cout << "  " << program_name << " -S SEASON_ID -q 720p -a \"ja-JP,en-US\"\n";
    std::cout << "  " << program_name << " -s SERIES_ID --quality best\n";
}

struct Args {
    std::string username;
    std::string password;
    std::string episode_id;
    std::string season_id;
    std::string series_id;
    std::string quality{"best"};
    std::string audio_langs{"ja-JP"};
    std::string output_dir;
    bool verbose{false};
    bool show_help{false};
    bool show_version{false};
};

Args parse_args(int argc, char* argv[]) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
        } else if (arg == "--version") {
            args.show_version = true;
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if ((arg == "-u" || arg == "--username") && i + 1 < argc) {
            args.username = argv[++i];
        } else if ((arg == "-p" || arg == "--password") && i + 1 < argc) {
            args.password = argv[++i];
        } else if ((arg == "-e" || arg == "--episode") && i + 1 < argc) {
            args.episode_id = argv[++i];
        } else if (arg == "-S" || arg == "--season") {
            if (i + 1 < argc) args.season_id = argv[++i];
        } else if ((arg == "-s" || arg == "--series") && i + 1 < argc) {
            args.series_id = argv[++i];
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            args.quality = argv[++i];
        } else if ((arg == "-a" || arg == "--audio") && i + 1 < argc) {
            args.audio_langs = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            args.output_dir = argv[++i];
        }
    }

    return args;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Set console to UTF-8 on Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto args = parse_args(argc, argv);

    if (args.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (args.show_version) {
        std::cout << "CRDL v1.0.0 - Professional C++ Edition\n";
        return 0;
    }

    try {
        // Initialize config
        crdl::Config config;
        config.verbose = args.verbose;

        // Setup logging
        config.setup_directories();
        auto log_file = (config.logs_dir / "crdl.log").string();
        crdl::Logger::init(log_file, args.verbose);

        LOG_INFO("CRDL starting...");

        // Load credentials from file if not provided
        if (args.username.empty() || args.password.empty()) {
            std::filesystem::path cred_path = config.credentials_path();
            if (std::filesystem::exists(cred_path)) {
                LOG_INFO("Loading credentials from: {}", cred_path.string());
                std::ifstream f(cred_path);
                if (f.is_open()) {
                    nlohmann::json cred_json;
                    f >> cred_json;
                    if (cred_json.contains("username")) config.username = cred_json["username"];
                    if (cred_json.contains("password")) config.password = cred_json["password"];
                    LOG_INFO("Credentials loaded successfully");
                }
            }
        }

        // Set credentials from command line (overrides file)
        if (!args.username.empty()) config.username = args.username;
        if (!args.password.empty()) config.password = args.password;

        // Set quality
        config.quality = crdl::string_to_quality(args.quality);

        // Set audio languages
        if (!args.audio_langs.empty()) {
            config.audio_languages = crdl::string_utils::split(args.audio_langs, ',');
        }

        // Set output directory
        if (!args.output_dir.empty()) {
            config.output_dir = args.output_dir;
        }

        // Validate config
        auto validation = config.validate();
        if (!validation) {
            LOG_ERROR("Configuration error: {}", validation.error_message());
            std::cerr << "Error: " << validation.error_message() << "\n";
            return 1;
        }

        // Check dependencies
        auto deps_check = crdl::Downloader::check_dependencies();
        if (!deps_check) {
            LOG_ERROR("Dependency check failed: {}", deps_check.error_message());
            std::cerr << "Error: " << deps_check.error_message() << "\n";
            std::cerr << "Please ensure N_m3u8DL-RE and mkvmerge are in your PATH\n";
            return 1;
        }

        // Create client
        crdl::CrunchyrollClient client(config);

        // Login
        LOG_INFO("Logging in...");
        auto login_result = client.login();
        if (!login_result) {
            LOG_ERROR("Login failed: {}", login_result.error_message());
            std::cerr << "Login failed: " << login_result.error_message() << "\n";
            return 1;
        }

        LOG_INFO("Login successful");

        // Save credentials if provided via command line
        if (!args.username.empty() && !args.password.empty()) {
            std::filesystem::path cred_path = config.credentials_path();
            LOG_INFO("Saving credentials to: {}", cred_path.string());
            std::ofstream f(cred_path);
            if (f.is_open()) {
                nlohmann::json cred_json;
                cred_json["username"] = config.username;
                cred_json["password"] = config.password;
                f << cred_json.dump(2);
                LOG_INFO("Credentials saved");
            }
        }
        std::cout << "✓ Logged in successfully\n";

        // Download content
        if (!args.episode_id.empty()) {
            LOG_INFO("Downloading episode: {}", args.episode_id);
            std::cout << "Downloading episode " << args.episode_id << "...\n";

            auto result = client.download_episode(args.episode_id, config.audio_languages);
            if (!result) {
                LOG_ERROR("Download failed: {}", result.error_message());
                std::cerr << "Error: " << result.error_message() << "\n";
                return 1;
            }

            std::cout << "✓ Episode downloaded successfully\n";
        } else if (!args.season_id.empty()) {
            LOG_INFO("Downloading season: {}", args.season_id);
            std::cout << "Downloading season " << args.season_id << "...\n";

            auto result = client.download_season(args.season_id, config.audio_languages);
            if (!result) {
                LOG_ERROR("Download failed: {}", result.error_message());
                std::cerr << "Error: " << result.error_message() << "\n";
                return 1;
            }

            std::cout << "✓ Season downloaded successfully\n";
        } else if (!args.series_id.empty()) {
            LOG_INFO("Downloading series: {}", args.series_id);
            std::cout << "Downloading series " << args.series_id << "...\n";

            auto result = client.download_series(args.series_id, config.audio_languages);
            if (!result) {
                LOG_ERROR("Download failed: {}", result.error_message());
                std::cerr << "Error: " << result.error_message() << "\n";
                return 1;
            }

            std::cout << "✓ Series downloaded successfully\n";
        } else {
            std::cerr << "Error: No content specified. Use -e, -S, or -s\n";
            print_usage(argv[0]);
            return 1;
        }

        LOG_INFO("CRDL completed successfully");
        crdl::Logger::shutdown();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
}
