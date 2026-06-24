#include "Renderer.h"
#include "../core/Block.h"
#include <iostream>
#include <vector>
#include <glm/gtc/type_ptr.hpp>

static const char* worldVertSrc = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in float aTexLayer;
layout(location=3) in float aAO;
out vec2 vTexCoord;
out float vTexLayer;
out float vAO;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
    vTexLayer = aTexLayer;
    vAO = aAO;
}
)";

static const char* worldFragSrc = R"(
#version 460 core
in vec2 vTexCoord;
in float vTexLayer;
in float vAO;
out vec4 FragColor;
uniform sampler2DArray uTexArray;
void main() {
    vec4 col = texture(uTexArray, vec3(vTexCoord, vTexLayer));
    FragColor = vec4(col.rgb * vAO, col.a);
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

    m_textureAtlas.generateProcedural();

    initCrosshair();
    initHighlight();
}

Renderer::~Renderer() {
    for (auto& [k, cgl] : chunkMeshes) {
        glDeleteVertexArrays(1, &cgl.vao);
        glDeleteBuffers(1, &cgl.vbo);
        glDeleteBuffers(1, &cgl.ebo);
    }
    glDeleteVertexArrays(1, &m_crosshairVAO);
    glDeleteBuffers(1, &m_crosshairVBO);
    glDeleteVertexArrays(1, &m_highlightVAO);
    glDeleteBuffers(1, &m_highlightVBO);
    glDeleteBuffers(1, &m_highlightEBO);
    glDeleteVertexArrays(1, &m_hotbarVAO);
    glDeleteBuffers(1, &m_hotbarVBO);
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
    if (cgl.vao) {
        glDeleteVertexArrays(1, &cgl.vao);
        glDeleteBuffers(1, &cgl.vbo);
        glDeleteBuffers(1, &cgl.ebo);
    }
    glCreateVertexArrays(1, &cgl.vao);
    glCreateBuffers(1, &cgl.vbo);
    glCreateBuffers(1, &cgl.ebo);
    glNamedBufferData(cgl.vbo, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(cgl.ebo, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexArrayAttrib(cgl.vao, 0);
    glVertexArrayAttribFormat(cgl.vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(cgl.vao, 0, 0);

    glEnableVertexArrayAttrib(cgl.vao, 1);
    glVertexArrayAttribFormat(cgl.vao, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(cgl.vao, 1, 0);

    glEnableVertexArrayAttrib(cgl.vao, 2);
    glVertexArrayAttribFormat(cgl.vao, 2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float));
    glVertexArrayAttribBinding(cgl.vao, 2, 0);

    glEnableVertexArrayAttrib(cgl.vao, 3);
    glVertexArrayAttribFormat(cgl.vao, 3, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
    glVertexArrayAttribBinding(cgl.vao, 3, 0);

    glVertexArrayVertexBuffer(cgl.vao, 0, cgl.vbo, 0, 8 * sizeof(float));
    glVertexArrayElementBuffer(cgl.vao, cgl.ebo);
    cgl.indexCount = (int)mesh.indices.size();
}

void Renderer::renderChunks(const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(m_worldProgram);
    m_textureAtlas.bind(0);
    for (auto& [key, cgl] : chunkMeshes) {
        glBindVertexArray(cgl.vao);
        glDrawElements(GL_TRIANGLES, cgl.indexCount, GL_UNSIGNED_INT, nullptr);
    }
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

void Renderer::renderCrosshair() {
    glUseProgram(m_uiProgram);
    glUniform4f(glGetUniformLocation(m_uiProgram, "uColor"), 1, 1, 1, 1);
    glBindTextureUnit(0, 0);
    glBindVertexArray(m_crosshairVAO);
    glDrawArrays(GL_LINES, 0, 8);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::renderBlockHighlight(const glm::ivec3& blockPos) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPos));
    glm::mat4 mvp = m_prevProjView * model;

    glUseProgram(m_worldProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_worldProgram, "uMVP"), 1, GL_FALSE, &mvp[0][0]);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(m_highlightVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
}
