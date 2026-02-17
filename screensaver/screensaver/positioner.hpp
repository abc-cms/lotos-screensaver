#pragma once

#include <cmath>
#include <random>
#include <tuple>
#include <iostream>

class positioner_t {
public:
    positioner_t(int animation_duration, int switch_duration, int window_width, int window_height, int button_height,
                 int bottom_margin, int bottom_margin_random, int side_margin, int side_margin_random, int radius)
        : m_animation_duration(animation_duration), m_switch_duration(switch_duration), m_window_width(window_width),
          m_window_height(window_height), m_button_height(button_height), m_bottom_margin(bottom_margin),
          m_bottom_margin_random(bottom_margin_random), m_side_margin(side_margin),
          m_side_margin_random(side_margin_random), m_full_duration(animation_duration + switch_duration) {
            update_offsets();
            update(0);
    }

    void update(const float time) {
        std::cout << time << std::endl;
        const int current_period = static_cast<int>(std::floor(time / m_full_duration));
        const float rest_period_time = std::fmod(time, m_full_duration);
    
        if (current_period != previous_period) {
            update_offsets();
            previous_period = current_period;
        }

        m_interpolation_factor = std::max(0.f, (rest_period_time - m_switch_duration)) / static_cast<float>(m_animation_duration);
        std::cout << current_period << " " << previous_period << " " << rest_period_time << " " <<  m_interpolation_factor << std::endl;
    }

    std::tuple<int, int, int, int> button_rectangle() const {
        const int side_offset = static_cast<int>(round(m_previous_side_offset * (1 - m_interpolation_factor) + m_side_offset * m_interpolation_factor));
        const int bottom_offset = static_cast<int>(round(m_previous_bottom_offset * (1 - m_interpolation_factor) + m_bottom_offset * m_interpolation_factor));
        return {
            side_offset,
            m_window_width - side_offset,
            m_window_height - bottom_offset - m_button_height,
            m_window_height - bottom_offset
        };
    }

private:
    void update_offsets() {
        m_previous_side_offset = m_side_offset;
        m_previous_bottom_offset = m_bottom_offset;
        m_side_offset = m_side_margin + static_cast<int>(round(static_cast<float>(m_side_margin_random) * distribution(generator)));
        m_bottom_offset = m_bottom_margin + static_cast<int>(round(static_cast<float>(m_bottom_margin_random) * distribution(generator)));
    }

private:
    const int m_animation_duration;
    const int m_switch_duration;
    const int m_window_width;
    const int m_window_height;
    const int m_button_height;
    const int m_bottom_margin;
    const int m_bottom_margin_random;
    const int m_side_margin;
    const int m_side_margin_random;

    const float m_full_duration;

    int previous_period = -1;
    int m_side_offset = 0;
    int m_bottom_offset = 0;
    int m_previous_side_offset = 0;
    int m_previous_bottom_offset = 0;
    float m_interpolation_factor = 0.f;

    std::mt19937 generator{23012012};
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
};
