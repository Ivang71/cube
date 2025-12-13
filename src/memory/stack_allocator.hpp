#pragma once

#include "allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace cube::mem {

class StackAllocator final : public IAllocator {
public:
    using Marker = std::size_t;

    StackAllocator() = default;
    StackAllocator(void* memory, std::size_t size) { reset(memory, size); }

    void reset(void* memory, std::size_t size) {
        base_ = static_cast<std::byte*>(memory);
        size_ = size;
        offset_ = 0;
        stats_ = {};
    }

    Marker mark() const { return offset_; }

    void pop(Marker m) {
        if (m > offset_) return;
        offset_ = m;
        stats_.free_count++;
        stats_.bytes_in_use = offset_;
    }

    void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) override {
        if (!base_ || size == 0) return nullptr;
        const std::size_t a = std::max<std::size_t>(align, alignof(Header));
        const std::size_t user = align_up(offset_ + sizeof(Header), a);
        const std::size_t header = user - sizeof(Header);
        const std::size_t end = user + size;
        if (end > size_) return nullptr;
        *reinterpret_cast<Header*>(base_ + header) = Header{offset_};
        offset_ = end;
        stats_.alloc_count++;
        stats_.bytes_in_use = offset_;
        stats_.peak_bytes_in_use = (std::max)(stats_.peak_bytes_in_use, stats_.bytes_in_use);
        return base_ + user;
    }

    void free(void* p) override {
        if (!p) return;
        std::byte* bp = static_cast<std::byte*>(p);
        if (bp < base_ + sizeof(Header) || bp > base_ + size_) return;
        const std::size_t user = static_cast<std::size_t>(bp - base_);
        if (user < sizeof(Header)) return;
        auto* h = reinterpret_cast<const Header*>(base_ + (user - sizeof(Header)));
        if (h->prev > offset_) return;
        offset_ = h->prev;
        stats_.free_count++;
        stats_.bytes_in_use = offset_;
    }

    void reset() override {
        offset_ = 0;
        stats_.free_count++;
        stats_.bytes_in_use = 0;
    }

    AllocStats stats() const override { return stats_; }

    std::size_t capacity() const { return size_; }
    std::size_t used() const { return offset_; }

private:
    struct Header { std::size_t prev; };

    std::byte* base_{};
    std::size_t size_{};
    std::size_t offset_{};
    AllocStats stats_{};
};

} // namespace cube::mem

