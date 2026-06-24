#pragma once
#include "../core/Math.h"
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
