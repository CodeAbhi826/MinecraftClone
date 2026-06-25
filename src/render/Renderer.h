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

    void setRenderDistance(int chunks) { m_cullRadius = 16.0f * (chunks + 0.5f); }
    void updateFrustum(const glm::mat4& view, const glm::mat4& proj);
    void renderFullscreenQuad(const glm::vec4& color);
    void renderButton(float x, float y, float w, float h, const glm::vec4& color, const std::string& label);
    void renderTextColored(const std::string& text, float ndcX, float ndcY, float scaleH, const glm::vec3& color);
    void renderPanel(float x, float y, float w, float h, const glm::vec4& bgColor);
    void renderSlider(float x, float y, float w, float value, float minV, float maxV);
    void renderCheckbox(float x, float y, bool checked, const std::string& label);
    void renderHotbarWithIcons(int selectedSlot, const TextureAtlas& atlas, const int hotbar[9]);
    void endFrame();
    glm::vec3 skyColorForTime(float t) const;
    glm::vec3 blockAverageColor(int blockId) const;
    void getRenderStats(int& outChunks, int& outTris, int& outDrawCalls) const;
    const TextureAtlas& getTextureAtlas() const { return m_textureAtlas; }

    GLFWwindow* window;
    glm::mat4 m_prevProjView = glm::mat4(1.0f);
    glm::vec3 m_camPos = glm::vec3(0.0f);
    float timeOfDay = 0.3f;
    float dayNightSpeed = 0.0f;
private:
    float m_cullRadius = 16.0f * 12.0f;
    bool chunkInFrustum(int cx, int cz) const;
    float m_frustumPlanes[6][4];
    GLuint m_worldProgram = 0;
    GLuint m_uiProgram = 0;
    GLuint m_lineProgram = 0;
    GLuint m_whiteTex = 0;
    GLuint m_fontTex = 0;
    GLuint m_textVAO = 0, m_textVBO = 0;
    TextureAtlas m_textureAtlas;
    struct ChunkGL { GLuint vao[2]={0,0}, vbo[2]={0,0}, ebo[2]={0,0}; int indexCount[2]={0,0}; };
    std::unordered_map<int64_t, ChunkGL> chunkMeshes;
    GLuint m_crosshairVAO=0, m_crosshairVBO=0;
    GLuint m_highlightVAO=0, m_highlightVBO=0, m_highlightEBO=0;
    GLuint m_hotbarVAO=0, m_hotbarVBO=0;
    GLuint m_fsVAO=0, m_fsVBO=0;
    GLuint m_panelVAO=0, m_panelVBO=0;
    void initCrosshair();
    void initHighlight();
    void initFont();
    void initTextVAO();
    void initHotbar();
    void initFullscreenQuad();
    int  glyphIndex(char c) const;
};
