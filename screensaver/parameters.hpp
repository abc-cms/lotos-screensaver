#pragma once

#include <filesystem>
#include <string>

#include <pwd.h>
#include <unistd.h>

constexpr const char *configuration_path = "~/Documents/config/config.json";
constexpr const char *logging_path = "~/lotos-screensaver/lotos.log";
constexpr const char *log_name = "file";

std::filesystem::path absolute_path(std::string path) {
    if (path.find('~') == 0) {
        path = getpwuid(getuid())->pw_dir + path.substr(1);
    }

    return std::filesystem::absolute(path);
}

std::filesystem::path get_configuration_path() { return absolute_path(configuration_path); }

std::filesystem::path get_logging_path() { return absolute_path(logging_path); }
