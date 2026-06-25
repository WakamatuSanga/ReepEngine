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
    void Draw(const Camera* camera);
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

    float baseBeamLength_ = 1.8f;
    float boostBeamLength_ = 3.4f;
    float beamStartWidth_ = 0.12f;
    float beamEndWidth_ = 0.45f;
    float baseBeamBrightness_ = 1.2f;
    float boostBeamBrightness_ = 2.0f;
    float beamFlickerStrength_ = 0.12f;
    float nozzleGlowSize_ = 0.35f;
    float boostNozzleGlowSize_ = 0.55f;
    float nozzleGlowBrightness_ = 1.5f;
    float boostNozzleGlowBrightness_ = 2.5f;
    float time_ = 0.0f;

    float currentBeamLength_ = 1.8f;
    float currentBeamEndWidth_ = 0.45f;
    float currentBeamBrightness_ = 1.2f;
    float currentGlowSize_ = 0.35f;
    float currentGlowBrightness_ = 1.5f;
    Vector3 currentNozzlePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentBeamEndPosition_{ 0.0f, 0.0f, -1.0f };
    Vector3 currentExhaustDirection_{ 0.0f, 0.0f, -1.0f };
};
