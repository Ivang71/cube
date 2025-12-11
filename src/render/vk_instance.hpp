#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class VkInstanceContext {
public:
    bool init(bool request_validation);
    bool create_surface(GLFWwindow* window, VkSurfaceKHR& out_surface) const;
    bool validation_enabled() const { return validation; }
    VkInstance handle() const { return instance; }
    void destroy();

private:
    bool setup_debug();

    bool validation = false;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT messenger{};
};

