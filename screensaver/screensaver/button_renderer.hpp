#pragma once

#include  <string>

#include "screensaver/screensaver/configuration.hpp"
#include <SDL.h>
#include <SDL_render.h>
#include <SDL2/SDL_ttf.h>


class button_renderer_t {
public:
    button_renderer_t() {
        if (!m_initialized) {
            TTF_Init();
            m_initialized = true;
        }

        m_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    }

    void set_renderer(SDL_Renderer* renderer) {
        m_renderer = renderer;
    }

    void render(const std::string& text, const int width, const int height, const SDL_Color& bg_color, const SDL_Color& fg_color, const int radius, const int window_width, const int bottom) {
        const bool text_equal = text == m_text;
        const bool width_equal = width == m_width;
        const bool height_equal = height == m_height;
        const bool bg_color_equal = compare_color(bg_color, m_bg_color);
        const bool fg_color_equal = compare_color(fg_color, m_fg_color);
        const bool radius_equal = radius == m_radius;
        if (!text_equal || !width_equal || !height_equal || !bg_color_equal || ! fg_color_equal || !radius_equal) {
            m_text = text;
            m_width = width;
            m_height = height;
            m_bg_color = bg_color;
            m_fg_color = fg_color;
            m_radius = radius;

            rebuild(
                !(text_equal && width_equal && bg_color_equal && radius_equal),
                !(bg_color_equal && radius_equal),
                !(text_equal && width_equal && fg_color_equal && radius_equal)  
            );
        }

        SDL_Rect destination_rect{(window_width - width) / 2, bottom - m_height, m_width, m_height };
        SDL_RenderCopy(m_renderer, m_button_texture, nullptr, &destination_rect);
    }

private:
    void rebuild(const bool rebuild_button, const bool rebuild_corner, const bool rebuild_text) {
        std::cout << "rebuild" << std::endl;
        if (rebuild_corner) {
            draw_circle_texture(m_radius, m_bg_color);
        }
        if (rebuild_text) {
            create_text_texture(m_text, m_fg_color, m_width - 2 * m_radius);
        }
        if (rebuild_button) {
            m_height = std::max(m_height, m_text_height + 2 * (m_radius + 1));
            create_button_texture(m_text.c_str(), m_width, m_height, m_bg_color, m_fg_color, m_radius);        
        }
    }

    static bool compare_color(const SDL_Color& left, const SDL_Color& right) {
        return left.a == right.a && left.b == right.b && left.g == right.g && left.r == right.r;
    }

    void draw_circle_texture(const int radius, const SDL_Color& color)
    {
        if (m_circle_texture != nullptr) {
            SDL_DestroyTexture(m_circle_texture);
        }

        const int size = 2 * radius + 1;
        m_circle_texture = SDL_CreateTexture(
            m_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            size,
            size 
        );
    
        SDL_SetTextureBlendMode(m_circle_texture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(m_renderer, m_circle_texture);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
        SDL_RenderClear(m_renderer);
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

        for (int dy = -radius; dy <= radius; ++dy) {
            const float dx = sqrt(radius * radius - dy * dy);
            SDL_RenderDrawLineF(m_renderer, radius - dx, radius + dy, radius + dx, radius + dy);
        }

        SDL_SetRenderTarget(m_renderer, nullptr);
    }

    void create_text_texture(const std::string& text, const SDL_Color& color, const int max_width) {
        if (m_text_texture != nullptr) {
            SDL_DestroyTexture(m_text_texture);
        }

        TTF_SetFontWrappedAlign(m_font, TTF_WRAPPED_ALIGN_CENTER);
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended_Wrapped(m_font, text.c_str(), color, max_width);
        m_text_texture = SDL_CreateTextureFromSurface(m_renderer, text_surface);      
        m_text_width = text_surface->w;
        m_text_height = text_surface->h;
        SDL_FreeSurface(text_surface);
    }

    void create_button_texture(const char* text, const int width, const int height, const SDL_Color& bg_color, const SDL_Color& fg_color, const int radius) {
        if (m_button_texture != nullptr) {
            SDL_DestroyTexture(m_button_texture);
        }

        m_button_texture = SDL_CreateTexture(
            m_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            width,
            height
        );

        std::cout << "bwh " << width << " " << height << std::endl;

        SDL_SetTextureBlendMode(m_button_texture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(m_renderer, m_button_texture);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
        SDL_RenderClear(m_renderer);
        SDL_SetRenderDrawColor(m_renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);

        SDL_Rect middle = { radius, 0, width - 2 * radius, height };
        SDL_Rect left = { 0, radius, radius, height - 2 * radius };
        SDL_Rect right = { width - radius, radius, radius, height - 2 * radius };

        SDL_RenderFillRect(m_renderer, &middle);
        SDL_RenderFillRect(m_renderer, &left);
        SDL_RenderFillRect(m_renderer, &right);

        const int circle_size = 2 * radius + 1;
        SDL_Rect lt_rect = {0, 0, circle_size, circle_size};
        SDL_RenderCopy(m_renderer, m_circle_texture, nullptr, &lt_rect);
        SDL_Rect lb_rect = {0, height - circle_size, circle_size, circle_size};
        SDL_RenderCopy(m_renderer, m_circle_texture, nullptr, &lb_rect);
        SDL_Rect rb_rect = {width - circle_size, height - circle_size, circle_size, circle_size};
        SDL_RenderCopy(m_renderer, m_circle_texture, nullptr, &rb_rect);
        SDL_Rect rt_rect = {width - circle_size, 0, circle_size, circle_size};
        SDL_RenderCopy(m_renderer, m_circle_texture, nullptr, &rt_rect); 

        SDL_Rect text_rect = {(width - m_text_width) / 2, (height - m_text_height) / 2, m_text_width, m_text_height};
        SDL_RenderCopy(m_renderer, m_text_texture, nullptr, &text_rect);
        SDL_SetRenderTarget(m_renderer, nullptr);
    }

private:
    TTF_Font* m_font = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_circle_texture = nullptr;
    SDL_Texture* m_text_texture = nullptr;
    SDL_Texture* m_button_texture = nullptr;

    int m_text_width = 0;
    int m_text_height = 0;

    inline static bool m_initialized = false;

    std::string m_text;
    int m_width = -1;
    int m_height = -1;
    SDL_Color m_bg_color{0, 0, 0, 0};
    SDL_Color m_fg_color{0, 0, 0, 0};
    int m_radius = -1;
};
