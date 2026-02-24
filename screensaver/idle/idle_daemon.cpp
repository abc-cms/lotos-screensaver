/*!
 * \file idle_daemon.cpp
 * \brief Implementation of the idle daemon class
 *
 * This file contains the implementation of the idle_daemon class that controls
 * screen savers based on user idle time.
 */

#include "idle_daemon.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <parameters.hpp>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

/*!
 * \brief Static pointer to the daemon instance for signal handling
 */
static idle_daemon *daemon_instance = nullptr;

/*!
 * \brief Signal handler for SIGTERM and SIGINT signals
 *
 * This function is called when the daemon receives a SIGTERM or SIGINT signal.
 * It sets a flag to indicate that the daemon should terminate.
 * \param signum Signal number
 */
void signal_handler(int signum) {
    if (daemon_instance) {
        std::cout << "Received signal " << signum << ", shutting down...\n";
        daemon_instance->stop_saver(); // Stop any running screensaver
        exit(0);                       // Exit cleanly
    }
}

/*!
 * \brief Constructor for idle_daemon
 */
idle_daemon::idle_daemon() {
    // Initialize libinput interface
    interface.open_restricted = open_restricted;
    interface.close_restricted = close_restricted;

    // Set the static instance pointer for signal handling
    daemon_instance = this;
}

/*!
 * \brief Destructor for idle_daemon
 */
idle_daemon::~idle_daemon() {
    if (config_reload_thread.joinable()) {
        config_reload_thread.join();
    }
    if (li) {
        libinput_unref(li);
    }
    if (udev_ctx) {
        udev_unref(udev_ctx);
    }
}

/*!
 * \brief Callback function to open restricted device files
 * \param path Path to the device file
 * \param flags File open flags
 * \param user_data User data pointer
 * \return File descriptor or -1 on error
 */
int idle_daemon::open_restricted(const char *path, int flags, void *) {
    int fd = open(path, flags);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

/*!
 * \brief Callback function to close restricted device files
 * \param fd File descriptor to close
 * \param user_data User data pointer
 */
void idle_daemon::close_restricted(int fd, void *) {
    close(fd);
}

/*!
 * \brief Load configuration from JSON file
 *
 * This function reads the configuration file and updates the settings.
 * It reads config from the path returned by get_configuration_path() function.
 */
void idle_daemon::load_config() {
    std::string config_path = get_configuration_path().string();
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Cannot open config at " << config_path << "\n";
        return;
    }

    nlohmann::json root;
    file >> root;

    int inactivity_timeout = 5;

    try {
        inactivity_timeout = root.at("screensaver_settings").at("inactivity_timeout").get<int>();
        std::cout << "inactivity_timeout = " << inactivity_timeout << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "JSON error: " << e.what() << std::endl;
    }

    idle_timeout_sec = 10; //inactivity_timeout * 60;

    saver_cmd.clear();
    saver_cmd.push_back("lotos-screensaver");

    std::cout << "Config reloaded: timeout=" << idle_timeout_sec << "s\n";
}

/*!
 * \brief Reload configuration from file every 10 seconds
 *
 * This method runs in a separate thread to periodically reload the
 * configuration file.
 */
void idle_daemon::reload_config_loop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (reload_config) {
            load_config();
            reload_config = false;
        }
    }
}

/*!
 * \brief Start the screensaver process
 *
 * This function forks a new process to execute the screensaver command.
 * If a screensaver is already running or the command is empty, nothing is done.
 */
void idle_daemon::start_saver() {
    if (saver_pid > 0 || saver_cmd.empty())
        return;

    saver_pid = fork();
    if (saver_pid == 0) {
        std::vector<char *> argv;
        for (auto &s : saver_cmd)
            argv.push_back(const_cast<char *>(s.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(1);
    }
}

/*!
 * \brief Stop the running screensaver process
 *
 * This function sends a SIGTERM signal to the screensaver process
 * and waits for it to terminate before resetting the process ID.
 */
void idle_daemon::stop_saver() {
    if (saver_pid > 0) {
        kill(saver_pid, SIGTERM);
        waitpid(saver_pid, nullptr, 0);
        saver_pid = -1;
    }
}

/*!
 * \brief Signal handler for SIGHUP signal
 *
 * This function is called when the daemon receives a SIGHUP signal,
 * which indicates that the configuration should be reloaded.
 * \param signum Signal number (ignored)
 */
void idle_daemon::on_sighup(int) {
    reload_config = true;
}

/*!
 * \brief Run the idle daemon
 *
 * This method starts the main event loop of the idle daemon.
 * \return Exit status of the program
 */
int idle_daemon::run() {
    // Register signal handlers
    signal(SIGHUP, on_sighup);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    load_config();

    udev_ctx = udev_new();
    li = libinput_udev_create_context(&interface, nullptr, udev_ctx);
    libinput_udev_assign_seat(li, "seat0");

    int li_fd = libinput_get_fd(li);

    struct pollfd fds;
    fds.fd = li_fd;
    fds.events = POLLIN;

    auto last_activity = std::chrono::steady_clock::now();

    while (true) {
        int ret = poll(&fds, 1, 1000);
        if (ret < 0) {
            perror("poll");
            continue;
        }

        libinput_dispatch(li);

        libinput_event *event;
        while ((event = libinput_get_event(li)) != nullptr) {
            last_activity = std::chrono::steady_clock::now();
            stop_saver();
            libinput_event_destroy(event);
        }

        auto now = std::chrono::steady_clock::now();
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();

        if (idle >= idle_timeout_sec) {
            start_saver();
        }
    }

    return 0;
}
