#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <button_layer.hpp>
#include <configuration.hpp>
#include <media_layer.hpp>
#include <parameters.hpp>
#include <saver.hpp>
#include <spdlog/spdlog.h>
#include <xcb/xcb.h>

class render_t : public saver_t {
public:
    ~render_t() { reset(); }

    void configure(xcb_connection_t *connection, xcb_drawable_t window, const configuration_t &configuration) {
        auto log = spdlog::get(log_name);
        log->info("Configure media screensaver");

        m_connection = connection;
        m_window = window;
        m_configuration = configuration;
        m_media_layer.configure(configuration.media());
        m_button_layer.configure(configuration.button());

        m_screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;
        xcb_depth_iterator_t depth_iterator;
        depth_iterator = xcb_screen_allowed_depths_iterator(m_screen);
        for (; depth_iterator.rem; xcb_depth_next(&depth_iterator)) {
            xcb_visualtype_iterator_t visual_iterator;
            visual_iterator = xcb_depth_visuals_iterator(depth_iterator.data);
            for (; visual_iterator.rem; xcb_visualtype_next(&visual_iterator)) {
                if (m_screen->root_visual == visual_iterator.data->visual_id) {
                    m_visual_type = visual_iterator.data;
                    break;
                }
            }
        }

        xcb_get_geometry_reply_t *geometry
            = xcb_get_geometry_reply(m_connection, xcb_get_geometry(m_connection, m_window), nullptr);
        if (!geometry) {
            return;
        }

        int width = geometry->width;
        int height = geometry->height;
        m_depth = geometry->depth;
        free(geometry);

        m_width = width;
        m_height = height;

        if (m_pixmap) {
            xcb_free_pixmap(m_connection, m_pixmap);
        }
        m_pixmap = xcb_generate_id(m_connection);
        xcb_create_pixmap(m_connection, m_depth, m_pixmap, m_window, width, height);

        if (m_gc) {
            xcb_free_gc(m_connection, m_gc);
        }
        m_gc = xcb_generate_id(m_connection);
        xcb_create_gc(m_connection, m_gc, m_pixmap, 0, nullptr);

        m_is_configured = true;
    }

    virtual void reset() {
        auto log = spdlog::get(log_name);
        log->info("Reset media screensaver");
        if (m_is_configured) {
            m_terminate_thread = true;
            if (m_render_thread.joinable()) {
                log->info("Wait for the render thread to be stopped...");
                m_render_thread.join();
                log->info("Render thread is stopped");
            }

            xcb_free_gc(m_connection, m_gc);
            xcb_free_pixmap(m_connection, m_pixmap);
            xcb_flush(m_connection);
            m_gc = 0;
            m_pixmap = 0;
            m_window = 0;
            m_connection = nullptr;
            m_visual_type = nullptr;
            m_screen = nullptr;

            m_is_configured = false;
        }
    }

    void run() {
        using std::chrono::operator""ms;
        using std::chrono::operator""s;

        auto log = spdlog::get(log_name);
        log->info("Run media screensaver");

        if (m_is_running || !m_is_configured) {
            return;
        }

        m_is_running = true;

        m_render_thread = std::thread([this]() {
            constexpr auto sleep_duration = 5ms;
            constexpr auto metrics_accumulation_duration = 30s;
            auto awake = std::chrono::steady_clock::now();

            auto log = spdlog::get(log_name);
            log->info("Start rendering thread, update period is {}s",
                      std::chrono::duration_cast<std::chrono::duration<float>>(sleep_duration).count());

            int frame_counter = 0;
            int invalidation_counter = 0;
            float media_layer_update_time_accumulator = 0.0;
            float button_layer_update_time_accumulator = 0.0;
            float frame_time_accumulator = 0.0;
            int starvation_counter = 0;
            float starvation_time_accumulator = 0.0;
            float media_layer_rendering_time_accumulator = 0.0;
            float button_layer_rendering_time_accumulator = 0.0;
            float flush_time_accumulator = 0.0;

            auto metrics_acc_start_timestamp = std::chrono::steady_clock::now();

            while (!m_terminate_thread) {
                auto now = std::chrono::steady_clock::now();
                m_media_layer.update(now);
                auto media_layer_update_timestamp = std::chrono::steady_clock::now();
                m_button_layer.update(now, m_width, m_height);
                auto button_layer_update_timestamp = std::chrono::steady_clock::now();

                if (m_media_layer.invalidated() || m_button_layer.invalidated()) {
                    xcb_change_gc(m_connection, m_gc, XCB_GC_FOREGROUND, &black);
                    xcb_rectangle_t rectangle = {0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height)};
                    xcb_poly_fill_rectangle(m_connection, m_pixmap, m_gc, 1, &rectangle);
                    auto start_invalidation_timestamp = std::chrono::steady_clock::now();
                    m_media_layer.render(m_connection, m_pixmap, m_gc, m_width, m_height, m_depth);
                    auto media_layer_rendering_timestamp = std::chrono::steady_clock::now();
                    m_button_layer.render(m_connection, m_visual_type, m_pixmap, m_gc, m_width, m_height, m_depth);
                    auto button_layer_rendering_timestamp = std::chrono::steady_clock::now();
                    xcb_copy_area(m_connection, m_pixmap, m_window, m_gc, 0, 0, 0, 0, m_width, m_height);
                    xcb_flush(m_connection);
                    auto flush_timestamp = std::chrono::steady_clock::now();

                    // Calculate metrics.
                    float media_layer_rendering_time
                        = std::chrono::duration_cast<std::chrono::duration<float>>(media_layer_rendering_timestamp
                                                                                   - start_invalidation_timestamp)
                              .count();

                    float button_layer_rendering_time
                        = std::chrono::duration_cast<std::chrono::duration<float>>(button_layer_rendering_timestamp
                                                                                   - media_layer_rendering_timestamp)
                              .count();

                    float flush_time = std::chrono::duration_cast<std::chrono::duration<float>>(
                                           flush_timestamp - button_layer_rendering_timestamp)
                                           .count();

                    media_layer_rendering_time_accumulator += media_layer_rendering_time;
                    button_layer_rendering_time_accumulator += button_layer_rendering_time;
                    flush_time_accumulator += flush_time;
                    ++invalidation_counter;
                }

                auto frame_finished_timestamp = std::chrono::steady_clock::now();
                ++frame_counter;

                // Calculate metrics.
                float media_layer_update_time
                    = std::chrono::duration_cast<std::chrono::duration<float>>(media_layer_update_timestamp - now)
                          .count();

                float button_layer_update_time = std::chrono::duration_cast<std::chrono::duration<float>>(
                                                     button_layer_update_timestamp - media_layer_update_timestamp)
                                                     .count();

                float frame_time
                    = std::chrono::duration_cast<std::chrono::duration<float>>(frame_finished_timestamp - now).count();

                if (frame_time > std::chrono::duration_cast<std::chrono::duration<float>>(sleep_duration).count()) {
                    ++starvation_counter;
                    starvation_time_accumulator
                        += frame_time
                           - std::chrono::duration_cast<std::chrono::duration<float>>(sleep_duration).count();
                }

                if (metrics_acc_start_timestamp + metrics_accumulation_duration <= frame_finished_timestamp) {
                    if (frame_counter != 0) {
                        float avg_media_layer_update_time
                            = media_layer_rendering_time_accumulator / static_cast<float>(frame_counter);
                        float avg_button_layer_update_time
                            = button_layer_rendering_time_accumulator / static_cast<float>(frame_counter);
                        float avg_frame_time = frame_time_accumulator / static_cast<float>(frame_counter);

                        log->info("Time spent: media - {}s, button - {}s, frame - {}s", avg_media_layer_update_time,
                                  avg_button_layer_update_time, avg_frame_time);
                    }

                    if (starvation_counter != 0) {
                        log->warn("There were {} starvations ({}s in average)", starvation_counter,
                                  starvation_time_accumulator / static_cast<float>(starvation_counter));
                    }

                    if (invalidation_counter != 0) {
                        float avg_media_layer_rendering_time
                            = media_layer_rendering_time_accumulator / static_cast<float>(invalidation_counter);
                        float avg_button_layer_rendering_time
                            = button_layer_rendering_time_accumulator / static_cast<float>(invalidation_counter);
                        float avg_flush_time = flush_time_accumulator / static_cast<float>(invalidation_counter);
                        log->info("Time spent (invalidation): media - {}s, button - {}s, flush - {}s",
                                  avg_media_layer_rendering_time, avg_button_layer_rendering_time, avg_flush_time);
                    }

                    frame_counter = 0;
                    invalidation_counter = 0;
                    media_layer_update_time_accumulator = 0.0;
                    button_layer_update_time_accumulator = 0.0;
                    frame_time_accumulator = 0.0;
                    starvation_counter = 0;
                    starvation_time_accumulator = 0.0;
                    media_layer_rendering_time_accumulator = 0.0;
                    button_layer_rendering_time_accumulator = 0.0;
                    flush_time_accumulator = 0.0;

                    metrics_acc_start_timestamp = frame_finished_timestamp;
                }

                awake += sleep_duration;
                std::this_thread::sleep_until(awake);
            }
        });
    }

private:
    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    xcb_visualtype_t *m_visual_type = nullptr;
    xcb_drawable_t m_window = 0;
    xcb_gcontext_t m_gc = 0;
    xcb_pixmap_t m_pixmap = 0;
    int m_depth = 0;
    int m_width = 0;
    int m_height = 0;
    configuration_t m_configuration;
    media_layer_t m_media_layer;
    button_layer_t m_button_layer;

    bool m_is_running = false;
    std::thread m_render_thread;
    std::atomic<bool> m_terminate_thread{false};

    bool m_is_configured = false;

    inline constexpr static uint32_t black = 0x00000000;
};
