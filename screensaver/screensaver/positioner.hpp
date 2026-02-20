#pragma once

#include <cmath>
#include <random>
#include <tuple>
#include <iostream>
#include "screensaver/screensaver/configuration.hpp"

class positioner_t {
public:
    void set_configuration(const button_configuration_t& configuration, const int window_width, const int window_height, const int button_height) {
        m_animation_duration = configuration.animation_duration();
        m_switch_duration = configuration.switch_duration();
        m_window_width = window_width;
        m_window_height = window_height;
        m_button_height = button_height;
        m_bottom_margin = configuration.bottom_margin();
        m_bottom_margin_random = configuration.bottom_margin_random();
        m_side_margin = configuration.side_margin();
        m_side_margin_random = configuration.side_margin_random();
        m_full_duration = m_animation_duration + m_switch_duration;

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
    int m_animation_duration;
    int m_switch_duration;
    int m_window_width;
    int m_window_height;
    int m_button_height;
    int m_bottom_margin;
    int m_bottom_margin_random;
    int m_side_margin;
    int m_side_margin_random;

    float m_full_duration;

    int previous_period = -1;
    int m_side_offset = 0;
    int m_bottom_offset = 0;
    int m_previous_side_offset = 0;
    int m_previous_bottom_offset = 0;
    float m_interpolation_factor = 0.f;

    std::mt19937 generator{23012012};
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
};
