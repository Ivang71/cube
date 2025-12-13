#include "leak.hpp"

#include "core/log.hpp"

#include <vector>
#include <mutex>

namespace cube::mem {

struct LeakCheck {
    const char* name{};
    void* ctx{};
    LeakBytesFn fn{};
};

static std::mutex g_mu;
static std::vector<LeakCheck> g_checks;

void register_leak_check(const char* name, void* ctx, LeakBytesFn fn) {
    if (!name || !ctx || !fn) return;
    std::scoped_lock lk(g_mu);
    g_checks.push_back(LeakCheck{name, ctx, fn});
}

void report_leaks() {
    std::scoped_lock lk(g_mu);
    for (const auto& c : g_checks) {
        const std::size_t b = c.fn ? c.fn(c.ctx) : 0;
        if (b) LOG_ERROR("Memory", "Leak: %s (%zu bytes)", c.name, b);
    }
}

} // namespace cube::mem

