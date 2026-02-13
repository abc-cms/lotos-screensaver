#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <chrono>
#include <string>
#include <vector>
#include <random>

struct button_t {
    std::string text;
    int32_t x, y, width, height;
    uint32_t background_color;
    uint32_t text_color;
    uint32_t corner_radius;
};

class button_layer_sdl {
public:
    void configure(const std::string &text, int text_size, uint32_t text_color,
                   uint32_t bg_color, int height, int corner_radius,
                   float switch_duration_sec = 5.f,
                   float animation_duration_sec = 0.3f,
                   int animation_steps = 10);
    
    void init(SDL_Renderer *renderer);
    void update(const std::chrono::steady_clock::time_point &now, int screen_w, int screen_h);
    void render();

private:
    button_t make_button(int screen_w, int screen_h);
    button_t interpolate(const button_t &start, const button_t &end, float f);

private:
    std::string m_text;
    int m_text_size;
    uint32_t m_text_color, m_bg_color;
    int m_height;
    int m_corner_radius;
    bool m_initialized = false;
    bool m_animation = false;

    std::chrono::steady_clock::time_point m_animation_start;
    std::chrono::steady_clock::time_point m_animation_end;
    std::chrono::steady_clock::time_point m_next_frame;

    SDL_Renderer *m_renderer = nullptr;
    TTF_Font *m_font = nullptr;

    button_t m_button, m_current_button, m_next_button;

    float m_switch_duration;
    float m_animation_duration;
    int m_animation_steps;
};

