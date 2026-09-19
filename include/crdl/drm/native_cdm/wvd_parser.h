#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace crdl {
namespace native_cdm {

// WVD file format parser
// WVD files contain Widevine device credentials (.wvd format from pywidevine)
class WVDParser {
public:
    struct DeviceInfo {
        std::vector<uint8_t> client_id;      // Device certificate
        std::vector<uint8_t> private_key;    // RSA private key (PKCS#8 DER)
        uint32_t security_level{3};           // L3 = 3
        uint32_t flags{0};
        std::string system_id;
    };

    static DeviceInfo parse(const std::vector<uint8_t>& wvd_data);
    static DeviceInfo load_from_file(const std::string& path);
};

} // namespace native_cdm
} // namespace crdl
