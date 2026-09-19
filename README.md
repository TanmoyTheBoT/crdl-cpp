# CRDL - Crunchyroll Downloader (C++)

Professional C++ port of the Crunchyroll content downloader with DRM support.

## Features

✅ **Modern C++17** - Clean, type-safe, RAII-based architecture  
✅ **Cross-platform** - Windows, Linux, macOS support  
✅ **Native Widevine** - Full C++ Widevine CDM (no Node.js/Python)  
✅ **Multi-audio** - Download multiple audio tracks  
✅ **Quality Selection** - 1080p, 720p, best, worst  
✅ **Subtitle Support** - Automatic subtitle download and muxing  
✅ **Chapter Support** - Embedded chapter markers  
✅ **Thread-safe** - Proper concurrency handling  
✅ **Error Recovery** - Robust retry logic and cleanup  

## Architecture

```
crdl-cpp/
├── include/crdl/          # Public headers
│   ├── api/               # API client interfaces
│   ├── drm/               # DRM and encryption
│   ├── media/             # Media utilities
│   ├── utils/             # Helper utilities
│   └── core/              # Core types and config
├── src/                   # Implementation
├── tests/                 # Unit tests
├── examples/              # Usage examples
└── docs/                  # Documentation
```

## Building

### Prerequisites

- CMake 3.15+
- C++17 compatible compiler (MSVC 2019+, GCC 8+, Clang 7+)
- vcpkg (for dependencies)
- [widevine-cpp](https://github.com/TanmoyTheBoT/cppwidevine) (native Widevine library)
- N_m3u8DL-RE (runtime dependency)
- mkvmerge (runtime dependency)

Dependencies installed via vcpkg:
- libcurl
- OpenSSL
- Protocol Buffers
- nlohmann-json
- spdlog

### Windows (MSVC)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Usage

```cpp
#include <crdl/api/crunchyroll_client.h>
#include <crdl/core/config.h>

int main() {
    crdl::Config config;
    config.username = "your_email";
    config.password = "your_password";
    config.quality = crdl::Quality::P1080;
    
    crdl::CrunchyrollClient client(config);
    client.login();
    
    // Download single episode
    client.download_episode("EPISODE_ID");
    
    return 0;
}
```

## Command Line

```bash
# Download episode
crdl.exe -e EPISODE_ID -u USERNAME -p PASSWORD

# Download season with multiple audio tracks
crdl.exe -s SEASON_ID -a "ja-JP,en-US" -q 1080p

# Download entire series
crdl.exe --series SERIES_ID --quality best
```

## Configuration

Config file: `~/.config/crdl/config.json`

```json
{
    "username": "your_email",
    "password": "your_password",
    "quality": "1080p",
    "output_dir": "~/Downloads",
    "audio_languages": ["ja-JP", "en-US"],
    "release_group": "CRDL"
}
```

## Improvements over Python version

- **Thread-safe stream management** - Fixed race conditions
- **Proper RAII cleanup** - No token leaks
- **Consistent retry logic** - All API calls use same retry mechanism
- **Better error handling** - Type-safe error codes
- **Memory efficient** - Streaming downloads, no buffer bloat
- **Faster** - Native performance, parallel downloads
- **Professional structure** - Clear separation of concerns

## License

MIT License - See LICENSE file

## Disclaimer

Educational purposes only. Respect Crunchyroll's Terms of Service.
