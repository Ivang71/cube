#include "log.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

namespace cube::log {

namespace {

struct State {
    Config cfg{};
    std::FILE* file{};
    std::mutex m;
    std::vector<Entry> entries;
};

State& s() {
    static State st;
    return st;
}

const char* level_str(Level l) {
    switch (l) {
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

unsigned char level_bit(Level l) {
    switch (l) {
        case Level::Info: return 1u << 0;
        case Level::Warn: return 1u << 1;
        case Level::Error: return 1u << 2;
    }
    return 1u << 0;
}

void localtime_safe(std::time_t t, std::tm& out) {
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
}

std::string timestamp_now() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t tt = clock::to_time_t(now);
    std::tm tm{};
    localtime_safe(tt, tm);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
    return buf;
}

void write_sinks(const std::string& line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);

    auto& st = s();
    if (!st.file) return;
    std::fwrite(line.data(), 1, line.size(), st.file);
    std::fwrite("\n", 1, 1, st.file);
    std::fflush(st.file);
}

}

void init(const Config& cfg) {
    auto& st = s();
    std::scoped_lock lk(st.m);
    st.cfg = cfg;
    st.entries.reserve(cfg.max_entries);
    if (st.file) {
        std::fclose(st.file);
        st.file = nullptr;
    }
    if (!st.cfg.file_path.empty()) {
        st.file = std::fopen(st.cfg.file_path.c_str(), "a");
        if (!st.file) write_sinks(std::string("WARN [Log] Failed to open log file: ") + st.cfg.file_path);
    }
}

void shutdown() {
    auto& st = s();
    std::scoped_lock lk(st.m);
    if (st.file) {
        std::fclose(st.file);
        st.file = nullptr;
    }
}

void set_level_mask(unsigned char mask) {
    auto& st = s();
    std::scoped_lock lk(st.m);
    st.cfg.level_mask = mask;
}

unsigned char level_mask() {
    auto& st = s();
    std::scoped_lock lk(st.m);
    return st.cfg.level_mask;
}

void log_line(Level level, std::string_view category, std::source_location loc, std::string msg) {
    auto& st = s();
    const unsigned char bit = level_bit(level);

    std::scoped_lock lk(st.m);
    if ((st.cfg.level_mask & bit) == 0) return;

    std::string ts = timestamp_now();

    char loc_buf[512];
    std::snprintf(loc_buf, sizeof(loc_buf), "%s:%u", loc.file_name(), loc.line());

    std::string line;
    line.reserve(ts.size() + msg.size() + category.size() + 64);
    line += ts;
    line += " [";
    line += level_str(level);
    line += "] [";
    line += category;
    line += "] ";
    line += loc_buf;
    line += " ";
    line += msg;

    st.entries.push_back(Entry{level, std::string(category), line});
    if (st.entries.size() > st.cfg.max_entries) {
        size_t drop = st.entries.size() - st.cfg.max_entries;
        st.entries.erase(st.entries.begin(), st.entries.begin() + static_cast<std::ptrdiff_t>(drop));
    }

    write_sinks(line);
}

std::vector<Entry> snapshot() {
    auto& st = s();
    std::scoped_lock lk(st.m);
    return st.entries;
}

void clear() {
    auto& st = s();
    std::scoped_lock lk(st.m);
    st.entries.clear();
}

}


