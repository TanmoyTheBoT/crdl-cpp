# CRDL C++ Build Instructions

## Prerequisites

### Windows

1. **Visual Studio 2019 or later**
   - Install "Desktop development with C++" workload
   - Or use Build Tools for Visual Studio

2. **CMake 3.15+**
   ```bash
   winget install Kitware.CMake
   ```

3. **vcpkg** (for dependencies)
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

4. **Install dependencies via vcpkg**
   ```bash
   vcpkg install curl:x64-windows openssl:x64-windows nlohmann-json:x64-windows spdlog:x64-windows
   ```

5. **Runtime tools** (must be in PATH)
   - [N_m3u8DL-RE](https://github.com/nilaoda/N_m3u8DL-RE/releases)
   - [mkvtoolnix](https://mkvtoolnix.download/downloads.html) (for mkvmerge)
   - [ffmpeg](https://ffmpeg.org/download.html)

### Linux (Ubuntu/Debian)

```bash
# Build essentials
sudo apt update
sudo apt install build-essential cmake git

# Dependencies
sudo apt install libcurl4-openssl-dev libssl-dev nlohmann-json3-dev libspdlog-dev

# Runtime tools
sudo apt install ffmpeg mkvtoolnix

# N_m3u8DL-RE (download binary from GitHub releases)
wget https://github.com/nilaoda/N_m3u8DL-RE/releases/latest/download/N_m3u8DL-RE_Beta_linux-x64 -O N_m3u8DL-RE
chmod +x N_m3u8DL-RE
sudo mv N_m3u8DL-RE /usr/local/bin/
```

### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake curl openssl nlohmann-json spdlog

# Runtime tools
brew install ffmpeg mkvtoolnix

# N_m3u8DL-RE (download from releases)
```

## Building

### Windows (Visual Studio)

```bash
# Configure
cmake -B build -G "Visual Studio 16 2019" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Output: build\bin\Release\crdl.exe
```

### Windows (Ninja)

```bash
# Open "x64 Native Tools Command Prompt for VS 2019"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

cmake --build build

# Output: build\bin\crdl.exe
```

### Linux / macOS

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Output: build/bin/crdl
```

## Running

### First Run Setup

1. Place your Widevine device file:
   ```
   Windows: %APPDATA%\crdl\widevine\device.wvd
   Linux/macOS: ~/.config/crdl/widevine/device.wvd
   ```

2. Run with credentials:
   ```bash
   crdl.exe -u your_email@example.com -p your_password -e EPISODE_ID
   ```

### Usage Examples

```bash
# Download single episode (1080p, Japanese audio)
crdl.exe -e G63VW2VWY -q 1080p

# Download with multiple audio tracks
crdl.exe -e G63VW2VWY -a "ja-JP,en-US" -q 720p

# Download entire season
crdl.exe -s G6P8KE4G6 -q best

# Download series
crdl.exe --series GR9P39NJ6 --quality 1080p

# Custom output directory
crdl.exe -e EPISODE_ID -o "D:\Anime"

# Verbose logging
crdl.exe -e EPISODE_ID -v
```

## Troubleshooting

### Missing Dependencies Error

If you see "Cannot find libcurl" or similar:
- **Windows**: Ensure vcpkg is properly integrated
- **Linux**: Run `sudo ldconfig` after installing libraries

### N_m3u8DL-RE Not Found

```bash
# Check if it's in PATH
N_m3u8DL-RE --version

# If not, add its directory to PATH or copy to system directory
```

### Widevine Device Not Found

Make sure `device.wvd` is in the correct location:
```bash
# Windows
dir %APPDATA%\crdl\widevine\device.wvd

# Linux/macOS
ls ~/.config/crdl/widevine/device.wvd
```

### Authentication Failed

- Check username/password are correct
- Verify Crunchyroll subscription is active
- Check logs at: `~/.config/crdl/logs/crdl.log`

## Development

### Build with Tests

```bash
cmake -B build -DCRDL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Enable Debug Symbols

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### IDE Integration

**Visual Studio**: Open `crdl-cpp` folder directly (CMake project support)

**VS Code**: Install CMake Tools extension
```json
// .vscode/settings.json
{
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
    ]
}
```

**CLion**: Import CMake project directly

## Installation

```bash
# Install to system (requires admin/sudo)
cmake --install build --prefix /usr/local

# Or install to custom directory
cmake --install build --prefix ~/crdl-install
```
