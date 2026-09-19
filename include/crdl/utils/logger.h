#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace crdl {

class Logger {
public:
    static void init(const std::string& log_file, bool verbose = false);
    static void shutdown();

    static std::shared_ptr<spdlog::logger>& get() { return logger_; }

    template<typename... Args>
    static void info(Args&&... args) {
        if (logger_) logger_->info(std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(Args&&... args) {
        if (logger_) logger_->warn(std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(Args&&... args) {
        if (logger_) logger_->error(std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(Args&&... args) {
        if (logger_) logger_->debug(std::forward<Args>(args)...);
    }

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

// Convenience macros
#define LOG_INFO(...) crdl::Logger::info(__VA_ARGS__)
#define LOG_WARN(...) crdl::Logger::warn(__VA_ARGS__)
#define LOG_ERROR(...) crdl::Logger::error(__VA_ARGS__)
#define LOG_DEBUG(...) crdl::Logger::debug(__VA_ARGS__)

} // namespace crdl
