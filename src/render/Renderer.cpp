#include "Renderer.h"
#include "../core/Block.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
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
    if (col.a < 0.5) discard;
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
    if (!glfwInit()) {
        std::cerr << "FATAL: Failed to initialize GLFW" << std::endl;
        std::exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    window = glfwCreateWindow(width, height, "VoxelCraft", nullptr, nullptr);
    if (!window) {
        const char* err = nullptr;
        glfwGetError(&err);
        std::cerr << "FATAL: Failed to create GLFW window: " << (err ? err : "unknown error") << std::endl;
        std::cerr << "  Required: OpenGL 4.6 core profile." << std::endl;
        std::cerr << "  Check: GPU drivers installed, GPU supports GL 4.6, display is connected." << std::endl;
        glfwTerminate();
        std::exit(1);
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "FATAL: Failed to load OpenGL functions via GLAD" << std::endl;
        std::exit(1);
    }
    std::cerr << "[engine] OpenGL " << glGetString(GL_VERSION) << std::endl;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glfwSwapInterval(1);

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
    initFullscreenQuad();
    glCreateVertexArrays(1, &m_panelVAO);
    glCreateBuffers(1, &m_panelVBO);
    glEnableVertexArrayAttrib(m_panelVAO, 0);
    glVertexArrayAttribFormat(m_panelVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_panelVAO, 0, 0);
    glEnableVertexArrayAttrib(m_panelVAO, 1);
    glVertexArrayAttribFormat(m_panelVAO, 1, 2, GL_FLOAT, GL_FALSE, 3*sizeof(float));
    glVertexArrayAttribBinding(m_panelVAO, 1, 0);
    glVertexArrayVertexBuffer(m_panelVAO, 0, m_panelVBO, 0, 5*sizeof(float));
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
    glDeleteVertexArrays(1, &m_panelVAO);
    glDeleteBuffers(1, &m_panelVBO);
    glDeleteProgram(m_worldProgram);
    glDeleteProgram(m_uiProgram);
    glfwTerminate();
}

void Renderer::beginFrame(const glm::mat4& projView) {
    glm::vec3 sky = skyColorForTime(timeOfDay);
    glClearColor(sky.r, sky.g, sky.b, 1.0f);
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

void Renderer::updateFrustum(const glm::mat4& view, const glm::mat4& proj) {
    glm::mat4 vp = proj * view;
    glm::mat4 m = glm::transpose(vp);
    m_frustumPlanes[0][0] = m[3][0] + m[0][0];
    m_frustumPlanes[0][1] = m[3][1] + m[0][1];
    m_frustumPlanes[0][2] = m[3][2] + m[0][2];
    m_frustumPlanes[0][3] = m[3][3] + m[0][3];
    m_frustumPlanes[1][0] = m[3][0] - m[0][0];
    m_frustumPlanes[1][1] = m[3][1] - m[0][1];
    m_frustumPlanes[1][2] = m[3][2] - m[0][2];
    m_frustumPlanes[1][3] = m[3][3] - m[0][3];
    m_frustumPlanes[2][0] = m[3][0] + m[1][0];
    m_frustumPlanes[2][1] = m[3][1] + m[1][1];
    m_frustumPlanes[2][2] = m[3][2] + m[1][2];
    m_frustumPlanes[2][3] = m[3][3] + m[1][3];
    m_frustumPlanes[3][0] = m[3][0] - m[1][0];
    m_frustumPlanes[3][1] = m[3][1] - m[1][1];
    m_frustumPlanes[3][2] = m[3][2] - m[1][2];
    m_frustumPlanes[3][3] = m[3][3] - m[1][3];
    m_frustumPlanes[4][0] = m[3][0] + m[2][0];
    m_frustumPlanes[4][1] = m[3][1] + m[2][1];
    m_frustumPlanes[4][2] = m[3][2] + m[2][2];
    m_frustumPlanes[4][3] = m[3][3] + m[2][3];
    m_frustumPlanes[5][0] = m[3][0] - m[2][0];
    m_frustumPlanes[5][1] = m[3][1] - m[2][1];
    m_frustumPlanes[5][2] = m[3][2] - m[2][2];
    m_frustumPlanes[5][3] = m[3][3] - m[2][3];
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(m_frustumPlanes[i][0]*m_frustumPlanes[i][0]
                            + m_frustumPlanes[i][1]*m_frustumPlanes[i][1]
                            + m_frustumPlanes[i][2]*m_frustumPlanes[i][2]);
        if (len > 0) for (int j = 0; j < 4; ++j) m_frustumPlanes[i][j] /= len;
    }
}

bool Renderer::chunkInFrustum(int cx, int cz) const {
    float cxPos = cx * 16.0f + 8.0f;
    float czPos = cz * 16.0f + 8.0f;
    float cyPos = 128.0f;
    float radius = 200.0f;
    for (int i = 0; i < 6; ++i) {
        float d = m_frustumPlanes[i][0] * cxPos
                + m_frustumPlanes[i][1] * cyPos
                + m_frustumPlanes[i][2] * czPos
                + m_frustumPlanes[i][3];
        if (d < -radius) return false;
    }
    return true;
}

void Renderer::renderChunks(const glm::mat4& view, const glm::mat4& proj) {
    updateFrustum(view, proj);

    glUseProgram(m_worldProgram);
    m_textureAtlas.bind(0);

    glUniform3f(glGetUniformLocation(m_worldProgram, "uCamPos"), m_camPos.x, m_camPos.y, m_camPos.z);
    glm::vec3 fog = skyColorForTime(timeOfDay);
    glUniform4f(glGetUniformLocation(m_worldProgram, "uFogColor"), fog.r, fog.g, fog.b, 1.0f);
    glUniform1f(glGetUniformLocation(m_worldProgram, "uFogStart"), 16.0f * 6.0f);
    glUniform1f(glGetUniformLocation(m_worldProgram, "uFogEnd"), 16.0f * 12.0f);

    glDisable(GL_BLEND);
    for (auto& [key, cgl] : chunkMeshes) {
        int cx = (int)(key >> 32), cz = (int)(key & 0xFFFFFFFF);
        if (!chunkInFrustum(cx, cz)) continue;
        float dx = (cx*16+8) - m_camPos.x, dz = (cz*16+8) - m_camPos.z;
        if (dx*dx + dz*dz > m_cullRadius * m_cullRadius) continue;
        if (cgl.indexCount[0] == 0) continue;
        glBindVertexArray(cgl.vao[0]);
        glDrawElements(GL_TRIANGLES, cgl.indexCount[0], GL_UNSIGNED_INT, nullptr);
    }

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    for (auto& [key, cgl] : chunkMeshes) {
        int cx = (int)(key >> 32), cz = (int)(key & 0xFFFFFFFF);
        if (!chunkInFrustum(cx, cz)) continue;
        float dx = (cx*16+8) - m_camPos.x, dz = (cz*16+8) - m_camPos.z;
        if (dx*dx + dz*dz > m_cullRadius * m_cullRadius) continue;
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
    static const uint8_t FONT5x7[64][7] = {
        {0,0,0,0,0,0,0},
        {0b00100,0b00100,0b00100,0b00100,0b00100,0b00000,0b00100},
        {0b01010,0b01010,0b01010,0b00000,0b00000,0b00000,0b00000},
        {0b01010,0b11111,0b01010,0b01010,0b11111,0b01010,0b00000},
        {0b00100,0b01111,0b10100,0b01110,0b00101,0b11110,0b00100},
        {0b11000,0b11001,0b00010,0b00100,0b01000,0b10011,0b00011},
        {0b01100,0b10010,0b10100,0b01000,0b10101,0b10010,0b01101},
        {0b00100,0b00100,0b00000,0b00000,0b00000,0b00000,0b00000},
        {0b00010,0b00100,0b01000,0b01000,0b01000,0b00100,0b00010},
        {0b01000,0b00100,0b00010,0b00010,0b00010,0b00100,0b01000},
        {0b00000,0b00100,0b10101,0b01110,0b10101,0b00100,0b00000},
        {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000},
        {0b00000,0b00000,0b00000,0b00000,0b00100,0b00100,0b01000},
        {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000},
        {0b00000,0b00000,0b00000,0b00000,0b00000,0b00110,0b00110},
        {0b00001,0b00010,0b00100,0b01000,0b10000,0b00000,0b00000},
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
        {0b00000,0b00100,0b00000,0b00000,0b00000,0b00100,0b00000},
        {0b00000,0b00100,0b00000,0b00000,0b00100,0b00100,0b01000},
        {0b00010,0b00100,0b01000,0b10000,0b01000,0b00100,0b00010},
        {0b00000,0b00000,0b11111,0b00000,0b11111,0b00000,0b00000},
        {0b01000,0b00100,0b00010,0b00001,0b00010,0b00100,0b01000},
        {0b01110,0b10001,0b00001,0b00010,0b00100,0b00000,0b00100},
        {0b01110,0b10001,0b10111,0b10101,0b10111,0b10000,0b01110},
        {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001},
        {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110},
        {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110},
        {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110},
        {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111},
        {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000},
        {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111},
        {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001},
        {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110},
        {0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100},
        {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001},
        {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111},
        {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001},
        {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001},
        {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110},
        {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000},
        {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101},
        {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001},
        {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110},
        {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100},
        {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110},
        {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100},
        {0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010},
        {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001},
        {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100},
        {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111},
        {0b01110,0b01000,0b01000,0b01000,0b01000,0b01000,0b01110},
        {0b10000,0b01000,0b00100,0b00010,0b00001,0b00000,0b00000},
        {0b01110,0b00010,0b00010,0b00010,0b00010,0b00010,0b01110},
        {0b00100,0b01010,0b10001,0b00000,0b00000,0b00000,0b00000},
        {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b11111},
    };
    const int GW=5, GH=7, N=64, texW=N*GW, texH=GH;
    std::vector<uint8_t> data((size_t)texW*texH*4, 0);
    for (int gy=0; gy<GH; ++gy) {
        int row = (GH-1) - gy;
        for (int g=0; g<N; ++g)
            for (int gx=0; gx<GW; ++gx) {
                bool on = (FONT5x7[g][gy] >> (4-gx)) & 1;
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
    if (c >= 32 && c <= 95) return c - 32;
    if (c >= 'a' && c <= 'z') return c - 'a' + 65;
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
    glUniform4f(glGetUniformLocation(m_lineProgram,"uColor"), 1.0f,1.0f,1.0f,1.0f);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(m_highlightVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
}

glm::vec3 Renderer::skyColorForTime(float t) const {
    glm::vec3 night(0.04f, 0.06f, 0.12f);
    glm::vec3 day(0.5f, 0.7f, 1.0f);
    glm::vec3 dusk(0.95f, 0.55f, 0.30f);
    float sunY = sin(t * 6.28318f - 1.5708f);
    if (sunY > 0.2f) return day;
    if (sunY > -0.2f) {
        float k = (sunY + 0.2f) / 0.4f;
        return glm::mix(night, day, k) + dusk * (1.0f - std::abs(sunY) * 5.0f) * 0.3f;
    }
    return night;
}

glm::vec3 Renderer::blockAverageColor(int blockId) const {
    switch ((Block::ID)blockId) {
        case Block::ID::grass_block: return {0.36f, 0.58f, 0.20f};
        case Block::ID::dirt:        return {0.55f, 0.45f, 0.33f};
        case Block::ID::stone:       return {0.50f, 0.50f, 0.50f};
        case Block::ID::cobblestone: return {0.39f, 0.39f, 0.39f};
        case Block::ID::wood:        return {0.63f, 0.47f, 0.31f};
        case Block::ID::leaves:      return {0.16f, 0.43f, 0.12f};
        case Block::ID::sand:        return {0.86f, 0.81f, 0.64f};
        case Block::ID::glass:       return {0.78f, 0.86f, 0.94f};
        case Block::ID::bedrock:     return {0.16f, 0.16f, 0.16f};
        default:                     return {0.5f, 0.5f, 0.5f};
    }
}

void Renderer::initFullscreenQuad() {
    if (m_fsVAO) return;
    float verts[] = {
        -1, -1, 0,  0, 0,
         1, -1, 0,  1, 0,
         1,  1, 0,  1, 1,
        -1, -1, 0,  0, 0,
         1,  1, 0,  1, 1,
        -1,  1, 0,  0, 1,
    };
    glCreateVertexArrays(1, &m_fsVAO);
    glCreateBuffers(1, &m_fsVBO);
    glNamedBufferData(m_fsVBO, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexArrayAttrib(m_fsVAO, 0);
    glVertexArrayAttribFormat(m_fsVAO, 0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float));
    glVertexArrayAttribBinding(m_fsVAO, 0, 0);
    glEnableVertexArrayAttrib(m_fsVAO, 1);
    glVertexArrayAttribFormat(m_fsVAO, 1, 2, GL_FLOAT, GL_FALSE, 3*sizeof(float));
    glVertexArrayAttribBinding(m_fsVAO, 1, 0);
    glVertexArrayVertexBuffer(m_fsVAO, 0, m_fsVBO, 0, 5*sizeof(float));
}

void Renderer::renderFullscreenQuad(const glm::vec4& color) {
    initFullscreenQuad();
    glUseProgram(m_uiProgram);
    glBindTextureUnit(0, m_whiteTex);
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), color.r, color.g, color.b, color.a);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_fsVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderPanel(float x, float y, float w, float h, const glm::vec4& bgColor) {
    float verts[] = {
        x,     y,     0, 0, 0,
        x+w,   y,     0, 1, 0,
        x+w,   y+h,   0, 1, 1,
        x,     y,     0, 0, 0,
        x+w,   y+h,   0, 1, 1,
        x,     y+h,   0, 0, 1,
    };
    glNamedBufferData(m_panelVBO, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glUseProgram(m_uiProgram);
    glBindTextureUnit(0, m_whiteTex);
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_panelVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderButton(float x, float y, float w, float h, const glm::vec4& color, const std::string& label) {
    renderPanel(x, y, w, h, color);
    float textW = label.size() * 0.035f * 0.6f;
    float textH = 0.04f;
    renderTextColored(label, x + (w - textW) / 2, y + (h - textH) / 2 + textH/2, 0.035f, glm::vec3(1, 1, 1));
}

void Renderer::renderTextColored(const std::string& text, float ndcX, float ndcY, float scaleH, const glm::vec3& color) {
    const float GW=5.0f, GH=7.0f, N=64.0f;
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
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), color.r, color.g, color.b, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_textVAO);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size()/5));
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderSlider(float x, float y, float w, float value, float minV, float maxV) {
    float t = (value - minV) / (maxV - minV);
    renderPanel(x, y, w, 0.02f, glm::vec4(0.3f, 0.3f, 0.35f, 1.0f));
    renderPanel(x, y, w * t, 0.02f, glm::vec4(0.16f, 0.7f, 0.4f, 1.0f));
    float knobX = x + w * t - 0.01f;
    renderPanel(knobX, y - 0.01f, 0.02f, 0.04f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void Renderer::renderCheckbox(float x, float y, bool checked, const std::string& label) {
    renderPanel(x, y, 0.03f, 0.03f, checked ? glm::vec4(0.16f, 0.7f, 0.4f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.22f, 1.0f));
    renderTextColored(label, x + 0.04f, y + 0.02f, 0.035f, glm::vec3(1, 1, 1));
}

void Renderer::renderHotbarWithIcons(int selectedSlot, const TextureAtlas& atlas, const int hotbar[9]) {
    float boxW = 0.08f, boxH = 0.12f, gap = 0.01f;
    float startX = -((9 * boxW + 8 * gap) / 2.0f);
    float y = -0.95f;
    for (int i = 0; i < 9; ++i) {
        float x = startX + i * (boxW + gap);
        glm::vec4 bg = (i == selectedSlot) ? glm::vec4(1, 1, 1, 0.8f) : glm::vec4(0.15f, 0.15f, 0.15f, 0.7f);
        renderPanel(x, y, boxW, boxH, bg);
        int blockId = hotbar[i];
        if (blockId > 0) {
            glm::vec3 bc = blockAverageColor(blockId);
            renderPanel(x + 0.01f, y + 0.01f, boxW - 0.02f, boxH - 0.02f, glm::vec4(bc, 1));
        }
        renderTextColored(std::to_string(i + 1), x + 0.005f, y + boxH - 0.02f, 0.025f, glm::vec3(0.7f, 0.7f, 0.7f));
    }
}

void Renderer::getRenderStats(int& outChunks, int& outTris, int& outDrawCalls) const {
    outChunks = (int)chunkMeshes.size();
    outTris = 0;
    outDrawCalls = 0;
    for (auto& [key, cgl] : chunkMeshes) {
        outTris += cgl.indexCount[0] / 3 + cgl.indexCount[1] / 3;
        if (cgl.indexCount[0] > 0) outDrawCalls++;
        if (cgl.indexCount[1] > 0) outDrawCalls++;
    }
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
}
