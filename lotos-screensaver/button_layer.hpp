#pragma once

#include <chrono>
#include <map>
#include <random>
#include <sstream>
#include <string>

#include <cairo-xcb.h>
#include <cairo.h>
#include <configuration.hpp>

struct button_t {
    std::string text;
    uint32_t text_size;
    uint32_t text_color;
    int32_t x, y, width, height;
    uint32_t background_color;
    uint32_t corner_radius;
};

class button_layer_t {
public:
    void configure(const button_configuration_t &configuration) { m_configuration = configuration; }

    void update(const std::chrono::steady_clock::time_point &time, uint32_t width, uint32_t height) {
        if (!m_initialized) {
            m_initialized = true;
            m_animation = false;
            m_next_frame = time
                           + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                               std::chrono::duration<float>(m_configuration.switch_duration()));
            m_button = make_button(width, height);
            m_current_button = m_button;
            m_next_button = make_button(width, height);
            m_is_valid = false;
        } else {
            if (time >= m_next_frame) {
                if (m_animation) {
                    if (time >= m_animation_end_time) {
                        m_animation = false;
                        m_button = m_next_button;
                        m_current_button = m_button;
                        m_next_button = make_button(width, height);
                        m_next_frame = m_animation_end_time
                                       + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                           std::chrono::duration<float>(m_configuration.switch_duration()));
                    } else {
                        float fraction
                            = std::chrono::duration_cast<std::chrono::duration<float>>(time - m_animation_start_time)
                                  .count()
                              / m_configuration.animation_duration();
                        m_current_button = interpolate(m_button, m_next_button, fraction);
                        m_next_frame += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            std::chrono::duration<float>(m_configuration.animation_duration()
                                                         / m_configuration.animation_steps()));
                    }
                } else {
                    m_animation_start_time = m_next_frame;
                    m_animation_end_time = m_animation_start_time
                                           + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                               std::chrono::duration<float>(m_configuration.animation_duration()));
                    m_next_frame += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<float>(m_configuration.animation_duration()
                                                     / static_cast<float>(m_configuration.animation_steps())));
                    m_animation = true;
                }
                m_is_valid = false;
            }
        }
    }

    void render(xcb_connection_t *connection, xcb_visualtype_t *visual_type, xcb_pixmap_t pixmap, xcb_gcontext_t gc,
                int target_width, int target_height, int depth) {
        xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, &m_current_button.background_color);
        // Draw 3 rectangles.
        int16_t x = m_current_button.x;
        int16_t y = m_current_button.y;
        uint16_t width = m_current_button.width;
        uint16_t height = m_current_button.height;
        uint16_t radius = m_current_button.corner_radius;
        xcb_rectangle_t rectangles[]
            = {{x, static_cast<int16_t>(y + radius), radius, static_cast<uint16_t>(height - 2 * radius)},
               {static_cast<int16_t>(x + radius), y, static_cast<uint16_t>(width - 2 * radius), height},
               {static_cast<int16_t>(x + width - radius), static_cast<int16_t>(y + radius), radius,
                static_cast<uint16_t>(height - 2 * radius)}};
        xcb_poly_fill_rectangle(connection, pixmap, gc, 3, rectangles);
        // Draw 4 corners.
        xcb_arc_t arcs[]
            = {{x, y, 2 * radius, 2 * radius, 90 << 6, 90 << 6},
               {x, static_cast<int16_t>(y + height - 2 * radius), 2 * radius, 2 * radius, 180 << 6, 90 << 6},
               {static_cast<int16_t>(x + width - 2 * radius), y, 2 * radius, 2 * radius, 0, 90 << 6},
               {static_cast<int16_t>(x + width - 2 * radius), static_cast<int16_t>(y + height - 2 * radius), 2 * radius,
                2 * radius, 0, -90 << 6}};
        xcb_poly_fill_arc(connection, pixmap, gc, 4, arcs);

        uint32_t black = 0x00000000;
        xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, &black);

        cairo_surface_t *surface
            = cairo_xcb_surface_create(connection, pixmap, visual_type, target_width, target_height);
        cairo_t *context = cairo_create(surface);
        cairo_select_font_face(context, "FreeSans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        // cairo_text_extents_t extents;
        cairo_set_font_size(context, m_current_button.text_size);

        float r = static_cast<float>(m_current_button.text_color >> 16 & 0xff) / 255.0;
        float g = static_cast<float>(m_current_button.text_color >> 8 & 0xff) / 255.0;
        float b = static_cast<float>(m_current_button.text_color & 0xff) / 255.0;
        cairo_set_source_rgb(context, r, g, b);

        if (std::get<1>(m_chunks).empty()) {
            m_chunks = split(m_current_button.text, context);
        }
        int space_width = std::get<0>(m_chunks);
        auto lines = split_in_lines(m_chunks, width - 2 * radius);

        size_t number_of_lines = lines.size();
        std::vector<cairo_text_extents_t> extents(number_of_lines);
        int total_height = 0;
        for (size_t i = 0; i < number_of_lines; ++i) {
            if (auto cached = m_extents_cache.find(lines[i]); cached != m_extents_cache.end()) {
                extents[i] = cached->second;
            } else {
                cairo_text_extents(context, lines[i].c_str(), &extents[i]);
                m_extents_cache[lines[i]] = extents[i];
            }

            total_height += extents[i].height;
        }

        total_height += ((number_of_lines - 1) * m_current_button.text_size / 2);

        int offset = 0;
        for (size_t i = 0; i < number_of_lines; ++i) {
            float x = static_cast<float>(m_current_button.x)
                      + 0.5 * (static_cast<float>(m_current_button.width) - static_cast<float>(extents[i].width));
            float y = static_cast<float>(m_current_button.y)
                      + 0.5 * (static_cast<float>(m_current_button.height) - static_cast<float>(total_height))
                      - static_cast<float>(extents[i].y_bearing) + static_cast<float>(offset);
            cairo_move_to(context, x, y);
            offset += extents[i].height + m_current_button.text_size / 2;
            cairo_show_text(context, lines[i].c_str());
        }

        cairo_destroy(context);
        cairo_surface_destroy(surface);

        m_is_valid = true;
    }

    std::tuple<int, std::vector<std::pair<std::string, cairo_text_extents_t>>> split(std::string text,
                                                                                     cairo_t *cairo_context) {
        std::list<std::pair<std::string, cairo_text_extents_t>> extents;
        size_t position;
        while (true) {
            position = text.find(" ");
            auto chunk = text.substr(0, position);
            text.erase(0, position + 1);
            cairo_text_extents_t word_extents;
            cairo_text_extents(cairo_context, chunk.c_str(), &word_extents);
            extents.emplace_back(chunk, word_extents);
            if (position == std::string::npos) {
                break;
            }
        }
        cairo_text_extents_t space_extents;
        cairo_text_extents(cairo_context, "_", &space_extents);
        return std::make_tuple(space_extents.width, std::vector<std::pair<std::string, cairo_text_extents_t>>(
                                                        std::begin(extents), std::end(extents)));
    }

    std::vector<std::string>
    split_in_lines(const std::tuple<int, std::vector<std::pair<std::string, cairo_text_extents_t>>> &chunks,
                   int width) {
        std::vector<std::string> result;
        result.reserve(std::get<1>(chunks).size());
        std::string line;
        int counter = 0;
        int line_width = 0;
        int space_width = std::get<0>(chunks);
        for (const auto &chunk : std::get<1>(chunks)) {
            line_width += chunk.second.width;
            if (line_width + counter * space_width > width) {
                result.push_back(line);
                line = "";
                counter = 0;
                line_width = chunk.second.width;
            }
            line += counter ? (" " + chunk.first) : chunk.first;
            ++counter;
        }
        if (!line.empty()) {
            result.push_back(line);
        }
        return result;
    }

    bool invalidated() const { return !m_is_valid; }

protected:
    std::tuple<uint32_t, uint32_t> next_margins() const {
        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> side(0, m_configuration.side_margin_random() - 1);
        std::uniform_int_distribution<> bottom(0, m_configuration.bottom_margin_random() - 1);

        return std::make_tuple(side(generator) + m_configuration.side_margin(),
                               bottom(generator) + m_configuration.bottom_margin());
    }

    button_t make_button(uint32_t width, uint32_t height) const {
        auto [side_margin, bottom_margin] = next_margins();
        return {m_configuration.text(),
                m_configuration.text_size(),
                m_configuration.text_color(),
                side_margin,
                height - bottom_margin - m_configuration.height(),
                width - 2 * side_margin,
                m_configuration.height(),
                m_configuration.background_color(),
                m_configuration.corner_radius()};
    }

    button_t interpolate(const button_t &start, const button_t &end, float fraction) {
        return {start.text,
                start.text_size,
                start.text_color,
                start.x + static_cast<int32_t>((end.x - start.x) * fraction),
                start.y + static_cast<int32_t>((end.y - start.y) * fraction),
                start.width + static_cast<int32_t>((end.width - start.width) * fraction),
                start.height,
                start.background_color,
                start.corner_radius};
    }

private:
    button_configuration_t m_configuration;
    button_t m_button, m_current_button, m_next_button;
    bool m_initialized = false;
    bool m_animation = false;
    std::chrono::steady_clock::time_point m_animation_start_time;
    std::chrono::steady_clock::time_point m_animation_end_time;
    std::chrono::steady_clock::time_point m_next_frame;
    bool m_is_valid = false;

    std::tuple<int, std::vector<std::pair<std::string, cairo_text_extents_t>>> m_chunks;
    std::map<std::string, cairo_text_extents_t> m_extents_cache;
};
