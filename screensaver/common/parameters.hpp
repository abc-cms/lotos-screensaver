/*!
 * \file parameters.hpp
 * \brief Header file containing configuration and path management functions
 * 
 * This file defines constants and functions for managing configuration and
 * logging paths used by the lotos screensaver application.
 */

#pragma once

#include <filesystem>
#include <string>

#include <pwd.h>
#include <unistd.h>

/*!
 * \brief Path to the configuration file
 * 
 * This constant defines the default location of the configuration file
 * relative to the user's home directory.
 * 
 * \note The path is relative to the user's home directory and uses the ~ 
 * shorthand notation.
 */
constexpr const char *configuration_path = "~/Documents/config/config.json";

/*!
 * \brief Path to the log file
 * 
 * This constant defines the default location of the log file
 * relative to the user's home directory.
 * 
 * \note The path is relative to the user's home directory and uses the ~ 
 * shorthand notation.
 */
constexpr const char *logging_path = "~/lotos-screensaver/lotos.log";

/*!
 * \brief Name of the logger
 * 
 * This constant defines the name used for the logger instance.
 * 
 * \note This is used when creating the spdlog logger instance.
 */
constexpr const char *log_name = "file";

/*!
 * \brief Converts a relative path to an absolute path
 * 
 * This function takes a path string and converts it to an absolute path.
 * If the path starts with '~', it will be expanded to the user's home directory.
 * 
 * \param path The path to convert (can be relative or absolute)
 * \return std::filesystem::path The absolute path
 */
std::filesystem::path absolute_path(std::string path) {
    if (path.find('~') == 0) {
        path = getpwuid(getuid())->pw_dir + path.substr(1);
    }

    return std::filesystem::absolute(path);
}

/*!
 * \brief Gets the absolute path to the configuration file
 * 
 * This function returns the absolute path to the configuration file
 * by calling absolute_path() on the configuration_path constant.
 * 
 * \return std::filesystem::path The absolute path to the configuration file
 */
std::filesystem::path get_configuration_path() {
    return absolute_path(configuration_path);
}

/*!
 * \brief Gets the absolute path to the log file
 * 
 * This function returns the absolute path to the log file
 * by calling absolute_path() on the logging_path constant.
 * 
 * \return std::filesystem::path The absolute path to the log file
 */
std::filesystem::path get_logging_path() {
    return absolute_path(logging_path);
}
