#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace crdl {
namespace string_utils {

// Trim whitespace
inline std::string trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

// Split string
inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : s) {
        if (c == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

// Join strings
inline std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) result += delimiter;
        result += vec[i];
    }
    return result;
}

// Replace all occurrences
inline std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

// Sanitize filename
inline std::string sanitize_filename(const std::string& filename) {
    std::string result = filename;

    // Replace invalid characters with dashes (matching Python version)
    const char* invalid_chars = "\\/*?:\"<>|";
    for (char c : std::string(invalid_chars)) {
        result = replace_all(result, std::string(1, c), "-");
    }

    // Replace spaces and remaining special chars
    result = replace_all(result, " ", ".");
    result = replace_all(result, ":", "");
    result = replace_all(result, "/", "-");
    result = replace_all(result, "-", ".");

    // Remove apostrophes
    result = replace_all(result, "'", "");
    result = replace_all(result, "`", "");

    // Remove multiple dots
    while (result.find("..") != std::string::npos) {
        result = replace_all(result, "..", ".");
    }

    return result;
}

// URL encode
std::string url_encode(const std::string& value);

// Base64 encode/decode
std::string base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& encoded);

} // namespace string_utils
} // namespace crdl
