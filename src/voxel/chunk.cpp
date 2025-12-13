#include "voxel/chunk.hpp"

#include <limits>

namespace cube::voxel {

static std::uint64_t mix64(std::uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

std::size_t ChunkCoordHash::operator()(const ChunkCoord& c) const noexcept {
    std::uint64_t h = 0;
    h ^= mix64((std::uint64_t)c.x + 0x9e3779b97f4a7c15ULL);
    h ^= mix64((std::uint64_t)c.y + 0xbf58476d1ce4e5b9ULL);
    h ^= mix64((std::uint64_t)c.z + 0x94d049bb133111ebULL);
    return (std::size_t)h;
}

static bool in_bounds16(int x, int y, int z) {
    return (unsigned)x < (unsigned)SUBCHUNK_SIZE && (unsigned)y < (unsigned)SUBCHUNK_SIZE && (unsigned)z < (unsigned)SUBCHUNK_SIZE;
}

bool Chunk::in_bounds(int x, int y, int z) {
    return (unsigned)x < (unsigned)CHUNK_SIZE && (unsigned)y < (unsigned)CHUNK_SIZE && (unsigned)z < (unsigned)CHUNK_SIZE;
}

int Chunk::scx(int v) { return v >> 4; }
int Chunk::scy(int v) { return v >> 4; }
int Chunk::scz(int v) { return v >> 4; }
int Chunk::lx(int v) { return v & (SUBCHUNK_SIZE - 1); }
int Chunk::ly(int v) { return v & (SUBCHUNK_SIZE - 1); }
int Chunk::lz(int v) { return v & (SUBCHUNK_SIZE - 1); }
int Chunk::sub_index(int sx, int sy, int sz) { return sx + SUBCHUNK_PER_AXIS * (sy + SUBCHUNK_PER_AXIS * sz); }

static std::uint32_t sidx(int x, int y, int z) {
    return (std::uint32_t)(x + SUBCHUNK_SIZE * (y + SUBCHUNK_SIZE * z));
}

static std::uint8_t bits_for_palette(std::size_t n) {
    if (n <= 1) return 0;
    std::uint8_t b = 0;
    std::size_t v = n - 1;
    while (v) {
        v >>= 1;
        ++b;
    }
    return b ? b : 1;
}

static std::uint32_t read_index(const std::vector<std::uint64_t>& packed, std::uint8_t bits, std::uint32_t i) {
    const std::uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    const std::uint64_t bit = (std::uint64_t)i * (std::uint64_t)bits;
    const std::size_t w = (std::size_t)(bit >> 6);
    const std::uint32_t o = (std::uint32_t)(bit & 63ULL);
    const std::uint64_t a = packed[w] >> o;
    if (o + bits <= 64) return (std::uint32_t)(a & mask);
    const std::uint64_t b = packed[w + 1] << (64 - o);
    return (std::uint32_t)((a | b) & mask);
}

static void write_index(std::vector<std::uint64_t>& packed, std::uint8_t bits, std::uint32_t i, std::uint32_t v) {
    const std::uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    const std::uint64_t bit = (std::uint64_t)i * (std::uint64_t)bits;
    const std::size_t w = (std::size_t)(bit >> 6);
    const std::uint32_t o = (std::uint32_t)(bit & 63ULL);
    const std::uint64_t vv = (std::uint64_t)v & mask;

    packed[w] &= ~(mask << o);
    packed[w] |= (vv << o);
    if (o + bits <= 64) return;
    const std::uint32_t hi = (std::uint32_t)(o + bits - 64);
    const std::uint64_t hmask = (hi == 64) ? ~0ULL : ((1ULL << hi) - 1ULL);
    packed[w + 1] &= ~hmask;
    packed[w + 1] |= (vv >> (64 - o)) & hmask;
}

static void repack(std::vector<std::uint64_t>& packed, std::uint8_t& bits, std::uint32_t volume, std::uint8_t new_bits) {
    if (new_bits == bits) return;
    std::vector<std::uint32_t> tmp;
    tmp.resize(volume);
    for (std::uint32_t i = 0; i < volume; ++i) tmp[i] = read_index(packed, bits, i);
    bits = new_bits ? new_bits : 1;
    const std::size_t total_bits = (std::size_t)volume * (std::size_t)bits;
    packed.assign((total_bits + 63) / 64, 0ULL);
    for (std::uint32_t i = 0; i < volume; ++i) write_index(packed, bits, i, tmp[i]);
}

static void maybe_collapse_or_compact(detail::SubChunk& s) {
    if (s.kind != detail::SubChunk::Kind::Palette) return;
    std::size_t live = 0;
    std::size_t live_idx = 0;
    for (std::size_t i = 0; i < s.counts.size(); ++i) {
        if (s.counts[i]) {
            ++live;
            live_idx = i;
            if (live > 1) break;
        }
    }
    if (live == 0) {
        s.kind = detail::SubChunk::Kind::Uniform;
        s.uniform = 0;
        s.palette.clear();
        s.counts.clear();
        s.packed.clear();
        s.bits = 0;
        return;
    }
    if (live == 1) {
        s.kind = detail::SubChunk::Kind::Uniform;
        s.uniform = s.palette[live_idx];
        s.palette.clear();
        s.counts.clear();
        s.packed.clear();
        s.bits = 0;
        return;
    }

    std::vector<std::uint32_t> remap;
    remap.resize(s.palette.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<BlockID> new_pal;
    std::vector<std::uint16_t> new_cnt;
    new_pal.reserve(s.palette.size());
    new_cnt.reserve(s.counts.size());
    for (std::size_t i = 0; i < s.palette.size(); ++i) {
        if (!s.counts[i]) continue;
        remap[i] = (std::uint32_t)new_pal.size();
        new_pal.push_back(s.palette[i]);
        new_cnt.push_back(s.counts[i]);
    }

    std::vector<std::uint32_t> tmp;
    tmp.resize(SUBCHUNK_VOLUME);
    for (std::uint32_t i = 0; i < (std::uint32_t)SUBCHUNK_VOLUME; ++i) tmp[i] = remap[read_index(s.packed, s.bits, i)];

    s.palette = std::move(new_pal);
    s.counts = std::move(new_cnt);
    s.bits = bits_for_palette(s.palette.size());
    if (!s.bits) s.bits = 1;
    const std::size_t total_bits = (std::size_t)SUBCHUNK_VOLUME * (std::size_t)s.bits;
    s.packed.assign((total_bits + 63) / 64, 0ULL);
    for (std::uint32_t i = 0; i < (std::uint32_t)SUBCHUNK_VOLUME; ++i) write_index(s.packed, s.bits, i, tmp[i]);
}

BlockID detail::SubChunk::get(int x, int y, int z) const {
    if (!in_bounds16(x, y, z)) return 0;
    if (kind == Kind::Uniform) return uniform;
    const auto pi = read_index(packed, bits, sidx(x, y, z));
    if ((std::size_t)pi >= palette.size()) return 0;
    return palette[pi];
}

bool detail::SubChunk::set(int x, int y, int z, BlockID id) {
    if (!in_bounds16(x, y, z)) return false;
    const BlockID prev = get(x, y, z);
    if (prev == id) return false;

    if (kind == Kind::Uniform) {
        kind = Kind::Palette;
        palette.clear();
        counts.clear();
        palette.push_back(uniform);
        counts.push_back((std::uint16_t)SUBCHUNK_VOLUME);
        bits = 1;
        const std::size_t total_bits = (std::size_t)SUBCHUNK_VOLUME * (std::size_t)bits;
        packed.assign((total_bits + 63) / 64, 0ULL);
        if (uniform == id) return true;
        palette.push_back(id);
        counts.push_back(0);
    }

    std::uint32_t prev_i = 0;
    std::uint32_t next_i = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t i = 0; i < palette.size(); ++i) {
        if (palette[i] == prev) prev_i = (std::uint32_t)i;
        if (palette[i] == id) { next_i = (std::uint32_t)i; break; }
    }
    if (next_i == std::numeric_limits<std::uint32_t>::max()) {
        palette.push_back(id);
        counts.push_back(0);
        const auto nb = bits_for_palette(palette.size());
        if (nb > bits) repack(packed, bits, SUBCHUNK_VOLUME, nb);
        next_i = (std::uint32_t)(palette.size() - 1);
    }

    const std::uint32_t li = sidx(x, y, z);
    write_index(packed, bits, li, next_i);
    if (counts[prev_i]) --counts[prev_i];
    if (counts[next_i] < std::numeric_limits<std::uint16_t>::max()) ++counts[next_i];
    maybe_collapse_or_compact(*this);
    return true;
}

std::size_t detail::SubChunk::payload_bytes() const {
    if (kind == Kind::Uniform) return sizeof(BlockID);
    return palette.size() * sizeof(BlockID) + counts.size() * sizeof(std::uint16_t) + packed.size() * sizeof(std::uint64_t) + 1;
}

Chunk::Chunk(ChunkCoord coord, BlockID fill) : coord_(coord) {
    subs_.resize(SUBCHUNK_COUNT);
    for (auto& s : subs_) {
        s.kind = detail::SubChunk::Kind::Uniform;
        s.uniform = fill;
        s.palette.clear();
        s.counts.clear();
        s.packed.clear();
        s.bits = 0;
    }
}

BlockID Chunk::get_block(int x, int y, int z) const {
    if (!in_bounds(x, y, z)) return 0;
    const int sx = scx(x), sy = scy(y), sz = scz(z);
    const int si = sub_index(sx, sy, sz);
    return subs_[(std::size_t)si].get(lx(x), ly(y), lz(z));
}

bool Chunk::set_block(int x, int y, int z, BlockID id) {
    if (!in_bounds(x, y, z)) return false;
    const int sx = scx(x), sy = scy(y), sz = scz(z);
    const int si = sub_index(sx, sy, sz);
    const bool changed = subs_[(std::size_t)si].set(lx(x), ly(y), lz(z), id);
    if (changed) dirty_ = true;
    return changed;
}

bool Chunk::is_uniform() const {
    if (subs_.empty()) return true;
    const BlockID v = subs_[0].is_uniform() ? subs_[0].uniform : 0;
    for (const auto& s : subs_) {
        if (!s.is_uniform()) return false;
        if (s.uniform != v) return false;
    }
    return true;
}

BlockID Chunk::uniform_value() const {
    return is_uniform() ? subs_[0].uniform : 0;
}

std::uint8_t Chunk::bits_per_block() const {
    std::uint8_t m = 0;
    for (const auto& s : subs_) m = (s.bits > m) ? s.bits : m;
    return m;
}

std::size_t Chunk::palette_size() const {
    std::size_t n = 0;
    for (const auto& s : subs_) n += s.palette.size();
    return n;
}

std::size_t Chunk::payload_bytes() const {
    std::size_t n = 0;
    for (const auto& s : subs_) n += s.payload_bytes();
    return n;
}

}

