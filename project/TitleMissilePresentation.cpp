#include "TitleMissilePresentation.h"
#include "Engine/Game/Player/LockedWingMissileExhaustController.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kFirstLaunchDelay = 2.0f;
    constexpr float kLaunchInterval = 4.0f;
    constexpr Vector3 kLocalLaunchPosition{ 0.0f, -0.45f, 0.20f };
    constexpr float kMissileSpeed = 2.5f;
    constexpr float kMissileLifetime = 2.5f;
    // Asset length is 2 model units. Its rear is thus 0.20 behind the center,
    // matching the existing missile exhaust controller's nozzle offset.
    constexpr float kMissileScale = 0.20f;
    constexpr float kModelYawOffset = 4.71238899f; // Same +X asset correction as gameplay.
    constexpr const char* kModelPath = "resources/EnemyBullet/EnemyBullet.obj";
}

TitleMissilePresentation::TitleMissilePresentation() = default;
TitleMissilePresentation::~TitleMissilePresentation() = default;

void TitleMissilePresentation::Initialize(Object3dCommon* objectCommon,
    DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera) {
    Finalize();
    objectCommon_ = objectCommon;
    nextLaunchRemaining_ = kFirstLaunchDelay;
    auto* models = ModelManager::GetInstance();
    models->LoadModel(kModelPath);
    auto* model = models->FindModel(kModelPath);
    if (!model) return;

    for (auto& missile : missiles_) {
        missile.object = std::make_unique<Object3d>();
        missile.object->Initialize(objectCommon);
        missile.object->SetCamera(camera);
        missile.object->SetModel(model); // ModelManager retains ownership.
        missile.object->SetEnvironmentMapEnabled(false);
        missile.object->SetScale({ kMissileScale, kMissileScale, kMissileScale });
    }
    exhaust_ = std::make_unique<LockedWingMissileExhaustController>();
    if (!exhaust_->Initialize(dxCommon, srvManager, camera)) {
        exhaust_->Finalize();
        exhaust_.reset();
    }
}

void TitleMissilePresentation::Finalize() {
    if (exhaust_) exhaust_->Finalize();
    exhaust_.reset();
    for (auto& missile : missiles_) missile = Missile{};
    objectCommon_ = nullptr;
    nextLaunchRemaining_ = kFirstLaunchDelay;
}

void TitleMissilePresentation::Launch(const Transform& aircraftTransform) {
    auto available = std::find_if(missiles_.begin(), missiles_.end(),
        [](const Missile& missile) { return missile.object && !missile.active; });
    if (available == missiles_.end()) return;
    auto& missile = *available;
    const Matrix4x4 world = MatrixMath::MakeAffine(aircraftTransform.scale,
        aircraftTransform.rotate, aircraftTransform.translate);
    const auto& p = kLocalLaunchPosition;
    missile.position = {
        p.x * world.m[0][0] + p.y * world.m[1][0] + p.z * world.m[2][0] + world.m[3][0],
        p.x * world.m[0][1] + p.y * world.m[1][1] + p.z * world.m[2][1] + world.m[3][1],
        p.x * world.m[0][2] + p.y * world.m[1][2] + p.z * world.m[2][2] + world.m[3][2] };
    const Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
    const float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (length <= 0.00001f) return;
    missile.forward = { forward.x / length, forward.y / length, forward.z / length };
    const float yaw = std::atan2(missile.forward.x, missile.forward.z);
    const float pitch = std::atan2(-missile.forward.y,
        std::sqrt(missile.forward.x * missile.forward.x + missile.forward.z * missile.forward.z));
    missile.object->SetRotate({ pitch, yaw + kModelYawOffset, 0.0f });
    missile.object->SetTranslate(missile.position);
    missile.object->Update();
    missile.age = 0.0f;
    missile.active = true;
    missile.exhaustHandle = exhaust_ ? exhaust_->Start(0, missile.position, missile.forward) : 0;
}

void TitleMissilePresentation::Update(float deltaTime, const Transform& aircraftTransform) {
    const float dt = (std::max)(deltaTime, 0.0f);
    for (auto& missile : missiles_) {
        if (!missile.active) continue;
        missile.age += dt;
        if (missile.age >= kMissileLifetime) {
            if (exhaust_) exhaust_->Stop(missile.exhaustHandle);
            missile.exhaustHandle = 0;
            missile.active = false;
            continue;
        }
        missile.position.x += missile.forward.x * kMissileSpeed * dt;
        missile.position.y += missile.forward.y * kMissileSpeed * dt;
        missile.position.z += missile.forward.z * kMissileSpeed * dt;
        missile.object->SetTranslate(missile.position);
        missile.object->Update();
        if (exhaust_) exhaust_->UpdateMissile(missile.exhaustHandle, missile.position, missile.forward);
    }
    nextLaunchRemaining_ -= dt;
    if (nextLaunchRemaining_ <= 0.0f) {
        Launch(aircraftTransform);
        nextLaunchRemaining_ += kLaunchInterval;
    }
    if (exhaust_) exhaust_->Update(dt);
}

void TitleMissilePresentation::DrawModels() {
    if (!objectCommon_) return;
    objectCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    for (const auto& missile : missiles_) {
        if (missile.active) missile.object->Draw();
    }
}

void TitleMissilePresentation::DrawExhaust() {
    if (exhaust_) exhaust_->Draw();
}
