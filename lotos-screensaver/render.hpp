#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <button_layer.hpp>
#include <configuration.hpp>
#include <media_layer.hpp>
#include <saver.hpp>
#include <xcb/xcb.h>

class render_t : public saver_t {
public:
    ~render_t() { reset(); }

    void configure(xcb_connection_t *connection, xcb_drawable_t window, const configuration_t &configuration) {
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
        if (m_is_configured) {
            m_terminate_thread = true;
            if (m_render_thread.joinable()) {
                m_render_thread.join();
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

        if (m_is_running || !m_is_configured) {
            return;
        }

        m_is_running = true;

        m_render_thread = std::thread([this]() {
            auto sleep_duration = 5ms;
            auto awake = std::chrono::steady_clock::now() + sleep_duration;

            while (!m_terminate_thread) {
                auto now = std::chrono::steady_clock::now();
                m_media_layer.update(now);
                m_button_layer.update(now, m_width, m_height);

                if (m_media_layer.invalidated() || m_button_layer.invalidated()) {
                    xcb_change_gc(m_connection, m_gc, XCB_GC_FOREGROUND, &black);
                    xcb_rectangle_t rectangle = {0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height)};
                    xcb_poly_fill_rectangle(m_connection, m_pixmap, m_gc, 1, &rectangle);
                    m_media_layer.render(m_connection, m_pixmap, m_gc, m_width, m_height, m_depth);
                    m_button_layer.render(m_connection, m_visual_type, m_pixmap, m_gc, m_width, m_height, m_depth);
                    xcb_copy_area(m_connection, m_pixmap, m_window, m_gc, 0, 0, 0, 0, m_width, m_height);
                    xcb_flush(m_connection);
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
