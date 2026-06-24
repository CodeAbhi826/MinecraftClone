#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include "../core/Block.h"

class ChunkSection {
public:
    static constexpr int kSize = 16;
    static constexpr int kVolume = kSize * kSize * kSize;

    ChunkSection();
    BlockStateID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockStateID state);

    uint8_t getBlockLight(int x, int y, int z) const;
    void setBlockLight(int x, int y, int z, uint8_t light);
    uint8_t getSkyLight(int x, int y, int z) const;
    void setSkyLight(int x, int y, int z, uint8_t light);

    bool dirty = true;
    std::vector<BlockStateID> palette;
    int bitsPerBlock = 4;
    std::vector<uint64_t> data;

    const std::vector<uint64_t>& getData() const { return data; }
    const std::vector<BlockStateID>& getPalette() const { return palette; }

private:
    uint8_t blockLight[kVolume / 2] = {};
    uint8_t skyLight[kVolume / 2] = {};
    int getPaletteIndex(BlockStateID state) const;
    void addToPalette(BlockStateID state);
    void resizePalette(int newBits);
    void ensureGuardWord();
};

inline ChunkSection::ChunkSection() {
    data.resize((kVolume * 4 + 63) / 64 + 1, 0);
    palette.push_back(0);
}
inline int ChunkSection::getPaletteIndex(BlockStateID state) const {
    auto it = std::find(palette.begin(), palette.end(), state);
    return (it != palette.end()) ? (int)(it - palette.begin()) : -1;
}
inline void ChunkSection::addToPalette(BlockStateID state) {
    palette.push_back(state);
    if (palette.size() > (1u << bitsPerBlock))
        resizePalette(bitsPerBlock + 1);
}
inline void ChunkSection::ensureGuardWord() {
    size_t needed = (kVolume * bitsPerBlock + 63) / 64 + 1;
    if (data.size() < needed) data.resize(needed, 0);
}

inline void ChunkSection::resizePalette(int newBits) {
    int oldBits = bitsPerBlock;
    bitsPerBlock = newBits;
    ensureGuardWord();
    std::vector<uint64_t> newData((kVolume * bitsPerBlock + 63) / 64 + 1, 0);
    for (int i = 0; i < kVolume; ++i) {
        int oldIndex = (i * oldBits);
        int oldWord = oldIndex >> 6;
        int oldOff = oldIndex & 63;
        uint64_t oldMask = (1ULL << oldBits) - 1;
        int value = (data[oldWord] >> oldOff) & oldMask;
        if (oldOff + oldBits > 64) {
            int bitsNext = oldOff + oldBits - 64;
            value |= (data[oldWord + 1] & ((1ULL << bitsNext) - 1)) << (64 - oldOff);
        }
        int newIndex = i * bitsPerBlock;
        int newWord = newIndex >> 6;
        int newOff = newIndex & 63;
        newData[newWord] |= (uint64_t)value << newOff;
        if (newOff + bitsPerBlock > 64) {
            int bitsNext = newOff + bitsPerBlock - 64;
            newData[newWord + 1] |= (value >> (64 - newOff)) & ((1ULL << bitsNext) - 1);
        }
    }
    data = std::move(newData);
}
inline BlockStateID ChunkSection::getBlock(int x, int y, int z) const {
    int idx = (y * kSize + z) * kSize + x;
    if (bitsPerBlock == 0) return palette[0];
    int bitIndex = idx * bitsPerBlock;
    int word = bitIndex >> 6;
    int off = bitIndex & 63;
    uint64_t mask = (1ULL << bitsPerBlock) - 1;
    int palIdx = (data[word] >> off) & mask;
    if (off + bitsPerBlock > 64) {
        int bitsNext = off + bitsPerBlock - 64;
        palIdx |= (data[word + 1] & ((1ULL << bitsNext) - 1)) << (64 - off);
    }
    return palette[palIdx];
}
inline void ChunkSection::setBlock(int x, int y, int z, BlockStateID state) {
    int idx = (y * kSize + z) * kSize + x;
    int palIdx = getPaletteIndex(state);
    if (palIdx == -1) {
        addToPalette(state);
        palIdx = (int)palette.size() - 1;
    }
    if (bitsPerBlock == 0) {
        palette[0] = state;
        dirty = true;
        return;
    }
    int bitIndex = idx * bitsPerBlock;
    int word = bitIndex >> 6;
    int off = bitIndex & 63;
    uint64_t mask = (1ULL << bitsPerBlock) - 1;
    data[word] &= ~(mask << off);
    data[word] |= (uint64_t(palIdx) & mask) << off;
    if (off + bitsPerBlock > 64) {
        int bitsNext = off + bitsPerBlock - 64;
        data[word + 1] &= ~((1ULL << bitsNext) - 1);
        data[word + 1] |= (palIdx >> (64 - off)) & ((1ULL << bitsNext) - 1);
    }
    dirty = true;
}
inline uint8_t ChunkSection::getBlockLight(int x, int y, int z) const {
    int idx = (y * kSize + z) * kSize + x;
    int byteIdx = idx / 2;
    int nibble = (idx % 2 == 0) ? (blockLight[byteIdx] & 0x0F) : (blockLight[byteIdx] >> 4);
    return nibble;
}
inline void ChunkSection::setBlockLight(int x, int y, int z, uint8_t light) {
    int idx = (y * kSize + z) * kSize + x;
    int byteIdx = idx / 2;
    if (idx % 2 == 0) blockLight[byteIdx] = (blockLight[byteIdx] & 0xF0) | (light & 0x0F);
    else               blockLight[byteIdx] = (blockLight[byteIdx] & 0x0F) | ((light & 0x0F) << 4);
}
inline uint8_t ChunkSection::getSkyLight(int x, int y, int z) const {
    int idx = (y * kSize + z) * kSize + x;
    int byteIdx = idx / 2;
    int nibble = (idx % 2 == 0) ? (skyLight[byteIdx] & 0x0F) : (skyLight[byteIdx] >> 4);
    return nibble;
}
inline void ChunkSection::setSkyLight(int x, int y, int z, uint8_t light) {
    int idx = (y * kSize + z) * kSize + x;
    int byteIdx = idx / 2;
    if (idx % 2 == 0) skyLight[byteIdx] = (skyLight[byteIdx] & 0xF0) | (light & 0x0F);
    else               skyLight[byteIdx] = (skyLight[byteIdx] & 0x0F) | ((light & 0x0F) << 4);
}
