#pragma once
#include <memory>
#include <deque>
#include <string>
#include <vector>
#include "../render/Renderer.h"
#include "../world/World.h"
#include "Player.h"

struct Settings {
    int renderDistance = 8;
    float fov = 70.0f;
    bool fogEnabled = true;
    float mouseSensitivity = 0.05f;
    float dayNightSpeed = 0.0f;
};

struct LogEntry {
    double time;
    std::string level;
    std::string scope;
    std::string msg;
};

enum class GameState { StartScreen, Playing, Settings, Logger };

class Game {
public:
    Game();
    void run();
    Settings settings;
    std::deque<LogEntry> logBuffer;
    size_t logMax = 200;
    void log(const std::string& level, const std::string& scope, const std::string& msg);
private:
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<World> world;
    Player player;
    double lastTime;
    GameState state = GameState::StartScreen;
    bool showHelp = true;
    float accumulator = 0;
    std::vector<World::PendingUpload> pendingUploads;
    bool leftPressed = false, rightPressed = false;
    double frameTimeAvg = 0.016;
    double lowFpsTimer = 0, highFpsTimer = 0;
    int fpsFrames = 0;
    double fpsLast = 0;
    std::string fpsStr = "FPS: 0";

    void processInput();
    void renderStartScreen();
    void renderHelpOverlay();
    void renderSettingsPanel();
    void renderLoggerPanel();
    void renderDebugHUD(int fps, float frameMs, int chunks, int tris, int drawCalls);

    static double scrollOffset;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
};
