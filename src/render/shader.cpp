#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

bool ShaderManager::runtime_compile_enabled() {
    const char* v = std::getenv("CUBE_SHADER_COMPILE");
    if (!v) return true;
    return !(v[0] == '0' && v[1] == '\0');
}

bool ShaderManager::ShaderModule::create(VkDevice device, const std::filesystem::path& source, const std::filesystem::path& spirv) {
    source_path = source;
    spirv_path = spirv;

    if (ShaderManager::runtime_compile_enabled()) {
        bool need = false;
        if (!std::filesystem::exists(spirv_path)) need = true;
        else if (std::filesystem::exists(source_path) && std::filesystem::last_write_time(source_path) > std::filesystem::last_write_time(spirv_path)) need = true;
        if (need) {
            if (!ShaderManager::compile_glsl_to_spirv(source_path, spirv_path, 5000)) {
                std::cerr << "Shader compile skipped/failed (using existing SPIR-V if present): " << source_path << std::endl;
            }
        }
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

    if (std::filesystem::exists(source_path)) source_last_modified = std::filesystem::last_write_time(source_path);
    if (std::filesystem::exists(spirv_path)) spirv_last_modified = std::filesystem::last_write_time(spirv_path);
    return true;
}

void ShaderManager::ShaderModule::destroy(VkDevice device) {
    if (module) vkDestroyShaderModule(device, module, nullptr);
    module = VK_NULL_HANDLE;
}

bool ShaderManager::ShaderModule::needs_reload() const {
    if (std::filesystem::exists(spirv_path) && std::filesystem::last_write_time(spirv_path) > spirv_last_modified) return true;
    if (ShaderManager::runtime_compile_enabled() && std::filesystem::exists(source_path) && std::filesystem::last_write_time(source_path) > source_last_modified) return true;
    return false;
}

bool ShaderManager::ShaderModule::reload(VkDevice device) {
    if (!needs_reload()) return true;

    if (ShaderManager::runtime_compile_enabled() && std::filesystem::exists(source_path)) {
        if (!ShaderManager::compile_glsl_to_spirv(source_path, spirv_path, 2000)) {
            std::cerr << "Failed to recompile shader (keeping old module): " << source_path << std::endl;
            return false;
        }
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

    if (std::filesystem::exists(source_path)) source_last_modified = std::filesystem::last_write_time(source_path);
    if (std::filesystem::exists(spirv_path)) spirv_last_modified = std::filesystem::last_write_time(spirv_path);
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

bool ShaderManager::compile_glsl_to_spirv(const std::filesystem::path& source_path, const std::filesystem::path& spirv_path, std::uint32_t timeout_ms) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(spirv_path.parent_path());

#ifdef _WIN32
    std::string cmd = "glslangValidator -V \"" + source_path.string() + "\" -o \"" + spirv_path.string() + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    DWORD w = WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD code = 1;
    if (w == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    int result = (code == 0) ? 0 : 1;
#else
    std::string command = "glslangValidator -V \"" + source_path.string() + "\" -o \"" + spirv_path.string() + "\"";
    int result = std::system(command.c_str());
#endif

    if (result != 0) {
        return false;
    }

    return true;
}