#include "PlayerAfterimageController.h"
#include "FlightTuning.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include <algorithm>
#include <array>

struct PlayerAfterimageController::Slot {
    Object3d object;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer;
    Model::Material* material = nullptr;
    float age = 0;
    float lifetime = 0;
    float sourceAlpha = 1;
    bool active = false;
};
PlayerAfterimageController::PlayerAfterimageController() = default;
PlayerAfterimageController::~PlayerAfterimageController() = default;
void PlayerAfterimageController::Initialize(Object3dCommon* common, Camera* camera) {
    slots_.clear(); common_ = common;
    if (!common || !common->GetDxCommon()) return;
    for (int i = 0; i < 6; ++i) {
        auto slot = std::make_unique<Slot>();
        slot->object.Initialize(common);
        slot->object.SetCamera(camera);
        slot->materialBuffer = common->GetDxCommon()->CreateBufferResource((sizeof(Model::Material) + 255) & ~255);
        slot->materialBuffer->Map(0, nullptr, reinterpret_cast<void**>(&slot->material));
        slot->object.SetMaterialBufferOverride(slot->materialBuffer->GetGPUVirtualAddress());
        slots_.push_back(std::move(slot));
    }
}
void PlayerAfterimageController::Update(float dt, bool active, bool rolling, Model* model, const Transform& pose) {
    const auto& tuning = FlightTuning::Get();
    for (auto& slot : slots_) {
        if (!active) slot->active = false;
        if (!slot->active) continue;
        slot->age += dt;
        slot->active = slot->age < slot->lifetime;
        const float fade = std::clamp(1.0f - slot->age / slot->lifetime, 0.0f, 1.0f);
        slot->material->color.w = slot->sourceAlpha * tuning.ghostAlpha * fade * fade;
    }
    if (!active || !rolling || !model || !model->GetMaterialData()) {
        sampleTimer_ = 0; wasRolling_ = false; return;
    }
    if (!wasRolling_) sampleTimer_ = tuning.ghostLifetime / 6;
    wasRolling_ = true;
    sampleTimer_ += dt;
    if (sampleTimer_ < tuning.ghostLifetime / 6) return;
    sampleTimer_ = 0; // at most one sample per game frame, no catch-up burst
    Slot* target = nullptr;
    for (auto& slot : slots_) {
        if (!slot->active) { target = slot.get(); break; }
        if (!target || slot->age > target->age) target = slot.get();
    }
    if (!target) return;
    target->active = true; target->age = 0; target->lifetime = tuning.ghostLifetime;
    *target->material = *model->GetMaterialData();
    target->sourceAlpha = target->material->color.w;
    target->material->color.w *= tuning.ghostAlpha;
    target->material->alphaReference = 0;
    target->object.SetModel(model);
    target->object.GetTransform() = pose;
}
void PlayerAfterimageController::RefreshCamera() {
    for (auto& slot : slots_) if (slot->active) slot->object.Update();
}
void PlayerAfterimageController::Draw() {
    if (!common_ || GetDrawCount() == 0) return;
    common_->CommonDrawSetting(Object3dCommon::BlendMode::kGhost);
    // Oldest first. Every sample has its own CB; shared model material is untouched.
    std::array<Slot*, 6> ordered{};
    size_t count = 0;
    for (auto& slot : slots_) if (slot->active) ordered[count++] = slot.get();
    std::sort(ordered.begin(), ordered.begin() + count, [](const Slot* a, const Slot* b) { return a->age > b->age; });
    for (size_t i = 0; i < count; ++i) ordered[i]->object.Draw();
}
uint32_t PlayerAfterimageController::GetDrawCount() const {
    uint32_t count = 0;
    for (const auto& slot : slots_) if (slot->active) ++count;
    return count;
}
