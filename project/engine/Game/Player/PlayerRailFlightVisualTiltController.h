#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <string>

class Player;
class RailShooterCameraRig;

class PlayerRailFlightVisualTiltController {
public:
    PlayerRailFlightVisualTiltController();
    ~PlayerRailFlightVisualTiltController();

    void Initialize(Player* player, RailShooterCameraRig* railCameraRig);
    void Finalize();
    void Update(float unscaledDeltaTime, bool gameModeActive, bool playerAlive);
    void Reset();
    void DrawImGui();

private:
    enum class ForcedBankMode {
        None,
        Right,
        Left,
        Zero,
    };

    enum class CurveDirection {
        Straight,
        Right,
        Left,
    };

    bool CalculateRailBankTarget();
    void SmoothRailBank(float unscaledDeltaTime);
    void ApplyRecommendedSettings();
    void ClampSettings();
    void ResetBankState(bool clearForcedState);
    void UpdatePlayerDiagnostics();

    Player* player_ = nullptr;
    RailShooterCameraRig* railCameraRig_ = nullptr;

    Vector3 currentRailForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 aheadRailForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 railUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 playerBaseForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 playerDisplayForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 playerVisualBaseRotation_{};
    Vector3 playerActionRotation_{};
    Vector3 playerFinalRotation_{};

    std::string bankDisabledReason_ = "未初期化";
    float railBankLookAheadDistance_ = 10.0f;
    float railBankGain_ = 1.20f;
    float maxRailBankDegrees_ = 15.0f;
    float railBankResponseTime_ = 0.18f;
    float railBankReturnTime_ = 0.28f;
    float railBankSnapEpsilonDegrees_ = 0.05f;
    float railDistance_ = 0.0f;
    float signedTurnDegrees_ = 0.0f;
    float targetRailBankDegrees_ = 0.0f;
    float currentRailBankDegrees_ = 0.0f;
    float playerBasePitchDegrees_ = 0.0f;
    float playerFinalRollDegrees_ = 0.0f;
    uint64_t bankApplyCount_ = 0;
    uint64_t currentRailRevision_ = 0;
    uint64_t previousRailRevision_ = 0;
    int currentRailIndex_ = -1;
    int previousRailIndex_ = -1;

    bool initialized_ = false;
    bool railBankEnabled_ = true;
    bool gameModeActive_ = false;
    bool playerAlive_ = true;
    bool playerEnabled_ = false;
    bool railRunning_ = false;
    bool railPoseValid_ = false;
    bool runtimeV2Active_ = false;
    bool horizontalForwardValid_ = false;
    bool visualTiltApplying_ = false;
    bool visualRotationFinite_ = true;
    bool actionRotationActive_ = false;
    bool previousGameModeActive_ = false;
    bool previousPlayerAlive_ = true;
    bool hasRailIdentity_ = false;
    CurveDirection curveDirection_ = CurveDirection::Straight;
    ForcedBankMode forcedBankMode_ = ForcedBankMode::None;
};
