#pragma once

#include <vector>
#include <array>
#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>

#include "math/math.hpp"
#include "render/gpu_memory.hpp"

class Console;
namespace cube::voxel { class BlockRegistry; class ChunkManager; }

struct DebugData {
    float fps;
    float frame_time_ms;
    glm::vec3 camera_position;
    cube::math::UniversalCoord render_origin;
    float distance_from_origin_m;
    size_t ram_used;
    size_t ram_total;
    size_t vram_used;
    size_t vram_total;
    cube::render::VmaTotals vma_totals;
    std::array<std::uint64_t, static_cast<std::size_t>(cube::render::GpuBudgetCategory::Count)> gpu_category_used;
    std::size_t frame_arena_used;
    std::size_t frame_arena_capacity;
    std::size_t frame_arena_peak;
    std::uint64_t staging_used;
    std::uint64_t staging_capacity;
    float cpu_usage;
    float gpu_usage;
    std::uint32_t job_worker_count;
    std::uint32_t job_pending_high;
    std::uint32_t job_pending_normal;
    std::uint32_t job_pending_low;
    std::uint32_t job_stall_warnings;
    std::array<float, 64> job_worker_utilization;
    bool show_overlay;
    bool show_log_viewer;
    bool show_voxel_debug;
    const cube::voxel::BlockRegistry* block_registry;
    const cube::voxel::ChunkManager* chunk_manager;
};

class ImGuiLayer {
public:
    bool init(VkDevice device, VkPhysicalDevice physical_device, VkInstance instance,
              VkQueue graphics_queue, uint32_t graphics_queue_family,
              VkFormat swapchain_format, VkExtent2D extent, GLFWwindow* window,
              uint32_t image_count);

    void shutdown(VkDevice device);

    bool create_render_pass(VkDevice device, VkFormat swapchain_format);
    bool create_framebuffers(VkDevice device, const std::vector<VkImageView>& image_views, VkExtent2D extent);
    void destroy_framebuffers(VkDevice device);

    void new_frame();
    void render(VkCommandBuffer cmd, uint32_t image_index, VkExtent2D extent, const DebugData& debug_data, class Console* console = nullptr, bool* show_console = nullptr, bool show_chat_messages = false);

    bool recreate_swapchain(VkDevice device, const std::vector<VkImageView>& image_views, VkExtent2D extent);

    VkRenderPass render_pass() const { return render_pass_; }
    VkDescriptorPool descriptor_pool() const { return descriptor_pool_; }

private:
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_;
    VkDescriptorPool descriptor_pool_{};
    bool initialized_{false};
};
