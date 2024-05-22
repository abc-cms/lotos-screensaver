#pragma once

#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include <blank.hpp>
#include <configuration.hpp>
#include <render.hpp>
#include <saver.hpp>
#include <xcb/screensaver.h>

using namespace std::chrono;
using namespace std::placeholders;

enum class saver_type_e : uint8_t { none = 0, blank, render };

class saver_manager_t {
public:
    saver_manager_t() {
        //std::cout << "CONNECT 1\n";
        m_connection = xcb_connect(nullptr, nullptr);
        //std::cout << "CONNECT 2\n";
        m_screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;
        //std::cout << "CONNECT 3\n";
    }

    ~saver_manager_t() {
        terminate();
        if (m_connection) {
            xcb_disconnect(m_connection);
            m_connection = nullptr;
        }
    }

    void run() {
        // Read configuration.
        //std::cout << "1\n";
        configuration_t configuration = configuration_t::load(get_configuration_path());
        //std::cout << "2\n";
        configure(configuration);
        // Start auxiliary threads.
        //std::cout << "3\n";
        m_configuration_thread = std::thread(&saver_manager_t::configuration_thread, this);
        //std::cout << "4\n";
        m_manager_thread = std::thread(&saver_manager_t::manager_thread, this);
        //std::cout << "5\n";
        // Set exit handler and start main loop.
        std::signal(SIGTERM, handle_signals);
        //std::cout << "6\n";
        std::signal(SIGINT, handle_signals);
        //std::cout << "7\n";
        
        main_loop();
    }

    void terminate() {
        // Terminate configuration thread.
        m_terminate_configuration_thread = true;
        m_terminate_configuration_thread_cv.notify_all();
        if (m_configuration_thread.joinable()) {
            m_configuration_thread.join();
        }

        // Terminate manager thread.
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
        xcb_generic_event_t *event;
        //std::cout << "LOOP\n";
        while (event = xcb_wait_for_event(m_connection)) {
            if (event->response_type == (m_first_event + XCB_SCREENSAVER_NOTIFY)) {
                xcb_screensaver_notify_event_t *ssn_event = reinterpret_cast<xcb_screensaver_notify_event_t *>(event);

                if (ssn_event->state == XCB_SCREENSAVER_STATE_ON) {
                    {
                        std::lock_guard<std::mutex> lock(m_saver_mutex);
                        m_saver_window = ssn_event->window;
                        m_saver_is_active = true;
                    }
                    update_saver();
                } else if (ssn_event->state == XCB_SCREENSAVER_STATE_OFF) {
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
        //std::cout << "!" << m_connection << " !!! " << event << "\n";
        terminate();
        m_connection = nullptr;
    }

    void configure(const configuration_t &configuration) {
        m_configuration = configuration;

        // Stop saver.
        //std::cout << "CONF 1\n";
        use_saver(saver_type_e::none);

        // Configure X11 SCREENSAVER extension.
        uint32_t mask = 0; // XCB_CW_BACK_PIXEL;
        uint32_t values[] = {m_screen->black_pixel};
        //std::cout << "CONF 2\n";
        auto sse_data = xcb_get_extension_data(m_connection, &xcb_screensaver_id);
        //std::cout << "CONF 3: " << sse_data << " | " << m_connection << "\n";
        m_first_event = sse_data->first_event;
        //std::cout << "CONF 4\n";
        xcb_set_screen_saver(m_connection, m_configuration.timeout(), 0, XCB_BLANKING_PREFERRED,
                             XCB_EXPOSURES_NOT_ALLOWED);
        //std::cout << "CONF 5\n";
        xcb_screensaver_set_attributes(m_connection, m_screen->root, 0, 0, m_screen->width_in_pixels,
                                       m_screen->height_in_pixels, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
                                       XCB_COPY_FROM_PARENT, m_screen->root_visual, mask, values);
        //std::cout << "CONF 6\n";
        xcb_screensaver_select_input(m_connection, m_screen->root, XCB_SCREENSAVER_EVENT_NOTIFY_MASK);
        //std::cout << "CONF 7\n";
        xcb_flush(m_connection);
        //std::cout << "CONF 8\n";
        update_saver();
    }

    void use_saver(saver_type_e saver_type) {
        std::lock_guard<std::mutex> lock(m_saver_mutex);
        if (saver_type != m_saver_type) {
            m_saver.reset();
            if (saver_type == saver_type_e::blank) {
                //std::cout << "XXXXXX4\n";
                auto saver = std::make_unique<blank_t>();
                saver->configure(m_connection, m_saver_window);
                saver->run();
                m_saver = std::move(saver);
            } else if (saver_type == saver_type_e::render) {
                //std::cout << "XXXXXX5\n";
                auto saver = std::make_unique<render_t>();
                saver->configure(m_connection, m_saver_window, m_configuration);
                saver->run();
                m_saver = std::move(saver);
            }
            m_saver_type = saver_type;
        }
    }

    void update_saver() {
        //std::cout << "UPDATE_SAVER\n";
        saver_type_e saver_type = get_appropriate_saver_type();
        //std::cout << "UPDATE_SAVER 2\n";
        use_saver(saver_type);
    }

    void configuration_thread() {
        std::unique_lock<std::mutex> lock(m_terminate_configuration_thread_mutex);

        while (true) {
            if (m_terminate_configuration_thread_cv.wait_for(
                    lock, update_configuration_rate, [this] { return m_terminate_configuration_thread.load(); })) {
                break;
            }

            // Load screensaver configuration.
            configuration_t configuration = configuration_t::load(get_configuration_path());

            if (configuration != m_configuration) {
                configure(configuration);
            }
        }
    }

    void manager_thread() {
        std::unique_lock<std::mutex> lock(m_terminate_manager_thread_mutex);

        while (true) {
            if (m_terminate_manager_thread_cv.wait_for(lock, update_saver_type_rate,
                                                       [this] { return m_terminate_manager_thread.load(); })) {
                break;
            }

            update_saver();
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
