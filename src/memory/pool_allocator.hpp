#pragma once

#include "allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace cube::mem {

class PoolAllocator final : public IAllocator {
public:
    PoolAllocator() = default;
    PoolAllocator(std::size_t block_size, std::size_t block_count) { init(block_size, block_count); }

    bool init(std::size_t block_size, std::size_t block_count) {
        if (block_size < sizeof(void*) || block_count == 0) return false;
        block_size_ = align_up(block_size, alignof(std::max_align_t));
        block_count_ = block_count;
        backing_.assign(block_size_ * block_count_, std::byte{0});
        free_ = nullptr;
        for (std::size_t i = 0; i < block_count_; ++i) {
            void* b = backing_.data() + i * block_size_;
            *reinterpret_cast<void**>(b) = free_;
            free_ = b;
        }
        in_use_ = 0;
        stats_ = {};
        return true;
    }

    void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) override {
        (void)align;
        if (!free_ || size == 0 || size > block_size_) return nullptr;
        void* b = free_;
        free_ = *reinterpret_cast<void**>(b);
        in_use_++;
        stats_.alloc_count++;
        stats_.bytes_in_use = in_use_ * block_size_;
        stats_.peak_bytes_in_use = (std::max)(stats_.peak_bytes_in_use, stats_.bytes_in_use);
        return b;
    }

    void free(void* p) override {
        if (!p) return;
        std::byte* bp = static_cast<std::byte*>(p);
        if (bp < backing_.data() || bp >= backing_.data() + backing_.size()) return;
        const std::size_t off = static_cast<std::size_t>(bp - backing_.data());
        if (off % block_size_ != 0) return;
        *reinterpret_cast<void**>(p) = free_;
        free_ = p;
        if (in_use_ > 0) in_use_--;
        stats_.free_count++;
        stats_.bytes_in_use = in_use_ * block_size_;
    }

    void reset() override {
        free_ = nullptr;
        for (std::size_t i = 0; i < block_count_; ++i) {
            void* b = backing_.data() + i * block_size_;
            *reinterpret_cast<void**>(b) = free_;
            free_ = b;
        }
        in_use_ = 0;
        stats_.free_count++;
        stats_.bytes_in_use = 0;
    }

    AllocStats stats() const override { return stats_; }

    std::size_t block_size() const { return block_size_; }
    std::size_t block_count() const { return block_count_; }
    std::size_t in_use() const { return in_use_; }

private:
    std::size_t block_size_{};
    std::size_t block_count_{};
    std::size_t in_use_{};
    std::vector<std::byte> backing_;
    void* free_{};
    AllocStats stats_{};
};

} // namespace cube::mem

