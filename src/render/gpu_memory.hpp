#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace cube::render {

enum class GpuBudgetCategory : std::uint8_t {
    Staging,
    Uniform,
    Vertex,
    Index,
    ImGui,
    Other,
    Count
};

struct VmaTotals {
    std::uint64_t allocation_bytes{};
    std::uint32_t allocation_count{};
    std::uint64_t block_bytes{};
    std::uint32_t block_count{};
};

class GpuMemoryTracker {
public:
    void init(VkPhysicalDevice physical_device);
    void update(VmaAllocator allocator);

    bool can_allocate_vram(std::uint64_t bytes) const;
    void note_vram_attempt(std::uint64_t bytes) const;

    void on_alloc(GpuBudgetCategory cat, VmaAllocator allocator, VmaAllocation alloc, std::uint64_t size_bytes);
    void on_free(VmaAllocation alloc);

    std::uint64_t vram_used() const { return vram_used_; }
    std::uint64_t vram_budget() const { return vram_budget_; }
    VmaTotals vma_totals() const { return totals_; }

    struct CategoryView { const char* name; std::uint64_t used; };
    std::array<CategoryView, static_cast<std::size_t>(GpuBudgetCategory::Count)> category_usage() const;

private:
    struct Entry { GpuBudgetCategory cat; std::uint64_t size; bool device_local; };

    VkPhysicalDeviceMemoryProperties mem_props_{};
    mutable std::mutex mu_;
    std::unordered_map<VmaAllocation, Entry> entries_;

    std::array<std::uint64_t, static_cast<std::size_t>(GpuBudgetCategory::Count)> used_device_local_{};
    std::array<std::uint64_t, static_cast<std::size_t>(GpuBudgetCategory::Count)> used_host_{};

    std::uint64_t vram_used_{};
    std::uint64_t vram_budget_{};
    VmaTotals totals_{};
};

const char* gpu_budget_category_name(GpuBudgetCategory c);

} // namespace cube::render

