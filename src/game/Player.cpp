#include "Player.h"
#include "../world/World.h"
#include "../core/Block.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

void Player::update(float dt, World& world) {
    if (!flying) velocity.y -= 20.0f * dt;
    velocity.y = glm::clamp(velocity.y, -30.0f, 30.0f);
    Vec3 delta = velocity * dt;

    auto collides = [&](int bx, int by, int bz) {
        uint16_t id = world.getBlock(bx, by, bz);
        if (id == (uint16_t)Block::ID::air) return false;
        return Block::getProperties((Block::ID)id).solid;
    };

    // X axis
    if (delta.x != 0) {
        int yMin = (int)floor(position.y);
        int yMax = (int)floor(position.y + 1.8f);
        int zMin = (int)floor(position.z - 0.3f);
        int zMax = (int)floor(position.z + 0.3f - 0.001f);
        if (delta.x > 0) {
            int edge = (int)floor(position.x + delta.x + 0.3f);
            bool hit = false;
            for (int y = yMin; y <= yMax && !hit; ++y)
                for (int z = zMin; z <= zMax && !hit; ++z)
                    if (collides(edge, y, z)) hit = true;
            if (hit) { position.x = (float)(edge - 0.3f - 0.001f); velocity.x = 0; delta.x = 0; }
            else position.x += delta.x;
        } else {
            int edge = (int)floor(position.x + delta.x - 0.3f);
            bool hit = false;
            for (int y = yMin; y <= yMax && !hit; ++y)
                for (int z = zMin; z <= zMax && !hit; ++z)
                    if (collides(edge, y, z)) hit = true;
            if (hit) { position.x = (float)(edge + 0.3f + 0.001f); velocity.x = 0; delta.x = 0; }
            else position.x += delta.x;
        }
    }

    // Y axis
    if (delta.y != 0) {
        int xMin = (int)floor(position.x - 0.3f);
        int xMax = (int)floor(position.x + 0.3f - 0.001f);
        int zMin = (int)floor(position.z - 0.3f);
        int zMax = (int)floor(position.z + 0.3f - 0.001f);
        if (delta.y > 0) {
            int edge = (int)floor(position.y + 1.8f + delta.y);
            bool hit = false;
            for (int x = xMin; x <= xMax && !hit; ++x)
                for (int z = zMin; z <= zMax && !hit; ++z)
                    if (collides(x, edge, z)) hit = true;
            if (hit) { position.y = (float)(edge - 1.8f - 0.001f); velocity.y = 0; delta.y = 0; }
            else position.y += delta.y;
        } else {
            int edge = (int)floor(position.y + delta.y);
            bool hit = false;
            for (int x = xMin; x <= xMax && !hit; ++x)
                for (int z = zMin; z <= zMax && !hit; ++z)
                    if (collides(x, edge, z)) hit = true;
            if (hit) { position.y = (float)(edge + 0.001f); velocity.y = 0; delta.y = 0; onGround = true; }
            else { position.y += delta.y; onGround = false; }
        }
    }

    // Z axis
    if (delta.z != 0) {
        int yMin = (int)floor(position.y);
        int yMax = (int)floor(position.y + 1.8f);
        int xMin = (int)floor(position.x - 0.3f);
        int xMax = (int)floor(position.x + 0.3f - 0.001f);
        if (delta.z > 0) {
            int edge = (int)floor(position.z + delta.z + 0.3f);
            bool hit = false;
            for (int y = yMin; y <= yMax && !hit; ++y)
                for (int x = xMin; x <= xMax && !hit; ++x)
                    if (collides(x, y, edge)) hit = true;
            if (hit) { position.z = (float)(edge - 0.3f - 0.001f); velocity.z = 0; }
            else position.z += delta.z;
        } else {
            int edge = (int)floor(position.z + delta.z - 0.3f);
            bool hit = false;
            for (int y = yMin; y <= yMax && !hit; ++y)
                for (int x = xMin; x <= xMax && !hit; ++x)
                    if (collides(x, y, edge)) hit = true;
            if (hit) { position.z = (float)(edge + 0.3f + 0.001f); velocity.z = 0; }
            else position.z += delta.z;
        }
    }

    velocity.x *= 0.9f;
    velocity.z *= 0.9f;
}

void Player::processInput(GLFWwindow* win, float dt) {
    float speed = flying ? 15.0f : 5.0f;
    Vec3 front(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw)));
    Vec3 right = glm::cross(front, Vec3(0, 1, 0));
    Vec3 move(0);
    if (glfwGetKey(win, GLFW_KEY_W)) move += front;
    if (glfwGetKey(win, GLFW_KEY_S)) move -= front;
    if (glfwGetKey(win, GLFW_KEY_A)) move -= right;
    if (glfwGetKey(win, GLFW_KEY_D)) move += right;
    if (glm::length(move) > 0) move = glm::normalize(move);
    velocity.x = move.x * speed;
    velocity.z = move.z * speed;
    if (flying) {
        if (glfwGetKey(win, GLFW_KEY_SPACE)) velocity.y = speed;
        if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)) velocity.y = -speed;
    } else {
        if (glfwGetKey(win, GLFW_KEY_SPACE) && onGround) velocity.y = 8.0f;
    }

    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    float dx = float(mx - lastMX) * 0.1f;
    float dy = float(lastMY - my) * 0.1f;
    lastMX = mx;
    lastMY = my;
    if (hasLastCursor) {
        yaw += dx;
        pitch += dy;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
    hasLastCursor = true;
}

void Player::breakBlock(World& world) {
    Vec3 dir(cos(glm::radians(pitch)) * cos(glm::radians(yaw)),
             sin(glm::radians(pitch)),
             cos(glm::radians(pitch)) * sin(glm::radians(yaw)));
    Vec3 eye = position + Vec3(0, 1.6f, 0);
    for (float i = 0; i < 8.0f; i += 0.1f) {
        IVec3 bp((int)floor(eye.x + dir.x * i),
                 (int)floor(eye.y + dir.y * i),
                 (int)floor(eye.z + dir.z * i));
        if (world.getBlock(bp.x, bp.y, bp.z) != (uint16_t)Block::ID::air) {
            world.setBlock(bp.x, bp.y, bp.z, (uint16_t)Block::ID::air);
            return;
        }
    }
}

bool Player::getTargetBlock(World& world, IVec3& outPos) const {
    Vec3 dir(cos(glm::radians(pitch)) * cos(glm::radians(yaw)),
             sin(glm::radians(pitch)),
             cos(glm::radians(pitch)) * sin(glm::radians(yaw)));
    Vec3 eye = position + Vec3(0, 1.6f, 0);
    for (float i = 0; i < 8.0f; i += 0.1f) {
        IVec3 bp((int)floor(eye.x + dir.x * i),
                 (int)floor(eye.y + dir.y * i),
                 (int)floor(eye.z + dir.z * i));
        if (world.getBlock(bp.x, bp.y, bp.z) != (uint16_t)Block::ID::air) {
            outPos = bp;
            return true;
        }
    }
    return false;
}

void Player::placeBlock(World& world) {
    Vec3 dir(cos(glm::radians(pitch)) * cos(glm::radians(yaw)),
             sin(glm::radians(pitch)),
             cos(glm::radians(pitch)) * sin(glm::radians(yaw)));
    Vec3 eye = position + Vec3(0, 1.6f, 0);

    IVec3 lastBp((int)floor(eye.x), (int)floor(eye.y), (int)floor(eye.z));

    for (float i = 0; i < 8.0f; i += 0.1f) {
        IVec3 bp((int)floor(eye.x + dir.x * i),
                 (int)floor(eye.y + dir.y * i),
                 (int)floor(eye.z + dir.z * i));

        if (world.getBlock(bp.x, bp.y, bp.z) != (uint16_t)Block::ID::air) {
            float pMinX = position.x - 0.3f, pMaxX = position.x + 0.3f;
            float pMinY = position.y, pMaxY = position.y + 1.8f;
            float pMinZ = position.z - 0.3f, pMaxZ = position.z + 0.3f;

            float bMinX = bp.x, bMaxX = bp.x + 1.0f;
            float bMinY = bp.y, bMaxY = bp.y + 1.0f;
            float bMinZ = bp.z, bMaxZ = bp.z + 1.0f;

            bool intersects = (pMinX < bMaxX && pMaxX > bMinX) &&
                              (pMinY < bMaxY && pMaxY > bMinY) &&
                              (pMinZ < bMaxZ && pMaxZ > bMinZ);

            if (!intersects) {
                world.setBlock(lastBp.x, lastBp.y, lastBp.z, hotbar[selectedSlot]);
            }
            return;
        }
        lastBp = bp;
    }
}
