#include <crdl/api/auth_manager.h>
#include <crdl/utils/logger.h>
#include <crdl/utils/string_utils.h>
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

namespace crdl {

AuthManager::AuthManager(const Config& config)
    : config_(config), http_client_() {
    http_client_.set_user_agent(config.user_agent);
}

Result<void> AuthManager::login(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO("Attempting login for user: {}", username);

    // Build request body
    std::string data =
        "grant_type=password"
        "&username=" + string_utils::url_encode(username) +
        "&password=" + string_utils::url_encode(password) +
        "&scope=offline_access"
        "&client_id=" + std::string(CLIENT_ID) +
        "&client_secret=" + std::string(CLIENT_SECRET) +
        "&device_id=" + config_.device_id +
        "&device_type=" + string_utils::url_encode(config_.device_type) +
        "&device_name=" + string_utils::url_encode(config_.device_name);

    // Set headers
    http_client_.clear_headers();
    http_client_.add_header("Accept", "application/json");
    http_client_.add_header("Accept-Charset", "UTF-8");
    http_client_.add_header("Request-Type", "SignIn");
    http_client_.add_header("Authorization", AUTHORIZATION);
    http_client_.add_header("ETP-Anonymous-ID", config_.device_id);

    // Make request
    auto response = http_client_.post(AUTH_URL, data, "application/x-www-form-urlencoded; charset=UTF-8");
    if (!response) {
        LOG_ERROR("Login request failed: {}", response.error_message());
        return Result<void>(ErrorCode::NetworkError, response.error_message());
    }

    if (!response.value().is_success()) {
        LOG_ERROR("Login failed with status: {}", response.value().status_code);
        return Result<void>(ErrorCode::AuthenticationFailed,
            "HTTP " + std::to_string(response.value().status_code));
    }

    // Parse response
    try {
        json resp_json = json::parse(response.value().body);

        access_token_ = resp_json["access_token"];
        refresh_token_ = resp_json["refresh_token"];

        int expires_in = resp_json.value("expires_in", 3600);
        token_expiry_ = now() + std::chrono::seconds(expires_in);
        last_refresh_ = now();

        LOG_INFO("Login successful");
        return Result<void>(ErrorCode::Success);

    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse login response: {}", e.what());
        return Result<void>(ErrorCode::AuthenticationFailed, "Invalid response format");
    }
}

Result<void> AuthManager::refresh_token() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check cooldown
    auto since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now() - last_refresh_
    ).count();

    if (since_last < MIN_REFRESH_INTERVAL_SEC) {
        LOG_WARN("Token refresh attempted too soon ({}s since last)", since_last);
        return Result<void>(ErrorCode::InvalidArgument, "Too many refresh attempts");
    }

    if (refresh_token_.empty()) {
        LOG_ERROR("No refresh token available");
        return Result<void>(ErrorCode::TokenExpired, "No refresh token");
    }

    LOG_INFO("Refreshing access token");

    // Build request
    std::string data =
        "grant_type=refresh_token"
        "&refresh_token=" + string_utils::url_encode(refresh_token_) +
        "&scope=offline_access"
        "&client_id=" + std::string(CLIENT_ID) +
        "&client_secret=" + std::string(CLIENT_SECRET) +
        "&device_id=" + config_.device_id +
        "&device_type=" + string_utils::url_encode(config_.device_type) +
        "&device_name=" + string_utils::url_encode(config_.device_name);

    http_client_.clear_headers();
    http_client_.add_header("Accept", "application/json");
    http_client_.add_header("Authorization", AUTHORIZATION);
    http_client_.add_header("ETP-Anonymous-ID", config_.device_id);

    auto response = http_client_.post(AUTH_URL, data, "application/x-www-form-urlencoded; charset=UTF-8");
    if (!response || !response.value().is_success()) {
        LOG_ERROR("Token refresh failed");
        return Result<void>(ErrorCode::TokenExpired, "Refresh failed");
    }

    try {
        json resp_json = json::parse(response.value().body);

        access_token_ = resp_json["access_token"];
        refresh_token_ = resp_json["refresh_token"];

        int expires_in = resp_json.value("expires_in", 3600);
        token_expiry_ = now() + std::chrono::seconds(expires_in);
        last_refresh_ = now();

        LOG_INFO("Token refreshed successfully");
        return Result<void>(ErrorCode::Success);

    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse refresh response: {}", e.what());
        return Result<void>(ErrorCode::TokenExpired, "Invalid response");
    }
}

Result<void> AuthManager::ensure_valid_token() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (access_token_.empty()) {
        return Result<void>(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    // Check if token expires soon (60 second buffer)
    auto time_until_expiry = std::chrono::duration_cast<std::chrono::seconds>(
        token_expiry_ - now()
    ).count();

    if (time_until_expiry < TOKEN_REFRESH_BUFFER_SEC) {
        LOG_DEBUG("Token expires in {}s, refreshing", time_until_expiry);

        // Unlock during refresh to avoid deadlock
        mutex_.unlock();
        auto result = refresh_token();
        mutex_.lock();

        return result;
    }

    return Result<void>(ErrorCode::Success);
}

std::string AuthManager::get_access_token() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return access_token_;
}

bool AuthManager::is_token_valid() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (access_token_.empty()) return false;

    auto time_until_expiry = std::chrono::duration_cast<std::chrono::seconds>(
        token_expiry_ - now()
    ).count();

    return time_until_expiry > TOKEN_REFRESH_BUFFER_SEC;
}

bool AuthManager::is_logged_in() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !access_token_.empty();
}

void AuthManager::logout() {
    std::lock_guard<std::mutex> lock(mutex_);

    access_token_.clear();
    refresh_token_.clear();
    account_id_.clear();
    external_id_.clear();

    LOG_INFO("Logged out");
}

} // namespace crdl
