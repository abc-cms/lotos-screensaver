// Build with: gcc -o main_sw main_sw.c `pkg-config --libs --cflags mpv sdl2` -std=c99

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <mpv/client.h>
#include <mpv/render.h>
#include "positioner.hpp"
#include <charconv>
#include <string>
#include <iostream>
#include "configuration.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

static configuration_t configuration;

static Uint32 wakeup_on_mpv_render_update, wakeup_on_mpv_events;

Uint32 TIMER_EVENT = SDL_RegisterEvents(1);

Uint32 timer_callback(Uint32 interval, void* param)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = *(Uint32*)param;

    SDL_PushEvent(&event);

    return interval;
}





static void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void on_mpv_events(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_events};
    SDL_PushEvent(&event);
}

static void on_mpv_render_update(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_render_update};
    SDL_PushEvent(&event);
}


void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius)
{
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt(radius * radius - dy * dy);
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}


SDL_Texture* create_button_texture(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    int w, int h,
    SDL_Color bg,
    SDL_Color fg,
    int radius
) {
    SDL_Texture* tex = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h
    );

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);

    // прозрачный фон
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // фон кнопки
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);

    // центральные части
    SDL_Rect mid = { radius, 0, w - 2 * radius, h };
    SDL_Rect left = { 0, radius, radius, h - 2 * radius };
    SDL_Rect right = { w - radius, radius, radius, h - 2 * radius };

    SDL_RenderFillRect(renderer, &mid);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    // углы
    drawFilledCircle(renderer, radius, radius, radius);
    drawFilledCircle(renderer, w - radius - 1, radius, radius);
    drawFilledCircle(renderer, radius, h - radius - 1, radius);
    drawFilledCircle(renderer, w - radius - 1, h - radius - 1, radius);

    // текст
    SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font, text, fg);
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surf);

    SDL_Rect text_rect = {
        (w - text_surf->w) / 2,
        (h - text_surf->h) / 2,
        text_surf->w,
        text_surf->h
    };

    SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);

    SDL_FreeSurface(text_surf);
    SDL_DestroyTexture(text_tex);

    SDL_SetRenderTarget(renderer, NULL);
    return tex;
}

static int indexi = 0;

void load_current(mpv_handle* mpv)
{
    
std::cout << "index: " << indexi << std::endl;
    auto media = configuration.media()[indexi];
    auto media_type = media.media_type();
    auto media_path = media.path();
    auto media_duration = media.duration();

    std::cout << static_cast<int>(media_type) << " " << media_path << " " << media_duration << std::endl;

    if (media_type == media_type_e::image) {
        auto duration = std::to_string(media_duration);
        const char* opts[] = {
            "set_property", "image-display-duration", duration.c_str(),
            nullptr
        };
        mpv_command_async(mpv, 0, opts);
    }
    
    
    // Потом загрузка файла
    const char* cmd[] = {"loadfile", media_path.c_str(), nullptr};
    mpv_command_async(mpv, 0, cmd);

    indexi = (indexi + 1) % configuration.media().size();
}

int main(int argc, char* argv[])
{
    if (argc != 2)
        die("pass a single media file as argument");

    // Initialize logger.
    auto max_size = 1024 * 1024; // Set max log size to 1MB.
    auto max_files = 2;
    auto logging_path = get_logging_path();
    auto logger = spdlog::rotating_logger_mt(log_name, logging_path, max_size, max_files);
    logger->flush_on(spdlog::level::err);

    mpv_handle *mpv = mpv_create();
    if (!mpv)
        die("context init failed");

    mpv_set_option_string(mpv, "vo", "libmpv");
    //mpv_set_option_string(mpv, "keep-open", "always");
    mpv_set_option_string(mpv, "idle", "yes");
    mpv_set_option_string(mpv, "force-window", "yes");

    mpv_observe_property(mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(mpv) < 0)
        die("mpv init failed");

    mpv_request_log_messages(mpv, "debug");

    // Jesus Christ SDL, you suck!
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        die("SDL init failed");

    SDL_Window *window;
    SDL_Renderer *renderer;
    if (SDL_CreateWindowAndRenderer(1000, 500, SDL_WINDOW_SHOWN |
                                    SDL_WINDOW_RESIZABLE,
                                    &window, &renderer))
        die("failed to create SDL window");

    std::cout << 111111 << std::endl;
    configuration = configuration_t::load("/home/user/development/ls/config/config.json");
    std::cout << 22222 << std::endl;
    positioner_t positioner(
        configuration.button().animation_duration(), 
        configuration.button().switch_duration(),
        1000, 
        500, 
        50, 
        configuration.button().bottom_margin(), 
        configuration.button().bottom_margin_random(), 
        configuration.button().side_margin(), 
        configuration.button().bottom_margin_random(), 
        15
    );

    int tempi = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, reinterpret_cast<void *>(const_cast<char *>(MPV_RENDER_API_TYPE_SW))},
        // Tell libmpv that you will call mpv_render_context_update() on render
        // context update callbacks, and that you will _not_ block on the core
        // ever (see <libmpv/render.h> "Threading" section for what libmpv
        // functions you can call at all when this is active).
        // In particular, this means you must call e.g. mpv_command_async()
        // instead of mpv_command().
        // If you want to use synchronous calls, either make them on a separate
        // thread, or remove the option below (this will disable features like
        // DR and is not recommended anyway).
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &tempi},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context *mpv_rd;
    if (mpv_render_context_create(&mpv_rd, mpv, params) < 0)
        die("failed to initialize mpv render context");

    // We use events for thread-safe notification of the SDL main loop.
    // Generally, the wakeup callbacks (set further below) should do as least
    // work as possible, and merely wake up another thread to do actual work.
    // On SDL, waking up the mainloop is the ideal course of action. SDL's
    // SDL_PushEvent() is thread-safe, so we use that.
    wakeup_on_mpv_render_update = SDL_RegisterEvents(1);
    wakeup_on_mpv_events = SDL_RegisterEvents(1);
    if (wakeup_on_mpv_render_update == (Uint32)-1 ||
        wakeup_on_mpv_events == (Uint32)-1)
        die("could not register events");

    // When normal mpv events are available.
    mpv_set_wakeup_callback(mpv, on_mpv_events, NULL);

    // When there is a need to call mpv_render_context_update(), which can
    // request a new frame to be rendered.
    // (Separate from the normal event handling mechanism for the sake of
    //  users which run OpenGL on a different thread.)
    mpv_render_context_set_update_callback(mpv_rd, on_mpv_render_update, NULL);

    SDL_Texture *tex = NULL;
    int tex_w = -1, tex_h = -1;

    const int btn_w = 128, btn_h = 64;
    //SDL_Texture *button_tex = SDL_CreateTexture(renderer,
    //                                            SDL_PIXELFORMAT_RGBA8888,
    //                                            SDL_TEXTUREACCESS_TARGET,
    //                                            btn_w, btn_h);
    // draw button
    //SDL_SetTextureBlendMode(button_tex, SDL_BLENDMODE_BLEND);
    //SDL_SetRenderTarget(renderer, button_tex);
    // прозрачный фон
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //SDL_RenderClear(renderer);
    
    //SDL_SetRenderTarget(renderer, NULL);


TTF_Init();
TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 28);

SDL_Color bg = {30, 30, 30, 220};
SDL_Color fg = {255, 255, 255, 255};

SDL_Texture* button_tex = create_button_texture(
    renderer,
    font,
    "EXIT",
    200, 60,
    bg, fg,
    15
);

SDL_Rect button_rect = {40, 40, 200, 60};

    // Play this file.
    //const char *cmd[] = {"loadfile", argv[1], NULL};
    //mpv_command_async(mpv, 0, cmd);
    load_current(mpv);

    SDL_TimerID timer_id = SDL_AddTimer(
        33,
        timer_callback,
        &TIMER_EVENT
    );
    bool rv = false;
    bool rb = false;

    while (1) {
        SDL_Event event;
        if (SDL_WaitEvent(&event) != 1)
            die("event loop error");
        int redraw = 0;
        switch (event.type) {
        case SDL_QUIT:
            std::cout << "!!!!!QUIT!!!!!!" << std::endl;
            goto done;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                redraw = 1;
                std::cout << "!!!!!!EXPOSED!!!!!!!" << std::endl;
            }
                
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_SPACE) {
                const char *cmd_pause[] = {"cycle", "pause", NULL};
                mpv_command_async(mpv, 0, cmd_pause);
            }
            if (event.key.keysym.sym == SDLK_s) {
                // Also requires MPV_RENDER_PARAM_ADVANCED_CONTROL if you want
                // screenshots to be rendered on GPU (like --vo=gpu would do).
                const char *cmd_scr[] = {"screenshot-to-file",
                                         "screenshot.png",
                                         "window",
                                         NULL};
                printf("attempting to save screenshot to %s\n", cmd_scr[1]);
                mpv_command_async(mpv, 0, cmd_scr);
            }
            break;
        default:
            // Happens when there is new work for the render thread (such as
            // rendering a new video frame or redrawing it).
            if (event.type == wakeup_on_mpv_render_update) {
                uint64_t flags = mpv_render_context_update(mpv_rd);
                if (flags & MPV_RENDER_UPDATE_FRAME) {
                    redraw = 1;
                    rb = true;
                    rv = true;
                }
            }
            // Happens when at least 1 new event is in the mpv event queue.
            if (event.type == wakeup_on_mpv_events) {
                // Handle all remaining mpv events.
                while (1) {
                    mpv_event *mp_event = mpv_wait_event(mpv, 0);
                    if (mp_event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                        mpv_event_property* prop = (mpv_event_property*)mp_event->data;
                    
                        if (strcmp(prop->name, "eof-reached") == 0 && prop->format == MPV_FORMAT_FLAG) {
                            if (*(int*)prop->data == 1) {
                                load_current(mpv);
                                mpv_set_property_string(mpv, "eof-reached", "no");
                            }
                        }
                    }

                    if (mp_event->event_id == MPV_EVENT_NONE)
                        break;

                    if (mp_event->event_id == MPV_EVENT_END_FILE) {
                        load_current(mpv);
                    }
                }
            }
            if (event.type == TIMER_EVENT) {
                redraw = 1;
                rb = true;
            }
        }
        Uint32 t = SDL_GetTicks();
        if (redraw) {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            if (!tex || tex_w != w || tex_h != h) {
                SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888,
                                        SDL_TEXTUREACCESS_STREAMING, w, h);
                if (!tex) {
                    printf("could not allocate texture\n");
                    exit(1);
                }
                tex_w = w;
                tex_h = h;
            }
            void *pixels;
            int pitch;
            if (rv) {
                std::cout << t << std::endl;
                if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
                    printf("could not lock texture\n");
                    exit(1);
                }
                size_t pp = pitch;
                int wh[2] = {w, h};
                mpv_render_param params[] = {
                    {MPV_RENDER_PARAM_SW_SIZE, &wh},
                    {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("0bgr")},
                    {MPV_RENDER_PARAM_SW_STRIDE, &pp},
                    {MPV_RENDER_PARAM_SW_POINTER, pixels},
                    {MPV_RENDER_PARAM_INVALID, nullptr}
                };
                int r = mpv_render_context_render(mpv_rd, params);
                if (r < 0) {
                    printf("mpv_render_context_render error: %s\n",
                        mpv_error_string(r));
                    exit(1);
                }
                SDL_UnlockTexture(tex);
            }
            SDL_RenderCopy(renderer, tex, NULL, NULL);

            // ---------- draw button ----------
        
        if (rb) {
            positioner.update(static_cast<float>(t) / 1000.f);

            auto [bl, br, bt, bb] = positioner.button_rectangle();

            //float fx = ( 0.5f + 0.5f) * (w - btn_w);
            //float fy = h - 100;

            SDL_Texture* button_tex = create_button_texture(
                renderer,
                font,
                "EXIT",
                br - bl, bb - bt,
                bg, fg,
                15
            );
            
            SDL_Rect button_rect = {bl, bt, br - bl, bb - bt};

            //SDL_Rect dst = { (int) 0, (int) 0, br - bl, bb - bt };
            // кнопка поверх видео
            SDL_RenderCopy(renderer, button_tex, NULL, &button_rect);
        }

        SDL_RenderPresent(renderer);

        //SDL_RenderPresent(renderer);
        }
    }
done:

    SDL_DestroyTexture(tex);

    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    mpv_render_context_free(mpv_rd);

    mpv_destroy(mpv);

    printf("properly terminated\n");
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