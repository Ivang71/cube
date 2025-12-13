#pragma once

#include "voxel/chunk.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

namespace cube::voxel {

class ChunkManager {
public:
    struct Stats {
        std::size_t chunk_count{0};
        std::size_t payload_bytes{0};
        std::size_t payload_limit{0};
        std::uint64_t evictions{0};
    };

    explicit ChunkManager(std::size_t payload_limit_bytes = 256ull * 1024ull * 1024ull);

    void set_payload_limit(std::size_t bytes);
    std::size_t payload_limit() const { return payload_limit_bytes_; }
    std::size_t payload_bytes() const { return payload_bytes_; }

    Chunk* get_chunk(ChunkCoord c);
    const Chunk* get_chunk(ChunkCoord c) const;
    Chunk& create_chunk(ChunkCoord c, BlockID fill = 0);
    void notify_modified(ChunkCoord c);

    bool set_block(ChunkCoord c, int x, int y, int z, BlockID id);
    BlockID get_block(ChunkCoord c, int x, int y, int z) const;

    Stats stats() const;
    std::vector<std::pair<ChunkCoord, std::size_t>> largest_chunks(std::size_t n) const;

private:
    struct Entry {
        Chunk chunk;
        std::list<ChunkCoord>::iterator it;
        std::size_t payload_bytes{0};
    };

    void touch_(Entry& e);
    void evict_if_needed_();
    void update_payload_(Entry& e, std::size_t new_bytes);

    std::unordered_map<ChunkCoord, Entry, ChunkCoordHash> chunks_;
    std::list<ChunkCoord> lru_;
    std::size_t payload_limit_bytes_{0};
    std::size_t payload_bytes_{0};
    std::uint64_t evictions_{0};
};

}

