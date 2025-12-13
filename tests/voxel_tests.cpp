#include "voxel/blocks.hpp"
#include "voxel/chunk.hpp"
#include "voxel/chunk_manager.hpp"

#include <cstdio>

static int vfail(int code, const char* what) {
    std::fprintf(stderr, "cube_tests: FAIL(%d): %s\n", code, what);
    return code;
}

int run_voxel_tests() {
    using namespace cube::voxel;

    {
        BlockRegistry r;
        auto d = register_default_blocks(r);
        if (r.size() < 4) return vfail(401, "BlockRegistry default blocks");
        if (!r.get(d.air) || r.get(d.air)->solid) return vfail(402, "air is non-solid");
        if (!r.get(d.stone) || r.get(d.stone)->name != "stone") return vfail(403, "stone registered");
    }

    {
        Chunk c(ChunkCoord{0, 0, 0}, 0);
        if (!c.is_uniform()) return vfail(411, "new chunk uniform");
        if (c.payload_bytes() > 16) return vfail(412, "uniform payload small");
        if (c.get_block(0, 0, 0) != 0) return vfail(413, "uniform get");
        if (!c.set_block(1, 2, 3, 7)) return vfail(414, "set_block changes");
        if (!c.dirty()) return vfail(415, "set_block marks dirty");
        if (c.get_block(1, 2, 3) != 7) return vfail(416, "set_block stores");
        if (c.is_uniform()) return vfail(417, "chunk switched to palette");
        if (c.palette_size() < 2) return vfail(418, "palette size >=2 after change");
    }

    {
        Chunk c(ChunkCoord{0, 0, 0}, 1);
        for (int i = 0; i < 1000; ++i) {
            c.set_block(i & 31, (i >> 5) & 31, (i >> 10) & 31, (BlockID)((i % 5) + 1));
        }
        if (c.payload_bytes() > 32ull * 1024ull) return vfail(421, "palette payload bounded for small palette");
    }

    {
        ChunkManager m(1024);
        m.create_chunk(ChunkCoord{0, 0, 0}, 0);
        m.create_chunk(ChunkCoord{1, 0, 0}, 0);
        m.create_chunk(ChunkCoord{2, 0, 0}, 0);
        auto st = m.stats();
        if (st.chunk_count == 0) return vfail(431, "chunks created");
        m.set_payload_limit(4);
        st = m.stats();
        if (st.payload_bytes > st.payload_limit) return vfail(432, "eviction respects limit");
        if (st.evictions == 0) return vfail(433, "evictions occur");
    }

    return 0;
}

