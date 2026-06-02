#pragma once
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Matrix4x4.h"
#include <wrl.h>
#include <list>
#include <string>
#include <memory>

class ParticleManager {
    friend struct std::default_delete<ParticleManager>;

public:
    struct Particle {
        Transform transform;
        Vector3 velocity;
        Vector4 color;
        float lifeTime;
        float currentTime;
    };

    struct EffectParams {
        Vector2 scaleXRange{ 0.08f, 0.18f };
        Vector2 scaleYRange{ 0.6f, 1.4f };
        Vector2 rotateZRange{ 0.0f, 6.2831853f };
        Vector2 lifeTimeRange{ 0.2f, 0.5f };
        Vector2 speedRange{ 0.08f, 0.2f };
        Vector2 colorRRange{ 1.0f, 1.0f };
        Vector2 colorGRange{ 0.55f, 1.0f };
        Vector2 colorBRange{ 0.15f, 0.35f };
        uint32_t spawnCount = 24;
    };

    struct ParticleForGPU {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
    };

public:
    static ParticleManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(Camera* camera);
    void Draw();
    void Finalize();

    void Emit(const std::string& name, const Vector3& pos, uint32_t count);
    EffectParams& GetHitEffectParams() { return hitEffectParams_; }
    EffectParams& GetFireballEffectParams() { return fireballEffectParams_; }
    EffectParams& GetWindEffectParams() { return windEffectParams_; }
    void SetTexture(const std::string& texturePath);
    const std::string& GetTexture() const { return textureName_; }

private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    void CreateRootSignature();
    void CreatePipelineState();
    void CreateModel();

    static std::unique_ptr<ParticleManager> instance_;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::list<Particle> particles_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;

    uint32_t srvIndex_ = 0;
    std::string textureName_;
    const uint32_t kMaxInstance = 100;
    EffectParams hitEffectParams_{ {0.03f, 0.06f}, {1.5f, 2.8f}, {0.0f, 6.2831853f}, {0.08f, 0.14f}, {0.18f, 0.30f}, {1.0f, 1.0f}, {0.78f, 1.0f}, {0.12f, 0.25f}, 16 };
    EffectParams fireballEffectParams_{ {0.14f, 0.28f}, {0.18f, 0.36f}, {0.0f, 6.2831853f}, {0.22f, 0.40f}, {0.06f, 0.14f}, {0.95f, 1.0f}, {0.35f, 0.65f}, {0.05f, 0.16f}, 16 };
    EffectParams windEffectParams_{ {0.03f, 0.07f}, {0.7f, 1.5f}, {-0.2f, 0.2f}, {0.35f, 0.70f}, {0.12f, 0.24f}, {0.85f, 1.0f}, {0.90f, 1.0f}, {0.95f, 1.0f}, 20 };
};
