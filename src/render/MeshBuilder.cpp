#include "MeshBuilder.h"
#include "../core/Block.h"
#include "../world/Chunk.h"
#include "../world/World.h"
#include <glm/glm.hpp>
#include <algorithm>

using namespace glm;
namespace {
const ivec3 dirs[6] = {
    {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}
};
const vec3 faceVerts[6][4] = {
    {{0,1,0}, {1,1,0}, {1,1,1}, {0,1,1}}, // Top
    {{0,0,1}, {1,0,1}, {1,0,0}, {0,0,0}}, // Bottom
    {{0,1,1}, {1,1,1}, {1,0,1}, {0,0,1}}, // Front
    {{1,1,0}, {0,1,0}, {0,0,0}, {1,0,0}}, // Back
    {{1,1,1}, {1,0,1}, {1,0,0}, {1,1,0}}, // Right
    {{0,1,0}, {0,0,0}, {0,0,1}, {0,1,1}}  // Left
};
const vec2 uvs[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
}

bool MeshBuilder::isVisible(const World& world, int wx, int wy, int wz, int dir, BlockStateID selfId) {
    ivec3 n = ivec3(wx, wy, wz) + dirs[dir];
    BlockStateID id = world.getBlock(n.x, n.y, n.z);
    if (id == (uint16_t)Block::ID::air) return true;
    if (id == selfId) return false;
    return !Block::getProperties((Block::ID)id).opaque;
}

ChunkMesh MeshBuilder::build(const Chunk& chunk, const World& world) {
    ChunkMesh mesh;
    if (chunk.maxY < chunk.minY) { mesh.ready = true; return mesh; }
    int yStart = std::max(-64, chunk.minY - 1);
    int yEnd   = std::min(320, chunk.maxY + 2);
    for (int y = yStart; y < yEnd; ++y) {
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                BlockStateID block = chunk.getBlock(x, y, z);
                if (block == (uint16_t)Block::ID::air) continue;

                const auto& props = Block::getProperties((Block::ID)block);
                bool isTransparent = !props.opaque;
                int wx = chunk.x * 16 + x;
                int wy = y;
                int wz = chunk.z * 16 + z;

                for (int d = 0; d < 6; ++d) {
                    if (isVisible(world, wx, wy, wz, d, block)) {
                        float ao = 1.0f;
                        if (d == 1) ao = 0.5f;
                        else if (d != 0) ao = 0.8f;

                        uint16_t texLayer = (uint16_t)Block::textureForFace((Block::ID)block, d);
                        auto& verts = isTransparent ? mesh.transparentVertices : mesh.opaqueVertices;
                        auto& idxs = isTransparent ? mesh.transparentIndices : mesh.opaqueIndices;

                        uint32_t baseIdx = (uint32_t)(verts.size() / 8);
                        for (int v = 0; v < 4; ++v) {
                            verts.push_back(wx + faceVerts[d][v].x);
                            verts.push_back(wy + faceVerts[d][v].y);
                            verts.push_back(wz + faceVerts[d][v].z);
                            verts.push_back(uvs[v].x);
                            verts.push_back(uvs[v].y);
                            verts.push_back(static_cast<float>(texLayer));
                            verts.push_back(ao);
                            verts.push_back(0.0f);
                        }
                        idxs.push_back(baseIdx + 0);
                        idxs.push_back(baseIdx + 1);
                        idxs.push_back(baseIdx + 2);
                        idxs.push_back(baseIdx + 2);
                        idxs.push_back(baseIdx + 3);
                        idxs.push_back(baseIdx + 0);
                    }
                }
            }
        }
    }
    mesh.ready = true;
    return mesh;
}
