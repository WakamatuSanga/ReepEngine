#include "GpuParticleEffectPlayer.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/Graphics/Particle/GpuParticleEffectSerializer.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "GpuParticleFollowingEffect.h"

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
    bool playing = false;
};

GpuParticleEffectPlayer::GpuParticleEffectPlayer() = default;

GpuParticleEffectPlayer::~GpuParticleEffectPlayer() = default;

bool GpuParticleEffectPlayer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    if (isInitialized_ && dxCommon_ == dxCommon && srvManager_ == srvManager
        && srvManager && descriptorGeneration_ == srvManager->GetGeneration()) return true;
    Finalize();
    descriptorGeneration_ = srvManager ? srvManager->GetGeneration() : 0;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    isInitialized_ = dxCommon_ && srvManager_;
    lastResult_ = isInitialized_ ? "Initialized" : "DirectXCommon or SrvManager missing";
    return isInitialized_;
}

void GpuParticleEffectPlayer::Finalize() {
    followingEffects_.clear();
    effects_.clear();
    activeParticleEstimate_ = 0;
    playCount_ = missingJsonCount_ = failedPlayCount_ = 0;
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    isInitialized_ = false;
}

void GpuParticleEffectPlayer::Update(float deltaTime, const Camera* camera) {
    activeParticleEstimate_ = 0;
    for (const std::unique_ptr<EffectInstance>& instance : effects_) {
        if (!instance || !instance->system || !instance->loaded || !instance->playing) {
            continue;
        }
        instance->system->SetDeltaTime(deltaTime);
        instance->system->Update(camera);
        activeParticleEstimate_ += instance->system->GetActiveCountEstimate();
    }
    UpdateFollowingEffects(deltaTime, camera);
}

void GpuParticleEffectPlayer::Draw() {
    DrawFollowingEffects();
    for (const std::unique_ptr<EffectInstance>& instance : effects_) {
        if (!instance || !instance->system || !instance->loaded || !instance->playing) {
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

    instance->playing = true;
    ++playCount_;
    RecordResult(jsonPath, position, "Played GPU particle effect");
    return true;
}

bool GpuParticleEffectPlayer::PlayGpuParticleEffectAt(const std::string& path, const Vector3& position,
    float scale, float brightness, float amount, float lifetime) {
    auto* instance=FindOrCreateInstance(path);
    if(!instance||(!instance->loaded&&!LoadInstance(*instance)))return false;
    auto data=instance->data;scale=std::clamp(scale,.1f,4.0f);brightness=std::clamp(brightness,0.0f,3.0f);
    amount=std::clamp(amount,.1f,2.0f);lifetime=std::clamp(lifetime,.2f,3.0f);
    for(auto& type:data.particleTypes){type.startScale*=scale;type.endScale*=scale;type.speedMin*=scale;type.speedMax*=scale;
        type.lifeTimeMin*=lifetime;type.lifeTimeMax*=lifetime;
        for(auto* color:{&type.baseColor,&type.startColor,&type.endColor}){color->x*=brightness;color->y*=brightness;color->z*=brightness;}}
    for(auto& emitter:data.emitters){emitter.radius*=scale;emitter.emitCount=static_cast<uint32_t>(std::clamp(static_cast<float>(emitter.emitCount)*amount,0.0f,1024.0f));}
    if(!instance->system->PlayEffectDataAt(data,position))return false;
    instance->playing=true;++playCount_;RecordResult(path,position,"Played bounded runtime variation");return true;
}
void GpuParticleEffectPlayer::StopEffect(const std::string& path){
    if(auto* instance=FindInstance(path);instance&&instance->system){instance->system->ResetParticlePool();instance->playing=false;}
}

bool GpuParticleEffectPlayer::PrepareEffect(const std::string& jsonPath) {
    if (!isInitialized_ || jsonPath.empty()) return false;
    EffectInstance* instance = FindOrCreateInstance(jsonPath);
    return instance && (instance->loaded || LoadInstance(*instance));
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
    if (!instance->system->Initialize(dxCommon_, srvManager_)) return nullptr;

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

    for (const auto& type : instance.data.particleTypes) {
        if (!type.texturePath.empty()) TextureManager::GetInstance()->LoadTexture(type.texturePath);
    }
    instance.system->ApplyEffectData(instance.data); // textures and upload buffers only; no emission
    instance.playing = false;
    instance.loaded = true;
    instance.status = "Loaded";
    return true;
}

void GpuParticleEffectPlayer::RecordResult(const std::string& path, const Vector3& position, const std::string& result) {
    lastEffectPath_ = path.empty() ? "None" : path;
    lastPosition_ = position;
    lastResult_ = result;
}
