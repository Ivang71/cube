#include "app.hpp"
#include "log.hpp"

#include <cstdio>
#include <iostream>
#include <cmath>
#include <array>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <charconv>
#include <imgui.h>
#if defined(TRACY_ENABLE)
  #include <tracy/TracyVulkan.hpp>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#include "math/math.hpp"
#include "memory/leak.hpp"

static std::filesystem::path exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0) return std::filesystem::current_path();
    return std::filesystem::path(std::string(buf, buf + n)).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

int App::run() {
    cube::log::Config log_cfg{};
    log_cfg.file_path = (exe_dir() / "cube.log").string();
    cube::log::init(log_cfg);
    LOG_INFO("Core", "Startup");
    if (!init_window()) { cube::log::shutdown(); return 1; }
    LOG_INFO("Core", "Window initialized");
    if (!init_vulkan()) { cleanup(); return 1; }
    LOG_INFO("Core", "Vulkan initialized");
    main_loop();
    cleanup();
    return 0;
}

bool App::init_window() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(1600, 900, "cube", nullptr, nullptr);
    if (!window) return false;
    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, [](GLFWwindow* win, int, int) {
        static_cast<App*>(glfwGetWindowUserPointer(win))->framebuffer_resized = true;
    });
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    camera.mouse_captured = true;
    glfwFocusWindow(window); // Ensure window has focus for input
    return true;
}

bool App::init_vulkan() {
    CUBE_PROFILE_SCOPE_N("init_vulkan");
    if (!instance.init(false)) return false;
    if (!instance.create_surface(window, surface)) return false;
    if (!device.pick(instance.handle(), surface)) return false;
    if (!device.create(instance.handle(), surface, instance.validation_enabled())) return false;
    if (!create_swapchain()) return false;
    if (!frames.create(device.handle(), *device.queues().graphics, 2)) return false;

    frame_arenas.resize(frames.frame_count());
    for (auto& a : frame_arenas) {
        a.backing.resize(FRAME_ARENA_BYTES);
        a.alloc.reset(a.backing.data(), a.backing.size());
        cube::mem::register_leak_check(
            "FrameArena",
            &a.alloc,
            +[](void* ctx) -> std::size_t { return static_cast<cube::mem::LinearAllocator*>(ctx)->used(); }
        );
    }

#if defined(TRACY_ENABLE)
    {
        VkCommandBuffer cb = frames.beginSingleTimeCommands(device.handle(), *device.queues().graphics);
        tracy_vk_ctx = TracyVkContext(device.physical(), device.handle(), device.graphics(), cb);
        frames.endSingleTimeCommands(device.handle(), device.graphics(), cb);
    }
#endif

    // Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = device.physical();
    allocatorInfo.device = device.handle();
    allocatorInfo.instance = instance.handle();
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        LOG_ERROR("Core", "Failed to create VMA allocator");
        return false;
    }
    gpu_mem.init(device.physical());
    gpu_mem.update(allocator);
    if (!gpu_uploader.init(allocator, 64ull * 1024ull * 1024ull)) return false;
    gpu_mem.on_alloc(cube::render::GpuBudgetCategory::Staging, allocator, gpu_uploader.staging_allocation(), (std::uint64_t)gpu_uploader.staging_capacity());

    VkDescriptorSetLayoutBinding ubo_binding{};
    ubo_binding.binding = 0;
    ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_binding.descriptorCount = 1;
    ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_binding;
    if (vkCreateDescriptorSetLayout(device.handle(), &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) return false;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(device.handle(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout;
    if (vkAllocateDescriptorSets(device.handle(), &alloc_info, &descriptor_set) != VK_SUCCESS) return false;

    VkBufferCreateInfo ubo_info{};
    ubo_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ubo_info.size = sizeof(UniformBufferObject);
    ubo_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ubo_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ubo_alloc{};
    ubo_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    ubo_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    if (vmaCreateBuffer(allocator, &ubo_info, &ubo_alloc, &uniformBuffer, &uniformBufferAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "Failed to create uniform buffer" << std::endl;
        return false;
    }
    gpu_mem.on_alloc(cube::render::GpuBudgetCategory::Uniform, allocator, uniformBufferAllocation, (std::uint64_t)ubo_info.size);
    vmaMapMemory(allocator, uniformBufferAllocation, &uniformBufferMapped);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniformBuffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(device.handle(), 1, &write, 0, nullptr);

    // Initialize rendering components
    if (!shaders.create(device.handle())) return false;
    if (!render_pass.create(device.handle(), device.physical(), swapchain.format)) return false;
    if (!framebuffers.create(device.handle(), device.physical(), render_pass.handle, render_pass.depth_format, swapchain.views, swapchain.extent)) return false;

    // Initialize ImGui
    if (!imgui_layer.init(device.handle(), device.physical(), instance.handle(),
                         device.graphics(), *device.queues().graphics,
                         swapchain.format, swapchain.extent, window, static_cast<uint32_t>(swapchain.images.size()))) {
        return false;
    }

    // Initialize debug stats
    update_debug_stats();
    last_debug_stats_update = glfwGetTime();

    // Create ImGui framebuffers
    if (!imgui_layer.create_framebuffers(device.handle(), swapchain.views, swapchain.extent)) return false;

    // Load shaders from the build directory (where CMake places them)
    // Detect build configuration from executable path
    std::filesystem::path exe_path;
    #ifdef _WIN32
        char exe_buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_buffer, MAX_PATH);
        exe_path = exe_buffer;
    #else
        exe_path = std::filesystem::read_symlink("/proc/self/exe");
    #endif

    // Extract build configuration from path (e.g., "debug", "release", "profile")
    std::string config = "debug"; // default fallback
    std::string exe_path_str = exe_path.string();

    // Look for build configuration in path (case-insensitive)
    std::vector<std::string> possible_configs = {"debug", "release", "profile", "relwithdebinfo"};
    for (const auto& conf : possible_configs) {
        std::string search_pattern = std::string("\\") + conf + std::string("\\");
        if (exe_path_str.find(search_pattern) != std::string::npos) {
            config = conf;
            break;
        }
        // Also check for case variations
        std::string upper_conf = conf;
        std::transform(upper_conf.begin(), upper_conf.end(), upper_conf.begin(), ::toupper);
        search_pattern = std::string("\\") + upper_conf + std::string("\\");
        if (exe_path_str.find(search_pattern) != std::string::npos) {
            config = conf;
            break;
        }
    }

    // Construct shader paths relative to executable location
    std::filesystem::path shader_dir = exe_path.parent_path().parent_path() / "shaders";
    std::filesystem::path vert_spirv_path = shader_dir / "triangle.vert.spv";
    std::filesystem::path frag_spirv_path = shader_dir / "triangle.frag.spv";

    // Source paths relative to project root
    std::filesystem::path project_root = exe_path.parent_path().parent_path().parent_path().parent_path();
    std::filesystem::path vert_source_path = project_root / "shaders" / "triangle.vert";
    std::filesystem::path frag_source_path = project_root / "shaders" / "triangle.frag";

    vert_shader = shaders.load_vertex(device.handle(), vert_source_path, vert_spirv_path);
    frag_shader = shaders.load_fragment(device.handle(), frag_source_path, frag_spirv_path);
    if (!vert_shader || !frag_shader) return false;

    // Create vertex buffer (quad)
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // bottom-left
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},   // bottom-right
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},    // top-right
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}    // top-left
    };

    // Create index buffer data
    const std::vector<uint16_t> indices = {
        0, 1, 2,  // first triangle
        2, 3, 0   // second triangle
    };

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create device-local vertex buffer
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = bufferSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vertexAllocInfo = {};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    gpu_mem.update(allocator);
    gpu_mem.note_vram_attempt((std::uint64_t)bufferSize);
    if (!gpu_mem.can_allocate_vram((std::uint64_t)bufferSize)) return false;
    if (vmaCreateBuffer(allocator, &vertexBufferInfo, &vertexAllocInfo, &vertexBuffer, &vertexBufferAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return false;
    }
    gpu_mem.on_alloc(cube::render::GpuBudgetCategory::Vertex, allocator, vertexBufferAllocation, (std::uint64_t)bufferSize);

    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    // Create device-local index buffer
    VkBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indexBufferSize;
    indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo indexAllocInfo = {};
    indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    gpu_mem.update(allocator);
    gpu_mem.note_vram_attempt((std::uint64_t)indexBufferSize);
    if (!gpu_mem.can_allocate_vram((std::uint64_t)indexBufferSize)) return false;
    if (vmaCreateBuffer(allocator, &indexBufferInfo, &indexAllocInfo, &indexBuffer, &indexBufferAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "Failed to create index buffer" << std::endl;
        return false;
    }
    gpu_mem.on_alloc(cube::render::GpuBudgetCategory::Index, allocator, indexBufferAllocation, (std::uint64_t)indexBufferSize);

    gpu_uploader.begin_frame();
    auto alloc_fn = +[](void* ctx, std::size_t sz, std::size_t al) -> void* { return static_cast<App*>(ctx)->frame_alloc(sz, al); };
    if (!gpu_uploader.enqueue_buffer_upload(this, alloc_fn, vertexBuffer, 0, vertices.data(), bufferSize)) return false;
    if (!gpu_uploader.enqueue_buffer_upload(this, alloc_fn, indexBuffer, 0, indices.data(), indexBufferSize)) return false;
    VkCommandBuffer upload_cb = frames.beginSingleTimeCommands(device.handle(), *device.queues().graphics);
    gpu_uploader.flush(upload_cb);
    frames.endSingleTimeCommands(device.handle(), device.graphics(), upload_cb);

    if (!pipeline.create(device.handle(), render_pass.handle, vert_shader->module, frag_shader->module, swapchain.extent, descriptor_set_layout)) return false;

    // Register console commands
    register_console_commands();

    camera.abs = render_origin + cube::math::UniversalCoord::from_meters(0, 0, 2);
    camera.frac = glm::vec3(0.0f);
    camera.local_position = camera.abs.to_relative(render_origin) + camera.frac;
    quad_abs = camera.abs + cube::math::UniversalCoord::from_meters(0, 0, -1);
    quad_frac = glm::vec3(0.0f);

    return true;
}

void App::register_console_commands() {
    // Help command
    console.register_command("help", "Show available commands",
        [this](const std::vector<std::string>& args) {
            console.add_log_message("Available commands:");
            for (const auto& [name, cmd] : console.get_commands()) {
                console.add_log_message("  /" + name + " - " + cmd.description);
            }
        });


    // Teleport command
    console.register_command("tp", "Teleport camera to position (/tp x y z)",
        [this](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                console.add_log_message("Usage: tp <x> <y> <z>");
                return;
            }

            try {
                float x = std::stof(args[1]);
                float y = std::stof(args[2]);
                float z = std::stof(args[3]);

                const double fx = std::floor((double)x);
                const double fy = std::floor((double)y);
                const double fz = std::floor((double)z);
                camera.abs = render_origin + cube::math::UniversalCoord::from_meters((std::int64_t)fx, (std::int64_t)fy, (std::int64_t)fz);
                camera.frac = glm::vec3((float)(x - fx), (float)(y - fy), (float)(z - fz));
                camera.local_position = camera.abs.to_relative(render_origin) + camera.frac;
                maybe_shift_origin();
                console.add_log_message("Teleported to: (" + std::to_string(x) + ", " +
                                       std::to_string(y) + ", " + std::to_string(z) + ")", true);
            } catch (const std::exception&) {
                console.add_log_message("Error: Invalid coordinates. Use numbers like: tp 10.5 20.0 -5.2");
            }
        });

    console.register_command("origin_add", "Shift render origin by integer meters and keep view stable (/origin_add x y z)",
        [this](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                console.add_log_message("Usage: origin_add <x> <y> <z>");
                return;
            }
            auto parse_i64 = [](const std::string& s, std::int64_t& out) -> bool {
                const char* b = s.data();
                const char* e = s.data() + s.size();
                std::from_chars_result r = std::from_chars(b, e, out, 10);
                return r.ec == std::errc{} && r.ptr == e;
            };

            std::int64_t x = 0, y = 0, z = 0;
            if (!parse_i64(args[1], x) || !parse_i64(args[2], y) || !parse_i64(args[3], z)) {
                console.add_log_message("Error: meters must be int64. Example: /origin_add 100000 0 0");
                return;
            }
                if (x == 0 && y == 0 && z == 0) return;
                const auto d = cube::math::UniversalCoord::from_meters(x, y, z);
                render_origin += d;
                camera.abs += d;
                quad_abs += d;
                camera.local_position = camera.abs.to_relative(render_origin) + camera.frac;
                console.add_log_message("Origin shifted by meters: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")", true);
        });

    console.register_command("quad", "Set quad local position in meters (/quad x y z)",
        [this](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                console.add_log_message("Usage: quad <x> <y> <z>");
                return;
            }
            try {
                float x = std::stof(args[1]);
                float y = std::stof(args[2]);
                float z = std::stof(args[3]);
                const double fx = std::floor((double)x);
                const double fy = std::floor((double)y);
                const double fz = std::floor((double)z);
                quad_abs = camera.abs + cube::math::UniversalCoord::from_meters((std::int64_t)fx, (std::int64_t)fy, (std::int64_t)fz);
                quad_frac = glm::vec3((float)(x - fx), (float)(y - fy), (float)(z - fz));
                console.add_log_message("Quad set to: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")", true);
            } catch (const std::exception&) {
                console.add_log_message("Error: Invalid coordinates.");
            }
        });
}

bool App::create_swapchain() {
    return swapchain.create(device.handle(), device.physical(), surface, device.queues(), window);
}

bool App::recreate_swapchain() {
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device.handle());

    framebuffers.destroy(device.handle());
    imgui_layer.destroy_framebuffers(device.handle());
    swapchain.destroy(device.handle());

    if (!create_swapchain()) return false;
    if (!framebuffers.create(device.handle(), device.physical(), render_pass.handle, render_pass.depth_format, swapchain.views, swapchain.extent)) return false;
    if (!imgui_layer.recreate_swapchain(device.handle(), swapchain.views, swapchain.extent)) return false;

    return true;
}

void App::main_loop() {
    double fps_timer = glfwGetTime();
    int fps_frames = 0;
    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        CUBE_PROFILE_FRAME();
        CUBE_PROFILE_SCOPE_N("frame");
        double current_time = glfwGetTime();
        float delta_time = static_cast<float>(current_time - last_time);
        last_time = current_time;

        {
            CUBE_PROFILE_SCOPE_N("events");
            glfwPollEvents();
        }
        if (framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swapchain();
        }

        // Update camera
        {
            CUBE_PROFILE_SCOPE_N("update_camera");
            update_camera(delta_time);
            maybe_shift_origin();
        }

        // Handle console mouse capture
        if (show_console != prev_show_console) {
            prev_show_console = show_console;
            if (show_console) {
                // Console opened - release mouse capture
                camera.mouse_captured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                // Console closed - recapture mouse
                camera.mouse_captured = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }

        // Update debug stats every 0.3 seconds
        if (current_time - last_debug_stats_update >= DEBUG_STATS_UPDATE_INTERVAL) {
            update_debug_stats();
            last_debug_stats_update = current_time;
        }

        // Update debug metrics
        frame_time_ms = delta_time * 1000.0f;

        fps_frames++;
        double now = glfwGetTime();
        if (now - fps_timer >= 1.0) {
            fps = static_cast<float>(fps_frames);
            char title[64];
            std::snprintf(title, sizeof(title), "cube %dfps", fps_frames);
            glfwSetWindowTitle(window, title);
            fps_frames = 0;
            fps_timer = now;
        }

        if (shaders.hot_reload(device.handle())) {
            LOG_INFO("Render", "Shaders reloaded, recreating pipeline");
            pipeline.recreate(device.handle(), render_pass.handle, vert_shader->module, frag_shader->module, swapchain.extent, descriptor_set_layout);
        }

        // Prepare matrices for push constants
        UniformBufferObject ubo{};

        // Model matrix - rotating triangle/quad (task 2.7.9)
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        const glm::vec3 quad_rel = quad_abs.to_relative(camera.abs) + (quad_frac - camera.frac);
        ubo.model = glm::translate(glm::mat4(1.0f), quad_rel) *
                   glm::rotate(glm::mat4(1.0f), time * glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec3 forward = glm::vec3(
            cos(camera.pitch) * sin(camera.yaw),
            cos(camera.pitch) * cos(camera.yaw),
            sin(camera.pitch)
        );
        glm::vec3 camera_up = glm::vec3(0.0f, 0.0f, 1.0f); // Z-up world
        ubo.view = glm::lookAt(glm::vec3(0.0f), forward, camera_up);

        float aspect = static_cast<float>(swapchain.extent.width) / static_cast<float>(swapchain.extent.height);
        float t = tan(camera.fov * 0.5f);
        ubo.proj = glm::mat4(0.0f);
        ubo.proj[0][0] = 1.0f / (aspect * t);
        ubo.proj[1][1] = 1.0f / t;
        ubo.proj[2][2] = 0.0f;
        ubo.proj[2][3] = -1.0f;
        ubo.proj[3][2] = camera.near_plane;

        std::memcpy(uniformBufferMapped, &ubo, sizeof(ubo));

        FrameSync& frame = frames.current();
        {
            CUBE_PROFILE_SCOPE_N("wait_fence");
            vkWaitForFences(device.handle(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX);
            vkResetFences(device.handle(), 1, &frame.in_flight);
        }
        if (!frame_arenas.empty()) {
            auto& a = frame_arenas[frames.current_frame_index()];
            a.overflowed = false;
            a.alloc.reset();
        }
        gpu_uploader.begin_frame();

        uint32_t imageIndex = 0;
        VkResult acq = VK_SUCCESS;
        {
            CUBE_PROFILE_SCOPE_N("acquire");
            acq = vkAcquireNextImageKHR(device.handle(), swapchain.handle, UINT64_MAX, frame.image_available, VK_NULL_HANDLE, &imageIndex);
        }
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(); continue; }
        if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) continue;

        {
            CUBE_PROFILE_SCOPE_N("record");
            vkResetCommandBuffer(frame.cmd, 0);
            record_command(frame.cmd, imageIndex);
        }

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.image_available;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &frame.cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &frame.render_finished;

        {
            CUBE_PROFILE_SCOPE_N("submit");
            if (vkQueueSubmit(device.graphics(), 1, &submit, frame.in_flight) != VK_SUCCESS) continue;
        }

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &frame.render_finished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain.handle;
        present.pImageIndices = &imageIndex;
        VkResult pres = VK_SUCCESS;
        {
            CUBE_PROFILE_SCOPE_N("present");
            pres = vkQueuePresentKHR(device.present(), &present);
        }
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swapchain();
        }
        frames.advance();
    }
}

// Input callbacks
void App::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        // If console is open, only allow console-related keys
        if (app->show_console) {
            switch (key) {
                case GLFW_KEY_ESCAPE:
                    app->show_console = false;
                    break;
                // Other keys are handled by ImGui/console
            }
        } else {
            // Normal gameplay controls
            switch (key) {
                case GLFW_KEY_W: app->input.w_pressed = true; break;
                case GLFW_KEY_A: app->input.a_pressed = true; break;
                case GLFW_KEY_S: app->input.s_pressed = true; break;
                case GLFW_KEY_D: app->input.d_pressed = true; break;
                case GLFW_KEY_LEFT_SHIFT: app->input.shift_pressed = true; break;
                case GLFW_KEY_F3: app->show_debug_overlay = !app->show_debug_overlay; break;
                case GLFW_KEY_F4: app->show_log_viewer = !app->show_log_viewer; break;
                case GLFW_KEY_T:
                    app->show_console = true;
                    app->console.set_focus();
                    app->console.set_input_text("");
                    break;
                case GLFW_KEY_SLASH:
                    app->show_console = true;
                    app->console.set_focus();
                    app->console.set_input_text("/");
                    break;
                case GLFW_KEY_ESCAPE:
                    app->camera.mouse_captured = !app->camera.mouse_captured;
                    glfwSetInputMode(window, GLFW_CURSOR,
                        app->camera.mouse_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                    break;
            }
        }
    } else if (action == GLFW_RELEASE) {
        // Only handle movement key releases when console is closed
        if (!app->show_console) {
            switch (key) {
                case GLFW_KEY_W: app->input.w_pressed = false; break;
                case GLFW_KEY_A: app->input.a_pressed = false; break;
                case GLFW_KEY_S: app->input.s_pressed = false; break;
                case GLFW_KEY_D: app->input.d_pressed = false; break;
                case GLFW_KEY_LEFT_SHIFT: app->input.shift_pressed = false; break;
            }
        }
    }
}

void App::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (!app->input.mouse_initialized) {
        app->input.last_mouse_x = xpos;
        app->input.last_mouse_y = ypos;
        app->input.mouse_initialized = true;
        return;
    }

    if (!app->camera.mouse_captured) return;

    double delta_x = xpos - app->input.last_mouse_x;
    double delta_y = ypos - app->input.last_mouse_y;

    app->input.last_mouse_x = xpos;
    app->input.last_mouse_y = ypos;

    // Mouse sensitivity
    const float sensitivity = 0.001f;
    app->camera.yaw += delta_x * sensitivity;  // Mouse right increases yaw (look right)
    app->camera.pitch += delta_y * sensitivity; // Mouse down increases pitch (look down)

    // Clamp pitch to prevent flipping
    app->camera.pitch = glm::clamp(app->camera.pitch, -glm::pi<float>()/2.0f + 0.1f, glm::pi<float>()/2.0f - 0.1f);
}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (app->show_console) return;

    // If ImGui wants to capture mouse input, let it handle the event
    if (ImGui::GetIO().WantCaptureMouse) return;
    (void)button;
    (void)action;
    (void)mods;
}

float App::get_cpu_usage() {
#ifdef _WIN32
    FILETIME idle_time, kernel_time, user_time;

    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        if (!cpu_initialized) {
            // First call - just store the values
            prev_idle_time = idle_time;
            prev_kernel_time = kernel_time;
            prev_user_time = user_time;
            cpu_initialized = true;
            return 0.0f;
        }

        // Calculate differences
        ULARGE_INTEGER prev_idle, curr_idle;
        prev_idle.LowPart = prev_idle_time.dwLowDateTime;
        prev_idle.HighPart = prev_idle_time.dwHighDateTime;

        ULARGE_INTEGER prev_kernel, curr_kernel;
        prev_kernel.LowPart = prev_kernel_time.dwLowDateTime;
        prev_kernel.HighPart = prev_kernel_time.dwHighDateTime;

        ULARGE_INTEGER prev_user, curr_user;
        prev_user.LowPart = prev_user_time.dwLowDateTime;
        prev_user.HighPart = prev_user_time.dwHighDateTime;

        curr_idle.LowPart = idle_time.dwLowDateTime;
        curr_idle.HighPart = idle_time.dwHighDateTime;

        curr_kernel.LowPart = kernel_time.dwLowDateTime;
        curr_kernel.HighPart = kernel_time.dwHighDateTime;

        curr_user.LowPart = user_time.dwLowDateTime;
        curr_user.HighPart = user_time.dwHighDateTime;

        // Calculate deltas
        ULONGLONG idle_delta = curr_idle.QuadPart - prev_idle.QuadPart;
        ULONGLONG kernel_delta = curr_kernel.QuadPart - prev_kernel.QuadPart;
        ULONGLONG user_delta = curr_user.QuadPart - prev_user.QuadPart;
        ULONGLONG total_delta = kernel_delta + user_delta;

        // Store current values for next call
        prev_idle_time = idle_time;
        prev_kernel_time = kernel_time;
        prev_user_time = user_time;

        // Calculate CPU usage percentage
        if (total_delta > 0) {
            float usage = 100.0f * (1.0f - (float)idle_delta / (float)total_delta);
            return usage;
        }
    }
#endif
    return 0.0f;
}

float App::get_gpu_usage() {
#ifdef _WIN32
    static PDH_HQUERY query = NULL;
    static PDH_HCOUNTER counter = NULL;
    static bool initialized = false;

    if (!initialized) {
        if (PdhOpenQueryW(NULL, 0, &query) == ERROR_SUCCESS) {
            // Try to get GPU usage counter
            if (PdhAddCounterW(query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &counter) == ERROR_SUCCESS) {
                initialized = true;
            } else {
                // Fallback: try a different counter
                PdhAddCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter);
                initialized = true;
            }
        }
    }

    if (initialized && query && counter) {
        PDH_FMT_COUNTERVALUE value;
        if (PdhCollectQueryData(query) == ERROR_SUCCESS &&
            PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS) {
            return (float)value.doubleValue;
        }
    }
#endif
    return 0.0f;
}

void App::update_debug_stats() {
    // Update CPU and GPU usage
    cpu_usage = get_cpu_usage();
    gpu_usage = get_gpu_usage();

    // Get system RAM information
#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        ram_used = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
        ram_total = memStatus.ullTotalPhys;
    }
#endif

    if (allocator) {
        gpu_mem.update(allocator);
        vram_used = (size_t)gpu_mem.vram_used();
        vram_total = (size_t)gpu_mem.vram_budget();
    }
}

void App::update_camera(float delta_time) {
    // Movement parameters
    const float max_speed = input.shift_pressed ? 1000.0f : 3.0f;
    const float acceleration = input.shift_pressed ? 3000.0f : 8.0f;
    const float deceleration = input.shift_pressed ? 3000.0f : 12.0f;

    // Calculate movement vectors based on camera yaw (horizontal rotation only)
    // Use Y as forward, X as right (more intuitive for FPS controls)
    glm::vec3 forward = glm::vec3(sin(camera.yaw), cos(camera.yaw), 0.0f);
    glm::vec3 right = glm::vec3(cos(camera.yaw), -sin(camera.yaw), 0.0f);

    // Calculate desired movement direction
    glm::vec3 desired_velocity{0.0f};
    if (input.w_pressed) desired_velocity += forward;
    if (input.s_pressed) desired_velocity -= forward;
    if (input.a_pressed) desired_velocity -= right;
    if (input.d_pressed) desired_velocity += right;

    // Normalize if moving diagonally
    if (glm::length(desired_velocity) > 0.0f) {
        desired_velocity = glm::normalize(desired_velocity) * max_speed;
    }

    // Smooth acceleration/deceleration
    glm::vec3 velocity_diff = desired_velocity - camera.velocity;
    float accel_rate = (glm::length(desired_velocity) > 0.0f) ? acceleration : deceleration;
    float delta_v = accel_rate * delta_time;
    if (delta_v > 1.0f) delta_v = 1.0f;
    camera.velocity += velocity_diff * delta_v;

    const glm::vec3 d = camera.velocity * delta_time;
    camera.frac += d;

    auto pull_int = [](float& v) -> std::int64_t {
        const double f = std::floor((double)v);
        v = (float)(v - f);
        return (std::int64_t)f;
    };

    const std::int64_t ix = pull_int(camera.frac.x);
    const std::int64_t iy = pull_int(camera.frac.y);
    const std::int64_t iz = pull_int(camera.frac.z);
    if (ix || iy || iz) camera.abs += cube::math::UniversalCoord::from_meters(ix, iy, iz);

    camera.local_position = camera.abs.to_relative(render_origin) + camera.frac;
}

void App::maybe_shift_origin() {
    const float t = origin_shift_threshold_m;
    if (glm::abs(camera.local_position.x) <= t &&
        glm::abs(camera.local_position.y) <= t &&
        glm::abs(camera.local_position.z) <= t) return;

    const std::int64_t sx = (std::int64_t)std::floor((double)camera.local_position.x);
    const std::int64_t sy = (std::int64_t)std::floor((double)camera.local_position.y);
    const std::int64_t sz = (std::int64_t)std::floor((double)camera.local_position.z);
    if (sx == 0 && sy == 0 && sz == 0) return;

    render_origin += cube::math::UniversalCoord::from_meters(sx, sy, sz);
    camera.local_position = camera.abs.to_relative(render_origin) + camera.frac;
}

void App::cleanup() {
    if (device.handle()) vkDeviceWaitIdle(device.handle());

#if defined(TRACY_ENABLE)
    if (tracy_vk_ctx) {
        TracyVkDestroy((TracyVkCtx)tracy_vk_ctx);
        tracy_vk_ctx = nullptr;
    }
#endif

    pipeline.destroy(device.handle());
    framebuffers.destroy(device.handle());
    render_pass.destroy(device.handle());
    shaders.destroy(device.handle());

    // Shutdown ImGui (this destroys the render pass too)
    imgui_layer.shutdown(device.handle());

    if (uniformBufferMapped) {
        vmaUnmapMemory(allocator, uniformBufferAllocation);
        uniformBufferMapped = nullptr;
    }
    if (uniformBuffer) {
        gpu_mem.on_free(uniformBufferAllocation);
        vmaDestroyBuffer(allocator, uniformBuffer, uniformBufferAllocation);
        uniformBuffer = VK_NULL_HANDLE;
        uniformBufferAllocation = VK_NULL_HANDLE;
    }

    if (descriptor_pool) {
        vkDestroyDescriptorPool(device.handle(), descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(device.handle(), descriptor_set_layout, nullptr);
        descriptor_set_layout = VK_NULL_HANDLE;
    }

    if (vertexBuffer) {
        gpu_mem.on_free(vertexBufferAllocation);
        vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
        vertexBuffer = VK_NULL_HANDLE;
        vertexBufferAllocation = VK_NULL_HANDLE;
    }

    if (indexBuffer) {
        gpu_mem.on_free(indexBufferAllocation);
        vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
        indexBuffer = VK_NULL_HANDLE;
        indexBufferAllocation = VK_NULL_HANDLE;
    }

    if (allocator) {
        gpu_mem.on_free(gpu_uploader.staging_allocation());
        gpu_uploader.shutdown(allocator);
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }

    swapchain.destroy(device.handle());
    frames.destroy(device.handle());
    device.destroy();
    if (surface) vkDestroySurfaceKHR(instance.handle(), surface, nullptr);
    instance.destroy();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
    LOG_INFO("Core", "Shutdown");
    for (auto& a : frame_arenas) a.alloc.reset();
    cube::mem::report_leaks();
    cube::log::shutdown();
}

void* App::frame_alloc(std::size_t size, std::size_t align) {
    if (frame_arenas.empty()) return nullptr;
    auto& a = frame_arenas[frames.current_frame_index()];
    void* p = a.alloc.alloc(size, align);
    if (!p && !a.overflowed) {
        a.overflowed = true;
        LOG_WARN("Memory", "Frame allocator overflow (requested %zu bytes)", size);
    }
    return p;
}

bool App::record_command(VkCommandBuffer cmd, uint32_t imageIndex) {
    CUBE_PROFILE_SCOPE_N("record_command");
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return false;

    gpu_uploader.flush(cmd);
    {
        VkMemoryBarrier mb{};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
    }

#if defined(TRACY_ENABLE)
    TracyVkCtx vkctx = (TracyVkCtx)tracy_vk_ctx;
#endif

    {
#if defined(TRACY_ENABLE)
        TracyVkZone(vkctx, cmd, "main_pass");
#endif
        // Begin render pass (this handles layout transitions automatically)
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {0.25f, 0.25f, 0.3f, 1.0f}; // Grayish background color
        clear_values[1].depthStencil = {0.0f, 0};

        VkRenderPassBeginInfo rp_bi{};
        rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_bi.renderPass = render_pass.handle;
        rp_bi.framebuffer = framebuffers.framebuffers[imageIndex];
        rp_bi.renderArea.offset = {0, 0};
        rp_bi.renderArea.extent = swapchain.extent;
        rp_bi.clearValueCount = static_cast<uint32_t>(clear_values.size());
        rp_bi.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &descriptor_set, 0, nullptr);

        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

        // Bind index buffer
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.extent.width);
        viewport.height = static_cast<float>(swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Draw quad using indices
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

        // End render pass (this handles layout transitions back to present automatically)
        vkCmdEndRenderPass(cmd);
    }

    // Render ImGui UI
    {
#if defined(TRACY_ENABLE)
        TracyVkZone(vkctx, cmd, "imgui");
#endif
        imgui_layer.new_frame();

        std::array<std::uint64_t, static_cast<std::size_t>(cube::render::GpuBudgetCategory::Count)> cat_used{};
        {
            auto cats = gpu_mem.category_usage();
            for (std::size_t i = 0; i < cats.size(); ++i) cat_used[i] = cats[i].used;
        }
        std::size_t arena_used = 0, arena_cap = 0, arena_peak = 0;
        if (!frame_arenas.empty()) {
            const auto& a = frame_arenas[frames.current_frame_index()].alloc;
            arena_used = a.used();
            arena_cap = a.capacity();
            arena_peak = a.stats().peak_bytes_in_use;
        }

        DebugData debug_data{
            fps,
            frame_time_ms,
            camera.local_position,
            render_origin,
            glm::length(camera.local_position),
            ram_used,
            ram_total,
            vram_used,
            vram_total,
            gpu_mem.vma_totals(),
            cat_used,
            arena_used,
            arena_cap,
            arena_peak,
            (std::uint64_t)gpu_uploader.staging_used(),
            (std::uint64_t)gpu_uploader.staging_capacity(),
            cpu_usage,
            gpu_usage,
            show_debug_overlay,
            show_log_viewer
        };
        imgui_layer.render(cmd, imageIndex, swapchain.extent, debug_data, &console, &show_console, !show_console);
    }

#if defined(TRACY_ENABLE)
    TracyVkCollect(vkctx, cmd);
#endif

    return vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

