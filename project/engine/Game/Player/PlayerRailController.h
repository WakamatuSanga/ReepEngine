#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>

class LevelRailRuntime;
class Player;

class PlayerRailController {
public:
    PlayerRailController();
    ~PlayerRailController();

    void Initialize(Player* player, LevelRailRuntime* railRuntime);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

private:
    void SyncSelectedRailDefaults();
    bool FetchSelectedRailInfo();
    void UpdateEvaluation(float deltaTime);
    void ApplyToPlayer();
    void ResetRailPosition();

    Player* player_ = nullptr;
    LevelRailRuntime* railRuntime_ = nullptr;
    std::string selectedRailId_;
    std::string selectedRailName_;
    std::string selectedRailType_;
    std::string previousRailId_;
    Vector3 currentRailPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentRailForward_{ 0.0f, 0.0f, 1.0f };
    size_t currentSegmentIndex_ = 0;
    size_t selectedRailPointCount_ = 0;
    size_t railCount_ = 0;
    int selectedRailIndex_ = 0;
    int moveMode_ = 0;
    float railT_ = 0.0f;
    float railDistance_ = 0.0f;
    float selectedRailTotalLength_ = 0.0f;
    float playSpeed_ = 1.0f;
    bool enableRailControl_ = false;
    bool autoPlay_ = false;
    bool debugLoop_ = false;
    bool selectedRailLoop_ = false;
    bool currentEvaluationValid_ = false;
};
