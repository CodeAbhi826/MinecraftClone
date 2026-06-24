#pragma once
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <thread>
#include "Chunk.h"
#include "../core/ThreadPool.h"
#include "WorldGenerator.h"
#include "../render/MeshBuilder.h"

class World {
public:
    World(uint64_t seed);
    ~World();
    Chunk* getChunk(int cx, int cz);
    const Chunk* getChunk(int cx, int cz) const;
    BlockStateID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockStateID state);
    void updatePlayerPosition(int px, int pz, int renderDistance);
    void loadChunkAsync(int cx, int cz);
    void markChunkDirty(int cx, int cz);

    struct PendingUpload { int cx, cz; ChunkMesh mesh; };
    std::vector<PendingUpload> drainUploads();

    WorldGenerator generator;
private:
    std::unordered_map<int64_t, std::shared_ptr<Chunk>> chunks;
    mutable std::shared_mutex chunkMutex;
    ThreadPool genPool;
    ThreadPool meshPool;

    std::mutex uploadMutex;
    std::vector<PendingUpload> pendingUploads;

    int centerX = 0, centerZ = 0, radius = -999;
    static int64_t chunkKey(int cx, int cz) { return (int64_t(cx) << 32) | uint32_t(cz); }
};
