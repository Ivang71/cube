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
        std::filesystem::path spirv_path;
        std::filesystem::file_time_type source_last_modified;

        bool create(VkDevice device, const std::filesystem::path& source_path, const std::filesystem::path& spirv_path);
        void destroy(VkDevice device);
        bool needs_reload() const;
        bool reload(VkDevice device);
    };

    bool create(VkDevice device);
    void destroy(VkDevice device);

    ShaderModule* load_vertex(VkDevice device, const std::filesystem::path& source_path, const std::filesystem::path& spirv_path);
    ShaderModule* load_fragment(VkDevice device, const std::filesystem::path& source_path, const std::filesystem::path& spirv_path);

    bool hot_reload(VkDevice device);

private:
    ShaderModule vert_shader;
    ShaderModule frag_shader;
    static std::vector<uint32_t> load_spirv(const std::filesystem::path& path);
    static bool compile_glsl_to_spirv(const std::filesystem::path& source_path, const std::filesystem::path& spirv_path);
};