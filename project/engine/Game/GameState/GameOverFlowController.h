#pragma once

class PlayerDeathSequenceController;

class GameOverFlowController {
public:
    GameOverFlowController();
    ~GameOverFlowController();

    void Initialize(PlayerDeathSequenceController* deathSequence);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    PlayerDeathSequenceController* deathSequence_ = nullptr;
    bool hasRequestedTransition_ = false;
};
