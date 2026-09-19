#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace crdl {
namespace native_cdm {

// Widevine License Types
enum class LicenseType : uint32_t {
    STREAMING = 1,
    OFFLINE = 2,
    AUTOMATIC = 3
};

// Simplified Widevine License Request (protobuf manual encode)
struct LicenseRequest {
    std::vector<uint8_t> client_id;           // Field 1: Client ID (certificate)
    std::vector<uint8_t> content_id;          // Field 2: Content ID from PSSH
    uint32_t type{1};                         // Field 3: License type (1=streaming)
    uint32_t request_time{0};                 // Field 4: Unix timestamp
    std::vector<uint8_t> key_control_nonce;   // Field 5: Random nonce
    std::vector<uint8_t> encrypted_client_id; // Field 6: For privacy

    std::vector<uint8_t> serialize() const;
};

// Widevine License Response parser
struct LicenseKey {
    std::vector<uint8_t> kid;  // Key ID
    std::vector<uint8_t> key;  // Key value
    uint32_t type{0};          // 1=CONTENT, 3=SIGNING, etc.
};

struct LicenseResponse {
    std::vector<LicenseKey> keys;
    uint32_t status{0};

    static LicenseResponse parse(const std::vector<uint8_t>& data);
};

// Protobuf encoding helpers
class ProtobufWriter {
public:
    void write_varint(uint32_t field_number, uint64_t value);
    void write_bytes(uint32_t field_number, const std::vector<uint8_t>& data);
    void write_string(uint32_t field_number, const std::string& str);

    std::vector<uint8_t> data() const { return buffer_; }

private:
    std::vector<uint8_t> buffer_;

    void encode_varint(uint64_t value);
    void encode_field_header(uint32_t field_number, uint32_t wire_type);
};

// Protobuf decoding helpers
class ProtobufReader {
public:
    explicit ProtobufReader(const std::vector<uint8_t>& data);

    bool has_next() const;
    bool read_field(uint32_t& field_number, uint32_t& wire_type);
    uint64_t read_varint();
    std::vector<uint8_t> read_bytes();

private:
    const std::vector<uint8_t>& data_;
    size_t pos_{0};
};

} // namespace native_cdm
} // namespace crdl
