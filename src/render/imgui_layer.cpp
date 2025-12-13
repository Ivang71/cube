#include "imgui_layer.hpp"
#include "../core/console.hpp"
#include "../core/log.hpp"
#include "voxel/blocks.hpp"
#include "voxel/chunk_manager.hpp"
#include <cstdio>
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

void ImGuiLayer::render(VkCommandBuffer cmd, uint32_t image_index, VkExtent2D extent, const DebugData& debug_data, class Console* console, bool* show_console, bool show_chat_messages) {
    // Show debug overlay if enabled
    if (debug_data.show_overlay) {
        ImVec2 display_size = ImGui::GetIO().DisplaySize;
        const float line_h = ImGui::GetTextLineHeightWithSpacing();
        const float overlay_h = (line_h * 10.0f) + 12.0f;

        // No background, no borders
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // No border
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)); // Add some padding

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_size.x, overlay_h), ImGuiCond_Always);
        ImGui::Begin("Debug Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground);

        // Make font bigger for better legibility
        ImGui::SetWindowFontScale(1.2f);

        ImGui::Text("FPS: %d", (int)debug_data.fps);
        ImGui::Text("Frame Time: %.2f ms", debug_data.frame_time_ms);
        ImGui::Text("CPU: %.1f%%", debug_data.cpu_usage);
        ImGui::Text("GPU: %.1f%%", debug_data.gpu_usage);
        ImGui::Text("Camera: (%.2f, %.2f, %.2f)",
            debug_data.camera_position.x,
            debug_data.camera_position.y,
            debug_data.camera_position.z);
        ImGui::Text("Origin: (%lld,%lld,%lld) + (%d,%d,%d)m",
            (long long)debug_data.render_origin.sx,
            (long long)debug_data.render_origin.sy,
            (long long)debug_data.render_origin.sz,
            (int)debug_data.render_origin.mx,
            (int)debug_data.render_origin.my,
            (int)debug_data.render_origin.mz);
        ImGui::Text("Dist from origin: %.2fm", debug_data.distance_from_origin_m);

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

        ImGui::SetNextWindowPos(ImVec2(display_size.x - 8.0f, 8.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.78f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.0f, 0.0f, 0.0f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.0f, 0.0f, 0.0f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
        if (ImGui::Begin("Memory", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Frame arena: %s / %s (peak %s)",
                format_memory(debug_data.frame_arena_used).c_str(),
                format_memory(debug_data.frame_arena_capacity).c_str(),
                format_memory(debug_data.frame_arena_peak).c_str()
            );
            ImGui::Text("Staging ring: %s / %s",
                format_memory((size_t)debug_data.staging_used).c_str(),
                format_memory((size_t)debug_data.staging_capacity).c_str()
            );
            ImGui::Separator();
            ImGui::Text("VMA: allocations=%u (%s) blocks=%u (%s)",
                debug_data.vma_totals.allocation_count,
                format_memory((size_t)debug_data.vma_totals.allocation_bytes).c_str(),
                debug_data.vma_totals.block_count,
                format_memory((size_t)debug_data.vma_totals.block_bytes).c_str()
            );
            ImGui::Separator();
            const float denom = debug_data.vram_total ? (float)debug_data.vram_total : 1.0f;
            for (std::size_t i = 0; i < debug_data.gpu_category_used.size(); ++i) {
                const auto used = debug_data.gpu_category_used[i];
                const float f = (float)used / denom;
                ImGui::Text("%s: %s", cube::render::gpu_budget_category_name((cube::render::GpuBudgetCategory)i), format_memory((size_t)used).c_str());
                ImGui::ProgressBar(f, ImVec2(-1.0f, 0.0f));
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(display_size.x - 8.0f, 396.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(520, 260), ImGuiCond_Always);
        if (ImGui::Begin("Jobs", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Workers: %u", debug_data.job_worker_count);
            ImGui::Text("Pending: high=%u normal=%u low=%u",
                debug_data.job_pending_high, debug_data.job_pending_normal, debug_data.job_pending_low);
            ImGui::Text("Stall warnings: %u", debug_data.job_stall_warnings);
            ImGui::Separator();
            const std::uint32_t n = debug_data.job_worker_count > 64 ? 64u : debug_data.job_worker_count;
            for (std::uint32_t i = 0; i < n; ++i) {
                const float u = debug_data.job_worker_utilization[i];
                ImGui::Text("W%u: %.1f%%", i, u);
                ImGui::ProgressBar(u / 100.0f, ImVec2(-1.0f, 0.0f));
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(12);
    }

    if (debug_data.show_log_viewer) {
        static bool show_info = true;
        static bool show_warn = true;
        static bool show_error = true;
        static ImGuiTextFilter filter;
        static bool auto_scroll = true;

        ImGui::SetNextWindowSize(ImVec2(900, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Log", nullptr)) {
            if (ImGui::Button("Clear")) cube::log::clear();
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &auto_scroll);
            ImGui::SameLine();
            ImGui::Checkbox("INFO", &show_info);
            ImGui::SameLine();
            ImGui::Checkbox("WARN", &show_warn);
            ImGui::SameLine();
            ImGui::Checkbox("ERROR", &show_error);
            filter.Draw("Filter");

            std::vector<cube::log::Entry> entries = cube::log::snapshot();
            ImGui::Separator();
            ImGui::BeginChild("log_scroller", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(entries.size()));
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& e = entries[static_cast<size_t>(i)];
                    bool ok = true;
                    if (e.level == cube::log::Level::Info && !show_info) ok = false;
                    if (e.level == cube::log::Level::Warn && !show_warn) ok = false;
                    if (e.level == cube::log::Level::Error && !show_error) ok = false;
                    if (ok && !filter.PassFilter(e.text.c_str())) ok = false;
                    if (!ok) continue;

                    if (e.level == cube::log::Level::Error) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                    else if (e.level == cube::log::Level::Warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
                    ImGui::TextUnformatted(e.text.c_str());
                    if (e.level != cube::log::Level::Info) ImGui::PopStyleColor(1);
                }
            }

            if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (debug_data.show_voxel_debug && debug_data.block_registry && debug_data.chunk_manager) {
        static ImGuiTextFilter block_filter;
        ImGui::SetNextWindowSize(ImVec2(560, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Voxel", nullptr)) {
            if (ImGui::BeginTabBar("voxel_tabs")) {
                if (ImGui::BeginTabItem("Blocks")) {
                    block_filter.Draw("Filter");
                    ImGui::Separator();
                    const auto& blocks = debug_data.block_registry->all();
                    ImGuiListClipper clipper;
                    clipper.Begin((int)blocks.size());
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                            const auto& b = blocks[(std::size_t)i];
                            if (!block_filter.PassFilter(b.name.c_str())) continue;
                            ImGui::Text("#%d  %s  %s", i, b.name.c_str(), b.solid ? "solid" : "air");
                        }
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Chunks")) {
                    const auto st = debug_data.chunk_manager->stats();
                    ImGui::Text("Chunks: %zu", st.chunk_count);
                    ImGui::Text("Payload: %s / %s", format_memory(st.payload_bytes).c_str(), format_memory(st.payload_limit).c_str());
                    ImGui::Text("Evictions: %llu", (unsigned long long)st.evictions);
                    ImGui::Separator();
                    ImGui::Text("Largest chunks:");
                    const auto largest = debug_data.chunk_manager->largest_chunks(12);
                    for (const auto& [c, sz] : largest) {
                        const auto* ch = debug_data.chunk_manager->get_chunk(c);
                        const std::uint8_t bits = ch ? ch->bits_per_block() : 0;
                        const std::size_t pal = ch ? ch->palette_size() : 0;
                        ImGui::Text("(%lld,%lld,%lld)  %s  pal=%zu  bpb=%u",
                            (long long)c.x, (long long)c.y, (long long)c.z,
                            format_memory(sz).c_str(), pal, (unsigned)bits);
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    // Render console if provided
    if (console && show_console) {
        console->render(show_console);
    }

    // Render chat messages if requested (when console is not open)
    if (console && show_chat_messages && (!show_console || !*show_console)) {
        console->render_chat_messages();
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

