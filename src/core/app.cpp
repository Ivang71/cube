#include "app.hpp"

#include <cstdio>
#include <cmath>

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
    swapchain.destroy(device.handle());
    return create_swapchain();
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
    swapchain.destroy(device.handle());
    frames.destroy(device.handle());
    device.destroy();
    if (surface) vkDestroySurfaceKHR(instance.handle(), surface, nullptr);
    instance.destroy();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool App::record_command(VkCommandBuffer cmd, uint32_t, VkImage image) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return false;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    float t = static_cast<float>(glfwGetTime());
    VkClearColorValue color{};
    color.float32[0] = 0.5f + 0.5f * std::sin(t * 0.7f);
    color.float32[1] = 0.5f + 0.5f * std::sin(t * 1.1f + 1.0f);
    color.float32[2] = 0.5f + 0.5f * std::sin(t * 1.7f + 2.0f);
    color.float32[3] = 1.0f;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    return vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

