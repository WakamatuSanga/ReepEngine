#include "PlayerJetExhaustController.h"
#include "BoostController.h"
#include "Player.h"
#include "PlayerJetExhaustBeamCore.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/Graphics/Particle/GpuParticleEffectSerializer.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kPi = 3.14159265358979323846f;
    struct VisualBasis {
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 NegateVector3(const Vector3& value) {
        return { -value.x, -value.y, -value.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float LerpFloat(float start, float end, float t) {
        return start + (end - start) * t;
    }

    Vector4 ScaleColor(const Vector4& value, float brightness) {
        return {
            std::clamp(value.x * brightness, 0.0f, 4.0f),
            std::clamp(value.y * brightness, 0.0f, 4.0f),
            std::clamp(value.z * brightness, 0.0f, 4.0f),
            std::clamp(value.w, 0.0f, 1.0f),
        };
    }

    Vector3 MakeLineRotation(const Vector3& direction) {
        const Vector3 normalized = Normalize(direction, { 0.0f, 0.0f, 1.0f });
        const float yaw = std::atan2(normalized.x, normalized.z);
        const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
        const float pitch = std::atan2(-normalized.y, horizontal);
        return { pitch, yaw, 0.0f };
    }

    VisualBasis MakeBasisFromRotation(const Vector3& rotation, const Vector3& fallbackForward) {
        const Matrix4x4 matrix = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, rotation, { 0.0f, 0.0f, 0.0f });
        VisualBasis basis;
        basis.right = Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
        basis.up = Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
        basis.forward = Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, fallbackForward);
        return basis;
    }

    void ApplyModelMaterial(Model* model, const Vector4& color) {
        if (!model) {
            return;
        }
        if (Model::Material* material = model->GetMaterialData()) {
            material->color = color;
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }

    GpuParticle::ParticleType MakeJetParticleType(
        const char* name,
        const Vector4& baseColor,
        const Vector4& startColor,
        const Vector4& endColor,
        float startScale,
        float endScale,
        float lifeMin,
        float lifeMax,
        float speedMin,
        float speedMax,
        bool affectedByRailFlow,
        float railFlowScale) {
        GpuParticle::ParticleType type;
        type.name = name;
        type.texturePath = GpuParticle::kFallbackParticleTexturePath;
        type.baseColor = baseColor;
        type.startColor = startColor;
        type.endColor = endColor;
        type.startScale = startScale;
        type.endScale = endScale;
        type.lifeTimeMin = lifeMin;
        type.lifeTimeMax = lifeMax;
        type.speedMin = speedMin;
        type.speedMax = speedMax;
        type.gravity = 0.0f;
        type.drag = 0.85f;
        type.enablePhysics = false;
        type.enablePlaneCollision = false;
        type.killBelowPlane = false;
        type.affectedByInfluenceField = false;
        type.influenceResponseScale = 0.0f;
        type.affectedByRailFlow = affectedByRailFlow;
        type.railFlowScale = railFlowScale;
        GpuParticle::NormalizeParticleEffectType(type);
        return type;
    }

    GpuParticle::Emitter MakeJetEmitter(
        const Vector3& position,
        const Vector3& direction,
        float coneAngleDegrees,
        float coneHeight,
        float emissionRate,
        uint32_t seed,
        uint32_t particleTypeIndex) {
        const float safeConeHeight = (std::max)(coneHeight, 0.001f);
        const float coneAngleRadians = std::clamp(coneAngleDegrees, 1.0f, 70.0f) * (kPi / 180.0f);
        GpuParticle::Emitter emitter;
        emitter.enabled = true;
        emitter.position = position;
        emitter.direction = Normalize(direction, { 0.0f, -1.0f, 0.0f });
        emitter.shape = GpuParticle::EmitterShape::Cone;
        emitter.radius = std::tan(coneAngleRadians) * safeConeHeight;
        emitter.coneHeight = safeConeHeight;
        emitter.boxSize = { 0.05f, 0.05f, 0.05f };
        emitter.emitCount = 96;
        emitter.emitInterval = 0.0f;
        emitter.emissionRate = (std::max)(emissionRate, 0.0f);
        emitter.randomSeed = seed;
        emitter.particleTypeIndex = particleTypeIndex;
        return emitter;
    }

    GpuParticle::ParticleEffectData MakeDefaultJetEffectData() {
        GpuParticle::ParticleEffectData data;
        data.runtime.randomEnabled = true;
        data.runtime.useFreeListEmit = true;
        data.runtime.generateUnusedList = true;
        data.runtime.useDeadList = true;
        data.runtime.autoRecycleDeadList = true;
        data.runtime.autoReuseDeadParticles = true;
        data.runtime.updateEnabled = true;
        data.runtime.maxActiveParticles = 1024;
        data.runtime.maxEmitPerFrame = 64;
        data.particleTypes.push_back(MakeJetParticleType(
            "JetCore",
            { 1.0f, 0.95f, 0.68f, 0.90f },
            { 1.0f, 0.96f, 0.72f, 0.88f },
            { 1.0f, 0.22f, 0.03f, 0.0f },
            0.055f,
            0.020f,
            0.16f,
            0.28f,
            10.2f,
            13.8f,
            false,
            0.0f));
        data.particleTypes.push_back(MakeJetParticleType(
            "JetOuterFlame",
            { 1.0f, 0.32f, 0.035f, 0.20f },
            { 1.0f, 0.38f, 0.045f, 0.18f },
            { 0.95f, 0.05f, 0.01f, 0.0f },
            0.032f,
            0.180f,
            0.010f,
            0.390f,
            3.23f,
            4.37f,
            false,
            0.0f));
        data.emitters.push_back(MakeJetEmitter({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 10.1f, 0.160f, 0.0f, 101u, 0u));
        data.emitters.back().enabled = false;
        data.emitters.push_back(MakeJetEmitter({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 10.1f, 0.160f, 2003.0f, 211u, 1u));
        GpuParticle::NormalizeParticleEffectData(data);
        return data;
    }
}

PlayerJetExhaustController::PlayerJetExhaustController() = default;
PlayerJetExhaustController::~PlayerJetExhaustController() = default;

bool PlayerJetExhaustController::Initialize(
    Object3dCommon* object3dCommon,
    Camera* camera,
    Player* player,
    BoostController* boostController,
    DirectXCommon* dxCommon,
    SrvManager* srvManager) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    player_ = player;
    boostController_ = boostController;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    if (!object3dCommon_ || !camera_ || !player_ || !dxCommon_ || !srvManager_) {
        loadStatus_ = "Missing dependencies";
        return false;
    }

    particleSystem_ = std::make_unique<GpuParticleSystem>();
    if (!particleSystem_->Initialize(dxCommon_, srvManager_)) {
        loadStatus_ = "Failed to initialize GpuParticleSystem";
        return false;
    }
    beamCore_ = std::make_unique<PlayerJetExhaustBeamCore>();
    if (!beamCore_->Initialize(dxCommon_)) {
        loadStatus_ = "Failed to initialize Jet Exhaust Beam Core";
        return false;
    }
    LoadPreset();
    return true;
}

void PlayerJetExhaustController::Finalize() {
    nozzleDebugObject_.reset();
    directionDebugObject_.reset();
    nozzleDebugModel_ = nullptr;
    directionDebugModel_ = nullptr;
    beamCore_.reset();
    particleSystem_.reset();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    boostController_ = nullptr;
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

bool PlayerJetExhaustController::LoadPreset() {
    if (!particleSystem_) {
        return false;
    }

    GpuParticle::ParticleEffectData data;
    if (std::filesystem::exists(std::filesystem::path(presetPath_)) &&
        GpuParticle::GpuParticleEffectSerializer::Load(presetPath_, data)) {
        loadStatus_ = "Loaded preset: " + presetPath_;
    } else {
        data = MakeDefaultJetEffectData();
        loadStatus_ = "Using built-in default. Missing or invalid preset: " + presetPath_;
    }

    if (data.emitters.size() < 2) {
        data.emitters.push_back(MakeJetEmitter({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 10.1f, 0.160f, 2003.0f, 211u, 1u));
    }
    if (data.particleTypes.size() < 2) {
        data.particleTypes.push_back(MakeJetParticleType(
            "JetOuterFlame",
            { 1.0f, 0.32f, 0.035f, 0.20f },
            { 1.0f, 0.38f, 0.045f, 0.18f },
            { 0.95f, 0.05f, 0.01f, 0.0f },
            outerStartSize_,
            outerEndSize_,
            outerLifeTimeMin_,
            outerLifeTimeMax_,
            outerExhaustSpeed_ * 0.85f,
            outerExhaustSpeed_ * 1.15f,
            false,
            0.0f));
    }
    generateUnusedList_ = data.runtime.useFreeListEmit || data.runtime.generateUnusedList;
    useDeadList_ = data.runtime.useDeadList;
    autoReuseDeadParticles_ = data.runtime.autoRecycleDeadList || data.runtime.autoReuseDeadParticles;
    maxActiveExhaustParticles_ = std::clamp(data.runtime.maxActiveParticles, 1u, GpuParticle::kParticleCount);
    maxEmitPerFrame_ = std::clamp(data.runtime.maxEmitPerFrame, 1u, GpuParticle::kParticleCount);
    maxOuterEmitPerFrame_ = (std::min)(maxOuterEmitPerFrame_, maxEmitPerFrame_);
    maxCoreEmitPerFrame_ = (std::min)(maxCoreEmitPerFrame_, maxEmitPerFrame_);
    data.runtime.useFreeListEmit = generateUnusedList_;
    data.runtime.generateUnusedList = generateUnusedList_;
    data.runtime.useDeadList = useDeadList_;
    data.runtime.autoRecycleDeadList = autoReuseDeadParticles_;
    data.runtime.autoReuseDeadParticles = autoReuseDeadParticles_;
    data.runtime.updateEnabled = true;
    data.runtime.maxActiveParticles = maxActiveExhaustParticles_;
    data.runtime.maxEmitPerFrame = maxEmitPerFrame_;
    GpuParticle::NormalizeParticleEffectData(data);
    particleSystem_->ApplyEffectData(data);
    return true;
}

void PlayerJetExhaustController::Update(float deltaTime) {
    ++updateCount_;
    if (!particleSystem_ || !player_ || !camera_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const float targetBoostPower = boostController_ ? std::clamp(boostController_->GetCurrentBoostPower(), 0.0f, 1.0f) : 0.0f;
    const float smoothT = std::clamp(safeDeltaTime * boostSmoothSpeed_, 0.0f, 1.0f);
    smoothedBoostPower_ = LerpFloat(smoothedBoostPower_, targetBoostPower, smoothT);

    currentLengthMultiplier_ = LerpFloat(1.0f, boostLengthMultiplier_, smoothedBoostPower_);
    currentSpeedMultiplier_ = LerpFloat(1.0f, boostSpeedMultiplier_, smoothedBoostPower_);
    currentSpawnRateMultiplier_ = LerpFloat(1.0f, boostSpawnRateMultiplier_, smoothedBoostPower_);
    currentBrightness_ = brightness_ * LerpFloat(1.0f, boostBrightnessMultiplier_, smoothedBoostPower_);

    const VisualBasis basis = MakeBasisFromRotation(player_->GetVisualModelRotation(), player_->GetBaseForward());
    currentNozzlePosition_ = AddVector3(
        AddVector3(
            AddVector3(player_->GetWorldPosition(), ScaleVector3(basis.forward, -nozzleBackOffset_)),
            ScaleVector3(basis.up, nozzleUpOffset_)),
        ScaleVector3(basis.right, nozzleSideOffset_));
    currentExhaustDirection_ = invertExhaustDirection_ ? basis.forward : NegateVector3(basis.forward);
    currentExhaustDirection_ = Normalize(currentExhaustDirection_, NegateVector3(player_->GetBaseForward()));
    const bool shouldEmit = enableJetExhaust_ && (!hideWhenPlayerDead_ || isPlayerAlive_);
    if (beamCore_) {
        beamCore_->Update(
            currentNozzlePosition_,
            currentExhaustDirection_,
            basis.right,
            camera_,
            smoothedBoostPower_,
            safeDeltaTime,
            shouldEmit);
    }

    ApplyRuntimeSettings(safeDeltaTime);
    particleSystem_->SetDeltaTime(safeDeltaTime);
    particleSystem_->SetParticleInfluenceEnabled(false);
    particleSystem_->SetRailParticleFlow(
        affectedByRailFlow_,
        camera_->GetTranslate(),
        currentExhaustDirection_,
        exhaustSpeed_ * currentSpeedMultiplier_,
        railFlowScale_,
        24.0f,
        8.0f);
    particleSystem_->Update(camera_);
    UpdateDebugObjects();
}

void PlayerJetExhaustController::ApplyRuntimeSettings(float deltaTime) {
    NormalizeFlameRanges();
    if (particleSystem_) {
        particleSystem_->SetRuntimePoolOptions(generateUnusedList_, useDeadList_, autoReuseDeadParticles_);
        particleSystem_->SetRuntimeParticleLimits(maxActiveExhaustParticles_, maxEmitPerFrame_);
        particleSystem_->SetCounterReadbackEnabled(autoReadbackPoolCounters_);
    }
    const bool shouldEmit = enableJetExhaust_ && (!hideWhenPlayerDead_ || isPlayerAlive_);
    const bool shouldEmitOuterParticles = shouldEmit && (!beamCore_ || beamCore_->IsOuterParticlesEnabled());
    const float coreSpeed = exhaustSpeed_ * currentSpeedMultiplier_;
    const float outerSpeed = outerExhaustSpeed_ * currentSpeedMultiplier_;
    const float lifeScale = currentLengthMultiplier_;
    currentCoreSpawnRate_ = 0.0f;
    currentOuterSpawnRate_ = shouldEmitOuterParticles
        ? LimitEmissionRateForFrame(outerSpawnRate_ * currentSpawnRateMultiplier_, maxOuterEmitPerFrame_, deltaTime)
        : 0.0f;

    GpuParticle::ParticleType coreType = MakeJetParticleType(
        "JetCore",
        ScaleColor({ 1.0f, 0.95f, 0.68f, 0.90f }, currentBrightness_),
        ScaleColor({ 1.0f, 0.96f, 0.72f, 0.88f }, currentBrightness_),
        { 1.0f, 0.22f, 0.03f, 0.0f },
        coreStartSize_,
        coreEndSize_,
        lifeTimeMin_ * lifeScale,
        lifeTimeMax_ * lifeScale,
        coreSpeed * 0.85f,
        coreSpeed * 1.15f,
        affectedByRailFlow_,
        railFlowScale_);
    GpuParticle::ParticleType outerType = MakeJetParticleType(
        "JetOuterFlame",
        ScaleColor({ 1.0f, 0.32f, 0.035f, 0.20f * outerParticleAlphaScale_ }, currentBrightness_),
        ScaleColor({ 1.0f, 0.38f, 0.045f, 0.18f * outerParticleAlphaScale_ }, currentBrightness_),
        { 0.95f, 0.05f, 0.01f, 0.0f },
        outerStartSize_ * outerParticleScale_,
        outerEndSize_ * outerParticleScale_,
        outerLifeTimeMin_ * lifeScale,
        outerLifeTimeMax_ * lifeScale,
        outerSpeed * 0.85f,
        outerSpeed * 1.15f,
        affectedByRailFlow_,
        railFlowScale_);
    particleSystem_->SetParticleTypeRuntime(0, coreType);
    particleSystem_->SetParticleTypeRuntime(1, outerType);

    GpuParticle::Emitter coreEmitter = MakeJetEmitter(
        currentNozzlePosition_,
        currentExhaustDirection_,
        coneAngleDegrees_,
        emitterConeHeight_,
        0.0f,
        101u,
        0u);
    coreEmitter.enabled = false;
    GpuParticle::Emitter outerEmitter = MakeJetEmitter(
        currentNozzlePosition_,
        currentExhaustDirection_,
        coneAngleDegrees_ * 1.10f,
        emitterConeHeight_ * 1.10f,
        currentOuterSpawnRate_,
        211u,
        1u);
    outerEmitter.enabled = shouldEmitOuterParticles;
    particleSystem_->SetEmitterRuntime(0, coreEmitter);
    particleSystem_->SetEmitterRuntime(1, outerEmitter);
}

void PlayerJetExhaustController::Draw() {
    DrawLayer(false);
}

void PlayerJetExhaustController::DrawAfterCloud() {
    DrawLayer(true);
}

void PlayerJetExhaustController::DrawLayer(bool afterCloudLayer) {
    if (afterCloudLayer != drawAfterCloud_) {
        return;
    }

    const float brightnessScale = afterCloudLayer ? afterCloudBrightnessScale_ : 1.0f;
    const float alphaScale = afterCloudLayer ? afterCloudAlphaScale_ : 1.0f;
    if (beamCore_) {
        beamCore_->Draw(camera_, brightnessScale, alphaScale);
    }
    if (particleSystem_ && (!beamCore_ || beamCore_->IsOuterParticlesEnabled())) {
        particleSystem_->Draw();
    }

    if (afterCloudLayer || !debugVisualsEnabled_ || !showDebugVisuals_ || !object3dCommon_) {
        return;
    }
    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    if (nozzleDebugObject_) {
        nozzleDebugObject_->Draw();
    }
    if (directionDebugObject_) {
        directionDebugObject_->Draw();
    }
}

void PlayerJetExhaustController::EnsureDebugObjects() {
    if (!object3dCommon_ || !camera_) {
        return;
    }
    ModelManager* modelManager = ModelManager::GetInstance();
    if (!nozzleDebugModel_) {
        nozzleDebugModel_ = modelManager->CreateSphere("PlayerJetExhaustNozzleDebug", 12);
        ApplyModelMaterial(nozzleDebugModel_, { 1.0f, 0.8f, 0.15f, 0.65f });
    }
    if (!directionDebugModel_) {
        directionDebugModel_ = modelManager->CreateBox("PlayerJetExhaustDirectionDebug");
        ApplyModelMaterial(directionDebugModel_, { 1.0f, 0.35f, 0.05f, 0.42f });
    }
    if (!nozzleDebugObject_) {
        nozzleDebugObject_ = std::make_unique<Object3d>();
        nozzleDebugObject_->Initialize(object3dCommon_);
        nozzleDebugObject_->SetCamera(camera_);
        nozzleDebugObject_->SetEnvironmentMapEnabled(false);
        nozzleDebugObject_->SetModel(nozzleDebugModel_);
    }
    if (!directionDebugObject_) {
        directionDebugObject_ = std::make_unique<Object3d>();
        directionDebugObject_->Initialize(object3dCommon_);
        directionDebugObject_->SetCamera(camera_);
        directionDebugObject_->SetEnvironmentMapEnabled(false);
        directionDebugObject_->SetModel(directionDebugModel_);
    }
}

void PlayerJetExhaustController::UpdateDebugObjects() {
    if (!debugVisualsEnabled_ || !showDebugVisuals_) {
        return;
    }
    EnsureDebugObjects();
    if (nozzleDebugObject_) {
        nozzleDebugObject_->SetTranslate(currentNozzlePosition_);
        nozzleDebugObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        nozzleDebugObject_->SetScale({ 0.06f, 0.06f, 0.06f });
        nozzleDebugObject_->Update();
    }
    if (directionDebugObject_) {
        const Vector3 end = AddVector3(currentNozzlePosition_, ScaleVector3(currentExhaustDirection_, debugDirectionLength_));
        const Vector3 diff = AddVector3(end, NegateVector3(currentNozzlePosition_));
        directionDebugObject_->SetTranslate(AddVector3(currentNozzlePosition_, ScaleVector3(diff, 0.5f)));
        directionDebugObject_->SetRotate(MakeLineRotation(diff));
        directionDebugObject_->SetScale({ 0.025f, 0.025f, (std::max)(Length(diff) * 0.5f, 0.001f) });
        directionDebugObject_->Update();
    }
}

void PlayerJetExhaustController::NormalizeFlameRanges() {
    lifeTimeMin_ = (std::max)(lifeTimeMin_, 0.01f);
    lifeTimeMax_ = (std::max)(lifeTimeMax_, 0.01f);
    if (lifeTimeMin_ > lifeTimeMax_) {
        std::swap(lifeTimeMin_, lifeTimeMax_);
    }
    outerLifeTimeMin_ = (std::max)(outerLifeTimeMin_, 0.01f);
    outerLifeTimeMax_ = (std::max)(outerLifeTimeMax_, 0.01f);
    if (outerLifeTimeMin_ > outerLifeTimeMax_) {
        std::swap(outerLifeTimeMin_, outerLifeTimeMax_);
    }
}

float PlayerJetExhaustController::LimitEmissionRateForFrame(float emissionRate, uint32_t maxEmitPerFrame, float deltaTime) const {
    if (emissionRate <= 0.0f || deltaTime <= 0.0f) {
        return 0.0f;
    }
    const float safeDeltaTime = (std::max)(deltaTime, 1.0f / 240.0f);
    const float frameLimitRate = static_cast<float>(std::clamp(maxEmitPerFrame, 1u, GpuParticle::kParticleCount)) / safeDeltaTime;
    return (std::min)(emissionRate, frameLimitRate);
}

void PlayerJetExhaustController::ApplyCurrentTunedPreset() {
    nozzleBackOffset_ = 0.22f;
    nozzleUpOffset_ = -0.02f;
    nozzleSideOffset_ = 0.0f;
    invertExhaustDirection_ = false;
    coneAngleDegrees_ = 10.1f;
    emitterConeHeight_ = 0.160f;
    spawnRate_ = 260.0f;
    exhaustSpeed_ = 12.0f;
    lifeTimeMin_ = 0.160f;
    lifeTimeMax_ = 0.280f;
    coreStartSize_ = 0.055f;
    coreEndSize_ = 0.020f;
    outerSpawnRate_ = 2003.0f;
    outerExhaustSpeed_ = 3.8f;
    outerLifeTimeMin_ = 0.010f;
    outerLifeTimeMax_ = 0.390f;
    outerStartSize_ = 0.032f;
    outerEndSize_ = 0.180f;
    outerParticleScale_ = 2.00f;
    outerParticleAlphaScale_ = 0.38f;
    brightness_ = 1.21f;
    boostLengthMultiplier_ = 1.80f;
    boostSpeedMultiplier_ = 1.65f;
    boostSpawnRateMultiplier_ = 1.75f;
    boostBrightnessMultiplier_ = 1.55f;
    affectedByRailFlow_ = false;
    railFlowScale_ = 0.0f;
    drawAfterCloud_ = true;
    afterCloudAlphaScale_ = 1.00f;
    afterCloudBrightnessScale_ = 0.69f;
    generateUnusedList_ = true;
    useDeadList_ = true;
    autoReuseDeadParticles_ = true;
    maxActiveExhaustParticles_ = 1024;
    maxEmitPerFrame_ = 64;
    maxOuterEmitPerFrame_ = 64;
    maxCoreEmitPerFrame_ = 16;
    if (beamCore_) {
        beamCore_->ApplyCurrentTunedPreset();
    }
    NormalizeFlameRanges();
}

void PlayerJetExhaustController::ApplyLightweightPreset() {
    outerSpawnRate_ = 120.0f;
    outerLifeTimeMin_ = 0.08f;
    outerLifeTimeMax_ = 0.18f;
    outerStartSize_ = 0.04f;
    outerEndSize_ = 0.13f;
    outerParticleScale_ = 1.0f;
    outerParticleAlphaScale_ = 0.35f;
    maxActiveExhaustParticles_ = 512;
    maxEmitPerFrame_ = 48;
    maxOuterEmitPerFrame_ = 48;
    maxCoreEmitPerFrame_ = 12;
    generateUnusedList_ = true;
    useDeadList_ = true;
    autoReuseDeadParticles_ = true;
    NormalizeFlameRanges();
}

void PlayerJetExhaustController::ResetToRecommendedSettings() {
    ApplyLightweightPreset();
}







