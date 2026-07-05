#include "Chunk.hpp"

namespace CnaCraft::Worlds {

Chunk::Chunk() {
    blocks_.fill(BlockType::Air);
}

bool Chunk::InBounds(int x, int y, int z) {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_SIZE &&
           z >= 0 && z < CHUNK_SIZE;
}

int Chunk::Index(int x, int y, int z) {
    return x + CHUNK_SIZE * (y + CHUNK_SIZE * z);
}

BlockType Chunk::GetBlock(int x, int y, int z) const {
    if (!InBounds(x, y, z)) return BlockType::Air;
    return blocks_[static_cast<std::size_t>(Index(x, y, z))];
}

void Chunk::SetBlock(int x, int y, int z, BlockType type) {
    if (!InBounds(x, y, z)) return;
    blocks_[static_cast<std::size_t>(Index(x, y, z))] = type;
    dirty_ = true;
}

}
