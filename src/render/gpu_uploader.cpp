#include "gpu_uploader.hpp"

#include <cstring>

namespace cube::render {

bool GpuUploader::init(VmaAllocator allocator, VkDeviceSize size_bytes) {
    allocator_ = allocator;
    size_ = size_bytes;
    offset_ = 0;
    head_ = nullptr;
    tail_ = nullptr;

    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size_bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(allocator, &bi, &ai, &staging_buffer_, &staging_alloc_, &info) != VK_SUCCESS) return false;
    mapped_ = info.pMappedData;
    return mapped_ != nullptr;
}

void GpuUploader::shutdown(VmaAllocator allocator) {
    if (staging_buffer_) {
        vmaDestroyBuffer(allocator, staging_buffer_, staging_alloc_);
        staging_buffer_ = VK_NULL_HANDLE;
        staging_alloc_ = VK_NULL_HANDLE;
        mapped_ = nullptr;
    }
    allocator_ = nullptr;
    size_ = 0;
    offset_ = 0;
    head_ = nullptr;
    tail_ = nullptr;
}

void GpuUploader::begin_frame() {
    offset_ = 0;
    head_ = nullptr;
    tail_ = nullptr;
}

bool GpuUploader::enqueue_buffer_upload(void* ctx, void* (*alloc_fn)(void*, std::size_t, std::size_t), VkBuffer dst, VkDeviceSize dst_offset, const void* data, VkDeviceSize size) {
    if (!mapped_ || !dst || !data || size == 0) return false;
    const VkDeviceSize aligned = (offset_ + 15) & ~VkDeviceSize(15);
    if (aligned + size > size_) return false;
    std::memcpy(static_cast<std::byte*>(mapped_) + aligned, data, static_cast<std::size_t>(size));

    if (!alloc_fn) return false;
    auto* cmd = static_cast<UploadCmd*>(alloc_fn(ctx, sizeof(UploadCmd), alignof(UploadCmd)));
    if (!cmd) return false;
    *cmd = UploadCmd{dst, dst_offset, aligned, size, nullptr};
    if (!head_) head_ = cmd;
    else tail_->next = cmd;
    tail_ = cmd;
    offset_ = aligned + size;
    return true;
}

void GpuUploader::flush(VkCommandBuffer cmd) {
    if (!cmd || !head_) return;
    for (UploadCmd* u = head_; u; u = u->next) {
        VkBufferCopy c{};
        c.srcOffset = u->src_offset;
        c.dstOffset = u->dst_offset;
        c.size = u->size;
        vkCmdCopyBuffer(cmd, staging_buffer_, u->dst, 1, &c);
    }
}

} // namespace cube::render

