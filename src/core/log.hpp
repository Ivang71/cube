#pragma once

#include <source_location>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace cube::log {

enum class Level : unsigned char { Info, Warn, Error };

struct Entry {
    Level level;
    std::string category;
    std::string text;
};

struct Config {
    std::string file_path{"cube.log"};
    size_t max_entries{5000};
    unsigned char level_mask{0xFF};
};

void init(const Config& cfg = {});
void shutdown();

void set_level_mask(unsigned char mask);
unsigned char level_mask();

void log_line(Level level, std::string_view category, std::source_location loc, std::string msg);
std::vector<Entry> snapshot();
void clear();

template <class... Args>
inline void log(Level level, std::string_view category, std::source_location loc, const char* fmt, Args&&... args) {
    char stack_buf[1024];
    int n = std::snprintf(stack_buf, sizeof(stack_buf), fmt, static_cast<Args&&>(args)...);
    if (n < 0) {
        log_line(level, category, loc, std::string(fmt));
        return;
    }
    if (static_cast<size_t>(n) < sizeof(stack_buf)) {
        log_line(level, category, loc, std::string(stack_buf, stack_buf + n));
        return;
    }
    std::string out;
    out.resize(static_cast<size_t>(n));
    std::snprintf(out.data(), out.size() + 1, fmt, static_cast<Args&&>(args)...);
    log_line(level, category, loc, static_cast<std::string&&>(out));
}

}

#define LOG_INFO(category, ...) ::cube::log::log(::cube::log::Level::Info, (category), std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(category, ...) ::cube::log::log(::cube::log::Level::Warn, (category), std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(category, ...) ::cube::log::log(::cube::log::Level::Error, (category), std::source_location::current(), __VA_ARGS__)


