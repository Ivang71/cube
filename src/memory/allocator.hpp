#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace cube::mem {

struct AllocStats {
    std::size_t bytes_in_use{};
    std::size_t peak_bytes_in_use{};
    std::uint64_t alloc_count{};
    std::uint64_t free_count{};
};

inline constexpr std::size_t align_up(std::size_t v, std::size_t a) {
    if (a <= 1) return v;
    if ((a & (a - 1)) == 0) return (v + (a - 1)) & ~(a - 1);
    return ((v + a - 1) / a) * a;
}

struct IAllocator {
    virtual ~IAllocator() = default;
    virtual void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) = 0;
    virtual void free(void* p) = 0;
    virtual void reset() {}
    virtual AllocStats stats() const = 0;
};

} // namespace cube::mem

