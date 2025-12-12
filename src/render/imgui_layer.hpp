#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

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
    void render(VkCommandBuffer cmd, uint32_t image_index, VkExtent2D extent);

    bool recreate_swapchain(VkDevice device, const std::vector<VkImageView>& image_views, VkExtent2D extent);

    VkRenderPass render_pass() const { return render_pass_; }
    VkDescriptorPool descriptor_pool() const { return descriptor_pool_; }

private:
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_;
    VkDescriptorPool descriptor_pool_{};
    bool initialized_{false};
};
