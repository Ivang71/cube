#include "voxel/blocks.hpp"

#include <limits>

namespace cube::voxel {

BlockRegistry::BlockRegistry() {
    blocks_.reserve(256);
    blocks_.push_back(BlockProperties{.name = "air", .solid = false});
}

BlockID BlockRegistry::register_block(BlockProperties props) {
    if (blocks_.size() >= std::numeric_limits<BlockID>::max()) return 0;
    blocks_.push_back(std::move(props));
    return (BlockID)(blocks_.size() - 1);
}

const BlockProperties* BlockRegistry::get(BlockID id) const {
    if ((std::size_t)id >= blocks_.size()) return nullptr;
    return &blocks_[(std::size_t)id];
}

DefaultBlocks register_default_blocks(BlockRegistry& r) {
    DefaultBlocks b{};
    b.air = 0;
    b.stone = r.register_block(BlockProperties{.name = "stone", .solid = true});
    b.dirt = r.register_block(BlockProperties{.name = "dirt", .solid = true});
    b.grass = r.register_block(BlockProperties{.name = "grass", .solid = true});
    return b;
}

}

