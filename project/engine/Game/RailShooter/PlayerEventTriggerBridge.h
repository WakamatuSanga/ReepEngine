#pragma once

class LevelEventRuntime;
class Player;

class PlayerEventTriggerBridge {
public:
    PlayerEventTriggerBridge();
    ~PlayerEventTriggerBridge();

    void Initialize(Player* player, LevelEventRuntime* eventRuntime);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    Player* player_ = nullptr;
    LevelEventRuntime* eventRuntime_ = nullptr;
    bool enabled_ = true;
};
