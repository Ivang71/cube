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
#include "render/imgui_layer.hpp"

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

    bool record_command(VkCommandBuffer cmd, uint32_t imageIndex);

    // Input handling
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    void update_camera(float delta_time);

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

    VkDescriptorSetLayout descriptor_set_layout{};
    VkDescriptorPool descriptor_pool{};
    VkDescriptorSet descriptor_set{};

    ShaderManager::ShaderModule* vert_shader{};
    ShaderManager::ShaderModule* frag_shader{};

    // ImGui
    ImGuiLayer imgui_layer;
    FramebufferContext imgui_framebuffers;

    // Camera state
    struct Camera {
        glm::vec3 position{0.0f, 0.0f, 2.0f};
        glm::vec3 velocity{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{-1.57079632679f}; // Look down at origin (-π/2)
        float fov{glm::radians(45.0f)};
        float near_plane{1.0f};
        float far_plane{0.0f};
        bool mouse_captured{false};
    } camera;

    // Input state
    struct InputState {
        bool w_pressed{false};
        bool a_pressed{false};
        bool s_pressed{false};
        bool d_pressed{false};
        double last_mouse_x{0.0};
        double last_mouse_y{0.0};
        bool mouse_initialized{false};
    } input;

    // VMA
    VmaAllocator allocator{};
    VkBuffer vertexBuffer{};
    VmaAllocation vertexBufferAllocation{};
    VkBuffer indexBuffer{};
    VmaAllocation indexBufferAllocation{};
    VkBuffer uniformBuffer{};
    VmaAllocation uniformBufferAllocation{};
    void* uniformBufferMapped{};

};

