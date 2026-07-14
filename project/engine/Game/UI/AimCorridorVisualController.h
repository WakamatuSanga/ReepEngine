#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <string>

class AimCorridorVisualRenderer;
class Camera;
class DirectXCommon;
class Player;

struct AimReticleScreenRect {
    Vector2 centerUV{};
    Vector2 halfSizeUV{};
    Vector2 minUV{};
    Vector2 maxUV{};
    bool valid = false;
};

class AimCorridorVisualController {
public:
    enum class AimReticleVisualState : uint8_t {
        Normal,
        CandidatePreview,
        AcquiringPreview,
        LockedPreview,
    };

    AimCorridorVisualController();
    ~AimCorridorVisualController();

    bool Initialize(DirectXCommon* dxCommon, Player* player, Camera* camera);
    void Finalize();
    void Reset();
    void SetGameModeActive(bool active);
    void SetPlayerAlive(bool alive);
    void Update(float unscaledDeltaTime);
    void Draw();
    void DrawImGui();
    void SetTargetingVisualState(AimReticleVisualState state, float lockProgress);

    const Vector3& GetAimOrigin() const { return aimOrigin_; }
    const Vector3& GetAimForward() const { return aimForward_; }
    const Vector3& GetNearFrameCenter() const { return nearCenter_; }
    const Vector3& GetFarFrameCenter() const { return farCenter_; }
    float GetNearDistance() const { return nearDistance_; }
    float GetFarDistance() const { return farDistance_; }
    bool IsVisible() const { return isVisible_; }
    bool IsGameModeActive() const { return gameModeActive_; }
    const Vector3& GetMainReticleWorldCenter() const { return mainReticleCenterWorld_; }
    const Vector2& GetMainReticleScreenUv() const { return mainReticleCenterScreenUv_; }
    Vector2 GetMainReticleScreenSize() const {
        return { estimatedMainReticlePixelWidth_, estimatedMainReticlePixelHeight_ };
    }
    const AimReticleScreenRect& GetMainReticleScreenRect() const { return mainReticleScreenRect_; }

private:
    enum class AxisMode : uint8_t {
        CameraForward,
        CameraThroughAimOrigin,
    };

    enum class DisplayMode : uint8_t {
        MainReticleOnly,
        WorldCorridorDebug,
    };

    struct AppearanceParameters {
        float alpha = 1.0f;
        float coreIntensity = 1.0f;
        float glowIntensity = 0.5f;
        float glowAlpha = 0.16f;
        float glowRadiusTexels = 1.0f;
        float pulseAmount = 0.05f;
    };

    void SyncWorldPlacement(float unscaledDeltaTime);
    void UpdateLeadLag(float unscaledDeltaTime);
    Vector3 ConvertScreenOffsetToWorld(const Vector2& screenOffset, float depth) const;
    void UpdateBaseScreenOffsetLayout();
    void UpdateBaseScreenOffsetDebug();
    void UpdatePresentation();
    void UpdateTargetingAppearance(float unscaledDeltaTime);
    void DrawPresentation(float pulse);
    void UpdateDepthAppearance();
    void ClampParameters();
    void ClampBaseScreenOffsetParameters();
    void ClampPresentationParameters();
    void UpdateProjectionDebug();
    bool ProjectWorldToScreenUv(const Vector3& worldPosition, Vector2& screenUv) const;
    void ResetPresentationParameters();
    void ResetPresentationRuntimeState();
    void ResetVisualParameters();
    void ResetPositionParameters();
    void ResetBaseScreenOffsetParameters();
    void ResetBaseScreenOffsetRuntimeState();
    void ResetLeadParameters();
    void ResetLeadState();
    void ResetDepthAppearanceParameters();
    bool HasDrawableTexture() const;

    DirectXCommon* dxCommon_ = nullptr;
    Player* player_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<AimCorridorVisualRenderer> renderer_;

    std::string nearTexturePath_ = "resources/ui/aim_corridor/Aiming_2.png";
    std::string farTexturePath_ = "resources/ui/aim_corridor/Aiming_1.png";

    Vector3 originOffset_{};
    Vector3 aimOrigin_{};
    Vector3 cameraPosition_{};
    Vector3 playerRenderPosition_{};
    Vector3 cameraForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 aimForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 cameraRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 nearCenter_{};
    Vector3 farCenter_{};
    Vector3 nearBaseCenter_{};
    Vector3 mainReticleCenterWorld_{};
    Vector3 farBaseCenter_{};
    Vector3 nearBaseScreenWorldOffset_{};
    Vector3 farBaseScreenWorldOffset_{};
    Vector3 nearFinalScreenWorldOffset_{};
    Vector3 farFinalScreenWorldOffset_{};
    Vector3 nearLeadWorldOffset_{};
    Vector3 farLeadWorldOffset_{};
    Vector2 nearOffset_{};
    Vector2 farOffset_{};
    Vector2 nearBaseScreenOffset_{ 0.0f, 0.09f };
    Vector2 farBaseScreenOffset_{ 0.0f, 0.16f };
    Vector2 nearFinalScreenOffset_{};
    Vector2 farFinalScreenOffset_{};
    Vector2 rawPlayerMoveInput_{};
    Vector2 normalizedMoveInput_{};
    Vector2 nearLeadTargetScreen_{};
    Vector2 nearLeadCurrentScreen_{};
    Vector2 farLeadTargetScreen_{};
    Vector2 farLeadCurrentScreen_{};
    Vector2 forcedMoveInputForDebug_{};
    Vector2 aimOriginScreenUv_{};
    Vector2 mainReticleCenterScreenUv_{};
    AimReticleScreenRect mainReticleScreenRect_{};
    Vector2 playerScreenUv_{};
    Vector2 nearScreenUv_{};
    Vector2 farScreenUv_{};
    Vector2 nearScreenDelta_{};
    Vector2 farScreenDelta_{};
    float nearLeadDepth_ = 0.0f;
    float farLeadDepth_ = 0.0f;
    float leadFovY_ = 0.0f;
    float leadAspectRatio_ = 0.0f;
    float effectiveFarWorldHeight_ = 7.5f;
    float nearProjectedHeightEstimate_ = 0.0f;
    float farProjectedHeightEstimate_ = 0.0f;
    float currentFarToNearScreenHeightRatio_ = 0.0f;
    float targetFarToNearScreenHeightRatio_ = 0.60f;
    float depthAppearanceRatioError_ = 0.0f;
    float mainReticleViewportHeightRatio_ = 0.095f;
    float manualMainReticleWorldHeight_ = 5.0f;
    float effectiveMainReticleWorldHeight_ = 5.0f;
    float effectiveMainReticleWorldWidth_ = 0.0f;
    float estimatedMainReticlePixelWidth_ = 0.0f;
    float estimatedMainReticlePixelHeight_ = 0.0f;
    float mainReticleDepth_ = 0.0f;
    float playerToNearScreenDistance_ = 0.0f;
    float playerToFarScreenDistance_ = 0.0f;
    float nearToFarScreenDistance_ = 0.0f;
    Vector3 frozenPlayerRenderPosition_{};
    Vector3 frozenCameraPosition_{};
    Vector3 frozenCameraForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 frozenCameraRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 frozenCameraUp_{ 0.0f, 1.0f, 0.0f };

    AxisMode axisMode_ = AxisMode::CameraThroughAimOrigin;
    DisplayMode displayMode_ = DisplayMode::MainReticleOnly;
    AimReticleVisualState reticleVisualState_ = AimReticleVisualState::Normal;
    Vector3 currentReticleTint_{ 1.0f, 0.85f, 0.29f };
    float reticleTintAmount_ = 0.0f;
    float targetingLockProgress_ = 0.0f;
    float aimRayLength_ = 0.0f;
    float aimForwardDotCameraForward_ = 1.0f;
    bool aimOriginProjectionValid_ = false;
    bool playerProjectionValid_ = false;
    bool nearProjectionValid_ = false;
    bool farProjectionValid_ = false;

    float nearDistance_ = 28.0f;
    float farDistance_ = 70.0f;
    float nearLeadAmountX_ = 0.055f;
    float nearLeadAmountY_ = 0.045f;
    float farLeadAmountX_ = 0.110f;
    float farLeadAmountY_ = 0.085f;
    float nearResponseTime_ = 0.120f;
    float farResponseTime_ = 0.055f;
    float nearReturnTime_ = 0.160f;
    float farReturnTime_ = 0.110f;
    float nearWorldHeight_ = 5.0f;
    float farWorldHeight_ = 7.5f;
    float nearWorldWidth_ = 5.0f;
    float farWorldWidth_ = 12.0f;
    AppearanceParameters nearAppearance_{};
    AppearanceParameters farAppearance_{};
    AppearanceParameters mainReticleAppearance_{ 0.68f, 1.00f, 0.35f, 0.20f, 1.15f, 0.015f };
    float pulseRate_ = 1.4f;
    float mainReticlePulseRate_ = 1.2f;
    float debugDiamondPreviewScale_ = 1.30f;
    float pulsePhase_ = 0.0f;
    float currentAlpha_ = 0.0f;

    bool initialized_ = false;
    bool enabled_ = true;
    bool gameModeActive_ = false;
    bool playerAlive_ = true;
    bool isVisible_ = false;
    bool freezePulse_ = false;
    bool disableGlow_ = false;
    bool showCoreOnly_ = false;
    bool forceFullAlpha_ = false;
    bool forceShow_ = false;
    bool forceHide_ = false;
    bool leadLagEnabled_ = true;
    bool autoFarSizeFromScreenRatio_ = true;
    bool baseScreenOffsetEnabled_ = true;
    bool baseScreenOffsetConversionValid_ = false;
    bool autoMainReticleSize_ = true;
    bool debugDiamondPreview_ = false;
    bool mainReticleSizeValid_ = false;
    bool mainReticleProjectionValid_ = false;
    bool forwardPlacementActive_ = false;
    bool verticalDirectionNormal_ = false;
    bool depthAppearanceValid_ = false;
    bool useNewDepthDefaults_ = true;
    bool freezeLeadState_ = false;
    bool forceMoveInputForDebug_ = false;
    bool freezePlayerPositionForDebug_ = false;
    bool freezeCameraForDebug_ = false;
    bool worldPlacementValid_ = false;
    uint32_t lastDrawCount_ = 0;
};
