#include "GpuParticleEffectPlayer.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/Graphics/Particle/GpuParticleEffectSerializer.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <filesystem>

struct GpuParticleEffectPlayer::EffectInstance {
    std::string path;
    GpuParticle::ParticleEffectData data;
    std::unique_ptr<GpuParticleSystem> system;
    std::string status = "Not loaded";
    bool loaded = false;
};

GpuParticleEffectPlayer::GpuParticleEffectPlayer() = default;

GpuParticleEffectPlayer::~GpuParticleEffectPlayer() = default;

bool GpuParticleEffectPlayer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    isInitialized_ = dxCommon_ && srvManager_;
    lastResult_ = isInitialized_ ? "Initialized" : "DirectXCommon or SrvManager missing";
    return isInitialized_;
}

void GpuParticleEffectPlayer::Finalize() {
    effects_.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    isInitialized_ = false;
}

void GpuParticleEffectPlayer::Update(float deltaTime, const Camera* camera) {
    activeParticleEstimate_ = 0;
    for (const std::unique_ptr<EffectInstance>& instance : effects_) {
        if (!instance || !instance->system || !instance->loaded) {
            continue;
        }
        instance->system->SetDeltaTime(deltaTime);
        instance->system->Update(camera);
        activeParticleEstimate_ += instance->system->GetActiveCountEstimate();
    }
}

void GpuParticleEffectPlayer::Draw() {
    for (const std::unique_ptr<EffectInstance>& instance : effects_) {
        if (!instance || !instance->system || !instance->loaded) {
            continue;
        }
        instance->system->Draw();
    }
}

bool GpuParticleEffectPlayer::PlayGpuParticleEffectAt(const std::string& jsonPath, const Vector3& position) {
    if (!isInitialized_) {
        ++failedPlayCount_;
        RecordResult(jsonPath, position, "GpuParticleEffectPlayer is not initialized");
        return false;
    }
    if (jsonPath.empty()) {
        ++failedPlayCount_;
        RecordResult(jsonPath, position, "JSON path is empty");
        return false;
    }

    EffectInstance* instance = FindOrCreateInstance(jsonPath);
    if (!instance) {
        ++failedPlayCount_;
        RecordResult(jsonPath, position, "Failed to create effect instance");
        return false;
    }
    if (!instance->loaded && !LoadInstance(*instance)) {
        ++failedPlayCount_;
        RecordResult(jsonPath, position, instance->status);
        return false;
    }
    if (!instance->system || !instance->system->PlayEffectDataAt(instance->data, position)) {
        ++failedPlayCount_;
        RecordResult(jsonPath, position, "Failed to play GPU particle effect");
        return false;
    }

    ++playCount_;
    RecordResult(jsonPath, position, "Played GPU particle effect");
    return true;
}

bool GpuParticleEffectPlayer::ReloadEffect(const std::string& jsonPath) {
    EffectInstance* instance = FindOrCreateInstance(jsonPath);
    if (!instance) {
        return false;
    }
    instance->loaded = false;
    return LoadInstance(*instance);
}

void GpuParticleEffectPlayer::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("GPU Particle Effect Player");
    ImGui::TextWrapped("Last GPU Particle Effect: %s", lastEffectPath_.c_str());
    ImGui::Text("Last GPU Particle Position: %.2f, %.2f, %.2f", lastPosition_.x, lastPosition_.y, lastPosition_.z);
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Text("GPU Particle Play Count: %llu", static_cast<unsigned long long>(playCount_));
    ImGui::Text("Missing GPU Particle JSON Count: %llu", static_cast<unsigned long long>(missingJsonCount_));
    ImGui::Text("Failed GPU Particle Play Count: %llu", static_cast<unsigned long long>(failedPlayCount_));
    ImGui::Text("Active Particle Estimate: %u", activeParticleEstimate_);
    if (ImGui::TreeNode("Loaded GPU Particle Effects")) {
        for (const std::unique_ptr<EffectInstance>& instance : effects_) {
            if (!instance) {
                continue;
            }
            ImGui::BulletText("%s : %s", instance->path.c_str(), instance->status.c_str());
        }
        ImGui::TreePop();
    }
#endif
}

GpuParticleEffectPlayer::EffectInstance* GpuParticleEffectPlayer::FindInstance(const std::string& jsonPath) {
    const auto it = std::find_if(
        effects_.begin(),
        effects_.end(),
        [&jsonPath](const std::unique_ptr<EffectInstance>& instance) {
            return instance && instance->path == jsonPath;
        });
    return it == effects_.end() ? nullptr : it->get();
}

GpuParticleEffectPlayer::EffectInstance* GpuParticleEffectPlayer::FindOrCreateInstance(const std::string& jsonPath) {
    if (EffectInstance* existing = FindInstance(jsonPath)) {
        return existing;
    }
    if (!isInitialized_) {
        return nullptr;
    }

    auto instance = std::make_unique<EffectInstance>();
    instance->path = jsonPath;
    instance->system = std::make_unique<GpuParticleSystem>();
    if (!instance->system->Initialize(dxCommon_, srvManager_)) {
        instance->status = "Failed to initialize GpuParticleSystem";
        effects_.push_back(std::move(instance));
        return effects_.back().get();
    }

    effects_.push_back(std::move(instance));
    return effects_.back().get();
}

bool GpuParticleEffectPlayer::LoadInstance(EffectInstance& instance) {
    if (instance.path.empty()) {
        instance.loaded = false;
        instance.status = "JSON path is empty";
        return false;
    }
    if (!std::filesystem::exists(std::filesystem::path(instance.path))) {
        instance.loaded = false;
        instance.status = "Missing GPU Particle JSON: " + instance.path;
        ++missingJsonCount_;
        return false;
    }
    if (!GpuParticle::GpuParticleEffectSerializer::Load(instance.path, instance.data)) {
        instance.loaded = false;
        instance.status = "Failed to load GPU Particle JSON: " + instance.path;
        return false;
    }

    instance.loaded = true;
    instance.status = "Loaded";
    return true;
}

void GpuParticleEffectPlayer::RecordResult(const std::string& path, const Vector3& position, const std::string& result) {
    lastEffectPath_ = path.empty() ? "None" : path;
    lastPosition_ = position;
    lastResult_ = result;
}
