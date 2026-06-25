#include "World.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>

World::World(uint64_t seed) : generator(seed),
    genPool(std::max(1u, std::thread::hardware_concurrency() > 1
        ? std::thread::hardware_concurrency() - 1 : 1u)),
    meshPool(std::max(1u, std::thread::hardware_concurrency() / 2)) {
}

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
    if (y < -64 || y >= 320) return;
    int cx = x >> 4, cz = z >> 4;
    int bx = x & 15, bz = z & 15;
    Chunk* c = getChunk(cx, cz);
    if (c) {
        c->setBlock(bx, y, bz, s);
        c->sections[(y+64)/16].dirty = true;
        markChunkDirty(cx, cz);
        if (bx == 0) markChunkDirty(cx - 1, cz);
        if (bx == 15) markChunkDirty(cx + 1, cz);
        if (bz == 0) markChunkDirty(cx, cz - 1);
        if (bz == 15) markChunkDirty(cx, cz + 1);
    }
}

void World::markChunkDirty(int cx, int cz) {
    Chunk* c = getChunk(cx, cz);
    if (c) {
        Chunk::State expected = Chunk::Ready;
        if (c->state.compare_exchange_strong(expected, Chunk::Dirty)) {
            std::shared_ptr<Chunk> sp;
            {
                std::shared_lock lock(chunkMutex);
                auto it = chunks.find(chunkKey(cx, cz));
                if (it != chunks.end()) sp = it->second;
            }
            if (sp) {
                meshPool.enqueue([this, sp] {
                    ChunkMesh mesh = MeshBuilder::build(*sp, *this);
                    {
                        std::lock_guard lk(uploadMutex);
                        pendingUploads.push_back({sp->x, sp->z, std::move(mesh)});
                    }
                    sp->state = Chunk::Ready;
                });
            }
        }
    }
}

void World::loadChunkAsync(int cx, int cz) {
    auto key = chunkKey(cx, cz);
    std::shared_ptr<Chunk> raw;
    {
        std::unique_lock lock(chunkMutex);
        if (chunks.contains(key)) return;
        raw = std::make_shared<Chunk>(cx, cz);
        raw->state = Chunk::Generating;
        chunks[key] = raw;
    }
    genPool.enqueue([this, raw] {
        generator.generate(*raw);
        raw->state = Chunk::Lit;

        meshPool.enqueue([this, raw] {
            ChunkMesh mesh = MeshBuilder::build(*raw, *this);
            {
                std::lock_guard lk(uploadMutex);
                pendingUploads.push_back({raw->x, raw->z, std::move(mesh)});
            }
            raw->state = Chunk::Ready;
        });
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
            Chunk::State s = c->state.load();
            bool far = std::abs(c->x - centerX) > radius + 1 || std::abs(c->z - centerZ) > radius + 1;
            if (far && (s == Chunk::Ready || s == Chunk::Empty)) {
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

bool World::isChunkLoaded(int cx, int cz) const {
    std::shared_lock lock(chunkMutex);
    return chunks.contains(chunkKey(cx, cz));
}

bool World::isChunkReady(int cx, int cz) const {
    std::shared_lock lock(chunkMutex);
    auto it = chunks.find(chunkKey(cx, cz));
    return it != chunks.end() && it->second->state == Chunk::Ready;
}

void World::ensureChunkLoaded(int cx, int cz) {
    std::shared_lock lock(chunkMutex);
    if (!chunks.contains(chunkKey(cx, cz))) {
        lock.unlock();
        loadChunkAsync(cx, cz);
    }
}

void World::processChunkLoading() {
    std::this_thread::yield();
}

std::vector<World::PendingUpload> World::drainUploads() {
    std::lock_guard lk(uploadMutex);
    std::vector<PendingUpload> out;
    out.swap(pendingUploads);
    return out;
}
