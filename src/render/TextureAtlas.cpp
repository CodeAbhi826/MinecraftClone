#include "TextureAtlas.h"
#include "../core/Block.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <algorithm>

static uint8_t noise(int x, int y, int seed) {
    unsigned n = (unsigned)(x + y * 57 + seed * 137);
    n = (n << 13) ^ n;
    return (unsigned char)((n * (n * n * 60493u + 19990303u) + 1376312589u) & 0xff);
}

TextureAtlas::~TextureAtlas() {
    if (m_textureId) glDeleteTextures(1, &m_textureId);
}

void TextureAtlas::generateProcedural() {
    m_textureSize = 16;
    m_layerCount = (int)Block::ID::COUNT;

    int texBytes = m_layerCount * m_textureSize * m_textureSize * 4;
    std::vector<uint8_t> data(texBytes, 0);

    for (int layer = 0; layer < m_layerCount; ++layer) {
        Block::ID id = static_cast<Block::ID>(layer);
        if (id == Block::ID::air) continue;

        const auto& props = Block::getProperties(id);
        glm::u8vec3 base(128, 128, 128);
        switch (id) {
            case Block::ID::stone:        base = glm::u8vec3(127,127,127); break;
            case Block::ID::grass_block:  base = glm::u8vec3(92,148,52); break;
            case Block::ID::dirt:          base = glm::u8vec3(139,115,85); break;
            case Block::ID::cobblestone:   base = glm::u8vec3(100,100,100); break;
            case Block::ID::wood:          base = glm::u8vec3(160,120,80); break;
            case Block::ID::leaves:        base = glm::u8vec3(40,110,30); break;
            case Block::ID::sand:          base = glm::u8vec3(219,207,163); break;
            case Block::ID::water:         base = glm::u8vec3(50,80,200); break;
            case Block::ID::glass:         base = glm::u8vec3(200,220,240); break;
            case Block::ID::bedrock:       base = glm::u8vec3(40,40,40); break;
            default:                       base = glm::u8vec3((layer*80)%255, (layer*130)%255, (layer*200)%255);
        }

        for (int y = 0; y < m_textureSize; ++y) {
            for (int x = 0; x < m_textureSize; ++x) {
                size_t idx = ((layer * m_textureSize + y) * m_textureSize + x) * 4;
                int n = noise(x, y, layer) % 20 - 10;
                data[idx + 0] = (uint8_t)std::clamp((int)base.r + n, 0, 255);
                data[idx + 1] = (uint8_t)std::clamp((int)base.g + n, 0, 255);
                data[idx + 2] = (uint8_t)std::clamp((int)base.b + n, 0, 255);
                data[idx + 3] = props.opaque ? 255 : 128;
            }
        }
    }

    int mipLevels = 1;
    while ((m_textureSize >> mipLevels) > 1) ++mipLevels;

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_textureId);
    glTextureStorage3D(m_textureId, mipLevels, GL_RGBA8, m_textureSize, m_textureSize, m_layerCount);
    glTextureSubImage3D(m_textureId, 0, 0, 0, 0,
                        m_textureSize, m_textureSize, m_layerCount,
                        GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glGenerateTextureMipmap(m_textureId);
    glTextureParameteri(m_textureId, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTextureParameteri(m_textureId, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
if (GLAD_GL_ARB_texture_filter_anisotropic)
    glTextureParameterf(m_textureId, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    m_loaded = true;
}

void TextureAtlas::bind(int unit) const {
    glBindTextureUnit(unit, m_textureId);
}
