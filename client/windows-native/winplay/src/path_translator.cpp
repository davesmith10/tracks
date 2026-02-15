#include "path_translator.h"
#include <algorithm>

PathTranslator::PathTranslator(const std::string& distro_name)
    : prefix_("\\\\wsl.localhost\\" + distro_name) {}

std::string PathTranslator::translate(const std::string& linux_path) const {
    if (linux_path.empty()) return linux_path;

    // Already a Windows path: drive letter (C:\...) or UNC (\\...)
    if (linux_path.size() >= 2 &&
        ((std::isalpha(static_cast<unsigned char>(linux_path[0])) && linux_path[1] == ':') ||
         (linux_path[0] == '\\' && linux_path[1] == '\\'))) {
        return linux_path;
    }

    // WSL2 mount of Windows drive: /mnt/c/... → C:\...
    if (linux_path.size() >= 6 &&
        linux_path.compare(0, 5, "/mnt/") == 0 &&
        std::isalpha(static_cast<unsigned char>(linux_path[5])) &&
        (linux_path.size() == 6 || linux_path[6] == '/')) {
        std::string result;
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(linux_path[5])));
        result += ':';
        if (linux_path.size() > 6) {
            result += linux_path.substr(6);
        } else {
            result += '\\';
        }
        std::replace(result.begin(), result.end(), '/', '\\');
        return result;
    }

    // Native WSL2 path: prepend UNC prefix, replace / with backslash
    std::string result = prefix_ + linux_path;
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}
