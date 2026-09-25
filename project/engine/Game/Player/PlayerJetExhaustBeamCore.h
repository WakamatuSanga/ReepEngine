#pragma once

#include "Engine/math/Matrix4x4.h"
#include "PlayerJetExhaustBeamRenderer.h"

#include <memory>
#include <vector>

class Camera;
class DirectXCommon;

class PlayerJetExhaustBeamCore {
public:
    bool Initialize(DirectXCommon* dxCommon);
    void Update(
        const Vector3& nozzlePosition,
        const Vector3& exhaustDirection,
        const Vector3& playerRight,
        const Camera* camera,
        float boostPower,
        float deltaTime,
        bool exhaustEnabled);
    void Draw(const Camera* camera, float brightnessScale = 1.0f, float alphaScale = 1.0f);
    void DrawImGui();
    void ApplyCurrentTunedPreset();

    bool IsBeamEnabled() const { return enableBeamCore_; }
    bool IsOuterParticlesEnabled() const { return enableOuterParticles_; }
    float GetCurrentBeamLength() const { return currentBeamLength_; }
    float GetCurrentBeamEndWidth() const { return currentBeamEndWidth_; }
    float GetCurrentBeamBrightness() const { return currentBeamBrightness_; }

private:
    using BeamVertex = PlayerJetExhaustBeamRenderer::Vertex;

    void BuildBeamVertices(const Vector3& side, const Vector3& upLike);
    void BuildGlowVertices(const Vector3& cameraRight, const Vector3& cameraUp);
    void AddQuad(
        std::vector<BeamVertex>& vertices,
        const Vector3& startA,
        const Vector3& startB,
        const Vector3& endA,
        const Vector3& endB);

    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    // Each draw owns its vertex and constant buffers until the frame-end GPU wait.
    std::unique_ptr<PlayerJetExhaustBeamRenderer> glowRenderer_;
    std::vector<BeamVertex> beamVertices_;
    std::vector<BeamVertex> glowVertices_;

    bool enableBeamCore_ = true;
    bool enableOuterParticles_ = true;
    bool enableNozzleGlow_ = true;
    bool useCrossBillboard_ = true;
    bool showBeamDebug_ = false;
    bool exhaustEnabled_ = true;

    float baseBeamLength_ = 1.30f;
    float boostBeamLength_ = 1.68f;
    float beamStartWidth_ = 0.08f;
    float beamEndWidth_ = 0.075f;
    float baseBeamBrightness_ = 2.12f;
    float boostBeamBrightness_ = 3.02f;
    float beamFlickerStrength_ = 0.480f;
    float beamEdgeSoftness_ = 3.28f;
    float beamTipFadePower_ = 2.89f;
    float nozzleGlowSize_ = 0.12f;
    float boostNozzleGlowSize_ = 0.01f;
    float nozzleGlowBrightness_ = 1.22f;
    float boostNozzleGlowBrightness_ = 2.04f;
    float time_ = 0.0f;

    float currentBeamLength_ = 1.30f;
    float currentBeamEndWidth_ = 0.075f;
    float currentBeamBrightness_ = 2.12f;
    float currentGlowSize_ = 0.12f;
    float currentGlowBrightness_ = 1.22f;
    Vector3 currentNozzlePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentBeamEndPosition_{ 0.0f, 0.0f, -1.0f };
    Vector3 currentExhaustDirection_{ 0.0f, 0.0f, -1.0f };
};

