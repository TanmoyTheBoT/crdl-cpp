#include <iostream>
#include <filesystem>
#include <cstdlib>

int main() {
    char* userprofile = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&userprofile, &sz, "USERPROFILE") == 0 && userprofile != nullptr) {
        std::filesystem::path config_dir = std::filesystem::path(userprofile) / ".config" / "crdl";
        std::filesystem::path device_path = config_dir / "widevine" / "device.wvd";
        
        std::cout << "Config dir: " << config_dir.string() << std::endl;
        std::cout << "Device path: " << device_path.string() << std::endl;
        std::cout << "Device exists: " << (std::filesystem::exists(device_path) ? "YES" : "NO") << std::endl;
        
        free(userprofile);
    }
    return 0;
}
