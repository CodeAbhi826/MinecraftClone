#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../core/Block.h"

class World;
struct Chunk;

struct ChunkMesh {
    std::vector<float> opaqueVertices;
    std::vector<uint32_t> opaqueIndices;
    std::vector<float> transparentVertices;
    std::vector<uint32_t> transparentIndices;
    bool ready = false;
};

class MeshBuilder {
public:
    static ChunkMesh build(const Chunk& chunk, const World& world);
private:
    static bool isVisible(const World& world, int wx, int wy, int wz, int dir, BlockStateID selfId);
};
