#include "imgui_layer.hpp"
#include <iostream>
#include <array>
#include <string>

std::string format_memory(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double value = static_cast<double>(bytes);

    while (value >= 1024.0 && unit_index < 4) {
        value /= 1024.0;
        unit_index++;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit_index]);
    return std::string(buffer);
}

bool ImGuiLayer::init(VkDevice device, VkPhysicalDevice physical_device, VkInstance instance,
                      VkQueue graphics_queue, uint32_t graphics_queue_family,
                      VkFormat swapchain_format, VkExtent2D extent, GLFWwindow* window,
                      uint32_t image_count) {
    if (initialized_) return true;

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Create descriptor pool for ImGui
    std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    }};

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create ImGui descriptor pool" << std::endl;
        return false;
    }

    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }

    // Create the render pass first
    if (!create_render_pass(device, swapchain_format)) {
        std::cerr << "Failed to create ImGui render pass" << std::endl;
        return false;
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = graphics_queue_family;
    init_info.Queue = graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.RenderPass = render_pass_;
    init_info.Subpass = 0;
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        std::cerr << "Failed to initialize ImGui Vulkan backend" << std::endl;
        return false;
    }

    // Upload fonts
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        std::cerr << "Failed to create ImGui fonts texture" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiLayer::shutdown(VkDevice device) {
    if (!initialized_) return;

    vkDeviceWaitIdle(device);

    destroy_framebuffers(device);

    if (render_pass_) {
        vkDestroyRenderPass(device, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    if (descriptor_pool_) {
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    ImGui::DestroyContext();

    initialized_ = false;
}

bool ImGuiLayer::create_render_pass(VkDevice device, VkFormat swapchain_format) {
    VkAttachmentDescription attachment{};
    attachment.format = swapchain_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment{};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &info, nullptr, &render_pass_) != VK_SUCCESS) {
        std::cerr << "Failed to create ImGui render pass" << std::endl;
        return false;
    }

    return true;
}

bool ImGuiLayer::create_framebuffers(VkDevice device, const std::vector<VkImageView>& image_views, VkExtent2D extent) {
    framebuffers_.resize(image_views.size());

    for (size_t i = 0; i < image_views.size(); i++) {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = &image_views[i];
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(device, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create ImGui framebuffer " << i << std::endl;
            return false;
        }
    }

    return true;
}

void ImGuiLayer::destroy_framebuffers(VkDevice device) {
    for (auto fb : framebuffers_) {
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers_.clear();
}

void ImGuiLayer::new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer cmd, uint32_t image_index, VkExtent2D extent, const DebugData& debug_data) {
    // Show debug overlay if enabled
    if (debug_data.show_overlay) {
        // Style the window background and remove borders
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f)); // Black with 30% opacity
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // No border
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)); // Add some padding

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs);

        // Make font bigger for better legibility
        ImGui::SetWindowFontScale(1.2f);

        ImGui::Text("FPS: %.1f", debug_data.fps);
        ImGui::Text("Frame Time: %.2f ms", debug_data.frame_time_ms);
        ImGui::Text("CPU: %.1f%%", debug_data.cpu_usage);
        ImGui::Text("GPU: %.1f%%", debug_data.gpu_usage);
        ImGui::Text("Camera: (%.2f, %.2f, %.2f)",
            debug_data.camera_position.x,
            debug_data.camera_position.y,
            debug_data.camera_position.z);

        // RAM display
        if (debug_data.ram_total > 0) {
            std::string ram_used_str = format_memory(debug_data.ram_used);
            std::string ram_total_str = format_memory(debug_data.ram_total);
            float ram_load_percent = (static_cast<float>(debug_data.ram_used) / static_cast<float>(debug_data.ram_total)) * 100.0f;
            ImGui::Text("RAM: %s / %s (%.1f%%)", ram_used_str.c_str(), ram_total_str.c_str(), ram_load_percent);
        } else {
            ImGui::Text("RAM: N/A");
        }

        // VRAM display
        if (debug_data.vram_total > 0) {
            std::string vram_used_str = format_memory(debug_data.vram_used);
            std::string vram_total_str = format_memory(debug_data.vram_total);
            float vram_load_percent = (static_cast<float>(debug_data.vram_used) / static_cast<float>(debug_data.vram_total)) * 100.0f;
            ImGui::Text("VRAM: %s / %s (%.1f%%)", vram_used_str.c_str(), vram_total_str.c_str(), vram_load_percent);
        } else {
            ImGui::Text("VRAM: N/A");
        }

        ImGui::End();

        // Pop the style changes
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    // Render ImGui
    ImGui::Render();

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = render_pass_;
    info.framebuffer = framebuffers_[image_index];
    info.renderArea.offset = {0, 0};
    info.renderArea.extent = extent;
    info.clearValueCount = 0;
    info.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);
}

bool ImGuiLayer::recreate_swapchain(VkDevice device, const std::vector<VkImageView>& image_views, VkExtent2D extent) {
    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(image_views.size()));

    destroy_framebuffers(device);
    return create_framebuffers(device, image_views, extent);
}

