#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

// Forward declarations
struct button_t;
class button_layer_sdl;

enum class display_mode {
    BLACK_SCREEN,
    VIDEO,
    IMAGE
};

struct media_item {
    std::string type;  // "video" or "image"
    std::string path;
    int time;  // in seconds for image, duration for video
};

struct screensaver_settings {
    bool enabled;
    int start_time;  // in seconds (0-86399) 
    int end_time;    // in seconds (0-86399)
    std::vector<media_item> media_files;
    int current_media_index;
};

class screensaver {
public:
    screensaver();
    ~screensaver();
    
    void init(SDL_Window *window, SDL_Renderer *renderer);
    void update();
    void render();
    void load_config();
    void handle_event(const SDL_Event &event);
    
    bool is_active() const { return m_active; }
    void set_active(bool active) { m_active = active; }

private:
    void update_display_mode();
    bool is_in_time_window() const;
    void play_media();
    void show_black_screen();

private:
    bool m_active; 
    bool m_initialized;
    SDL_Window *m_window;
    SDL_Renderer *m_renderer;
    
    std::unique_ptr<button_layer_sdl> m_button_layer;
    
    // Configuration
    screensaver_settings m_settings;
    
    // Media playback
    std::chrono::steady_clock::time_point m_media_start_time;
    int m_current_media_index;
    display_mode m_current_display_mode;
    
    // Timing
    std::chrono::steady_clock::time_point m_last_update;
    std::chrono::steady_clock::time_point m_frame_start;
    
    // Config file path
    std::string m_config_path;
};

