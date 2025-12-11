#include "vk_device.hpp"

#include <cstdio>
#include <cstring>

bool VkDeviceContext::check_device_extensions(VkPhysicalDevice dev) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, props.data());
    for (const auto& p : props) {
        if (std::strcmp(p.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) return true;
    }
    return false;
}

QueueFamilies VkDeviceContext::find_queue_families(VkPhysicalDevice dev, VkSurfaceKHR surface) const {
    QueueFamilies f;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) f.graphics = i;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) f.present = i;
        if (f.complete()) break;
    }
    return f;
}

bool VkDeviceContext::pick(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) return false;
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    VkPhysicalDevice best = nullptr;
    int best_score = -1;
    for (auto dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        QueueFamilies fam = find_queue_families(dev, surface);
        bool ok = fam.complete() && check_device_extensions(dev);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if (ok && score > best_score) {
            best = dev;
            best_score = score;
            families = fam;
        }
        std::printf("gpu: %s (%u)%s\n",
                    props.deviceName,
                    props.deviceType,
                    ok ? "" : " - unsuitable");
    }
    physical_device = best;
    return best != nullptr;
}

bool VkDeviceContext::create(VkInstance instance, VkSurfaceKHR surface, bool enable_validation) {
    if (!physical_device && !pick(instance, surface)) return false;
    if (!families.complete()) families = find_queue_families(physical_device, surface);
    if (!families.complete()) return false;

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queues;
    std::vector<uint32_t> unique{ *families.graphics };
    if (families.present.value() != families.graphics.value()) unique.push_back(*families.present);
    for (uint32_t idx : unique) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = idx;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queues.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    const char* dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
    ci.pQueueCreateInfos = queues.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = dev_exts;
    if (enable_validation) {
        static const char* layer = "VK_LAYER_KHRONOS_validation";
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layer;
    }

    if (vkCreateDevice(physical_device, &ci, nullptr, &device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(device, *families.graphics, 0, &graphics_queue);
    vkGetDeviceQueue(device, *families.present, 0, &present_queue);
    return true;
}

void VkDeviceContext::destroy() {
    if (device) vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    physical_device = VK_NULL_HANDLE;
    graphics_queue = VK_NULL_HANDLE;
    present_queue = VK_NULL_HANDLE;
    families = {};
}

