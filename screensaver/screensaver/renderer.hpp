#pragma once

#include <cstdint>

#include "SDL_pixels.h"
#include "button_renderer.hpp"
#include "configuration.hpp"
#include "positioner.hpp"
#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <mpv/client.h>
#include <mpv/render.h>

class renderer_t {
public:
    renderer_t() {
        m_mpv = mpv_create();

        mpv_set_option_string(m_mpv, "vo", "libmpv");
        // mpv_set_option_string(m_mpv, "keep-open", "always");
        mpv_set_option_string(m_mpv, "idle", "yes");
        mpv_set_option_string(m_mpv, "force-window", "yes");

        // mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

        mpv_initialize(m_mpv);
        mpv_request_log_messages(m_mpv, "debug");

        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        SDL_Init(SDL_INIT_VIDEO);

        SDL_CreateWindowAndRenderer(1000, 500, SDL_WINDOW_FULLSCREEN_DESKTOP, &m_window, &m_renderer);

        m_button_renderer.set_renderer(m_renderer);
    }

    ~renderer_t() {
        mpv_render_context_free(m_render_context);
        mpv_destroy(m_mpv);
    }

    int run() {
        std::cout << get_configuration_path() << std::endl;
        m_configuration = configuration_t::load(get_configuration_path());

        char *api_type = MPV_RENDER_API_TYPE_SW;
        int advanced_control = 1;
        mpv_render_param params[] = {{MPV_RENDER_PARAM_API_TYPE, api_type},
                                     {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced_control},
                                     {MPV_RENDER_PARAM_INVALID, nullptr}};

        mpv_render_context_create(&m_render_context, m_mpv, params);

        m_wakeup_on_mpv_render_update = SDL_RegisterEvents(1);
        m_wakeup_on_mpv_events = SDL_RegisterEvents(1);
        m_timer_event = SDL_RegisterEvents(1);

        mpv_set_wakeup_callback(m_mpv, &renderer_t::on_mpv_events, nullptr);
        mpv_render_context_set_update_callback(m_render_context, &renderer_t::on_mpv_render_update, nullptr);

        m_timer_id = SDL_AddTimer(33, &renderer_t::timer_callback, &m_timer_event);

        int result = loop();

        cleanup();

        return result;
    }

private:
    void cleanup() {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }
    }

    int loop() {
        SDL_Event event;
        bool loop_active = true;

        int window_height;
        int window_width;
        SDL_GetWindowSize(m_window, &window_width, &window_height);
        m_positioner.set_configuration(m_configuration.button(), window_width, window_height, 75);

        while (loop_active) {
            if (SDL_WaitEvent(&event) != 1) {
                return -1;
            }

            bool redraw_frame = false;
            bool redraw_button = false;

            switch (event.type) {
            case SDL_QUIT: {
                loop_active = false;
                break;
            }

            case SDL_WINDOWEVENT: {
                if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    load();
                    redraw_frame = true;
                    redraw_button = true;
                }
                break;
            }

            case SDL_KEYDOWN:
                break;

            default: {
                if (event.type == m_wakeup_on_mpv_render_update) {
                    const uint64_t flags = mpv_render_context_update(m_render_context);
                    if (flags & MPV_RENDER_UPDATE_FRAME) {
                        redraw_frame = true;
                        redraw_button = true;
                    }
                }

                if (event.type == m_wakeup_on_mpv_events) {
                    while (true) {
                        const mpv_event *event = mpv_wait_event(m_mpv, 0);

                        if (event->event_id == MPV_EVENT_END_FILE) {
                            load();
                            redraw_frame = true;
                            redraw_button = true;
                        } else if (event->event_id == MPV_EVENT_NONE) {
                            break;
                        }
                    }
                }

                if (event.type == m_timer_event) {
                    redraw_button = true;
                }

                break;
            }
            }
            // std::cout << "redraw frame: " << redraw_frame << std::endl;
            if (redraw_frame) {
                redraw_window_frame(true);
                redraw_button_frame();
            } else if (redraw_button) {
                redraw_window_frame(false);
                redraw_button_frame();
            }

            if (redraw_frame || redraw_button) {
                SDL_RenderPresent(m_renderer);
            }
        }

        return 0;
    }

    void redraw_window_frame(const bool update_video) {
        if (update_video || !m_texture) {
            int window_width, window_height;
            SDL_GetWindowSize(m_window, &window_width, &window_height);

            if (!m_texture || window_width != m_screen_width || window_height != m_screen_height) {
                SDL_DestroyTexture(m_texture);
                m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING,
                                              window_width, window_height);

                m_screen_width = window_width;
                m_screen_height = window_height;
            }

            void *pixels;
            int pitch;
            SDL_LockTexture(m_texture, nullptr, &pixels, &pitch);

            size_t cpitch = pitch;
            int window_rect[2] = {window_width, window_height};
            mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, window_rect},
                                         {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("0bgr")},
                                         {MPV_RENDER_PARAM_SW_STRIDE, &cpitch},
                                         {MPV_RENDER_PARAM_SW_POINTER, pixels},
                                         {MPV_RENDER_PARAM_INVALID, nullptr}};
            mpv_render_context_render(m_render_context, params);

            SDL_UnlockTexture(m_texture);
        }

        SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    }

    void redraw_button_frame() {
        std::cout << "button" << std::endl;
        const uint32_t ticks = SDL_GetTicks();
        const button_configuration_t &button_configuration = m_configuration.button();

        m_positioner.update(static_cast<float>(ticks) / 1000.f);
        auto [left, right, top, bottom] = m_positioner.button_rectangle();

        auto bg_color_raw = button_configuration.background_color();
        SDL_Color bg_color{static_cast<uint8_t>(bg_color_raw & 0xff), static_cast<uint8_t>((bg_color_raw >> 8) & 0xff),
                           static_cast<uint8_t>((bg_color_raw >> 16) & 0xff),
                           static_cast<uint8_t>((bg_color_raw >> 24) & 0xff)};
        auto fg_color_raw = button_configuration.text_color();
        SDL_Color fg_color{static_cast<uint8_t>(fg_color_raw & 0xff), static_cast<uint8_t>((fg_color_raw >> 8) & 0xff),
                           static_cast<uint8_t>((fg_color_raw >> 16) & 0xff),
                           static_cast<uint8_t>((fg_color_raw >> 24) & 0xff)};
        m_button_renderer.render(button_configuration.text(), right - left, bottom - top, bg_color, fg_color,
                                 button_configuration.corner_radius(), m_screen_width, bottom);

        // SDL_Texture* button_tex = create_button_texture(
        //     renderer,
        //     font,
        //     "EXIT",
        //     br - bl, bb - bt,
        //     bg, fg,
        //     15
        // );

        // SDL_Rect button_rect = {bl, bt, br - bl, bb - bt};

        // //SDL_Rect dst = { (int) 0, (int) 0, br - bl, bb - bt };
        // // кнопка поверх видео

        // SDL_RenderCopy(m_renderer, button_tex, NULL, &button_rect);
    }

    void load() {
        auto media = m_configuration.media()[m_media_index];
        auto media_type = media.media_type();
        auto media_path = absolute_path(media.path());
        auto media_duration = media.duration();

        if (media_type == media_type_e::image) {
            auto duration = std::to_string(media_duration);
            const char *cmd[] = {"set_property", "image-display-duration", duration.c_str(), nullptr};
            mpv_command_async(m_mpv, 0, cmd);
        }

        const char *cmd[] = {"loadfile", media_path.c_str(), nullptr};
        mpv_command_async(m_mpv, 0, cmd);

        m_media_index = (m_media_index + 1) % m_configuration.media().size();
    }

    static uint32_t timer_callback(const uint32_t interval, void *param) {
        SDL_Event event;
        SDL_zero(event);
        event.type = *static_cast<uint32_t *>(param);
        SDL_PushEvent(&event);
        return interval;
    }

    static void on_mpv_events(void *ctx) {
        SDL_Event event = {.type = m_wakeup_on_mpv_events};
        SDL_PushEvent(&event);
    }

    static void on_mpv_render_update(void *ctx) {
        SDL_Event event = {.type = m_wakeup_on_mpv_render_update};
        SDL_PushEvent(&event);
    }

private:
    configuration_t m_configuration;
    positioner_t m_positioner;
    button_renderer_t m_button_renderer;

    inline static uint32_t m_wakeup_on_mpv_render_update;
    inline static uint32_t m_wakeup_on_mpv_events;
    inline static uint32_t m_timer_event;

    int m_media_index = 0;

    mpv_handle *m_mpv = nullptr;
    SDL_Window *m_window = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    mpv_render_context *m_render_context = nullptr;

    int m_screen_width = -1;
    int m_screen_height = -1;
    SDL_Texture *m_texture = nullptr;

    SDL_TimerID m_timer_id;
};
