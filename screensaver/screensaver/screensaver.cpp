// Build with: gcc -o main_sw main_sw.c `pkg-config --libs --cflags mpv sdl2` -std=c99

#include "screensaver/screensaver/renderer.hpp"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <iostream>

#include "positioner.hpp"
#include <charconv>
#include <string>
#include <iostream>
#include "configuration.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>




int main(int argc, char* argv[])
{
    // Initialize logger.
    auto max_size = 1024 * 1024; // Set max log size to 1MB.
    auto max_files = 2;
    auto logging_path = get_logging_path();
    auto logger = spdlog::rotating_logger_mt(log_name, logging_path, max_size, max_files);
    logger->flush_on(spdlog::level::err);

    renderer_t renderer;
    renderer.run();
    
    return 0;
}


























/*#include "screensaver.hpp"
#include "button.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <pwd.h>
#include <unistd.h>
#include <parameters.hpp>

// Helper function to get full path from ~
std::string get_full_path(const std::string &path) {
    if (path.find('~') == 0) {
        // Get home directory
        char *home_dir = getenv("HOME");
        if (home_dir == nullptr) {
            // Fallback to getpwuid
            home_dir = getpwuid(getuid())->pw_dir;
        }
        return std::string(home_dir) + path.substr(1);
    }
    return path;
}

screensaver::screensaver() 
    : m_active(false),
      m_initialized(false),
      m_window(nullptr),
      m_renderer(nullptr),
      m_current_media_index(0),
      m_current_display_mode(display_mode::BLACK_SCREEN),
      m_config_path(get_configuration_path().string()) {
    m_button_layer = std::make_unique<button_layer_sdl>();
}

screensaver::~screensaver() = default;

void screensaver::init(SDL_Window *window, SDL_Renderer *renderer) {
    m_window = window;
    m_renderer = renderer;
    m_initialized = true;
    
    // Load configuration
    load_config();
    
    // Initialize button layer
    if (m_button_layer) {
        m_button_layer->init(renderer);
    }
}

void screensaver::update() {
    if (!m_initialized) return;
    
    auto now = std::chrono::steady_clock::now();
    
    // Update button layer
    if (m_button_layer) {
        m_button_layer->update(now, 1920, 1080); // Assuming 1080p screen
    }
    
    // Update display mode periodically
    update_display_mode();
    
    // Play media if needed
    if (m_current_display_mode != display_mode::BLACK_SCREEN) {
        play_media();
    }
}

void screensaver::render() {
    if (!m_initialized) return;
    
    // Clear with black background as base
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    
    // Handle different display modes
    if (m_current_display_mode == display_mode::VIDEO || 
        m_current_display_mode == display_mode::IMAGE) {
        // In a real implementation, we would render the media here
        // For now, we just show a placeholder in the main window
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    } else {
        // Black screen
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    }
    
    SDL_RenderFillRect(m_renderer, nullptr);
    
    // Render button layer  
    if (m_button_layer) {
        m_button_layer->render();
    }
    
    SDL_RenderPresent(m_renderer);
}

void screensaver::load_config() {
    nlohmann::json config;
    
    // Try to read config file
    std::ifstream f(m_config_path);
    if (f.is_open()) {
        try {
            f >> config;
        } catch (const std::exception&) {
            std::cerr << "Failed to parse config file " << m_config_path << std::endl;
            return;
        }
    } else {
        std::cerr << "Cannot open config at " << m_config_path << std::endl;
        return;
    }
    
    // Load button settings if available
    if (config.contains("button")) {
        // The button configuration loading would go here
        auto& button_config = config["button"];
        // This is where we would parse the button configuration
    }
    
    // Load screensaver settings
    if (config.contains("screensaver_settings")) {
        auto& screensaver_config = config["screensaver_settings"];
        
        m_settings.enabled = screensaver_config.value("enabled", true);
        m_settings.start_time = screensaver_config.value("start_time", 0);
        m_settings.end_time = screensaver_config.value("end_time", 86400);
        
        // Load media files
        if (screensaver_config.contains("media_files")) {
            m_settings.media_files.clear();
            for (const auto& item : screensaver_config["media_files"]) {
                media_item media;
                media.type = item.value("type", "");
                media.path = item.value("path", "");
                media.time = item.value("time", 0);
                m_settings.media_files.push_back(media);
            }
        }
    }
}

void screensaver::handle_event(const SDL_Event &event) {
    if (m_button_layer) {
        // Handle button events if needed
    }
}

void screensaver::update_display_mode() {
    // This would be updated each frame based on current time and settings
    // The logic here would determine whether to show black screen or media
    
    if (!m_settings.enabled) {
        m_current_display_mode = display_mode::BLACK_SCREEN;
        return;
    }
    
    if (is_in_time_window()) {
        // We're in the time window where media should play
        m_current_display_mode = (m_settings.media_files.empty()) 
            ? display_mode::BLACK_SCREEN 
            : display_mode::IMAGE; // Default to showing image if media exists
    } else {
        // Out of time window - show black screen
        m_current_display_mode = display_mode::BLACK_SCREEN;
    }
}

bool screensaver::is_in_time_window() const {
    // In a real implementation, we would get the current time and compare
    // to start_time and end_time, but since we don't have a time implementation here,
    // we'll just return true to enable media playback
    
    // This is where the actual time comparison logic would be implemented:
    // int current_seconds = get_current_time_in_seconds(); 
    // return (current_seconds >= m_settings.start_time && current_seconds <= m_settings.end_time);
    
    return true;  // For this prototype, always return true
}

void screensaver::play_media() {
    // In a real implementation, this would handle the actual media playback
    // This is where we'd cycle through the media_files and display them 
    // based on their type and duration
}

void screensaver::show_black_screen() {
    // The black screen display is handled in render() method
    // Just ensuring the correct mode is set
    m_current_display_mode = display_mode::BLACK_SCREEN;
}

*/