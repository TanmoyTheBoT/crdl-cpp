# Core library source files
set(CRDL_SOURCES
    # Core
    core/types.cpp
    core/config.cpp

    # Utils
    utils/logger.cpp
    utils/http_client.cpp
    utils/string_utils.cpp

    # API
    api/crunchyroll_client.cpp
    api/auth_manager.cpp

    # DRM
    drm/widevine_cdm.cpp

    # Media
    media/downloader.cpp
    media/muxer.cpp
)

# Create core library
add_library(crdl_core STATIC ${CRDL_SOURCES})

target_link_libraries(crdl_core
    PUBLIC
        widevine::widevine
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        ${CURL_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        Threads::Threads
)

target_include_directories(crdl_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Main executable
add_executable(crdl main.cpp)

target_link_libraries(crdl
    PRIVATE
        crdl_core
)

# Set output name
set_target_properties(crdl PROPERTIES
    OUTPUT_NAME "crdl"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Windows specific settings
if(WIN32)
    target_compile_definitions(crdl PRIVATE _UNICODE UNICODE)
endif()
