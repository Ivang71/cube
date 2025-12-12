#pragma once

#include <vector>
#include <string>
#include <filesystem>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class ShaderManager {
public:
    struct ShaderModule {
        VkShaderModule module{};
        std::filesystem::path source_path;
        std::filesystem::file_time_type last_modified;

        bool create(VkDevice device, const std::filesystem::path& path);
        void destroy(VkDevice device);
        bool needs_reload() const;
    };

    bool create(VkDevice device);
    void destroy(VkDevice device);

    VkShaderModule load_vertex(VkDevice device, const std::filesystem::path& path);
    VkShaderModule load_fragment(VkDevice device, const std::filesystem::path& path);

private:
    static std::vector<uint32_t> load_spirv(const std::filesystem::path& path);
};