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
    std::vector<BeamVertex> beamVertices_;
    std::vector<BeamVertex> glowVertices_;

    bool enableBeamCore_ = true;
    bool enableOuterParticles_ = true;
    bool enableNozzleGlow_ = true;
    bool useCrossBillboard_ = true;
    bool showBeamDebug_ = false;
    bool exhaustEnabled_ = true;

    float baseBeamLength_ = 1.5f;
    float boostBeamLength_ = 2.8f;
    float beamStartWidth_ = 0.08f;
    float beamEndWidth_ = 0.30f;
    float baseBeamBrightness_ = 1.0f;
    float boostBeamBrightness_ = 1.7f;
    float beamFlickerStrength_ = 0.035f;
    float beamEdgeSoftness_ = 2.8f;
    float beamTipFadePower_ = 1.55f;
    float nozzleGlowSize_ = 0.22f;
    float boostNozzleGlowSize_ = 0.36f;
    float nozzleGlowBrightness_ = 1.1f;
    float boostNozzleGlowBrightness_ = 1.8f;
    float time_ = 0.0f;

    float currentBeamLength_ = 1.5f;
    float currentBeamEndWidth_ = 0.30f;
    float currentBeamBrightness_ = 1.0f;
    float currentGlowSize_ = 0.22f;
    float currentGlowBrightness_ = 1.1f;
    Vector3 currentNozzlePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentBeamEndPosition_{ 0.0f, 0.0f, -1.0f };
    Vector3 currentExhaustDirection_{ 0.0f, 0.0f, -1.0f };
};
