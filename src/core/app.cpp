#include "app.hpp"

#include <cstdio>
#include <iostream>
#include <cmath>
#include <array>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

int App::run() {
    if (!init_window()) return 1;
    if (!init_vulkan()) { cleanup(); return 1; }
    main_loop();
    cleanup();
    return 0;
}

bool App::init_window() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(1280, 720, "cube", nullptr, nullptr);
    if (!window) return false;
    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, [](GLFWwindow* win, int, int) {
        static_cast<App*>(glfwGetWindowUserPointer(win))->framebuffer_resized = true;
    });
    return true;
}

bool App::init_vulkan() {
    if (!instance.init(true)) return false;
    if (!instance.create_surface(window, surface)) return false;
    if (!device.pick(instance.handle(), surface)) return false;
    if (!device.create(instance.handle(), surface, instance.validation_enabled())) return false;
    if (!create_swapchain()) return false;
    if (!frames.create(device.handle(), *device.queues().graphics, 2)) return false;

    // Initialize rendering components
    if (!shaders.create(device.handle())) return false;
    if (!render_pass.create(device.handle(), device.physical(), swapchain.format)) return false;
    if (!framebuffers.create(device.handle(), device.physical(), render_pass.handle, swapchain.views, swapchain.extent)) return false;

    // Load shaders from the build directory (where CMake places them)
    // Detect build configuration from executable path
    std::filesystem::path exe_path;
    #ifdef _WIN32
        char exe_buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_buffer, MAX_PATH);
        exe_path = exe_buffer;
    #else
        exe_path = std::filesystem::read_symlink("/proc/self/exe");
    #endif

    // Extract build configuration from path (e.g., "debug", "release", "profile")
    std::string config = "debug"; // default fallback
    std::string exe_path_str = exe_path.string();

    // Look for build configuration in path (case-insensitive)
    std::vector<std::string> possible_configs = {"debug", "release", "profile", "relwithdebinfo"};
    for (const auto& conf : possible_configs) {
        std::string search_pattern = std::string("\\") + conf + std::string("\\");
        if (exe_path_str.find(search_pattern) != std::string::npos) {
            config = conf;
            break;
        }
        // Also check for case variations
        std::string upper_conf = conf;
        std::transform(upper_conf.begin(), upper_conf.end(), upper_conf.begin(), ::toupper);
        search_pattern = std::string("\\") + upper_conf + std::string("\\");
        if (exe_path_str.find(search_pattern) != std::string::npos) {
            config = conf;
            break;
        }
    }

    // Construct shader paths relative to executable location
    std::filesystem::path shader_dir = exe_path.parent_path().parent_path() / "shaders";
    std::filesystem::path vert_spirv_path = shader_dir / "triangle.vert.spv";
    std::filesystem::path frag_spirv_path = shader_dir / "triangle.frag.spv";

    // Source paths relative to project root
    std::filesystem::path project_root = exe_path.parent_path().parent_path().parent_path().parent_path();
    std::filesystem::path vert_source_path = project_root / "shaders" / "triangle.vert";
    std::filesystem::path frag_source_path = project_root / "shaders" / "triangle.frag";

    vert_shader = shaders.load_vertex(device.handle(), vert_source_path, vert_spirv_path);
    frag_shader = shaders.load_fragment(device.handle(), frag_source_path, frag_spirv_path);
    if (!vert_shader || !frag_shader) return false;

    if (!pipeline.create(device.handle(), render_pass.handle, vert_shader->module, frag_shader->module, swapchain.extent)) return false;

    return true;
}

bool App::create_swapchain() {
    return swapchain.create(device.handle(), device.physical(), surface, device.queues(), window);
}

bool App::recreate_swapchain() {
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device.handle());

    framebuffers.destroy(device.handle());
    swapchain.destroy(device.handle());

    if (!create_swapchain()) return false;
    if (!framebuffers.create(device.handle(), device.physical(), render_pass.handle, swapchain.views, swapchain.extent)) return false;

    return true;
}

void App::main_loop() {
    double fps_timer = glfwGetTime();
    int fps_frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swapchain();
        }
        fps_frames++;
        double now = glfwGetTime();
        if (now - fps_timer >= 1.0) {
            char title[64];
            std::snprintf(title, sizeof(title), "cube %dfps", fps_frames);
            glfwSetWindowTitle(window, title);
            fps_frames = 0;
            fps_timer = now;
        }

        // Check for shader hot reload
        if (shaders.hot_reload(device.handle())) {
            std::cout << "Shaders reloaded, recreating pipeline" << std::endl;
            // Shaders were reloaded, recreate pipeline
            pipeline.recreate(device.handle(), render_pass.handle, vert_shader->module, frag_shader->module, swapchain.extent);
        }

        FrameSync& frame = frames.current();
        vkWaitForFences(device.handle(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX);
        vkResetFences(device.handle(), 1, &frame.in_flight);

        uint32_t imageIndex = 0;
        VkResult acq = vkAcquireNextImageKHR(device.handle(), swapchain.handle, UINT64_MAX, frame.image_available, VK_NULL_HANDLE, &imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(); continue; }
        if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) continue;

        vkResetCommandBuffer(frame.cmd, 0);
        record_command(frame.cmd, imageIndex, swapchain.images[imageIndex]);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.image_available;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &frame.cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &frame.render_finished;

        if (vkQueueSubmit(device.graphics(), 1, &submit, frame.in_flight) != VK_SUCCESS) continue;

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &frame.render_finished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain.handle;
        present.pImageIndices = &imageIndex;
        VkResult pres = vkQueuePresentKHR(device.present(), &present);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swapchain();
        }
        frames.advance();
    }
}

void App::cleanup() {
    if (device.handle()) vkDeviceWaitIdle(device.handle());

    pipeline.destroy(device.handle());
    framebuffers.destroy(device.handle());
    render_pass.destroy(device.handle());
    shaders.destroy(device.handle());

    swapchain.destroy(device.handle());
    frames.destroy(device.handle());
    device.destroy();
    if (surface) vkDestroySurfaceKHR(instance.handle(), surface, nullptr);
    instance.destroy();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool App::record_command(VkCommandBuffer cmd, uint32_t imageIndex, VkImage image) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return false;

    // Begin render pass (this handles layout transitions automatically)
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp_bi{};
    rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_bi.renderPass = render_pass.handle;
    rp_bi.framebuffer = framebuffers.framebuffers[imageIndex];
    rp_bi.renderArea.offset = {0, 0};
    rp_bi.renderArea.extent = swapchain.extent;
    rp_bi.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_bi.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.extent.width);
    viewport.height = static_cast<float>(swapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Draw triangle (vertices are hardcoded in shader)
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // End render pass (this handles layout transitions back to present automatically)
    vkCmdEndRenderPass(cmd);

    return vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

