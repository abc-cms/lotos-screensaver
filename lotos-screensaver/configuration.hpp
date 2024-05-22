#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <list>
#include <regex>
#include <string>

#include <json/json.h>
#include <parameters.hpp>
#include <spdlog/spdlog.h>

enum class media_type_e : uint8_t { image, video };

class media_configuration_t {
public:
    bool operator==(const media_configuration_t &) const = default;
    bool operator!=(const media_configuration_t &) const = default;

    media_configuration_t(media_type_e media_type, const std::filesystem::path &path, float duration = 0.0)
        : m_media_type(media_type), m_path(path), m_duration(duration) {}

    media_type_e media_type() const { return m_media_type; }

    const std::filesystem::path &path() const { return m_path; }

    float duration() const { return m_duration; }

private:
    media_type_e m_media_type;
    std::filesystem::path m_path;
    float m_duration;
};

class button_configuration_t {
public:
    bool operator==(const button_configuration_t &) const = default;
    bool operator!=(const button_configuration_t &) const = default;

    const std::string &text() const { return m_text; }

    uint32_t text_color() const { return m_text_color; }

    uint32_t text_size() const { return m_text_size; }

    uint32_t background_color() const { return m_background_color; }

    uint32_t height() const { return m_height; }

    uint32_t corner_radius() const { return m_corner_radius; }

    uint32_t side_margin() const { return m_side_margin; }

    uint32_t side_margin_random() const { return m_side_margin_random; }

    uint32_t bottom_margin() const { return m_bottom_margin; }

    uint32_t bottom_margin_random() const { return m_bottom_margin_random; }

    float switch_duration() const { return m_switch_duration; }

    uint32_t animation_steps() const { return m_animation_steps; }

    float animation_duration() const { return m_animation_duration; }

private:
    std::string m_text;
    uint32_t m_text_color;
    uint32_t m_text_size;
    uint32_t m_background_color;
    uint32_t m_height;
    uint32_t m_corner_radius;
    uint32_t m_side_margin;
    uint32_t m_side_margin_random;
    uint32_t m_bottom_margin;
    uint32_t m_bottom_margin_random;
    float m_animation_duration;
    uint32_t m_animation_steps;
    float m_switch_duration;

    friend class configuration_t;
};

struct int_time_t {
    bool operator==(const int_time_t &) const = default;
    bool operator!=(const int_time_t &) const = default;

    uint32_t hours = 0;
    uint32_t minutes = 0;
};

struct int_interval_t {
    bool operator==(const int_interval_t &) const = default;
    bool operator!=(const int_interval_t &) const = default;

    int_time_t start;
    int_time_t end;
};

class configuration_t {
public:
    bool operator==(const configuration_t &) const = default;
    bool operator!=(const configuration_t &) const = default;

    int timeout() const { return m_timeout; }

    const auto &button() const { return m_button; }

    const auto &media() const { return m_media; }

    const auto &activity_frames() const { return m_activity_frames; }

    static configuration_t load(const std::filesystem::path &path) {
        configuration_t configuration;

        //auto log = spdlog::get(log_name);
        //log->info("Loading configuration from {}", static_cast<std::string>(path));

        Json::Value root;
        std::ifstream file(path);

        if (file.bad()) {
            //log->error("Unable to open configuration file: {}", static_cast<std::string>(path));
            throw std::runtime_error("Read configuration error");
        }

        JSONCPP_STRING errors;
        Json::CharReaderBuilder builder;

        if (!parseFromStream(builder, file, &root, &errors)) {
            //log->error("Unable to load configuration from file {} due to errors: {}", static_cast<std::string>(path),
            //           errors);
            throw std::runtime_error("Read configuration error");
        }

        // Read list of media.
        std::list<media_configuration_t> media;
        auto media_list = root["media_files"];
        for (auto media_node : media_list) {
            media_type_e media_type
                = (std::string(media_node["type"].asString()) == "image") ? media_type_e::image : media_type_e::video;
            std::filesystem::path path(media_node["path"].asString());
            float duration = (media_type == media_type_e::image) ? media_node["time"].asFloat() : 0.0;
            media.emplace_back(media_type, path, duration);
        }

        configuration.m_media = media;

        // Load button configuration.
        auto button = root["button"];
        button_configuration_t &button_configuration = configuration.m_button;
        button_configuration.m_text = button["text"].asString();
        button_configuration.m_text_color = std::stoul(button["text_color"].asString(), nullptr, 16);
        button_configuration.m_text_size = button["text_size"].asInt();
        button_configuration.m_background_color = std::stoul(button["background_color"].asString(), nullptr, 16);
        button_configuration.m_height = button["height"].asInt();
        button_configuration.m_corner_radius = button["corner_radius"].asInt();
        button_configuration.m_side_margin = button["side_margin"].asInt();
        button_configuration.m_side_margin_random = button["side_margin_random"].asInt();
        button_configuration.m_bottom_margin = button["bottom_margin"].asInt();
        button_configuration.m_bottom_margin_random = button["bottom_margin_random"].asInt();
        button_configuration.m_animation_duration = button["animation_duration"].asFloat();
        button_configuration.m_animation_steps = button["animation_steps"].asInt();
        button_configuration.m_switch_duration = button["switch_duration"].asFloat();

        // Screensaver configuration.
        int_interval_t interval;
        std::smatch matches;
        std::regex time("([0-9]|0[0-9]|1[0-9]|2[0-3]):([0-5][0-9])");
        auto screensaver_settings = root["screensaver_settings"];
        configuration.m_timeout = screensaver_settings["inactivity_timeout"].asInt();
        std::string start_time = screensaver_settings["start_time"].asString();
        std::regex_search(start_time, matches, time);
        interval.start.hours = std::atoi(matches.str(1).c_str());
        interval.start.minutes = std::atoi(matches.str(2).c_str());
        std::string end_time = screensaver_settings["end_time"].asString();
        std::regex_search(end_time, matches, time);
        interval.end.hours = std::atoi(matches.str(1).c_str());
        interval.end.minutes = std::atoi(matches.str(2).c_str());

        configuration.m_activity_frames.push_back(interval);

        //log->info("Configuration loaded");

        return configuration;
    }

private:
    int m_timeout;
    button_configuration_t m_button;
    std::list<media_configuration_t> m_media;
    std::list<int_interval_t> m_activity_frames;
};
