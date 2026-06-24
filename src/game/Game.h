#pragma once
#include <memory>
#include "../render/Renderer.h"
#include "../world/World.h"
#include "Player.h"

struct Settings {
    int renderDistance = 8;
    float fov = 70.0f;
    bool fogEnabled = true;
};

class Game {
public:
    Game();
    void run();
    Settings settings;
private:
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<World> world;
    Player player;
    double lastTime;
    void processInput();

    static double scrollOffset;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
