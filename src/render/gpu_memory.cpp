#include "gpu_memory.hpp"

#include "core/log.hpp"

namespace cube::render {

const char* gpu_budget_category_name(GpuBudgetCategory c) {
    switch (c) {
        case GpuBudgetCategory::Staging: return "Staging";
        case GpuBudgetCategory::Uniform: return "Uniform";
        case GpuBudgetCategory::Vertex: return "Vertex";
        case GpuBudgetCategory::Index: return "Index";
        case GpuBudgetCategory::ImGui: return "ImGui";
        case GpuBudgetCategory::Other: return "Other";
        default: return "Unknown";
    }
}

void GpuMemoryTracker::init(VkPhysicalDevice physical_device) {
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props_);
}

void GpuMemoryTracker::update(VmaAllocator allocator) {
    if (!allocator) return;

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
    vmaGetHeapBudgets(allocator, budgets);
    std::uint64_t used = 0;
    std::uint64_t bud = 0;
    for (std::uint32_t i = 0; i < mem_props_.memoryHeapCount; ++i) {
        if (mem_props_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            used += budgets[i].usage;
            bud += budgets[i].budget;
        }
    }
    vram_used_ = used;
    vram_budget_ = bud;

    VmaTotalStatistics ts{};
    vmaCalculateStatistics(allocator, &ts);
    totals_.allocation_bytes = static_cast<std::uint64_t>(ts.total.statistics.allocationBytes);
    totals_.allocation_count = ts.total.statistics.allocationCount;
    totals_.block_bytes = static_cast<std::uint64_t>(ts.total.statistics.blockBytes);
    totals_.block_count = ts.total.statistics.blockCount;
}

bool GpuMemoryTracker::can_allocate_vram(std::uint64_t bytes) const {
    if (!vram_budget_) return true;
    const double next = static_cast<double>(vram_used_ + bytes) / static_cast<double>(vram_budget_);
    return next < 0.95;
}

void GpuMemoryTracker::note_vram_attempt(std::uint64_t bytes) const {
    if (!vram_budget_) return;
    const double next = static_cast<double>(vram_used_ + bytes) / static_cast<double>(vram_budget_);
    if (next >= 0.95) LOG_ERROR("Memory", "VRAM budget exceeded (%.1f%%)", next * 100.0);
    else if (next >= 0.80) LOG_WARN("Memory", "VRAM budget high (%.1f%%)", next * 100.0);
}

void GpuMemoryTracker::on_alloc(GpuBudgetCategory cat, VmaAllocator allocator, VmaAllocation alloc, std::uint64_t size_bytes) {
    if (!alloc) return;
    VkMemoryPropertyFlags flags{};
    vmaGetAllocationMemoryProperties(allocator, alloc, &flags);
    const bool device_local = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

    {
        std::scoped_lock lk(mu_);
        entries_.insert_or_assign(alloc, Entry{cat, size_bytes, device_local});
        const std::size_t idx = static_cast<std::size_t>(cat);
        if (device_local) used_device_local_[idx] += size_bytes;
        else used_host_[idx] += size_bytes;
    }
}

void GpuMemoryTracker::on_free(VmaAllocation alloc) {
    if (!alloc) return;
    std::scoped_lock lk(mu_);
    auto it = entries_.find(alloc);
    if (it == entries_.end()) return;
    const auto e = it->second;
    const std::size_t idx = static_cast<std::size_t>(e.cat);
    if (e.device_local) used_device_local_[idx] -= e.size;
    else used_host_[idx] -= e.size;
    entries_.erase(it);
}

std::array<GpuMemoryTracker::CategoryView, static_cast<std::size_t>(GpuBudgetCategory::Count)> GpuMemoryTracker::category_usage() const {
    std::array<CategoryView, static_cast<std::size_t>(GpuBudgetCategory::Count)> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i].name = gpu_budget_category_name(static_cast<GpuBudgetCategory>(i));
        out[i].used = used_device_local_[i] + used_host_[i];
    }
    return out;
}

} // namespace cube::render

