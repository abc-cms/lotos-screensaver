/*!
 * \file idle_daemon.hpp
 * \brief Header file for idle daemon class
 * 
 * This file contains the declaration of the idle_daemon class that controls
 * screen savers based on user idle time.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <thread>

#include <libinput.h>
#include <libudev.h>

/*!
 * \class idle_daemon
 * \brief Class that implements the idle daemon functionality
 * 
 * This class monitors user activity and manages the screensaver based on
 * configurable idle timeout settings.
 */
class idle_daemon {
public:
    /*!
     * \brief Constructor for idle_daemon
     */
    idle_daemon();
    
    /*!
     * \brief Destructor for idle_daemon
     */
    ~idle_daemon();
    
    /*!
     * \brief Run the idle daemon
     * 
     * This method starts the main event loop of the idle daemon.
     * \return Exit status of the program
     */
    int run();
    
    /*!
     * \brief Load configuration from JSON file
     * 
     * This method reads the configuration file and updates the settings.
     */
    void load_config();
    
    /*!
     * \brief Reload configuration from file every 10 seconds
     * 
     * This method runs in a separate thread to periodically reload the
     * configuration file.
     */
    void reload_config_loop();

    /*!
     * \brief Start the screensaver process
     *
     * This function forks a new process to execute the screensaver command.
     * If a screensaver is already running or the command is empty, nothing is done.
     */
     void start_saver();
    
     /*!
      * \brief Stop the running screensaver process
      *
      * This function sends a SIGTERM signal to the screensaver process
      * and waits for it to terminate before resetting the process ID.
      */
     void stop_saver();
     
     /*!
      * \brief Signal handler for SIGHUP signal
      *
      * This function is called when the daemon receives a SIGHUP signal,
      * which indicates that the configuration should be reloaded.
      * \param signum Signal number (ignored)
      */
     static void on_sighup(int);

private:
    /*!
     * \var reload_config
     * \brief Atomic boolean flag indicating whether configuration needs to be reloaded
     */
    inline static std::atomic<bool> reload_config{false};
    
    /*!
     * \var saver_pid
     * \brief Process ID of the running screensaver process
     */
    pid_t saver_pid = -1;
    
    /*!
     * \var idle_timeout_sec
     * \brief Idle timeout in seconds before screensaver starts
     */
    std::atomic<int> idle_timeout_sec{300};
    
    /*!
     * \var saver_cmd
     * \brief Command to execute for starting the screensaver
     */
    std::vector<std::string> saver_cmd;
    
    /*!
     * \var udev_ctx
     * \brief libudev context for device access
     */
    udev *udev_ctx = nullptr;
    
    /*!
     * \var li
     * \brief libinput context for device access
     */
    libinput *li = nullptr;
    
    /*!
     * \var libinput_interface
     * \brief libinput interface structure for device access
     */
    libinput_interface interface;
    
    /*!
     * \var config_reload_thread
     * \brief Thread for periodically reloading configuration
     */
    std::thread config_reload_thread;
    
    /*!
     * \brief Callback function to open restricted device files
     * \param path Path to the device file
     * \param flags File open flags
     * \param user_data User data pointer
     * \return File descriptor or -1 on error
     */
    static int open_restricted(const char *path, int flags, void *);
    
    /*!
     * \brief Callback function to close restricted device files
     * \param fd File descriptor to close
     * \param user_data User data pointer
     */
    static void close_restricted(int fd, void *);
};
