#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../utils/http_client.h"
#include <mutex>

namespace crdl {

class AuthManager {
public:
    explicit AuthManager(const Config& config);
    ~AuthManager() = default;

    // Login and get access token
    Result<void> login(const std::string& username, const std::string& password);

    // Refresh access token
    Result<void> refresh_token();

    // Check if token is valid (auto-refresh if needed)
    Result<void> ensure_valid_token();

    // Get current access token
    std::string get_access_token() const;

    // Get account info
    std::string get_account_id() const { return account_id_; }
    std::string get_external_id() const { return external_id_; }

    // Token validation
    bool is_token_valid() const;
    bool is_logged_in() const;

    // Clear session
    void logout();

private:
    const Config& config_;
    HttpClient http_client_;

    mutable std::mutex mutex_;
    std::string access_token_;
    std::string refresh_token_;
    TimePoint token_expiry_;
    TimePoint last_refresh_;

    std::string account_id_;
    std::string external_id_;

    // Constants
    static constexpr const char* AUTH_URL = "https://beta-api.crunchyroll.com/auth/v1/token";
    static constexpr const char* CLIENT_ID = "rjs0ltx0dbwkliwxdzdf";
    static constexpr const char* CLIENT_SECRET = "4V7rf21-UFXeZ-5XAd0X_QPwr1gu_i1s";
    static constexpr const char* AUTHORIZATION = "Basic cmpzMGx0eDBkYndrbGl3eGR6ZGY6NFY3cmYyMS1VRlhlWi01WEFkMFhfUVB3cjFndV9pMXM=";

    static constexpr int TOKEN_REFRESH_BUFFER_SEC = 60;
    static constexpr int MIN_REFRESH_INTERVAL_SEC = 5;
};

} // namespace crdl
