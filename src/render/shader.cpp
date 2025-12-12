#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>

bool ShaderManager::ShaderModule::create(VkDevice device, const std::filesystem::path& source, const std::filesystem::path& spirv) {
    source_path = source;
    spirv_path = spirv;

    // Ensure SPIR-V is up to date
    if (!ShaderManager::compile_glsl_to_spirv(source_path, spirv_path)) {
        return false;
    }

    auto spirv_data = ShaderManager::load_spirv(spirv_path);
    if (spirv_data.empty()) return false;

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv_data.size() * sizeof(uint32_t);
    ci.pCode = spirv_data.data();

    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module for " << spirv_path << std::endl;
        return false;
    }

    source_last_modified = std::filesystem::last_write_time(source_path);
    return true;
}

void ShaderManager::ShaderModule::destroy(VkDevice device) {
    if (module) vkDestroyShaderModule(device, module, nullptr);
    module = VK_NULL_HANDLE;
}

bool ShaderManager::ShaderModule::needs_reload() const {
    return std::filesystem::last_write_time(source_path) > source_last_modified;
}

bool ShaderManager::ShaderModule::reload(VkDevice device) {
    if (!needs_reload()) return true;

    // Recompile GLSL to SPIR-V
    if (!ShaderManager::compile_glsl_to_spirv(source_path, spirv_path)) {
        std::cerr << "Failed to recompile shader: " << source_path << std::endl;
        return false;
    }

    // Destroy old module
    destroy(device);

    // Create new module
    auto spirv_data = ShaderManager::load_spirv(spirv_path);
    if (spirv_data.empty()) return false;

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv_data.size() * sizeof(uint32_t);
    ci.pCode = spirv_data.data();

    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module for " << spirv_path << std::endl;
        return false;
    }

    source_last_modified = std::filesystem::last_write_time(source_path);
    return true;
}

bool ShaderManager::create(VkDevice device) {
    return true; // No initialization needed
}

void ShaderManager::destroy(VkDevice device) {
    vert_shader.destroy(device);
    frag_shader.destroy(device);
}

ShaderManager::ShaderModule* ShaderManager::load_vertex(VkDevice device, const std::filesystem::path& source_path, const std::filesystem::path& spirv_path) {
    if (!vert_shader.create(device, source_path, spirv_path)) return nullptr;
    return &vert_shader;
}

ShaderManager::ShaderModule* ShaderManager::load_fragment(VkDevice device, const std::filesystem::path& source_path, const std::filesystem::path& spirv_path) {
    if (!frag_shader.create(device, source_path, spirv_path)) return nullptr;
    return &frag_shader;
}

bool ShaderManager::hot_reload(VkDevice device) {
    bool reloaded = false;
    if (vert_shader.needs_reload()) {
        if (vert_shader.reload(device)) {
            reloaded = true;
        }
    }
    if (frag_shader.needs_reload()) {
        if (frag_shader.reload(device)) {
            reloaded = true;
        }
    }
    return reloaded;
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

bool ShaderManager::compile_glsl_to_spirv(const std::filesystem::path& source_path, const std::filesystem::path& spirv_path) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(spirv_path.parent_path());

    // Compile using glslangValidator
    std::string command = "glslangValidator -V \"" + source_path.string() + "\" -o \"" + spirv_path.string() + "\"";
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Failed to compile shader: " << source_path << std::endl;
        return false;
    }

    return true;
}