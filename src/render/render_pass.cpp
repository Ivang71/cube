#include "render_pass.hpp"

#include <iostream>
#include <array>

bool RenderPassContext::create(VkDevice device, VkPhysicalDevice physical_device, VkFormat color_format) {
    depth_format = find_depth_format(physical_device);
    if (depth_format == VK_FORMAT_UNDEFINED) {
        std::cerr << "Failed to find supported depth format" << std::endl;
        return false;
    }

    VkAttachmentDescription color_attachment{};
    color_attachment.format = color_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments = attachments.data();
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &ci, nullptr, &handle) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    return true;
}

void RenderPassContext::destroy(VkDevice device) {
    if (handle) vkDestroyRenderPass(device, handle, nullptr);
    handle = VK_NULL_HANDLE;
}

VkFormat RenderPassContext::find_depth_format(VkPhysicalDevice physical_device) {
    std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

bool FramebufferContext::create(VkDevice device, VkPhysicalDevice physical_device, VkRenderPass render_pass,
                               VkFormat depth_format, const std::vector<VkImageView>& color_views, VkExtent2D extent) {
    // Create depth image
    VkImageCreateInfo depth_ci{};
    depth_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_ci.format = depth_format;
    depth_ci.extent = {extent.width, extent.height, 1};
    depth_ci.mipLevels = 1;
    depth_ci.arrayLayers = 1;
    depth_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depth_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &depth_ci, nullptr, &depth_image) != VK_SUCCESS) {
        std::cerr << "Failed to create depth image" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, depth_image, &mem_req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = mem_req.size;
    alloc.memoryTypeIndex = 0; // Simplified - should find proper memory type

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            alloc.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &alloc, nullptr, &depth_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate depth memory" << std::endl;
        return false;
    }

    if (vkBindImageMemory(device, depth_image, depth_memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind depth memory" << std::endl;
        return false;
    }

    VkImageViewCreateInfo view_ci{};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = depth_image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = depth_format;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &view_ci, nullptr, &depth_view) != VK_SUCCESS) {
        std::cerr << "Failed to create depth image view" << std::endl;
        return false;
    }

    // Create framebuffers
    framebuffers.resize(color_views.size());
    for (size_t i = 0; i < color_views.size(); i++) {
        std::array<VkImageView, 2> attachments = {color_views[i], depth_view};

        VkFramebufferCreateInfo fb_ci{};
        fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_ci.renderPass = render_pass;
        fb_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        fb_ci.pAttachments = attachments.data();
        fb_ci.width = extent.width;
        fb_ci.height = extent.height;
        fb_ci.layers = 1;

        if (vkCreateFramebuffer(device, &fb_ci, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer " << i << std::endl;
            return false;
        }
    }

    return true;
}

void FramebufferContext::destroy(VkDevice device) {
    for (auto fb : framebuffers) {
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers.clear();

    if (depth_view) vkDestroyImageView(device, depth_view, nullptr);
    if (depth_image) vkDestroyImage(device, depth_image, nullptr);
    if (depth_memory) vkFreeMemory(device, depth_memory, nullptr);

    depth_view = VK_NULL_HANDLE;
    depth_image = VK_NULL_HANDLE;
    depth_memory = VK_NULL_HANDLE;
}
