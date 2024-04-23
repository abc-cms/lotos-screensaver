#pragma once

#include <chrono>
#include <tuple>

#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <parameters.hpp>
#include <xcb/xcb_image.h>

class slider_t {
public:
    void configure(const std::list<media_configuration_t> &media) {
        m_media.clear();
        m_media.reserve(media.size());
        for (const auto &item : media) {
            m_media.push_back(item);
        }
    }

    std::tuple<cv::Mat, std::chrono::steady_clock::duration> next() {
        while (true) {
            auto media = m_media[m_current_index];

            if (media.media_type() == media_type_e::video) {
                if (!m_capture.isOpened()) {
                    m_capture.open(absolute_path(media.path()).string());
                }

                cv::Mat frame;
                m_capture >> frame;

                if (!frame.empty()) {
                    return std::make_tuple(frame, std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                      std::chrono::duration<float>(
                                                          1.0 / static_cast<float>(m_capture.get(cv::CAP_PROP_FPS)))));
                }

                m_current_index = (m_current_index + 1) % m_media.size();
                m_capture.release();
            }

            media = m_media[m_current_index];
            if (media.media_type() == media_type_e::image) {
                m_current_index = (m_current_index + 1) % m_media.size();

                return std::make_tuple(cv::imread(absolute_path(media.path()).string()),
                                       std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                           std::chrono::duration<float>(media.duration())));
            }
        }
    }

private:
    int m_current_index = 0;
    std::vector<media_configuration_t> m_media;
    cv::VideoCapture m_capture;
};

class media_layer_t {
public:
    void configure(const std::list<media_configuration_t> &media) { m_slider.configure(media); }

    void update(const auto &time) {
        if (m_image_data.empty()) {
            auto [image_data, duration] = m_slider.next();
            m_next_frame = time + duration;
            m_image_data = image_data;
            cv::cvtColor(m_image_data, m_image_data, cv::COLOR_BGR2BGRA);
            m_is_valid = false;
        } else {
            if (time >= m_next_frame) {
                auto [image_data, duration] = m_slider.next();
                m_next_frame += duration;
                m_image_data = image_data;
                cv::cvtColor(m_image_data, m_image_data, cv::COLOR_BGR2BGRA);
                m_is_valid = false;
            }
        }
    }

    void render(xcb_connection_t *connection, xcb_pixmap_t pixmap, xcb_gcontext_t gc, int target_width,
                int target_height, int depth) {
        auto image_size = m_image_data.size;
        auto [width, height, scale] = target_size(target_width, target_height, image_size[1], image_size[0]);

        cv::Mat image(height, width, CV_8UC3);
        resize(m_image_data, image, image.size(), (scale > 1.0) ? cv::INTER_CUBIC : cv::INTER_AREA);

        void *base = malloc(width * height * 4);
        xcb_image_t *native_image = xcb_image_create_native(connection, width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, depth,
                                                            base, width * height * 4, image.ptr());
        xcb_image_put(connection, pixmap, gc, native_image, (target_width - width) / 2, (target_height - height) / 2,
                      0);
        xcb_image_destroy(native_image);
        m_is_valid = true;
    }

    bool invalidated() const { return !m_is_valid; }

protected:
    static std::tuple<int, int, float> target_size(int target_width, int target_height, int frame_width,
                                                   int frame_height) {
        int width, height;
        float target_ratio = static_cast<float>(target_width) / static_cast<float>(target_height);
        float frame_ration = static_cast<float>(frame_width) / static_cast<float>(frame_height);

        float scale;
        if (frame_ration <= target_ratio) {
            scale = static_cast<float>(target_height) / static_cast<float>(frame_height);
            height = target_height;
            width = static_cast<int>(scale * frame_width);
        } else {
            scale = static_cast<float>(target_width) / static_cast<float>(frame_width);
            height = static_cast<int>(scale * frame_height);
            width = target_width;
        }

        return std::make_tuple(width, height, scale);
    }

private:
    slider_t m_slider;
    bool m_is_valid = false;
    cv::Mat m_image_data;
    std::chrono::time_point<std::chrono::steady_clock> m_next_frame;
};
