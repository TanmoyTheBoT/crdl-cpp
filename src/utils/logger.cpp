#include <crdl/utils/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace crdl {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(const std::string& log_file, bool verbose) {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
        sinks.push_back(console_sink);

        // File sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_level(spdlog::level::debug);
        sinks.push_back(file_sink);

        // Create logger
        logger_ = std::make_shared<spdlog::logger>("crdl", sinks.begin(), sinks.end());
        logger_->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
        logger_->flush_on(spdlog::level::info);

        spdlog::register_logger(logger_);

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void Logger::shutdown() {
    if (logger_) {
        logger_->flush();
        spdlog::drop_all();
        logger_.reset();
    }
}

} // namespace crdl
