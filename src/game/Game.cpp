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
    if (!self) return;
    self->mouseX = xpos;
    self->mouseY = ypos;
    if (self->state == GameState::Playing) {
        self->player.onMouseMove(xpos, ypos);
    }
}

void Game::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Game* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            self->leftPressed = true;
            if (self->state == GameState::Settings) {
                double mx = self->mouseX;
                double my = self->mouseY;
                int winW, winH;
                glfwGetWindowSize(window, &winW, &winH);
                float ndcX = (float)(mx / winW * 2.0 - 1.0);
                float ndcY = (float)(1.0 - my / winH * 2.0);
                static const struct { float x, y, w; } sliderAreas[4] = {
                    {-0.55f, 0.34f, 0.5f},
                    {-0.55f, 0.22f, 0.5f},
                    {-0.55f, 0.10f, 0.5f},
                    {-0.55f, -0.12f, 0.5f},
                };
                for (int i = 0; i < 4; ++i) {
                    float sx = sliderAreas[i].x;
                    float sy = sliderAreas[i].y;
                    float sw = sliderAreas[i].w;
                    float sh = 0.06f;
                    if (ndcX >= sx && ndcX <= sx + sw && ndcY >= sy - sh/2 && ndcY <= sy + sh/2) {
                        self->draggingSlider = i;
                        break;
                    }
                }
            }
        } else if (action == GLFW_RELEASE) {
            self->leftPressed = false;
            self->draggingSlider = -1;
        }
    }
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
    player.position = Vec3(0, 120, 0);
    lastTime = glfwGetTime();
    fpsLast = lastTime;
    renderer->setRenderDistance(settings.renderDistance);
    world->updatePlayerPosition(0, 0, settings.renderDistance);
    glfwSetScrollCallback(renderer->window, scrollCallback);
    glfwSetWindowUserPointer(renderer->window, this);
    glfwSetCursorPosCallback(renderer->window, cursorCallback);
    glfwSetMouseButtonCallback(renderer->window, mouseButtonCallback);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(renderer->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    log("INFO", "world", "World initialized with seed 12345");
    log("INFO", "player", "Spawned at (0, 120, 0)");
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
                log("INFO", "perf", "Auto-lowered render distance to " + std::to_string(settings.renderDistance));
                lowFpsTimer = 0;
            }
        } else if (fps > 80) {
            highFpsTimer += dt;
            lowFpsTimer = 0;
            if (highFpsTimer > 5.0 && settings.renderDistance < settings.userRenderDistance) {
                settings.renderDistance++;
                renderer->setRenderDistance(settings.renderDistance);
                highFpsTimer = 0;
            }
        } else {
            lowFpsTimer = 0;
            highFpsTimer = 0;
        }

        if (state == GameState::Loading) {
            static int loadStep = 0;
            static double loadStart = glfwGetTime();

            if (loadStep == 0) {
                log("INFO", "world", "Generating terrain...");
                loadStep = 1;
            }

            // Populate world chunks around spawn
            int loaded = 0;
            const int targetChunks = 49;
            int px = 0, pz = 0;
            int taken = 0;
            for (int dz = -6; dz <= 6 && loaded < targetChunks/3; ++dz) {
                for (int dx = -6; dx <= 6 && loaded < targetChunks/3; ++dx) {
                    int cx = dx, cz = dz;
                    if (!world->isChunkLoaded(cx, cz)) {
                        world->ensureChunkLoaded(cx, cz);
                        taken++;
                    }
                    loaded++;
                }
            }
            world->processChunkLoading();

            float progress = 0.0f;
            int totalChunks = 0, readyChunks = 0;
            for (int dz = -6; dz <= 6; ++dz) {
                for (int dx = -6; dx <= 6; ++dx) {
                    totalChunks++;
                    if (world->isChunkReady(dx, dz))
                        readyChunks++;
                }
            }
            progress = totalChunks > 0 ? (float)readyChunks / totalChunks : 0.0f;

            renderer->beginFrame(glm::mat4(1.0f));
            renderLoadingScreen(progress);
            renderer->endFrame();
            glfwPollEvents();

            if (progress >= 1.0f || (glfwGetTime() - loadStart > 8.0)) {
                log("INFO", "engine", "Loading complete (" + std::to_string(readyChunks) + "/" + std::to_string(totalChunks) + " chunks)");
                state = GameState::StartScreen;
                loadStep = 0;
            }
            continue;
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

            Vec3 startCamPos(0, 180, 80);
            float startCamYaw = 180.0f;
            float startCamPitch = -25.0f;
            glm::mat4 startView = glm::lookAt(
                glm::vec3(startCamPos.x, startCamPos.y, startCamPos.z),
                glm::vec3(0, 60, 0),
                glm::vec3(0, 1, 0)
            );
            glm::mat4 proj = glm::perspective(glm::radians(settings.fov), 1280.0f / 720.0f, 0.1f, 1000.0f);
            renderer->m_camPos = startCamPos;
            renderer->beginFrame(proj * startView);
            renderer->renderChunks(startView, proj);
            renderer->endFrame();

            renderStartScreen();
            glfwPollEvents();
            continue;
        }

        if (state == GameState::Settings) {
            if (glfwGetKey(renderer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                state = GameState::StartScreen;
                draggingSlider = -1;
            }

            if (draggingSlider >= 0) {
                int winW, winH;
                glfwGetWindowSize(renderer->window, &winW, &winH);
                float ndcX = (float)(mouseX / winW * 2.0 - 1.0);
                static const struct { float x, w; } sliderArea[4] = {
                    {-0.55f, 0.5f}, {-0.55f, 0.5f}, {-0.55f, 0.5f}, {-0.55f, 0.5f},
                };
                float clamped = glm::clamp((ndcX - sliderArea[draggingSlider].x) / sliderArea[draggingSlider].w, 0.0f, 1.0f);
                switch (draggingSlider) {
                    case 0: settings.userRenderDistance = (int)glm::round(glm::mix(2.0f, 12.0f, clamped)); settings.renderDistance = settings.userRenderDistance; renderer->setRenderDistance(settings.renderDistance); break;
                    case 1: settings.fov = glm::mix(60.0f, 110.0f, clamped); break;
                    case 2: settings.mouseSensitivity = glm::mix(0.01f, 0.2f, clamped); break;
                    case 3: settings.dayNightSpeed = glm::mix(0.0f, 10.0f, clamped); break;
                }
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

        if (glfwGetKey(renderer->window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fPressed) {
                player.flying = !player.flying;
                log("DEBUG", "player", player.flying ? "Fly mode ON" : "Fly mode OFF");
                fPressed = true;
            }
        } else {
            fPressed = false;
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
            player.position = Vec3(0, 256, 0);
            bool found = false;
            for (int y = 256; y > -64; --y) {
                if (world->getBlock(0, y, 0) != (uint16_t)Block::ID::air) {
                    player.position = Vec3(0, y + 2, 0);
                    found = true;
                    break;
                }
            }
            if (!found) player.position = Vec3(0, 120, 0);
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

void Game::renderLoadingScreen(float progress) {
    float barW = 0.6f, barH = 0.04f;
    renderer->renderTextColored("LOADING", -0.12f, 0.1f, 0.08f, glm::vec3(1, 1, 1));
    renderer->renderPanel(-barW/2, -0.04f, barW, barH, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
    renderer->renderPanel(-barW/2, -0.04f, barW * progress, barH, glm::vec4(0.16f, 0.7f, 0.4f, 1.0f));
    renderer->renderTextColored(std::to_string((int)(progress * 100)) + "%",
                                -0.05f, -0.12f, 0.035f, glm::vec3(1, 1, 1));
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
        {"CTRL DESCEND", "CTRL (FLY)"},
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
        renderer->renderTextColored(line.size() > 77 ? line.substr(0, 77) + "..." : line, -0.75f, y, 0.025f, color);
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
