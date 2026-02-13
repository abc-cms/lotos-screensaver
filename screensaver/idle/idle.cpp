/*!
 * \file idle.cpp
 * \brief Main entry point for the idle daemon
 *
 * This file contains the main function that initializes and runs the idle daemon.
 */

#include "idle_daemon.hpp"
#include <iostream>

/*!
 * \brief Main function for the idle daemon
 * 
 * This function creates an idle_daemon instance and runs it.
 * \return Exit status of the program
 */
int main() {
    try {
        idle_daemon daemon;
        return daemon.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
