#pragma once

#include <crdl/drm/native_cdm/wvd_parser.h>
#include <crdl/drm/native_cdm/widevine_proto.h>
#include <crdl/core/types.h>
#include <memory>

namespace crdl {
namespace native_cdm {

// Native C++ Widevine CDM implementation
// Handles license challenge generation and key extraction
class NativeCDM {
public:
    explicit NativeCDM(const std::string& wvd_path);
    ~NativeCDM();

    // Create a session for a PSSH
    struct Session {
        std::vector<uint8_t> pssh_data;
        std::vector<uint8_t> content_id;
        LicenseType type{LicenseType::STREAMING};
    };

    Session create_session(const std::string& pssh_base64, LicenseType type = LicenseType::STREAMING);

    // Generate license challenge (to be sent to license server)
    std::vector<uint8_t> generate_challenge(const Session& session);

    // Parse license response and extract keys
    std::vector<DRMKey> parse_license(const std::vector<uint8_t>& license_response);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace native_cdm
} // namespace crdl
