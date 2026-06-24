#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "../world/Chunk.h"
#include "../world/World.h"

struct ChunkMesh {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    bool ready = false;
};

class MeshBuilder {
public:
    static ChunkMesh build(const Chunk& chunk, const World& world);
private:
    static bool isVisible(const World& world, int wx, int wy, int wz, int dir);
};
