#pragma once

#include <string>

class PathTranslator {
public:
    explicit PathTranslator(const std::string& distro_name);

    // Translate a WSL2 Linux path to a Windows UNC path.
    // If the path is already a Windows path (drive letter or UNC), return unchanged.
    std::string translate(const std::string& linux_path) const;

private:
    std::string prefix_;  // e.g. "\\\\wsl.localhost\\Ubuntu"
};
