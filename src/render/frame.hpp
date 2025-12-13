#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct FrameSync {
    VkCommandBuffer cmd{};
    VkSemaphore image_available{};
    VkSemaphore render_finished{};
    VkFence in_flight{};
};

class FrameManager {
public:
    bool create(VkDevice device, uint32_t queue_family, uint32_t count = 2);
    void destroy(VkDevice device);

    FrameSync& current() { return frames[current_index]; }
    std::size_t current_frame_index() const { return current_index; }
    std::size_t frame_count() const { return frames.size(); }
    void advance() { current_index = (current_index + 1) % frames.size(); }
    VkCommandPool command_pool() const { return pool; }

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, uint32_t queue_family);
    void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandBuffer commandBuffer);

private:
    VkCommandPool pool{};
    std::vector<FrameSync> frames;
    size_t current_index = 0;
};

