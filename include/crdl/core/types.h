#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>

namespace crdl {

// Forward declarations
class Episode;
class Season;
class Series;

// Quality enum
enum class Quality {
    P1080,
    P720,
    Best,
    Worst
};

// Error codes
enum class ErrorCode {
    Success = 0,
    AuthenticationFailed,
    NetworkError,
    InvalidCredentials,
    TokenExpired,
    TooManyActiveStreams,
    StreamNotFound,
    DownloadFailed,
    MuxingFailed,
    DRMError,
    FileNotFound,
    InvalidArgument,
    Unknown
};

// Audio language
struct AudioLanguage {
    std::string code;        // e.g., "ja-JP"
    std::string name;        // e.g., "Japanese"
    bool is_original{false};

    AudioLanguage() = default;
    AudioLanguage(const std::string& c, const std::string& n = "", bool orig = false)
        : code(c), name(n), is_original(orig) {}
};

// Subtitle info
struct Subtitle {
    std::string language;
    std::string url;
    std::string format;  // "ass", "srt", etc.
};

// Stream info
struct StreamInfo {
    std::string mpd_url;
    std::string video_token;
    std::string pssh;
    std::string kid;
    std::string license_url;
    std::vector<std::pair<std::string, std::string>> keys;  // (kid, key)
    std::map<std::string, Subtitle> subtitles;
};

// Episode metadata
struct EpisodeMetadata {
    std::string id;
    std::string title;
    std::string series_title;
    std::string series_id;
    int season_number{1};
    int episode_number{1};
    std::string audio_locale;
    std::vector<AudioLanguage> available_audio;

    std::string to_filename(const std::string& quality, const std::string& release_group) const;
};

// Chapter marker
struct Chapter {
    double time_seconds;
    std::string title;
};

// Version info (for multi-audio)
struct Version {
    std::string guid;
    std::string audio_locale;
    bool is_original{false};
};

// Result type for error handling
template<typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_(ErrorCode::Success) {}
    Result(ErrorCode error, std::string message = "")
        : error_(error), error_message_(std::move(message)) {}

    bool is_ok() const { return error_ == ErrorCode::Success; }
    bool is_error() const { return error_ != ErrorCode::Success; }

    const T& value() const { return value_; }
    T& value() { return value_; }

    ErrorCode error() const { return error_; }
    const std::string& error_message() const { return error_message_; }

    explicit operator bool() const { return is_ok(); }

    // Helper to propagate errors
    template<typename U>
    Result<U> propagate_error() const {
        return Result<U>(error_, error_message_);
    }

private:
    T value_{};
    ErrorCode error_;
    std::string error_message_;
};

// Specialization for void (for functions that return nothing on success)
template<>
class Result<void> {
public:
    Result() : error_(ErrorCode::Success) {}
    Result(ErrorCode error, std::string message = "")
        : error_(error), error_message_(std::move(message)) {}

    bool is_ok() const { return error_ == ErrorCode::Success; }
    bool is_error() const { return error_ != ErrorCode::Success; }

    ErrorCode error() const { return error_; }
    const std::string& error_message() const { return error_message_; }

    explicit operator bool() const { return is_ok(); }

    template<typename U>
    Result<U> propagate_error() const {
        return Result<U>(error_, error_message_);
    }

private:
    ErrorCode error_;
    std::string error_message_;
};

// Time utilities
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::seconds;

inline TimePoint now() {
    return std::chrono::system_clock::now();
}

inline int64_t timestamp_seconds(const TimePoint& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()
    ).count();
}

} // namespace crdl
