#include "Game.h"
#include "../render/MeshBuilder.h"
#include <GLFW/glfw3.h>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

double Game::scrollOffset = 0.0;

void Game::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    scrollOffset += yoffset;
}

void Game::cursorCallback(GLFWwindow* window, double xpos, double ypos) {
    Game* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (self) self->player.onMouseMove(xpos, ypos);
}

void Game::log(const std::string& level, const std::string& scope, const std::string& msg) {
    logBuffer.push_back({glfwGetTime(), level, scope, msg});
    if (logBuffer.size() > logMax) logBuffer.pop_front();
    std::cerr << "[" << level << "] [" << scope << "] " << msg << std::endl;
}

Game::Game()
: renderer(std::make_unique<Renderer>(1280, 720)),
  world(std::make_unique<World>(12345ull))
{
    player.position = Vec3(0, 100, 0);
    lastTime = glfwGetTime();
    fpsLast = lastTime;
    renderer->setRenderDistance(settings.renderDistance);
    world->updatePlayerPosition(0, 0, settings.renderDistance);
    glfwSetScrollCallback(renderer->window, scrollCallback);
    glfwSetWindowUserPointer(renderer->window, this);
    glfwSetCursorPosCallback(renderer->window, cursorCallback);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(renderer->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    log("INFO", "world", "World initialized with seed 12345");
    log("INFO", "player", "Spawned at (0, 100, 0)");
}

void Game::run() {
    while (!glfwWindowShouldClose(renderer->window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        frameTimeAvg = frameTimeAvg * 0.9 + dt * 0.1;
        double fps = 1.0 / frameTimeAvg;

        if (fps < 50) {
            lowFpsTimer += dt;
            highFpsTimer = 0;
            if (lowFpsTimer > 2.0 && settings.renderDistance > 3) {
                settings.renderDistance--;
                renderer->setRenderDistance(settings.renderDistance);
                world->updatePlayerPosition((int)player.position.x, (int)player.position.z, settings.renderDistance);
                lowFpsTimer = 0;
            }
        } else if (fps > 80) {
            highFpsTimer += dt;
            lowFpsTimer = 0;
            if (highFpsTimer > 5.0 && settings.renderDistance < 12) {
                settings.renderDistance++;
                renderer->setRenderDistance(settings.renderDistance);
                highFpsTimer = 0;
            }
        } else {
            lowFpsTimer = 0;
            highFpsTimer = 0;
        }

        if (state == GameState::StartScreen) {
            if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(renderer->window, true);
            }
            if (glfwGetMouseButton(renderer->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                state = GameState::Playing;
                glfwSetInputMode(renderer->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                log("DEBUG", "engine", "Pointer lock acquired");
            }
            if (glfwGetKey(renderer->window, GLFW_KEY_O) == GLFW_PRESS) {
                state = GameState::Settings;
            }
            if (glfwGetKey(renderer->window, GLFW_KEY_L) == GLFW_PRESS) {
                state = GameState::Logger;
            }
            if (glfwGetKey(renderer->window, GLFW_KEY_H) == GLFW_PRESS) {
                showHelp = !showHelp;
            }

            glm::mat4 proj = glm::perspective(glm::radians(settings.fov), 1280.0f / 720.0f, 0.1f, 1000.0f);
            glm::mat4 view = player.getViewMatrix();
            renderer->m_camPos = player.position + Vec3(0, 1.6f, 0);
            renderer->beginFrame(proj * view);
            renderer->renderChunks(view, proj);
            renderer->endFrame();

            renderStartScreen();
            glfwPollEvents();
            continue;
        }

        if (state == GameState::Settings) {
            if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                state = GameState::StartScreen;
            }

            renderer->beginFrame(glm::mat4(1.0f));
            renderSettingsPanel();
            renderer->endFrame();
            glfwPollEvents();
            continue;
        }

        if (state == GameState::Logger) {
            if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                state = GameState::StartScreen;
            }

            renderer->beginFrame(glm::mat4(1.0f));
            renderLoggerPanel();
            renderer->endFrame();
            glfwPollEvents();
            continue;
        }

        // === Playing state ===
        if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            state = GameState::StartScreen;
            glfwSetInputMode(renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        player.mouseSensitivity = settings.mouseSensitivity;

        if (state == GameState::Playing && settings.dayNightSpeed > 0) {
            renderer->timeOfDay = fmod(renderer->timeOfDay + dt * settings.dayNightSpeed / 60.0f, 1.0f);
            if (renderer->timeOfDay < 0) renderer->timeOfDay += 1.0f;
        }

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

        accumulator += dt;
        while (accumulator >= 0.05f) {
            player.update(0.05f, *world);
            world->updatePlayerPosition((int)player.position.x, (int)player.position.z, settings.renderDistance);
            accumulator -= 0.05f;
        }

        if (player.position.y < -100) {
            log("WARN", "player", "Fell out of world, respawning");
            player.position = Vec3(0, 100, 0);
            player.velocity = Vec3(0);
        }

        if (pendingUploads.empty())
            pendingUploads = world->drainUploads();

        if (pendingUploads.size() > 1) {
            int px = (int)player.position.x;
            int pz = (int)player.position.z;
            std::sort(pendingUploads.begin(), pendingUploads.end(),
                [px, pz](const World::PendingUpload& a, const World::PendingUpload& b) {
                    int ax = a.cx * 16 + 8, az = a.cz * 16 + 8;
                    int bx = b.cx * 16 + 8, bz = b.cz * 16 + 8;
                    int da = (ax - px) * (ax - px) + (az - pz) * (az - pz);
                    int db = (bx - px) * (bx - px) + (bz - pz) * (bz - pz);
                    return da < db;
                });
        }

        int budget = frameTimeAvg < 0.011 ? 8
                   : frameTimeAvg < 0.016 ? 4
                   : frameTimeAvg < 0.025 ? 2
                   : 1;
        int uploadBudget = std::min(budget, (int)pendingUploads.size());
        for (int i = 0; i < uploadBudget; ++i) {
            renderer->uploadChunkMesh(pendingUploads[i].cx, pendingUploads[i].cz, pendingUploads[i].mesh);
        }
        pendingUploads.erase(pendingUploads.begin(), pendingUploads.begin() + uploadBudget);

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
        renderer->renderHotbarWithIcons(player.selectedSlot, renderer->getTextureAtlas(), player.hotbar);

        ++fpsFrames;
        if (now - fpsLast >= 0.25) {
            fpsStr = "FPS: " + std::to_string((int)std::round(fpsFrames / (now - fpsLast)));
            fpsFrames = 0;
            fpsLast = now;
        }

        int chunkCount = 0, triCount = 0, drawCallCount = 0;
        renderer->getRenderStats(chunkCount, triCount, drawCallCount);
        renderDebugHUD((int)std::round(fps), (float)frameTimeAvg * 1000.0f, chunkCount, triCount, drawCallCount);

        renderer->endFrame();
        glfwPollEvents();
    }
}

void Game::renderStartScreen() {
    renderer->renderFullscreenQuad(glm::vec4(0, 0, 0, 0.6f));
    renderer->renderTextColored("VOXELCRAFT", -0.35f, 0.2f, 0.12f, glm::vec3(1, 1, 1));
    renderer->renderTextColored("A MINECRAFT-STYLE VOXEL SANDBOX", -0.42f, 0.05f, 0.04f, glm::vec3(0.7f, 0.7f, 0.7f));
    renderer->renderButton(-0.15f, -0.15f, 0.30f, 0.10f, glm::vec4(0.16f, 0.7f, 0.4f, 1.0f), "CLICK TO PLAY");
    renderer->renderTextColored("PRESS O FOR SETTINGS  |  L FOR LOGGER  |  H FOR HELP",
                                -0.5f, -0.4f, 0.035f, glm::vec3(0.5f, 0.5f, 0.5f));
    if (showHelp) {
        renderHelpOverlay();
    }
}

void Game::renderHelpOverlay() {
    renderer->renderFullscreenQuad(glm::vec4(0, 0, 0, 0.6f));
    renderer->renderPanel(-0.4f, -0.5f, 0.8f, 1.0f, glm::vec4(0.12f, 0.12f, 0.14f, 0.95f));
    renderer->renderTextColored("HOW TO PLAY", -0.15f, 0.4f, 0.06f, glm::vec3(1, 1, 1));

    struct Line { const char* action; const char* key; };
    Line lines[] = {
        {"MOVE",        "WASD / ARROWS"},
        {"LOOK",        "MOUSE"},
        {"JUMP",        "SPACE"},
        {"SPRINT",      "SHIFT"},
        {"BREAK BLOCK", "HOLD LEFT CLICK"},
        {"PLACE BLOCK", "RIGHT CLICK"},
        {"HOTBAR",      "1-9 / SCROLL WHEEL"},
        {"FLY TOGGLE",  "F"},
        {"SETTINGS",    "O"},
        {"LOGGER",      "L"},
        {"RELEASE MOUSE", "ESC"},
    };

    float y = 0.3f;
    for (auto& l : lines) {
        renderer->renderTextColored(l.action, -0.35f, y, 0.035f, glm::vec3(0.6f, 0.6f, 0.6f));
        renderer->renderTextColored(l.key,    0.05f, y, 0.035f, glm::vec3(1, 1, 1));
        y -= 0.07f;
    }

    renderer->renderButton(-0.15f, -0.42f, 0.30f, 0.08f, glm::vec4(0.16f, 0.7f, 0.4f, 1.0f), "GOT IT");
}

void Game::renderSettingsPanel() {
    renderer->renderFullscreenQuad(glm::vec4(0, 0, 0, 0.6f));
    renderer->renderPanel(-0.6f, -0.7f, 1.2f, 1.4f, glm::vec4(0.15f, 0.15f, 0.17f, 0.95f));

    renderer->renderTextColored("SETTINGS", -0.55f, 0.6f, 0.07f, glm::vec3(1, 1, 1));

    renderer->renderTextColored("RENDERING", -0.55f, 0.5f, 0.04f, glm::vec3(0.6f, 0.6f, 0.6f));
    renderer->renderTextColored("RENDER DISTANCE: " + std::to_string(settings.renderDistance) + " CHUNKS",
                                -0.55f, 0.42f, 0.035f, glm::vec3(1, 1, 1));
    renderer->renderSlider(-0.55f, 0.38f, 0.5f, (float)settings.renderDistance, 2, 12);

    renderer->renderTextColored("FIELD OF VIEW: " + std::to_string((int)settings.fov) + " DEG",
                                -0.55f, 0.30f, 0.035f, glm::vec3(1, 1, 1));
    renderer->renderSlider(-0.55f, 0.26f, 0.5f, settings.fov, 60, 110);

    renderer->renderTextColored("MOUSE SENSITIVITY: " + std::to_string(settings.mouseSensitivity),
                                -0.55f, 0.18f, 0.035f, glm::vec3(1, 1, 1));
    renderer->renderSlider(-0.55f, 0.14f, 0.5f, settings.mouseSensitivity, 0.01f, 0.2f);

    renderer->renderTextColored("WORLD", -0.55f, 0.04f, 0.04f, glm::vec3(0.6f, 0.6f, 0.6f));
    renderer->renderTextColored("DAY/NIGHT SPEED: " + std::to_string(settings.dayNightSpeed),
                                -0.55f, -0.04f, 0.035f, glm::vec3(1, 1, 1));
    renderer->renderSlider(-0.55f, -0.08f, 0.5f, settings.dayNightSpeed, 0, 10);

    renderer->renderCheckbox(-0.55f, -0.18f, settings.fogEnabled, "FOG");

    renderer->renderTextColored("CONTROLS", -0.55f, -0.30f, 0.04f, glm::vec3(0.6f, 0.6f, 0.6f));
    renderer->renderTextColored("WASD: MOVE  |  SPACE: JUMP  |  SHIFT: SPRINT",
                                -0.55f, -0.38f, 0.03f, glm::vec3(0.8f, 0.8f, 0.8f));
    renderer->renderTextColored("LMB: BREAK  |  RMB: PLACE  |  1-9: HOTBAR",
                                -0.55f, -0.45f, 0.03f, glm::vec3(0.8f, 0.8f, 0.8f));
    renderer->renderTextColored("F: FLY  |  ESC: MENU  |  O: SETTINGS  |  L: LOGGER",
                                -0.55f, -0.52f, 0.03f, glm::vec3(0.8f, 0.8f, 0.8f));

    renderer->renderButton(0.3f, -0.62f, 0.25f, 0.08f, glm::vec4(0.2f, 0.2f, 0.22f, 1.0f), "CLOSE (ESC)");
}

void Game::renderLoggerPanel() {
    renderer->renderFullscreenQuad(glm::vec4(0, 0, 0, 0.6f));
    renderer->renderPanel(-0.8f, -0.8f, 1.6f, 1.6f, glm::vec4(0.08f, 0.08f, 0.10f, 0.95f));

    renderer->renderTextColored("DEBUG LOGGER", -0.75f, 0.7f, 0.06f, glm::vec3(1, 1, 1));
    renderer->renderTextColored(std::to_string(logBuffer.size()) + " ENTRIES",
                                0.3f, 0.7f, 0.04f, glm::vec3(0.5f, 0.5f, 0.5f));

    float y = 0.6f;
    int count = 0;
    for (auto it = logBuffer.rbegin(); it != logBuffer.rend() && count < 25; ++it, ++count) {
        glm::vec3 color = it->level == "ERROR" ? glm::vec3(0.95f, 0.3f, 0.3f)
                       : it->level == "WARN"  ? glm::vec3(0.95f, 0.75f, 0.2f)
                       : it->level == "INFO"  ? glm::vec3(0.9f, 0.9f, 0.9f)
                                              : glm::vec3(0.5f, 0.5f, 0.5f);
        std::string line = "[" + it->level + "] [" + it->scope + "] " + it->msg;
        renderer->renderTextColored(line.substr(0, 80), -0.75f, y, 0.025f, color);
        y -= 0.035f;
    }

    renderer->renderButton(0.4f, -0.75f, 0.3f, 0.07f, glm::vec4(0.2f, 0.2f, 0.22f, 1.0f), "CLOSE (ESC)");
}

void Game::renderDebugHUD(int fps, float frameMs, int chunks, int tris, int drawCalls) {
    float y = 0.92f;
    float lineH = 0.04f;

    glm::vec3 fpsColor = fps >= 55 ? glm::vec3(0.3f, 0.9f, 0.4f)
                      : fps >= 30 ? glm::vec3(0.95f, 0.7f, 0.2f)
                                  : glm::vec3(0.95f, 0.3f, 0.3f);
    renderer->renderTextColored("FPS: " + std::to_string(fps),
                                -0.98f, y, 0.06f, fpsColor);
    y -= lineH * 1.5f;

    std::vector<std::string> lines = {
        "FRAME: " + std::to_string((int)frameMs) + " MS",
        "CHUNKS: " + std::to_string(chunks),
        "TRIS: " + std::to_string(tris),
        "DRAW CALLS: " + std::to_string(drawCalls),
        "POS: " + std::to_string((int)player.position.x) + ", " +
                   std::to_string((int)player.position.y) + ", " +
                   std::to_string((int)player.position.z),
        "YAW: " + std::to_string((int)player.yaw) + "  PITCH: " + std::to_string((int)player.pitch),
        "SPEED: " + std::to_string((int)sqrt(player.velocity.x*player.velocity.x + player.velocity.z*player.velocity.z)) + " B/S",
        "RD: " + std::to_string(settings.renderDistance) + "  FOV: " + std::to_string((int)settings.fov),
    };

    for (auto& line : lines) {
        renderer->renderTextColored(line, -0.98f, y, 0.035f, glm::vec3(1, 1, 1));
        y -= lineH;
    }
}
