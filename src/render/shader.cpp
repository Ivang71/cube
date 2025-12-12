#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>

bool ShaderManager::ShaderModule::create(VkDevice device, const std::filesystem::path& path) {
    source_path = path;
    auto spirv = ShaderManager::load_spirv(path);
    if (spirv.empty()) return false;

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();

    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module for " << path << std::endl;
        return false;
    }

    last_modified = std::filesystem::last_write_time(path);
    return true;
}

void ShaderManager::ShaderModule::destroy(VkDevice device) {
    if (module) vkDestroyShaderModule(device, module, nullptr);
    module = VK_NULL_HANDLE;
}

bool ShaderManager::ShaderModule::needs_reload() const {
    return std::filesystem::last_write_time(source_path) > last_modified;
}

bool ShaderManager::create(VkDevice device) {
    return true; // No initialization needed
}

void ShaderManager::destroy(VkDevice device) {
    // Shader modules are destroyed individually
}

VkShaderModule ShaderManager::load_vertex(VkDevice device, const std::filesystem::path& path) {
    ShaderModule shader;
    if (!shader.create(device, path)) return VK_NULL_HANDLE;
    return shader.module; // Note: caller takes ownership
}

VkShaderModule ShaderManager::load_fragment(VkDevice device, const std::filesystem::path& path) {
    ShaderModule shader;
    if (!shader.create(device, path)) return VK_NULL_HANDLE;
    return shader.module; // Note: caller takes ownership
}

std::vector<uint32_t> ShaderManager::load_spirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open SPIR-V file: " << path << std::endl;
        return {};
    }

    size_t size = file.tellg();
    file.seekg(0);

    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    if (!file) {
        std::cerr << "Failed to read SPIR-V file: " << path << std::endl;
        return {};
    }

    return buffer;
}