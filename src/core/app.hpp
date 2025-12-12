#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>
#ifdef _WIN32
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#endif

#include "render/types.hpp"
#include "render/vk_instance.hpp"
#include "render/vk_device.hpp"
#include "render/swapchain.hpp"
#include "render/frame.hpp"
#include "render/shader.hpp"
#include "render/render_pass.hpp"
#include "render/pipeline.hpp"
#include "render/imgui_layer.hpp"
#include "console.hpp"

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
    void register_console_commands();

    bool record_command(VkCommandBuffer cmd, uint32_t imageIndex);

    // Input handling
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    void update_camera(float delta_time);
    void update_debug_stats();
    float get_cpu_usage();
    float get_gpu_usage();

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

    // Debug overlay state
    bool show_debug_overlay{false};

    // Console
    Console console;
    bool show_console{false};
    bool prev_show_console{false}; // Track previous console state for mouse capture

    // Debug metrics
    float fps{0.0f};
    float frame_time_ms{0.0f};
    size_t ram_used{0};
    size_t ram_total{0};
    size_t vram_used{0};
    size_t vram_total{0};
    float cpu_usage{0.0f};
    float gpu_usage{0.0f};

    // Debug stats update timer
    double last_debug_stats_update{0.0};
    static constexpr double DEBUG_STATS_UPDATE_INTERVAL = 0.3; // seconds

    // CPU usage tracking
    FILETIME prev_idle_time{};
    FILETIME prev_kernel_time{};
    FILETIME prev_user_time{};
    bool cpu_initialized{false};

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

