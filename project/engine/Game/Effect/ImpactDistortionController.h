#pragma once

#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

class Camera;
class DirectXCommon;
class SrvManager;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12RootSignature;

class ImpactDistortionController {
public:
    enum class TriggerType {
        BulletCancel,
        EnemyDefeat,
    };

    enum class QualityMode {
        Balanced,
        Visual,
    };

    ImpactDistortionController();
    ~ImpactDistortionController();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const Camera* camera);
    void Finalize();
    void BeginFrame();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void TriggerBulletCancel(const Vector3& worldPosition);
    void TriggerEnemyDefeat(const Vector3& worldPosition);

private:
    struct EffectPreset {
        float lifetime = 0.18f;
        float startRadius = 0.012f;
        float endRadius = 0.085f;
        float distortionStrength = 0.024f;
        float ringThickness = 0.018f;
        float ringStrength = 0.35f;
        float chromaticStrength = 0.006f;
        float flashStrength = 0.25f;
        int maxSpawnPerFrame = 6;
    };

    struct Instance {
        TriggerType type = TriggerType::BulletCancel;
        Vector3 worldPosition{};
        Vector2 screenUv{};
        float cameraDepth = 0.0f;
        float elapsed = 0.0f;
        EffectPreset preset{};
        bool active = false;
    };

    struct DistanceScaleSettings {
        float nearDepth = 25.0f;
        float farDepth = 80.0f;
        float nearRadiusScale = 0.70f;
        float farRadiusScale = 1.20f;
        float nearStrengthScale = 0.75f;
        float farStrengthScale = 1.05f;
        float nearChromaticScale = 0.70f;
        float farChromaticScale = 1.00f;
        float nearRingScale = 0.80f;
        float farRingScale = 1.10f;
    };

    struct ComputedDistanceScale {
        float radius = 1.0f;
        float strength = 1.0f;
        float chromatic = 1.0f;
        float ring = 1.0f;
    };

    struct GpuInstance {
        std::array<float, 4> data0{};
        std::array<float, 4> data1{};
        std::array<float, 4> data2{};
    };

    struct GpuConstants {
        std::array<float, 4> screenSizeAndOptions{};
        std::array<float, 4> commonParams{};
        std::array<GpuInstance, 32> instances{};
    };

    static constexpr uint32_t kMaxGpuInstances = 32;

    bool ProjectWorldToScreenUv(const Vector3& worldPosition, Vector2& screenUv) const;
    bool CalculateCameraDepth(const Vector3& worldPosition, float& cameraDepth) const;
    ComputedDistanceScale CalculateDistanceScale(TriggerType type, float cameraDepth) const;
    void ApplyDistanceScale(EffectPreset& preset, const ComputedDistanceScale& scale) const;
    void Trigger(TriggerType type, const Vector3& worldPosition);
    void EnsureResources();
    void CreatePipeline();
    void CreateSourceCopy(uint32_t width, uint32_t height);
    void CreateConstantBuffer();
    void UpdateConstantBuffer();
    void CopySceneToSource();
    const char* GetTriggerTypeName(TriggerType type) const;
    const char* GetQualityModeName() const;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    const Camera* camera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> sourceCopyResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    GpuConstants* constantData_ = nullptr;
    D3D12_RESOURCE_STATES sourceCopyState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    uint32_t sourceCopySrvIndex_ = 0;
    bool sourceCopySrvAllocated_ = false;
    uint32_t sourceCopyWidth_ = 0;
    uint32_t sourceCopyHeight_ = 0;

    std::vector<Instance> instances_;
    EffectPreset bulletCancelPreset_{};
    EffectPreset enemyDefeatPreset_{};

    bool enabled_ = true;
    QualityMode qualityMode_ = QualityMode::Balanced;
    int maxActiveInstances_ = 32;
    bool showScreenPositionMarker_ = false;
    bool forceStrongDistortion_ = false;
    bool disableChromatic_ = false;
    bool disableRingHighlight_ = false;
    bool disableFlash_ = false;
    bool enableDistanceBasedScale_ = true;
    bool applyDistanceScaleToBulletCancel_ = false;
    float bulletCancelDistanceScaleStrength_ = 0.25f;
    DistanceScaleSettings distanceScaleSettings_{};
    int bulletCancelSpawnedThisFrame_ = 0;
    int enemyDefeatSpawnedThisFrame_ = 0;
    int activeInstanceCount_ = 0;
    uint64_t droppedInstanceCount_ = 0;
    TriggerType lastTriggerType_ = TriggerType::BulletCancel;
    Vector3 lastWorldPosition_{};
    Vector2 lastScreenUv_{};
    float lastImpactDepth_ = 0.0f;
    float lastComputedRadiusScale_ = 1.0f;
    float lastComputedStrengthScale_ = 1.0f;
    float lastComputedChromaticScale_ = 1.0f;
    float lastComputedRingScale_ = 1.0f;
    std::string lastResult_ = "Not triggered";
};
