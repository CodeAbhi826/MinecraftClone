#pragma once
#include <cstdint>
#include <array>

using BlockStateID = uint16_t;

namespace Block {
    enum class ID : uint16_t {
        air = 0,
        stone,
        grass_block,
        dirt,
        cobblestone,
        wood,
        leaves,
        sand,
        water,
        glass,
        bedrock,
        COUNT
    };

    struct Properties {
        bool opaque = true;
        bool solid = true;
        bool liquid = false;
    };

    inline const std::array<Properties, (size_t)ID::COUNT>& getAllProperties() {
        static std::array<Properties, (size_t)ID::COUNT> props = []{
            std::array<Properties, (size_t)ID::COUNT> p;
            p[(int)ID::air] = {false, false, false};
            p[(int)ID::stone] = {true, true, false};
            p[(int)ID::grass_block] = {true, true, false};
            p[(int)ID::dirt] = {true, true, false};
            p[(int)ID::cobblestone] = {true, true, false};
            p[(int)ID::wood] = {true, true, false};
            p[(int)ID::leaves] = {false, true, false};
            p[(int)ID::sand] = {true, true, false};
            p[(int)ID::water] = {false, false, true};
            p[(int)ID::glass] = {false, true, false};
            p[(int)ID::bedrock] = {true, true, false};
            return p;
        }();
        return props;
    }

    inline const Properties& getProperties(ID id) {
        return getAllProperties()[(size_t)id];
    }

    inline ID textureForFace(ID block, int faceDir) {
        if (block == ID::grass_block) {
            if (faceDir == 0) return ID::grass_block;
            if (faceDir == 1) return ID::dirt;
            return ID::dirt;
        }
        return block;
    }
}
