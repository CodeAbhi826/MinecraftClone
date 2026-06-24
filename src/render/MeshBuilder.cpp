#include "MeshBuilder.h"
#include "../core/Block.h"
#include "../core/Math.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

using namespace glm;

namespace {
    const ivec3 dirs[6] = {
        {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}
    };

    const vec3 faceVerts[6][4] = {
        {{0,1,0}, {1,1,0}, {1,1,1}, {0,1,1}},
        {{0,0,1}, {1,0,1}, {1,0,0}, {0,0,0}},
        {{0,1,1}, {1,1,1}, {1,0,1}, {0,0,1}},
        {{1,1,0}, {0,1,0}, {0,0,0}, {1,0,0}},
        {{1,1,1}, {1,0,1}, {1,0,0}, {1,1,0}},
        {{0,1,0}, {0,0,0}, {0,0,1}, {0,1,1}}
    };

    const vec2 uvs[4] = {
        {0,1}, {1,1}, {1,0}, {0,0}
    };
}

bool MeshBuilder::isVisible(const World& world, int wx, int wy, int wz, int dir) {
    ivec3 n = ivec3(wx, wy, wz) + dirs[dir];
    BlockStateID id = world.getBlock(n.x, n.y, n.z);
    return id == (uint16_t)Block::ID::air || !Block::getProperties((Block::ID)id).opaque;
}

ChunkMesh MeshBuilder::build(const Chunk& chunk, const World& world) {
    ChunkMesh mesh;

    for (int y = -64; y < 320; ++y) {
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                BlockStateID block = chunk.getBlock(x, y, z);
                if (block == (uint16_t)Block::ID::air) continue;

                int wx = chunk.x * 16 + x;
                int wy = y;
                int wz = chunk.z * 16 + z;

                for (int d = 0; d < 6; ++d) {
                    if (isVisible(world, wx, wy, wz, d)) {
                        uint16_t texLayer = block;
                        uint32_t baseIdx = (uint32_t)(mesh.vertices.size() / 8);
                        for (int v = 0; v < 4; ++v) {
                            mesh.vertices.push_back(wx + faceVerts[d][v].x);
                            mesh.vertices.push_back(wy + faceVerts[d][v].y);
                            mesh.vertices.push_back(wz + faceVerts[d][v].z);
                            mesh.vertices.push_back(uvs[v].x);
                            mesh.vertices.push_back(uvs[v].y);
                            mesh.vertices.push_back(static_cast<float>(texLayer));
                            mesh.vertices.push_back(1.0f);
                            mesh.vertices.push_back(0.0f);
                        }
                        mesh.indices.push_back(baseIdx + 0);
                        mesh.indices.push_back(baseIdx + 1);
                        mesh.indices.push_back(baseIdx + 2);
                        mesh.indices.push_back(baseIdx + 2);
                        mesh.indices.push_back(baseIdx + 3);
                        mesh.indices.push_back(baseIdx + 0);
                    }
                }
            }
        }
    }
    mesh.ready = true;
    return mesh;
}
