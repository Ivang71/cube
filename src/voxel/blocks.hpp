#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cube::voxel {

using BlockID = std::uint16_t;

struct BlockProperties {
    std::string name;
    bool solid{true};
};

class BlockRegistry {
public:
    BlockRegistry();

    BlockID register_block(BlockProperties props);
    const BlockProperties* get(BlockID id) const;
    std::size_t size() const { return blocks_.size(); }
    const std::vector<BlockProperties>& all() const { return blocks_; }

private:
    std::vector<BlockProperties> blocks_;
};

struct DefaultBlocks {
    BlockID air{0};
    BlockID stone{0};
    BlockID dirt{0};
    BlockID grass{0};
};

DefaultBlocks register_default_blocks(BlockRegistry& r);

}

