#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct GraphicsPipelineContext {
    VkPipeline handle{};
    VkPipelineLayout layout{};

    bool create(VkDevice device, VkRenderPass render_pass, VkShaderModule vert_shader, VkShaderModule frag_shader, VkExtent2D extent, VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE);
    bool recreate(VkDevice device, VkRenderPass render_pass, VkShaderModule vert_shader, VkShaderModule frag_shader, VkExtent2D extent, VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE);
    void destroy(VkDevice device);

private:
    VkPipelineCache cache{};
};
