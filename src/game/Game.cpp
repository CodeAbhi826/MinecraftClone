#include "Game.h"
#include "../render/MeshBuilder.h"
#include <GLFW/glfw3.h>
#include <string>
#include <cmath>

double Game::scrollOffset = 0.0;

void Game::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    scrollOffset += yoffset;
}

Game::Game()
: renderer(std::make_unique<Renderer>(1280, 720)),
  world(std::make_unique<World>(12345ull))
{
    player.position = Vec3(0, 100, 0);
    lastTime = glfwGetTime();
    world->updatePlayerPosition(0, 0, settings.renderDistance);
    glfwSetScrollCallback(renderer->window, scrollCallback);
}

void Game::run() {
    int fpsFrames = 0;
    double fpsLast = lastTime;
    std::string fpsStr = "FPS: 0";

    while (!glfwWindowShouldClose(renderer->window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        player.processInput(renderer->window, dt);

        if (scrollOffset != 0.0) {
            int dir = scrollOffset > 0 ? -1 : 1;
            player.selectedSlot = (player.selectedSlot + dir + 9) % 9;
            scrollOffset = 0.0;
        }
        for (int i = 0; i < 9; ++i) {
            if (glfwGetKey(renderer->window, GLFW_KEY_1 + i) == GLFW_PRESS) {
                player.selectedSlot = i;
            }
        }

        static float accumulator = 0;
        accumulator += dt;
        while (accumulator >= 0.05f) {
            player.update(0.05f, *world);
            world->updatePlayerPosition((int)player.position.x, (int)player.position.z, settings.renderDistance);
            accumulator -= 0.05f;
        }

        static std::vector<World::PendingUpload> pendingUploads;
        if (pendingUploads.empty())
            pendingUploads = world->drainUploads();
        for (int i = 0; i < 8 && !pendingUploads.empty(); ++i) {
            renderer->uploadChunkMesh(pendingUploads.back().cx, pendingUploads.back().cz, pendingUploads.back().mesh);
            pendingUploads.pop_back();
        }

        if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(renderer->window, true);

        static bool leftPressed = false, rightPressed = false;
        if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !leftPressed) {
            player.breakBlock(*world); leftPressed = true;
        } else if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) leftPressed = false;
        if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !rightPressed) {
            player.placeBlock(*world); rightPressed = true;
        } else if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE) rightPressed = false;

        IVec3 targetBlock;
        bool hasTarget = player.getTargetBlock(*world, targetBlock);
        glm::mat4 proj = glm::perspective(glm::radians(settings.fov), 1280.0f / 720.0f, 0.1f, 1000.0f);
        glm::mat4 view = player.getViewMatrix();

        renderer->m_camPos = player.position + Vec3(0, 1.6f, 0);
        renderer->beginFrame(proj * view);
        renderer->renderChunks(view, proj);
        if (hasTarget) renderer->renderBlockHighlight(targetBlock);
        renderer->renderCrosshair();
        renderer->renderHotbar(player.selectedSlot);

        ++fpsFrames;
        if (now - fpsLast >= 0.25) {
            fpsStr = "FPS: " + std::to_string((int)std::round(fpsFrames / (now - fpsLast)));
            fpsFrames = 0;
            fpsLast = now;
        }
        renderer->renderText(fpsStr, -0.98f, 0.92f, 0.06f);

        renderer->endFrame();
        glfwPollEvents();
    }
}
