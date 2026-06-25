#pragma once
#include "../core/Math.h"
#include <glm/glm.hpp>
#include "../core/Block.h"
#include <GLFW/glfw3.h>

class World;
class Player {
public:
    Vec3 position = Vec3(0, 80, 0);
    Vec3 velocity = Vec3(0);
    float yaw = 0, pitch = 0;
    bool onGround = false, flying = false;

    double lastMX = 0, lastMY = 0;
    bool hasLastCursor = false;

    int hotbar[9] = {
        (int)Block::ID::grass_block,
        (int)Block::ID::dirt,
        (int)Block::ID::stone,
        (int)Block::ID::cobblestone,
        (int)Block::ID::wood,
        (int)Block::ID::leaves,
        (int)Block::ID::sand,
        (int)Block::ID::glass,
        (int)Block::ID::bedrock
    };
    int selectedSlot = 0;
    float mouseSensitivity = 0.05f;

    void onMouseMove(double xpos, double ypos) {
        if (!hasLastCursor) {
            lastMX = xpos;
            lastMY = ypos;
            hasLastCursor = true;
            return;
        }
        float dx = float(xpos - lastMX) * mouseSensitivity;
        float dy = float(lastMY - ypos) * mouseSensitivity;
        lastMX = xpos;
        lastMY = ypos;
        yaw   += dx;
        pitch += dy;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    void update(float dt, World& world);
    void processInput(GLFWwindow* win, float dt);
    void breakBlock(World& world);
    void placeBlock(World& world);
    bool getTargetBlock(World& world, IVec3& outPos) const;
    glm::mat4 getViewMatrix() const {
        Vec3 front(cos(glm::radians(pitch)) * cos(glm::radians(yaw)),
                   sin(glm::radians(pitch)),
                   cos(glm::radians(pitch)) * sin(glm::radians(yaw)));
        return glm::lookAt(position + Vec3(0, 1.6f, 0), position + Vec3(0, 1.6f, 0) + front, Vec3(0, 1, 0));
    }
};
