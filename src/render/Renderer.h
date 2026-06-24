#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <string>
#include "MeshBuilder.h"
#include "TextureAtlas.h"

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    void beginFrame(const glm::mat4& projView);
    void uploadChunkMesh(int cx, int cz, const ChunkMesh& mesh);
    void renderChunks(const glm::mat4& view, const glm::mat4& proj);
    void renderCrosshair();
    void renderBlockHighlight(const glm::ivec3& blockPos);
    void renderText(const std::string& text, float ndcX, float ndcY, float scaleH);
    void endFrame();
    GLFWwindow* window;
    glm::mat4 m_prevProjView = glm::mat4(1.0f);
private:
    GLuint m_worldProgram = 0;
    GLuint m_uiProgram = 0;
    GLuint m_lineProgram = 0;
    GLuint m_whiteTex = 0;
    GLuint m_fontTex = 0;
    GLuint m_textVAO = 0, m_textVBO = 0;
    TextureAtlas m_textureAtlas;
    struct ChunkGL { GLuint vao=0, vbo=0, ebo=0; int indexCount=0; };
    std::unordered_map<int64_t, ChunkGL> chunkMeshes;
    GLuint m_crosshairVAO=0, m_crosshairVBO=0;
    GLuint m_highlightVAO=0, m_highlightVBO=0, m_highlightEBO=0;
    void initCrosshair();
    void initHighlight();
    void initFont();
    void initTextVAO();
    int  glyphIndex(char c) const;
};
