#pragma once

#include "allocator.hpp"

#include <cstddef>
#include <cstdint>

namespace cube::mem {

class LinearAllocator final : public IAllocator {
public:
    LinearAllocator() = default;
    LinearAllocator(void* memory, std::size_t size) { reset(memory, size); }

    void reset(void* memory, std::size_t size) {
        base_ = static_cast<std::byte*>(memory);
        size_ = size;
        offset_ = 0;
        stats_ = {};
    }

    void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) override {
        if (!base_ || size == 0) return nullptr;
        const std::size_t aligned = align_up(offset_, align);
        const std::size_t end = aligned + size;
        if (end > size_) return nullptr;
        offset_ = end;
        stats_.alloc_count++;
        stats_.bytes_in_use = offset_;
        stats_.peak_bytes_in_use = (std::max)(stats_.peak_bytes_in_use, stats_.bytes_in_use);
        return base_ + aligned;
    }

    void free(void*) override {}

    void reset() override {
        offset_ = 0;
        stats_.free_count++;
        stats_.bytes_in_use = 0;
    }

    AllocStats stats() const override { return stats_; }

    std::size_t capacity() const { return size_; }
    std::size_t used() const { return offset_; }

private:
    std::byte* base_{};
    std::size_t size_{};
    std::size_t offset_{};
    AllocStats stats_{};
};

} // namespace cube::mem

