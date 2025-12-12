#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>

#include "render/types.hpp"
#include "render/vk_instance.hpp"
#include "render/vk_device.hpp"
#include "render/swapchain.hpp"
#include "render/frame.hpp"
#include "render/shader.hpp"
#include "render/render_pass.hpp"
#include "render/pipeline.hpp"

class App {
public:
    int run();

private:
    bool init_window();
    bool init_vulkan();
    bool create_swapchain();
    bool recreate_swapchain();
    void main_loop();
    void cleanup();

    bool record_command(VkCommandBuffer cmd, uint32_t imageIndex, VkImage image);

    GLFWwindow* window{};
    VkInstanceContext instance;
    VkDeviceContext device;
    SwapchainContext swapchain;
    FrameManager frames;
    VkSurfaceKHR surface{};
    bool framebuffer_resized = false;

    ShaderManager shaders;
    RenderPassContext render_pass;
    FramebufferContext framebuffers;
    GraphicsPipelineContext pipeline;

    ShaderManager::ShaderModule* vert_shader{};
    ShaderManager::ShaderModule* frag_shader{};

    // VMA
    VmaAllocator allocator{};
    VkBuffer vertexBuffer{};
    VmaAllocation vertexBufferAllocation{};
};

