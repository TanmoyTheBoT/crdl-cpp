#include <crdl/utils/http_client.h>
#include <crdl/utils/logger.h>
#include <curl/curl.h>
#include <fstream>
#include <sstream>

namespace crdl {

// CURL callback for writing data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// CURL callback for header parsing
static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);

    std::string header(buffer, total_size);
    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 2);

        // Trim newline
        if (!value.empty() && value.back() == '\n') value.pop_back();
        if (!value.empty() && value.back() == '\r') value.pop_back();

        (*headers)[key] = value;
    }

    return total_size;
}

class HttpClient::Impl {
public:
    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_ = curl_easy_init();
        timeout_sec_ = 30;

        // Enable cookie engine
        if (curl_) {
            curl_easy_setopt(curl_, CURLOPT_COOKIEFILE, "");
        }
    }

    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
        curl_global_cleanup();
    }

    void set_timeout(int seconds) {
        timeout_sec_ = seconds;
    }

    void set_user_agent(const std::string& ua) {
        user_agent_ = ua;
    }

    void add_header(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    void clear_headers() {
        headers_.clear();
    }

    std::string get_cookies_json() {
        if (!curl_) return "{}";

        struct curl_slist* cookies = nullptr;
        curl_easy_getinfo(curl_, CURLINFO_COOKIELIST, &cookies);

        std::stringstream json;
        json << "{";
        bool first = true;

        for (struct curl_slist* nc = cookies; nc; nc = nc->next) {
            // Cookie format: domain\tflag\tpath\tsecure\texpiration\tname\tvalue
            std::string cookie_line(nc->data);
            auto tabs = 0;
            size_t name_pos = 0, value_pos = 0;

            for (size_t i = 0; i < cookie_line.length(); ++i) {
                if (cookie_line[i] == '\t') {
                    tabs++;
                    if (tabs == 5) name_pos = i + 1;
                    if (tabs == 6) value_pos = i + 1;
                }
            }

            if (name_pos > 0 && value_pos > 0) {
                std::string name = cookie_line.substr(name_pos, value_pos - name_pos - 1);
                std::string value = cookie_line.substr(value_pos);

                if (!first) json << ",";
                json << "\"" << name << "\":\"" << value << "\"";
                first = false;
            }
        }

        json << "}";
        curl_slist_free_all(cookies);
        return json.str();
    }

    Result<HttpResponse> perform_request(const std::string& url, const std::string& method,
                                         const std::string& data = "",
                                         const std::string& content_type = "") {
        if (!curl_) {
            return Result<HttpResponse>(ErrorCode::NetworkError, "CURL not initialized");
        }

        HttpResponse response;
        std::string response_body;

        // Don't reset - preserve cookies between requests
        // curl_easy_reset(curl_);
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response.headers);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_sec_);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);

        if (!user_agent_.empty()) {
            curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent_.c_str());
        }

        // Build headers
        struct curl_slist* header_list = nullptr;
        for (const auto& [key, value] : headers_) {
            std::string header = key + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }

        if (!content_type.empty()) {
            std::string ct_header = "Content-Type: " + content_type;
            header_list = curl_slist_append(header_list, ct_header.c_str());
        }

        if (header_list) {
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
        }

        // Reset method-specific options first
        curl_easy_setopt(curl_, CURLOPT_POST, 0L);
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 0L);
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, nullptr);

        // Set method
        if (method == "POST") {
            curl_easy_setopt(curl_, CURLOPT_POST, 1L);
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
        } else if (method == "DELETE") {
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else {
            curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
        }

        // Perform request
        CURLcode res = curl_easy_perform(curl_);

        if (header_list) {
            curl_slist_free_all(header_list);
        }

        if (res != CURLE_OK) {
            std::string error = "CURL error: " + std::string(curl_easy_strerror(res));
            return Result<HttpResponse>(ErrorCode::NetworkError, error);
        }

        long status_code;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status_code);

        response.status_code = static_cast<int>(status_code);
        response.body = response_body;

        return Result<HttpResponse>(response);
    }

private:
    CURL* curl_;
    int timeout_sec_;
    std::string user_agent_;
    std::map<std::string, std::string> headers_;
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

void HttpClient::set_timeout(int seconds) {
    impl_->set_timeout(seconds);
}

void HttpClient::set_user_agent(const std::string& ua) {
    impl_->set_user_agent(ua);
}

void HttpClient::add_header(const std::string& key, const std::string& value) {
    impl_->add_header(key, value);
}

void HttpClient::clear_headers() {
    impl_->clear_headers();
}

Result<HttpResponse> HttpClient::get(const std::string& url) {
    return impl_->perform_request(url, "GET");
}

Result<HttpResponse> HttpClient::post(const std::string& url, const std::string& data,
                                     const std::string& content_type) {
    return impl_->perform_request(url, "POST", data, content_type);
}

Result<HttpResponse> HttpClient::post_json(const std::string& url, const std::string& json) {
    return impl_->perform_request(url, "POST", json, "application/json");
}

Result<HttpResponse> HttpClient::post_binary(const std::string& url, const std::vector<uint8_t>& data) {
    std::string data_str(data.begin(), data.end());
    return impl_->perform_request(url, "POST", data_str, "application/octet-stream");
}

Result<HttpResponse> HttpClient::delete_request(const std::string& url) {
    return impl_->perform_request(url, "DELETE");
}

Result<void> HttpClient::download_file(const std::string& url, const std::string& output_path,
                                       std::function<void(size_t, size_t)> progress_callback) {
    auto response = get(url);
    if (!response) {
        return Result<void>(response.error(), response.error_message());
    }

    if (!response.value().is_success()) {
        return Result<void>(ErrorCode::NetworkError,
            "HTTP " + std::to_string(response.value().status_code));
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) {
        return Result<void>(ErrorCode::Unknown, "Cannot create output file");
    }

    file.write(response.value().body.data(), response.value().body.size());
    file.close();

    return Result<void>(ErrorCode::Success);
}

std::string HttpClient::get_cookies_json() {
    return impl_->get_cookies_json();
}

} // namespace crdl
