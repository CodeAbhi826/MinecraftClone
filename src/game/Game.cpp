#include "Game.h"
#include "../render/MeshBuilder.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

Game::Game()
    : renderer(std::make_unique<Renderer>(1280, 720)),
      world(std::make_unique<World>(12345ull))
{
    player.position = Vec3(0, 70, 0);
    lastTime = glfwGetTime();
    world->updatePlayerPosition(0, 0, 10);
}

void Game::run() {
    while (!glfwWindowShouldClose(renderer->window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        player.processInput(renderer->window, dt);

        static float accumulator = 0;
        accumulator += dt;
        while (accumulator >= 0.05f) {
            player.update(0.05f, *world);
            world->updatePlayerPosition((int)player.position.x, (int)player.position.z, 10);
            accumulator -= 0.05f;
        }

        for (auto* ch : world->getReadyChunks()) {
            if (ch->state == Chunk::Ready) {
                ChunkMesh mesh = MeshBuilder::build(*ch, *world);
                renderer->uploadChunkMesh(ch->x, ch->z, mesh);
                ch->state = Chunk::Empty;
            }
        }

        static bool leftPressed = false, rightPressed = false;
        if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !leftPressed) {
            player.breakBlock(*world);
            leftPressed = true;
        } else if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
            leftPressed = false;
        }
        if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !rightPressed) {
            player.placeBlock(*world);
            rightPressed = true;
        } else if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE) {
            rightPressed = false;
        }

        IVec3 targetBlock;
        bool hasTarget = player.getTargetBlock(*world, targetBlock);

        glm::mat4 proj = glm::perspective(glm::radians(70.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);
        glm::mat4 view = player.getViewMatrix();
        renderer->beginFrame(proj * view);
        renderer->renderChunks(view, proj);
        if (hasTarget)
            renderer->renderBlockHighlight(targetBlock);
        renderer->renderCrosshair();
        renderer->endFrame();
        glfwPollEvents();
    }
}
