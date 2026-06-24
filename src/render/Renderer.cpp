#include "Renderer.h"
#include "../core/Block.h"
#include <iostream>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

static const char* worldVertSrc = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in float aTexLayer;
layout(location=3) in float aAO;
out vec2 vTexCoord;
out float vTexLayer;
out float vAO;
out vec3 vWorldPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
    vTexLayer = aTexLayer;
    vAO = aAO;
    vWorldPos = aPos;
}
)";

static const char* worldFragSrc = R"(
#version 460 core
in vec2 vTexCoord;
in float vTexLayer;
in float vAO;
in vec3 vWorldPos;
out vec4 FragColor;
uniform sampler2DArray uTexArray;
uniform vec3 uCamPos;
uniform vec4 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
void main() {
    vec4 col = texture(uTexArray, vec3(vTexCoord, vTexLayer));
    col.rgb *= vAO;
    float dist = length(vWorldPos - uCamPos);
    float fogFactor = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    FragColor = mix(col, uFogColor, fogFactor);
}
)";

static const char* uiVertSrc = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* uiFragSrc = R"(
#version 460 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vTexCoord) * uColor;
}
)";

static const char* lineVertSrc = R"(
#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)";
static const char* lineFragSrc = R"(
#version 460 core
uniform vec4 uColor;
out vec4 FragColor;
void main(){ FragColor = uColor; }
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
    return s;
}

static GLuint createProgram(const char* vs, const char* fs) {
    GLuint vsObj = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsObj = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vsObj);
    glAttachShader(program, fsObj);
    glLinkProgram(program);
    int ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, 1024, nullptr, log);
        std::cerr << "Program link error: " << log << std::endl;
    }
    glDeleteShader(vsObj);
    glDeleteShader(fsObj);
    return program;
}

Renderer::Renderer(int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, "VoxelCraft", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);

    m_worldProgram = createProgram(worldVertSrc, worldFragSrc);
    m_uiProgram = createProgram(uiVertSrc, uiFragSrc);
    m_lineProgram = createProgram(lineVertSrc, lineFragSrc);

    m_textureAtlas.generateProcedural();

    glUseProgram(m_uiProgram);
    glUniform1i(glGetUniformLocation(m_uiProgram, "uTexture"), 0);
    glUseProgram(0);

    uint8_t white[4] = {255,255,255,255};
    glCreateTextures(GL_TEXTURE_2D, 1, &m_whiteTex);
    glTextureStorage2D(m_whiteTex, 1, GL_RGBA8, 1, 1);
    glTextureSubImage2D(m_whiteTex, 0,0,0, 1,1, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTextureParameteri(m_whiteTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_whiteTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    initCrosshair();
    initHighlight();
    initFont();
    initTextVAO();
    initHotbar();
}

Renderer::~Renderer() {
    for (auto& [k, cgl] : chunkMeshes) {
        for (int i = 0; i < 2; ++i) {
            glDeleteVertexArrays(1, &cgl.vao[i]);
            glDeleteBuffers(1, &cgl.vbo[i]);
            glDeleteBuffers(1, &cgl.ebo[i]);
        }
    }
    glDeleteVertexArrays(1, &m_crosshairVAO);
    glDeleteBuffers(1, &m_crosshairVBO);
    glDeleteVertexArrays(1, &m_highlightVAO);
    glDeleteBuffers(1, &m_highlightVBO);
    glDeleteBuffers(1, &m_highlightEBO);
    glDeleteVertexArrays(1, &m_hotbarVAO);
    glDeleteBuffers(1, &m_hotbarVBO);
    glDeleteProgram(m_lineProgram);
    glDeleteTextures(1, &m_whiteTex);
    glDeleteTextures(1, &m_fontTex);
    glDeleteVertexArrays(1, &m_textVAO);
    glDeleteBuffers(1, &m_textVBO);
    glDeleteProgram(m_worldProgram);
    glDeleteProgram(m_uiProgram);
    glfwTerminate();
}

void Renderer::beginFrame(const glm::mat4& projView) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_worldProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_worldProgram, "uMVP"), 1, GL_FALSE, &projView[0][0]);
    m_textureAtlas.bind(0);
    m_prevProjView = projView;
}

void Renderer::uploadChunkMesh(int cx, int cz, const ChunkMesh& mesh) {
    int64_t key = (int64_t(cx) << 32) | uint32_t(cz);
    auto& cgl = chunkMeshes[key];

    for (int i = 0; i < 2; ++i) {
        if (cgl.vao[i]) {
            glDeleteVertexArrays(1, &cgl.vao[i]);
            glDeleteBuffers(1, &cgl.vbo[i]);
            glDeleteBuffers(1, &cgl.ebo[i]);
        }
        const auto& verts = (i == 0) ? mesh.opaqueVertices : mesh.transparentVertices;
        const auto& idxs = (i == 0) ? mesh.opaqueIndices : mesh.transparentIndices;
        if (verts.empty()) { cgl.indexCount[i] = 0; continue; }

        glCreateVertexArrays(1, &cgl.vao[i]);
        glCreateBuffers(1, &cgl.vbo[i]);
        glCreateBuffers(1, &cgl.ebo[i]);
        glNamedBufferData(cgl.vbo[i], verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glNamedBufferData(cgl.ebo[i], idxs.size() * sizeof(uint32_t), idxs.data(), GL_STATIC_DRAW);

        glEnableVertexArrayAttrib(cgl.vao[i], 0);
        glVertexArrayAttribFormat(cgl.vao[i], 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(cgl.vao[i], 0, 0);
        glEnableVertexArrayAttrib(cgl.vao[i], 1);
        glVertexArrayAttribFormat(cgl.vao[i], 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
        glVertexArrayAttribBinding(cgl.vao[i], 1, 0);
        glEnableVertexArrayAttrib(cgl.vao[i], 2);
        glVertexArrayAttribFormat(cgl.vao[i], 2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float));
        glVertexArrayAttribBinding(cgl.vao[i], 2, 0);
        glEnableVertexArrayAttrib(cgl.vao[i], 3);
        glVertexArrayAttribFormat(cgl.vao[i], 3, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
        glVertexArrayAttribBinding(cgl.vao[i], 3, 0);

        glVertexArrayVertexBuffer(cgl.vao[i], 0, cgl.vbo[i], 0, 8 * sizeof(float));
        glVertexArrayElementBuffer(cgl.vao[i], cgl.ebo[i]);
        cgl.indexCount[i] = (int)idxs.size();
    }
}

void Renderer::renderChunks(const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(m_worldProgram);
    m_textureAtlas.bind(0);

    glUniform3f(glGetUniformLocation(m_worldProgram, "uCamPos"), m_camPos.x, m_camPos.y, m_camPos.z);
    glUniform4f(glGetUniformLocation(m_worldProgram, "uFogColor"), 0.5f, 0.7f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(m_worldProgram, "uFogStart"), 16.0f * 6.0f);
    glUniform1f(glGetUniformLocation(m_worldProgram, "uFogEnd"), 16.0f * 12.0f);

    glDisable(GL_BLEND);
    for (auto& [key, cgl] : chunkMeshes) {
        if (cgl.indexCount[0] == 0) continue;
        glBindVertexArray(cgl.vao[0]);
        glDrawElements(GL_TRIANGLES, cgl.indexCount[0], GL_UNSIGNED_INT, nullptr);
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    for (auto& [key, cgl] : chunkMeshes) {
        if (cgl.indexCount[1] == 0) continue;
        glBindVertexArray(cgl.vao[1]);
        glDrawElements(GL_TRIANGLES, cgl.indexCount[1], GL_UNSIGNED_INT, nullptr);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::initCrosshair() {
    float size = 0.02f, gap = 0.005f;
    float verts[] = {
        -size-gap, 0, 0,   -gap, 0, 0,
         gap, 0, 0,         size+gap, 0, 0,
         0, -size-gap, 0,   0, -gap, 0,
         0, gap, 0,        0, size+gap, 0
    };
    glCreateVertexArrays(1, &m_crosshairVAO);
    glCreateBuffers(1, &m_crosshairVBO);
    glNamedBufferData(m_crosshairVBO, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexArrayAttrib(m_crosshairVAO, 0);
    glVertexArrayAttribFormat(m_crosshairVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_crosshairVAO, 0, 0);
    glVertexArrayVertexBuffer(m_crosshairVAO, 0, m_crosshairVBO, 0, 3 * sizeof(float));
}

void Renderer::initHighlight() {
    float verts[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
         0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
         0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    unsigned int indices[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };
    glCreateVertexArrays(1, &m_highlightVAO);
    glCreateBuffers(1, &m_highlightVBO);
    glCreateBuffers(1, &m_highlightEBO);
    glNamedBufferData(m_highlightVBO, sizeof(verts), verts, GL_STATIC_DRAW);
    glNamedBufferData(m_highlightEBO, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexArrayAttrib(m_highlightVAO, 0);
    glVertexArrayAttribFormat(m_highlightVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_highlightVAO, 0, 0);
    glVertexArrayVertexBuffer(m_highlightVAO, 0, m_highlightVBO, 0, 3 * sizeof(float));
    glVertexArrayElementBuffer(m_highlightVAO, m_highlightEBO);
}

void Renderer::initFont() {
    static const uint8_t G[15][7] = {
        {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110},
        {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110},
        {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111},
        {0b01110,0b10001,0b00001,0b00110,0b00001,0b10001,0b01110},
        {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010},
        {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110},
        {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110},
        {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000},
        {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110},
        {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100},
        {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000},
        {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000},
        {0b01110,0b10001,0b10000,0b01110,0b00001,0b10001,0b01110},
        {0b00000,0b00100,0b00000,0b00000,0b00000,0b00100,0b00000},
        {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000},
    };
    const int GW=5, GH=7, N=15, texW=N*GW, texH=GH;
    std::vector<uint8_t> data((size_t)texW*texH*4, 0);
    for (int gy=0; gy<GH; ++gy) {
        int row = (GH-1) - gy;
        for (int g=0; g<N; ++g)
            for (int gx=0; gx<GW; ++gx) {
                bool on = (G[g][gy] >> (4-gx)) & 1;
                size_t idx = ((size_t)row*texW + (g*GW+gx))*4;
                uint8_t v = on?255:0;
                data[idx+0]=v; data[idx+1]=v; data[idx+2]=v; data[idx+3]=v;
            }
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &m_fontTex);
    glTextureStorage2D(m_fontTex, 1, GL_RGBA8, texW, texH);
    glTextureSubImage2D(m_fontTex, 0,0,0, texW,texH, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTextureParameteri(m_fontTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_fontTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_fontTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_fontTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Renderer::initTextVAO() {
    glCreateVertexArrays(1, &m_textVAO);
    glCreateBuffers(1, &m_textVBO);
    glEnableVertexArrayAttrib(m_textVAO, 0);
    glVertexArrayAttribFormat(m_textVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_textVAO, 0, 0);
    glEnableVertexArrayAttrib(m_textVAO, 1);
    glVertexArrayAttribFormat(m_textVAO, 1, 2, GL_FLOAT, GL_FALSE, 3*sizeof(float));
    glVertexArrayAttribBinding(m_textVAO, 1, 0);
    glVertexArrayVertexBuffer(m_textVAO, 0, m_textVBO, 0, 5*sizeof(float));
}

int Renderer::glyphIndex(char c) const {
    if (c>='0'&&c<='9') return c-'0';
    if (c=='F') return 10;
    if (c=='P') return 11;
    if (c=='S') return 12;
    if (c==':') return 13;
    if (c==' ') return 14;
    return -1;
}

void Renderer::initHotbar() {
    float boxW = 0.08f;
    float boxH = 0.12f;
    float gap = 0.01f;
    float startX = -((9 * boxW + 8 * gap) / 2.0f);
    float y = -0.95f;

    std::vector<float> verts;
    for (int i = 0; i < 9; ++i) {
        float x = startX + i * (boxW + gap);
        float u = 0.0f, v = 0.0f;
        verts.insert(verts.end(), {
            x, y, 0.0f, u, v,
            x + boxW, y, 0.0f, u, v,
            x + boxW, y + boxH, 0.0f, u, v,

            x, y, 0.0f, u, v,
            x + boxW, y + boxH, 0.0f, u, v,
            x, y + boxH, 0.0f, u, v
        });
    }

    glCreateVertexArrays(1, &m_hotbarVAO);
    glCreateBuffers(1, &m_hotbarVBO);
    glNamedBufferData(m_hotbarVBO, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glEnableVertexArrayAttrib(m_hotbarVAO, 0);
    glVertexArrayAttribFormat(m_hotbarVAO, 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float));
    glVertexArrayAttribBinding(m_hotbarVAO, 0, 0);

    glEnableVertexArrayAttrib(m_hotbarVAO, 1);
    glVertexArrayAttribFormat(m_hotbarVAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(m_hotbarVAO, 1, 0);

    glVertexArrayVertexBuffer(m_hotbarVAO, 0, m_hotbarVBO, 0, 5 * sizeof(float));
}

void Renderer::renderHotbar(int selectedSlot) {
    glUseProgram(m_uiProgram);
    glBindTextureUnit(0, m_whiteTex);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(m_hotbarVAO);

    for (int i = 0; i < 9; ++i) {
        if (i == selectedSlot) {
            glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), 1.0f, 1.0f, 1.0f, 0.8f);
        } else {
            glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), 0.2f, 0.2f, 0.2f, 0.6f);
        }
        glDrawArrays(GL_TRIANGLES, i * 6, 6);
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

void Renderer::renderCrosshair() {
    glUseProgram(m_uiProgram);
    glBindTextureUnit(0, m_whiteTex);
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), 1, 1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_crosshairVAO);
    glDrawArrays(GL_LINES, 0, 8);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

void Renderer::renderBlockHighlight(const glm::ivec3& blockPos) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPos));
    glm::mat4 mvp = m_prevProjView * model;
    glUseProgram(m_lineProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_lineProgram,"uMVP"),1,GL_FALSE,&mvp[0][0]);
    glUniform4f(glGetUniformLocation(m_lineProgram,"uColor"), 0.0f,0.0f,0.0f,1.0f);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(m_highlightVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::renderText(const std::string& text, float ndcX, float ndcY, float scaleH) {
    const float GW=5.0f, GH=7.0f, N=15.0f;
    float gh = scaleH;
    float gw = scaleH * (GW/GH);
    float adv = gw * 1.15f;
    static std::vector<float> v;
    v.clear();
    float curX = ndcX;
    for (char c : text) {
        int gi = glyphIndex(c);
        if (gi < 0) { curX += adv; continue; }
        float u0 = (gi*GW)/(N*GW), u1 = ((gi+1)*GW)/(N*GW);
        float top = ndcY, bot = ndcY - gh;
        float q[6][5] = {
            {curX,    top, 0, u0, 1.0f},
            {curX+gw, top, 0, u1, 1.0f},
            {curX+gw, bot, 0, u1, 0.0f},
            {curX,    top, 0, u0, 1.0f},
            {curX+gw, bot, 0, u1, 0.0f},
            {curX,    bot, 0, u0, 0.0f},
        };
        for (int i=0;i<6;++i) for (int j=0;j<5;++j) v.push_back(q[i][j]);
        curX += adv;
    }
    if (v.empty()) return;
    glNamedBufferData(m_textVBO, v.size()*sizeof(float), v.data(), GL_DYNAMIC_DRAW);
    glUseProgram(m_uiProgram);
    glBindTextureUnit(0, m_fontTex);
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), 1,1,1,1);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_textVAO);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size()/5));
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
}
