#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct RenderPassContext {
    VkRenderPass handle{};
    VkFormat depth_format{};

    bool create(VkDevice device, VkPhysicalDevice physical_device, VkFormat color_format);
    void destroy(VkDevice device);

private:
    VkFormat find_depth_format(VkPhysicalDevice physical_device);
};

struct FramebufferContext {
    std::vector<VkFramebuffer> framebuffers;
    VkImage depth_image{};
    VkDeviceMemory depth_memory{};
    VkImageView depth_view{};

    bool create(VkDevice device, VkPhysicalDevice physical_device, VkRenderPass render_pass,
                VkFormat depth_format, const std::vector<VkImageView>& color_views, VkExtent2D extent);
    void destroy(VkDevice device);
};
