#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "render/types.hpp"

struct SwapchainContext {
    VkSwapchainKHR handle{};
    VkFormat format{};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;

    bool create(VkDevice device, VkPhysicalDevice phys, VkSurfaceKHR surface, const QueueFamilies& fam, GLFWwindow* window);
    void destroy(VkDevice device);
};

