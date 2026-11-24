#include "EnemyBreakupEffect.h"

#include "Engine/Graphics/Appearance/RenderStyleComparison.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kLifetime = 1.25f;
    constexpr float kShrinkStart = 1.02f;
    constexpr float kGravity = 9.8f;
    constexpr float kWindDelay = 0.085f;
    constexpr float kWindSpeed = 18.0f;
    constexpr float kWindResponse = 4.5f;
    constexpr float kLaunchRamp = 0.06f;

    struct FragmentTuning {
        const char* name;
        float pivotZ;
        Vector3 separationSpeed;
        Vector3 spin;
        float separationDelay;
    };

    // The aircraft assets are centered at these pivots in Enemy.obj's local space.
    constexpr std::array kFragments{
        FragmentTuning{ "Left", -0.82f, { -0.25f, 0.5f, -3.2f }, { -5.6f, 3.7f, -1.8f }, 0.018f },
        FragmentTuning{ "Body", 0.0f, { 0.3f, -0.25f, 0.05f }, { 3.7f, -2.8f, 2.4f }, 0.085f },
        FragmentTuning{ "Right", 0.82f, { -0.15f, 0.35f, 3.4f }, { 5.1f, -3.4f, 2.2f }, 0.018f },
    };

    Vector3 Scale(const Vector3& vector, float amount) {
        return { vector.x * amount, vector.y * amount, vector.z * amount };
    }

    Vector3 Add(const Vector3& left, const Vector3& right) {
        return { left.x + right.x, left.y + right.y, left.z + right.z };
    }

    Vector3 TransformDirection(const Vector3& vector, const Matrix4x4& matrix) {
        return {
            vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0],
            vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1],
            vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2],
        };
    }

    float Smooth01(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    Model* LoadPiece(ModelManager& manager, const std::string& path) {
        manager.LoadModel(path);
        return manager.FindModel(path);
    }
}

EnemyBreakupEffect::EnemyBreakupEffect(Object3dCommon* objects, Camera* camera)
    : objects_(objects), camera_(camera) {}

EnemyBreakupEffect::~EnemyBreakupEffect() = default;

bool EnemyBreakupEffect::Prepare() {
    if (prepared_) {
        return true;
    }
    if (!objects_ || !camera_) {
        return false;
    }

    auto& models = *ModelManager::GetInstance();
    auto& textures = *TextureManager::GetInstance();
    const std::string enemyTexture = "resources/Enemy/textures/Material.003_baseColor.png";
    const std::string whiteTexture = "resources/human/white.png";
    textures.LoadTexture(enemyTexture);
    textures.LoadTexture(whiteTexture);
    const auto enemyTextureIndex = textures.GetTextureIndexByFilePath(enemyTexture);
    const auto whiteTextureIndex = textures.GetTextureIndexByFilePath(whiteTexture);

    for (size_t index = 0; index < kFragmentCount; ++index) {
        const std::string stem = std::string("resources/Enemy/EnemyBreakup") + kFragments[index].name;
        shells_[index] = LoadPiece(models, stem + ".obj");
        cutFaces_[index] = LoadPiece(models, stem + "Cut.obj");
        if (!shells_[index] || !cutFaces_[index] || !cutFaces_[index]->GetMaterialData()) {
            return false;
        }
        shells_[index]->SetTextureIndex(enemyTextureIndex);
        cutFaces_[index]->SetTextureIndex(whiteTextureIndex);
        cutFaces_[index]->GetMaterialData()->color = { 0.12f, 0.11f, 0.11f, 1.0f };
    }

    box_ = models.CreateBox("stage1-enemy-breakup-proxy");
    if (!box_ || !box_->GetMaterialData()) {
        return false;
    }
    box_->SetTextureIndex(whiteTextureIndex);
    box_->GetMaterialData()->color = { 0.45f, 0.11f, 0.09f, 1.0f };

    // Like the player death fragments, reuse initialized Object3d instances.
    for (auto& breakup : breakups_) {
        for (auto& fragment : breakup.fragments) {
            fragment.object = std::make_unique<Object3d>();
            fragment.object->Initialize(objects_);
            if (!fragment.object->IsValid()) {
                return false;
            }
            fragment.object->SetCamera(camera_);
            fragment.object->SetStyleTarget(RenderStyleComparison::Target::NormalEnemy);
            fragment.object->SetEnvironmentMapEnabled(false);
        }
    }
    prepared_ = true;
    return true;
}

bool EnemyBreakupEffect::Spawn(const EnemyBreakupPose& pose) {
    if (!prepared_) {
        return false;
    }
    auto slot = std::find_if(breakups_.begin(), breakups_.end(), [](const Breakup& breakup) {
        return !breakup.active;
    });
    if (slot == breakups_.end()) {
        ++droppedCount_;
        return false;
    }

    slot->pose = pose;
    slot->orientation = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, pose.transform.rotate, {});
    slot->windVelocity = {};
    slot->age = 0.0f;
    slot->firstUpdate = true;
    slot->active = true;
    for (size_t index = 0; index < kFragmentCount; ++index) {
        auto& fragment = slot->fragments[index];
        const float pivotZ = pose.boxVisual
            ? (static_cast<float>(index) - 1.0f) * (2.0f / 3.0f)
            : kFragments[index].pivotZ;
        const Vector3 offset = TransformDirection({ 0.0f, 0.0f, pivotZ * pose.transform.scale.z }, slot->orientation);
        fragment.position = Add(pose.transform.translate, offset);
        fragment.velocity = pose.velocity;
        UpdateFragment(*slot, index, 0.0f, {});
    }
    return true;
}

void EnemyBreakupEffect::Update(float deltaTime) {
    const float step = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    for (auto& breakup : breakups_) {
        if (!breakup.active) {
            continue;
        }
        const float previousAge = breakup.age;
        // Preserve the assembled silhouette for the first rendered frame.
        if (!breakup.firstUpdate) {
            breakup.age += step;
        }
        breakup.firstUpdate = false;
        if (breakup.age >= kLifetime) {
            breakup.active = false;
            continue;
        }
        const float windStep = (std::max)(breakup.age - kWindDelay, 0.0f)
            - (std::max)(previousAge - kWindDelay, 0.0f);
        const Vector3 previousWindVelocity = breakup.windVelocity;
        const float windDamping = std::exp(-kWindResponse * windStep);
        breakup.windVelocity = Add(Scale(previousWindVelocity, windDamping),
            Scale(wind_, kWindSpeed * (1.0f - windDamping)));
        const Vector3 windDisplacement = Scale(Add(previousWindVelocity, breakup.windVelocity), 0.5f * windStep);
        for (size_t index = 0; index < kFragmentCount; ++index) {
            UpdateFragment(breakup, index, previousAge, windDisplacement);
        }
    }
}

void EnemyBreakupEffect::UpdateFragment(Breakup& breakup, size_t index, float previousAge, const Vector3& windDisplacement) {
    auto& fragment = breakup.fragments[index];
    const auto& tuning = kFragments[index];
    const float step = breakup.age - previousAge;
    const float launchAge = (std::max)(breakup.age - tuning.separationDelay, 0.0f);
    const float previousLaunchAge = (std::max)(previousAge - tuning.separationDelay, 0.0f);
    const float launchBlend = Smooth01(launchAge / kLaunchRamp);
    const float launchChange = launchBlend - Smooth01(previousLaunchAge / kLaunchRamp);

    const Vector3 oldVelocity = fragment.velocity;
    fragment.velocity = Add(fragment.velocity,
        TransformDirection(Scale(tuning.separationSpeed, launchChange), breakup.orientation));
    fragment.velocity.y -= kGravity * step;
    const Vector3 ballisticDisplacement = Scale(Add(oldVelocity, fragment.velocity), 0.5f * step);
    fragment.position = Add(fragment.position, Add(ballisticDisplacement, windDisplacement));

    const float shrink = 1.0f - Smooth01((breakup.age - kShrinkStart) / (kLifetime - kShrinkStart));
    Vector3 scale = Scale(breakup.pose.transform.scale, (std::max)(shrink, 0.001f));
    if (breakup.pose.boxVisual) {
        scale.z /= 3.0f;
    }
    Vector3 rotation = breakup.pose.transform.rotate;
    if (launchAge > 0.0f) {
        const auto spin = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f },
            Scale(tuning.spin, launchAge * launchBlend), {});
        rotation = MatrixMath::ExtractEulerXYZ(MatrixMath::Multipty(spin, breakup.orientation));
    }
    fragment.object->SetTranslate(fragment.position);
    fragment.object->SetRotate(rotation);
    fragment.object->SetScale(scale);
    fragment.object->Update();
}

void EnemyBreakupEffect::Draw() {
    if (!prepared_) {
        return;
    }
    for (auto& breakup : breakups_) {
        if (!breakup.active) {
            continue;
        }
        objects_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
        for (size_t index = 0; index < kFragmentCount; ++index) {
            auto& object = *breakup.fragments[index].object;
            object.SetModel(breakup.pose.boxVisual ? box_ : shells_[index]);
            object.Draw();
            if (!breakup.pose.boxVisual && breakup.age > kFragments[index].separationDelay) {
                object.SetModel(cutFaces_[index]);
                object.Draw();
            }
        }
    }
}

void EnemyBreakupEffect::DrawImGui() {
#ifdef USE_IMGUI
    const auto activeCount = static_cast<int>(std::count_if(breakups_.begin(), breakups_.end(),
        [](const Breakup& breakup) { return breakup.active; }));
    ImGui::Text("Stage 1 3D Breakups: %d / %d", activeCount, static_cast<int>(kCapacity));
    ImGui::Text("Stage 1 dropped breakups: %llu", static_cast<unsigned long long>(droppedCount_));
#endif
}
