#pragma once

#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include <blank.hpp>
#include <configuration.hpp>
#include <parameters.hpp>
#include <render.hpp>
#include <saver.hpp>
#include <spdlog/spdlog.h>
#include <xcb/screensaver.h>

using namespace std::chrono;
using namespace std::placeholders;

enum class saver_type_e : uint8_t { none = 0, blank, render };

class saver_manager_t {
public:
    saver_manager_t() {
        auto log = spdlog::get(log_name);
        log->info("Create screensaver manager");
        m_connection = xcb_connect(nullptr, nullptr);
        m_screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;
        log->info("Screensaver manager created");
    }

    ~saver_manager_t() {
        auto log = spdlog::get(log_name);
        log->info("Destroy screensaver manager");
        terminate();
        if (m_connection) {
            xcb_disconnect(m_connection);
            m_connection = nullptr;
        }
        log->info("Screensaver manager destroyed");
    }

    void run() {
        auto log = spdlog::get(log_name);
        log->info("Run screensaver");
        // Read configuration.
        configuration_t configuration = configuration_t::load(get_configuration_path());
        configure(configuration);
        // Start auxiliary threads.
        log->info("Start utility thread");
        m_configuration_thread = std::thread(&saver_manager_t::configuration_thread, this);
        log->info("Start rendering thread");
        m_manager_thread = std::thread(&saver_manager_t::manager_thread, this);
        // Set exit handler and start main loop.
        log->info("Set SIGTERM and SIGINT signals");
        std::signal(SIGTERM, handle_signals);
        std::signal(SIGINT, handle_signals);

        main_loop();
    }

    void terminate() {
        auto log = spdlog::get(log_name);

        // Terminate configuration thread.
        log->info("Waiting for the utility thread to be stopped...");
        m_terminate_configuration_thread = true;
        m_terminate_configuration_thread_cv.notify_all();
        if (m_configuration_thread.joinable()) {
            m_configuration_thread.join();
        }

        // Terminate manager thread.
        log->info("Waiting for the rendering thread to be stopped...");
        m_terminate_manager_thread = true;
        m_terminate_manager_thread_cv.notify_all();
        if (m_manager_thread.joinable()) {
            m_manager_thread.join();
        }
    }

protected:
    static void handle_signals(int signal) {
        if (m_connection) {
            xcb_screensaver_unset_attributes(m_connection, m_screen->root);
            xcb_flush(m_connection);
            xcb_disconnect(m_connection);
        }
    }

    void main_loop() {
        auto log = spdlog::get(log_name);
        log->info("Start main screensaver loop");

        xcb_generic_event_t *event;
        while (event = xcb_wait_for_event(m_connection)) {
            if (event->response_type == (m_first_event + XCB_SCREENSAVER_NOTIFY)) {
                xcb_screensaver_notify_event_t *ssn_event = reinterpret_cast<xcb_screensaver_notify_event_t *>(event);

                if (ssn_event->state == XCB_SCREENSAVER_STATE_ON) {
                    log->info("Activate screensaver");
                    {
                        std::lock_guard<std::mutex> lock(m_saver_mutex);
                        m_saver_window = ssn_event->window;
                        m_saver_is_active = true;
                    }
                    update_saver();
                } else if (ssn_event->state == XCB_SCREENSAVER_STATE_OFF) {
                    log->info("Deactivate screensaver");
                    {
                        std::lock_guard<std::mutex> lock(m_saver_mutex);
                        m_saver_window = 0;
                        m_saver_is_active = false;
                    }
                    update_saver();
                }
            }

            free(event);
        }

        log->info("The main screensaver loop exited");

        terminate();
        m_connection = nullptr;
    }

    void configure(const configuration_t &configuration) {
        auto log = spdlog::get(log_name);
        log->info("Start screensaver configuration");

        m_configuration = configuration;

        // Stop saver.
        log->info("Deactivate the current screensaver (if active)");
        use_saver(saver_type_e::none);

        // Configure X11 SCREENSAVER extension.
        log->info("Configure X11 screensaver extension");
        uint32_t mask = 0; // XCB_CW_BACK_PIXEL;
        uint32_t values[] = {m_screen->black_pixel};
        auto sse_data = xcb_get_extension_data(m_connection, &xcb_screensaver_id);
        m_first_event = sse_data->first_event;
        xcb_set_screen_saver(m_connection, m_configuration.timeout(), 0, XCB_BLANKING_PREFERRED,
                             XCB_EXPOSURES_NOT_ALLOWED);
        xcb_screensaver_set_attributes(m_connection, m_screen->root, 0, 0, m_screen->width_in_pixels,
                                       m_screen->height_in_pixels, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
                                       XCB_COPY_FROM_PARENT, m_screen->root_visual, mask, values);
        xcb_screensaver_select_input(m_connection, m_screen->root, XCB_SCREENSAVER_EVENT_NOTIFY_MASK);
        xcb_flush(m_connection);
        update_saver();
    }

    void use_saver(saver_type_e saver_type) {
        auto log = spdlog::get(log_name);

        std::lock_guard<std::mutex> lock(m_saver_mutex);

        if (saver_type != m_saver_type) {
            m_saver.reset();
            if (saver_type == saver_type_e::blank) {
                log->info("Use a blank screensaver");
                auto saver = std::make_unique<blank_t>();
                saver->configure(m_connection, m_saver_window);
                saver->run();
                m_saver = std::move(saver);
            } else if (saver_type == saver_type_e::render) {
                log->info("Use a media screensaver");
                auto saver = std::make_unique<render_t>();
                saver->configure(m_connection, m_saver_window, m_configuration);
                saver->run();
                m_saver = std::move(saver);
            } else {
                log->info("Set no screensaver");
            }
            m_saver_type = saver_type;
        }
    }

    void update_saver() {
        saver_type_e saver_type = get_appropriate_saver_type();
        use_saver(saver_type);
    }

    void configuration_thread() {
        auto log = spdlog::get(log_name);
        log->info("Utility thread started, update period is {}s",
                  std::chrono::duration_cast<std::chrono::duration<float>>(update_configuration_rate).count());

        std::unique_lock<std::mutex> lock(m_terminate_configuration_thread_mutex);

        while (true) {
            // Load screensaver configuration.
            log->info("Reload configuration (if updated externally)");
            configuration_t configuration = configuration_t::load(get_configuration_path());

            if (configuration != m_configuration) {
                configure(configuration);
            }

            if (m_terminate_configuration_thread_cv.wait_for(
                    lock, update_configuration_rate, [this] { return m_terminate_configuration_thread.load(); })) {
                break;
            }
        }
    }

    void manager_thread() {
        auto log = spdlog::get(log_name);
        log->info("Manager thread started, update period is {}s",
                  std::chrono::duration_cast<std::chrono::duration<float>>(update_saver_type_rate).count());

        std::unique_lock<std::mutex> lock(m_terminate_manager_thread_mutex);

        while (true) {
            update_saver();

            if (m_terminate_manager_thread_cv.wait_for(lock, update_saver_type_rate,
                                                       [this] { return m_terminate_manager_thread.load(); })) {
                break;
            }
        }
    }

    saver_type_e get_appropriate_saver_type() const {
        if (m_saver_is_active) {
            return is_active_period() ? saver_type_e::render : saver_type_e::blank;
        }

        return saver_type_e::none;
    }

    bool is_active_period() const {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local = *std::localtime(&now);

        int hours = local.tm_hour;
        int minutes = local.tm_min;

        for (const auto &interval : m_configuration.activity_frames()) {
            const auto start_hours = interval.start.hours;
            const auto start_minutes = interval.start.minutes;
            const auto end_hours = interval.end.hours;
            const auto end_minutes = interval.end.minutes;

            if ((hours > start_hours || (hours == start_hours && minutes >= start_minutes))
                && (hours < end_hours || (hours == end_hours && minutes < end_minutes))) {
                return true;
            }
        }
        return false;
    }

private:
    std::thread m_configuration_thread;
    std::mutex m_terminate_configuration_thread_mutex;
    std::condition_variable m_terminate_configuration_thread_cv;
    std::atomic<bool> m_terminate_configuration_thread = false;

    std::thread m_manager_thread;
    std::mutex m_terminate_manager_thread_mutex;
    std::condition_variable m_terminate_manager_thread_cv;
    std::atomic<bool> m_terminate_manager_thread = false;

    xcb_window_t m_saver_window = 0;
    std::unique_ptr<saver_t> m_saver;
    saver_type_e m_saver_type = saver_type_e::none;
    bool m_saver_is_active = false;
    std::mutex m_saver_mutex;

    configuration_t m_configuration;

    inline static xcb_connection_t *m_connection = nullptr;
    inline static xcb_screen_t *m_screen = nullptr;
    uint8_t m_first_event = 0;

    constexpr static std::chrono::steady_clock::duration update_configuration_rate = 60s;
    constexpr static std::chrono::steady_clock::duration update_saver_type_rate = 1s;
};
