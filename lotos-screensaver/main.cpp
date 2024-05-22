#include <memory>

#include <blank.hpp>
#include <parameters.hpp>
#include <saver_manager.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/screensaver.h>
#include <xcb/xcb.h>

int main(int argc, char *argv[]) {
    // Initialize logger.
    auto max_size = 1024 * 1024; // Set max log size to 1MB.
    auto max_files = 2;
    //auto logger = spdlog::rotating_logger_mt("file", get_logging_path(), max_size, max_files);
    //return 0;

    saver_manager_t manager;
    manager.run();

    return 0;
}
