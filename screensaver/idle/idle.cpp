/*!
 * \file idle.cpp
 * \brief Implementation of the idle daemon that controls screen savers
 *
 * This daemon monitors user activity and starts a screensaver when the user is idle
 * for a specified timeout period.
 */

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

/*!
 * \var reload_config
 * \brief Atomic boolean flag indicating whether configuration needs to be reloaded
 */
static std::atomic<bool> reload_config{false};

/*!
 * \var saver_pid
 * \brief Process ID of the running screensaver process
 */
static pid_t saver_pid = -1;

/*!
 * \var idle_timeout_sec
 * \brief Idle timeout in seconds before screensaver starts
 */
static int idle_timeout_sec = 300;

/*!
 * \var saver_cmd
 * \brief Command to execute for starting the screensaver
 */
static std::vector<std::string> saver_cmd;

/*!
 * \var CONFIG_PATH
 * \brief Path to the configuration file
 */
constexpr const char *CONFIG_PATH = "/etc/idle-daemon.json";

/*!
 * \brief Callback function to open restricted device files
 * \param path Path to the device file
 * \param flags File open flags
 * \param user_data User data pointer
 * \return File descriptor or -1 on error
 */
static int open_restricted(const char *path, int flags, void *) {
    int fd = open(path, flags);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

/*!
 * \brief Callback function to close restricted device files
 * \param fd File descriptor to close
 * \param user_data User data pointer
 */
static void close_restricted(int fd, void *) {
    close(fd);
}

/*!
 * \var interface
 * \brief libinput interface structure for device access
 */
static const libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

/*!
 * \brief Load configuration from JSON file
 *
 * This function reads the configuration file and updates the global
 * variables with the new settings. If the file cannot be opened,
 * an error message is printed to stderr.
 */
void load_config() {
    std::ifstream f(CONFIG_PATH);
    if (!f.is_open()) {
        std::cerr << "Cannot open config\n";
        return;
    }

    nlohmann::json j;
    f >> j;

    idle_timeout_sec = j.value("idle_timeout_sec", 300);

    saver_cmd.clear();
    for (auto &a : j["screensaver_cmd"]) {
        saver_cmd.push_back(a.get<std::string>());
    }

    std::cout << "Config reloaded: timeout=" << idle_timeout_sec << "s\n";
}

/*!
 * \brief Start the screensaver process
 *
 * This function forks a new process to execute the screensaver command.
 * If a screensaver is already running or the command is empty, nothing is done.
 */
void start_saver() {
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
void stop_saver() {
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
void on_sighup(int) {
    reload_config = true;
}

/*!
 * \brief Main daemon loop
 *
 * This function implements the main event loop of the idle daemon.
 * It monitors user activity through libinput events and manages
 * the screensaver based on the configured idle timeout.
 * \return Exit status of the program
 */
int main() {
    signal(SIGHUP, on_sighup);

    load_config();

    udev *udev_ctx = udev_new();
    libinput *li = libinput_udev_create_context(&interface, nullptr, udev_ctx);
    libinput_udev_assign_seat(li, "seat0");

    int li_fd = libinput_get_fd(li);

    struct pollfd fds;
    fds.fd = li_fd;
    fds.events = POLLIN;

    auto last_activity = std::chrono::steady_clock::now();

    while (true) {
        if (reload_config) {
            load_config();
            reload_config = false;
        }

        int ret = poll(&fds, 1, 1000);
        if (ret < 0) {
            perror("poll");
            continue;
        }

        libinput_dispatch(li);

        libinput_event *event;
        while ((event = libinput_get_event(li)) != nullptr) {
            last_activity = std::chrono::steady_clock::now();

            std::cout << "INPUT EVENT → stop_saver\n";
            stop_saver();

            libinput_event_destroy(event);
        }

        auto now = std::chrono::steady_clock::now();
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();

        if (idle >= idle_timeout_sec) {
            start_saver();
        }
    }
}
