#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "render/types.hpp"

class VkDeviceContext {
public:
    bool pick(VkInstance instance, VkSurfaceKHR surface);
    bool create(VkInstance instance, VkSurfaceKHR surface, bool enable_validation);
    void destroy();

    VkDevice handle() const { return device; }
    VkQueue graphics() const { return graphics_queue; }
    VkQueue present() const { return present_queue; }
    QueueFamilies queues() const { return families; }
    VkPhysicalDevice physical() const { return physical_device; }

private:
    bool check_device_extensions(VkPhysicalDevice dev) const;
    QueueFamilies find_queue_families(VkPhysicalDevice dev, VkSurfaceKHR surface) const;

    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkQueue graphics_queue{};
    VkQueue present_queue{};
    QueueFamilies families;
};

