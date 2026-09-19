#pragma once

#include "../core/types.h"
#include <map>
#include <functional>

namespace crdl {

struct HttpResponse {
    int status_code{0};
    std::string body;
    std::map<std::string, std::string> headers;

    bool is_success() const { return status_code >= 200 && status_code < 300; }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Non-copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Configure
    void set_timeout(int seconds);
    void set_user_agent(const std::string& ua);
    void add_header(const std::string& key, const std::string& value);
    void clear_headers();
    std::string get_cookies_json();

    // HTTP methods
    Result<HttpResponse> get(const std::string& url);
    Result<HttpResponse> post(const std::string& url, const std::string& data,
                              const std::string& content_type = "application/x-www-form-urlencoded");
    Result<HttpResponse> post_json(const std::string& url, const std::string& json);
    Result<HttpResponse> post_binary(const std::string& url, const std::vector<uint8_t>& data);
    Result<HttpResponse> delete_request(const std::string& url);

    // Download to file
    Result<void> download_file(const std::string& url, const std::string& output_path,
                               std::function<void(size_t, size_t)> progress_callback = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace crdl
