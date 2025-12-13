#include "voxel/chunk_manager.hpp"

#include <algorithm>

namespace cube::voxel {

ChunkManager::ChunkManager(std::size_t payload_limit_bytes) : payload_limit_bytes_(payload_limit_bytes) {}

void ChunkManager::set_payload_limit(std::size_t bytes) {
    payload_limit_bytes_ = bytes;
    evict_if_needed_();
}

void ChunkManager::touch_(Entry& e) {
    lru_.splice(lru_.begin(), lru_, e.it);
    e.it = lru_.begin();
}

void ChunkManager::update_payload_(Entry& e, std::size_t new_bytes) {
    if (new_bytes == e.payload_bytes) return;
    if (payload_bytes_ >= e.payload_bytes) payload_bytes_ -= e.payload_bytes;
    payload_bytes_ += new_bytes;
    e.payload_bytes = new_bytes;
}

void ChunkManager::evict_if_needed_() {
    while (payload_limit_bytes_ && payload_bytes_ > payload_limit_bytes_ && !lru_.empty()) {
        const ChunkCoord c = lru_.back();
        lru_.pop_back();
        auto it = chunks_.find(c);
        if (it == chunks_.end()) continue;
        payload_bytes_ -= it->second.payload_bytes;
        chunks_.erase(it);
        ++evictions_;
    }
}

Chunk* ChunkManager::get_chunk(ChunkCoord c) {
    auto it = chunks_.find(c);
    if (it == chunks_.end()) return nullptr;
    touch_(it->second);
    update_payload_(it->second, it->second.chunk.payload_bytes());
    evict_if_needed_();
    return &it->second.chunk;
}

const Chunk* ChunkManager::get_chunk(ChunkCoord c) const {
    auto it = chunks_.find(c);
    if (it == chunks_.end()) return nullptr;
    return &it->second.chunk;
}

Chunk& ChunkManager::create_chunk(ChunkCoord c, BlockID fill) {
    auto it = chunks_.find(c);
    if (it != chunks_.end()) {
        touch_(it->second);
        update_payload_(it->second, it->second.chunk.payload_bytes());
        evict_if_needed_();
        return it->second.chunk;
    }

    lru_.push_front(c);
    Entry e{Chunk(c, fill), lru_.begin(), 0};
    e.payload_bytes = e.chunk.payload_bytes();
    payload_bytes_ += e.payload_bytes;
    auto [ins, ok] = chunks_.emplace(c, std::move(e));
    (void)ok;
    evict_if_needed_();
    return ins->second.chunk;
}

void ChunkManager::notify_modified(ChunkCoord c) {
    auto it = chunks_.find(c);
    if (it == chunks_.end()) return;
    touch_(it->second);
    update_payload_(it->second, it->second.chunk.payload_bytes());
    evict_if_needed_();
}

bool ChunkManager::set_block(ChunkCoord c, int x, int y, int z, BlockID id) {
    Chunk& ch = create_chunk(c, 0);
    auto it = chunks_.find(c);
    if (it != chunks_.end()) touch_(it->second);
    const bool changed = ch.set_block(x, y, z, id);
    if (it != chunks_.end()) update_payload_(it->second, ch.payload_bytes());
    evict_if_needed_();
    return changed;
}

BlockID ChunkManager::get_block(ChunkCoord c, int x, int y, int z) const {
    const Chunk* ch = get_chunk(c);
    if (!ch) return 0;
    return ch->get_block(x, y, z);
}

ChunkManager::Stats ChunkManager::stats() const {
    return Stats{
        .chunk_count = chunks_.size(),
        .payload_bytes = payload_bytes_,
        .payload_limit = payload_limit_bytes_,
        .evictions = evictions_
    };
}

std::vector<std::pair<ChunkCoord, std::size_t>> ChunkManager::largest_chunks(std::size_t n) const {
    std::vector<std::pair<ChunkCoord, std::size_t>> v;
    v.reserve(chunks_.size());
    for (const auto& [c, e] : chunks_) v.push_back({c, e.chunk.payload_bytes()});
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > n) v.resize(n);
    return v;
}

}

