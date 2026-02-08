#pragma once

#include <cstdint>

#include <parameters.hpp>
#include <saver.hpp>
#include <spdlog/spdlog.h>
#include <xcb/xcb.h>

class blank_t : public saver_t {
public:
    virtual ~blank_t() { reset(); }

    void configure(xcb_connection_t *connection, xcb_window_t window) {
        auto log = spdlog::get(log_name);
        log->info("Configure blank screensaver");

        // Clean up first.
        reset();

        // Initialize connection and window.
        m_connection = connection;
        m_window = window;
        // Fetch screen geometry.
        xcb_get_geometry_reply_t *geometry
            = xcb_get_geometry_reply(m_connection, xcb_get_geometry(m_connection, window), nullptr);
        m_width = geometry->width;
        m_height = geometry->height;
        uint8_t depth = geometry->depth;
        free(geometry);
        // Create pixmap.
        m_pixmap = xcb_generate_id(m_connection);
        xcb_create_pixmap(m_connection, depth, m_pixmap, window, m_width, m_height);
        // Create global context.
        m_gc = xcb_generate_id(m_connection);
        xcb_create_gc(m_connection, m_gc, m_pixmap, 0, nullptr);

        m_is_configured = true;
    }

    virtual void reset() {
        auto log = spdlog::get(log_name);
        log->info("Reset blank screensaver");
        if (m_is_configured) {
            xcb_free_gc(m_connection, m_gc);
            xcb_free_pixmap(m_connection, m_pixmap);
            xcb_flush(m_connection);
            m_gc = 0;
            m_pixmap = 0;
            m_connection = nullptr;
            m_window = 0;

            m_is_configured = false;
        }
    }

    virtual void run() {
        auto log = spdlog::get(log_name);
        log->info("Run blank screensaver");
        if (m_is_configured) {
            xcb_change_gc(m_connection, m_gc, XCB_GC_FOREGROUND, &black);
            xcb_rectangle_t rectangle = {0, 0, m_width, m_height};
            xcb_poly_fill_rectangle(m_connection, m_pixmap, m_gc, 1, &rectangle);
            xcb_copy_area(m_connection, m_pixmap, m_window, m_gc, 0, 0, 0, 0, m_width, m_height);
            xcb_flush(m_connection);
            log->info("Blank screensaver became active");
        }
    }

private:
    bool m_is_configured = false;

    xcb_connection_t *m_connection = nullptr;
    xcb_drawable_t m_window = 0;
    xcb_gcontext_t m_gc = 0;
    xcb_pixmap_t m_pixmap = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;

    inline constexpr static uint32_t black = 0x00000000;
};
