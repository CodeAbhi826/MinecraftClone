#include "World.h"
#include <iostream>

World::World(uint64_t seed) : generator(seed),
    genPool(std::thread::hardware_concurrency() - 1),
    meshPool(std::thread::hardware_concurrency() - 2) {}

World::~World() = default;

Chunk* World::getChunk(int cx, int cz) {
    std::shared_lock lock(chunkMutex);
    auto it = chunks.find(chunkKey(cx, cz));
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

const Chunk* World::getChunk(int cx, int cz) const {
    std::shared_lock lock(chunkMutex);
    auto it = chunks.find(chunkKey(cx, cz));
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

BlockStateID World::getBlock(int x, int y, int z) const {
    int cx = x >> 4, cz = z >> 4;
    int bx = x & 15, bz = z & 15;
    const Chunk* c = getChunk(cx, cz);
    if (!c) return (uint16_t)Block::ID::air;
    return c->getBlock(bx, y, bz);
}

void World::setBlock(int x, int y, int z, BlockStateID s) {
    int cx = x >> 4, cz = z >> 4;
    int bx = x & 15, bz = z & 15;
    Chunk* c = getChunk(cx, cz);
    if (c) {
        c->setBlock(bx, y, bz, s);
        c->sections[(y+64)/16].dirty = true;
    }
}

void World::loadChunkAsync(int cx, int cz) {
    auto key = chunkKey(cx, cz);
    {
        std::unique_lock lock(chunkMutex);
        if (chunks.contains(key)) return;
        auto c = std::make_unique<Chunk>(cx, cz);
        c->state = Chunk::Generating;
        chunks[key] = std::move(c);
    }
    Chunk* raw = chunks[key].get();
    genPool.enqueue([this, raw] {
        generator.generate(*raw);
        lightChunk(raw);
        meshPool.enqueue([this, raw] { meshChunk(raw); });
    });
}

void World::updatePlayerPosition(int px, int pz, int dist) {
    int newCX = px >> 4, newCZ = pz >> 4;
    if (newCX == centerX && newCZ == centerZ && dist == radius) return;
    centerX = newCX; centerZ = newCZ; radius = dist;
    std::vector<int64_t> remove;
    {
        std::unique_lock lock(chunkMutex);
        for (auto& [k, c] : chunks) {
            if (std::abs(c->x - centerX) > radius + 2 || std::abs(c->z - centerZ) > radius + 2) {
                c->state = Chunk::Unloading;
                remove.push_back(k);
            }
        }
        for (auto k : remove) chunks.erase(k);
    }
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz)
            loadChunkAsync(centerX + dx, centerZ + dz);
}

void World::lightChunk(Chunk* chunk) {
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            int h = chunk->heightmap[x + z * 16];
            for (int y = h + 1; y < 320; ++y) {
                int sec = (y + 64) / 16;
                int ly = (y + 64) % 16;
                chunk->sections[sec].setSkyLight(x, ly, z, 15);
            }
        }
    }
    chunk->state = Chunk::Lit;
}

void World::meshChunk(Chunk* chunk) {
    chunk->state = Chunk::Ready;
}

std::vector<Chunk*> World::getReadyChunks() {
    std::shared_lock lock(chunkMutex);
    std::vector<Chunk*> ready;
    for (auto& [k, c] : chunks) if (c->state == Chunk::Ready) ready.push_back(c.get());
    return ready;
}
