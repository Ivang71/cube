#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

namespace cube::render {

class GpuUploader {
public:
    bool init(VmaAllocator allocator, VkDeviceSize size_bytes);
    void shutdown(VmaAllocator allocator);

    void begin_frame();

    struct UploadCmd {
        VkBuffer dst{};
        VkDeviceSize dst_offset{};
        VkDeviceSize src_offset{};
        VkDeviceSize size{};
        UploadCmd* next{};
    };

    bool enqueue_buffer_upload(void* ctx, void* (*alloc_fn)(void*, std::size_t, std::size_t), VkBuffer dst, VkDeviceSize dst_offset, const void* data, VkDeviceSize size);
    void flush(VkCommandBuffer cmd);

    VkBuffer staging_buffer() const { return staging_buffer_; }
    VmaAllocation staging_allocation() const { return staging_alloc_; }
    VkDeviceSize staging_capacity() const { return size_; }
    VkDeviceSize staging_used() const { return offset_; }

private:
    VmaAllocator allocator_{};
    VkBuffer staging_buffer_{};
    VmaAllocation staging_alloc_{};
    void* mapped_{};
    VkDeviceSize size_{};
    VkDeviceSize offset_{};
    UploadCmd* head_{};
    UploadCmd* tail_{};
};

} // namespace cube::render

