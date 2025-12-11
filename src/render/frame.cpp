#include "frame.hpp"

bool FrameManager::create(VkDevice device, uint32_t queue_family, uint32_t count) {
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.queueFamilyIndex = queue_family;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &pi, nullptr, &pool) != VK_SUCCESS) return false;

    frames.resize(count);
    std::vector<VkCommandBuffer> cmds(count);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = count;
    if (vkAllocateCommandBuffers(device, &ai, cmds.data()) != VK_SUCCESS) return false;
    for (uint32_t i = 0; i < count; ++i) frames[i].cmd = cmds[i];

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (auto& f : frames) {
        if (vkCreateSemaphore(device, &si, nullptr, &f.image_available) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(device, &si, nullptr, &f.render_finished) != VK_SUCCESS) return false;
        if (vkCreateFence(device, &fi, nullptr, &f.in_flight) != VK_SUCCESS) return false;
    }
    return true;
}

void FrameManager::destroy(VkDevice device) {
    for (auto& f : frames) {
        if (f.in_flight) vkDestroyFence(device, f.in_flight, nullptr);
        if (f.image_available) vkDestroySemaphore(device, f.image_available, nullptr);
        if (f.render_finished) vkDestroySemaphore(device, f.render_finished, nullptr);
    }
    frames.clear();
    if (pool) vkDestroyCommandPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
    current_index = 0;
}

