#pragma once

#include "voxel/blocks.hpp"

#include <cstdint>
#include <vector>

namespace cube::voxel {

inline constexpr int CHUNK_SIZE = 32;
inline constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
inline constexpr int SUBCHUNK_SIZE = 16;
inline constexpr int SUBCHUNK_VOLUME = SUBCHUNK_SIZE * SUBCHUNK_SIZE * SUBCHUNK_SIZE;
inline constexpr int SUBCHUNK_PER_AXIS = CHUNK_SIZE / SUBCHUNK_SIZE;
inline constexpr int SUBCHUNK_COUNT = SUBCHUNK_PER_AXIS * SUBCHUNK_PER_AXIS * SUBCHUNK_PER_AXIS;

struct ChunkCoord {
    std::int64_t x{0}, y{0}, z{0};
    friend bool operator==(const ChunkCoord& a, const ChunkCoord& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept;
};

namespace detail {
struct SubChunk {
    enum class Kind : std::uint8_t { Uniform, Palette };
    Kind kind{Kind::Uniform};
    BlockID uniform{0};
    std::vector<BlockID> palette;
    std::vector<std::uint16_t> counts;
    std::vector<std::uint64_t> packed;
    std::uint8_t bits{0};

    BlockID get(int x, int y, int z) const;
    bool set(int x, int y, int z, BlockID id);
    std::size_t payload_bytes() const;
    bool is_uniform() const { return kind == Kind::Uniform; }
};
}

class Chunk {
public:
    explicit Chunk(ChunkCoord coord, BlockID fill = 0);

    ChunkCoord coord() const { return coord_; }
    bool dirty() const { return dirty_; }
    void clear_dirty() { dirty_ = false; }

    BlockID get_block(int x, int y, int z) const;
    bool set_block(int x, int y, int z, BlockID id);

    std::size_t payload_bytes() const;
    bool is_uniform() const;
    BlockID uniform_value() const;
    std::uint8_t bits_per_block() const;
    std::size_t palette_size() const;

private:
    static bool in_bounds(int x, int y, int z);
    static int scx(int v);
    static int scy(int v);
    static int scz(int v);
    static int sub_index(int sx, int sy, int sz);
    static int lx(int v);
    static int ly(int v);
    static int lz(int v);

    ChunkCoord coord_{};
    bool dirty_{false};
    std::vector<detail::SubChunk> subs_;
};

}

