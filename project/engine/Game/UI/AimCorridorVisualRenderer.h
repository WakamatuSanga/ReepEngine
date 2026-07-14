#pragma once

#include "Engine/math/Matrix4x4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

class Camera;
class DirectXCommon;

class AimCorridorVisualRenderer {
public:
    struct FrameDraw {
        Vector3 center{};
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        float width = 0.0f;
        float height = 0.0f;
        float alpha = 0.0f;
        float coreIntensity = 1.0f;
        float glowIntensity = 0.0f;
        float glowAlpha = 0.0f;
        float glowRadiusTexels = 1.0f;
        float pulseScale = 1.0f;
        Vector3 coreTint{ 1.0f, 1.0f, 1.0f };
        Vector3 glowTint{ 1.0f, 1.0f, 1.0f };
        float tintAmount = 0.0f;
    };

    AimCorridorVisualRenderer() = default;
    ~AimCorridorVisualRenderer();

    bool Initialize(
        DirectXCommon* dxCommon,
        const std::string& nearTexturePath,
        const std::string& farTexturePath);
    void Finalize();
    uint32_t Draw(
        const Camera* camera,
        const FrameDraw& farFrame,
        const FrameDraw& nearFrame,
        bool drawFar,
        bool drawNear,
        bool disableGlow,
        bool showCoreOnly);

    bool IsInitialized() const { return initialized_; }
    bool IsNearTextureLoaded() const { return textures_[1].loaded; }
    bool IsFarTextureLoaded() const { return textures_[0].loaded; }
    float GetNearAspectRatio() const { return textures_[1].aspectRatio; }
    float GetFarAspectRatio() const { return textures_[0].aspectRatio; }

private:
    struct Vertex {
        Vector3 position{};
        Vector2 uv{};
    };

    struct Constants {
        Matrix4x4 viewProjection{};
        Vector4 appearance{};
        Vector4 sampling{};
        Vector4 flags{};
        Vector4 coreTint{};
        Vector4 glowTint{};
    };

    struct TextureState {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};
        float inverseWidth = 0.0f;
        float inverseHeight = 0.0f;
        float aspectRatio = 1.0f;
        bool loaded = false;
    };

    static constexpr uint32_t kFrameCount = 2;
    static constexpr uint32_t kVerticesPerFrame = 6;

    bool LoadTexture(uint32_t frameIndex, const std::string& texturePath);
    bool CreateRootSignature();
    bool CreatePipelineState();
    bool CreateBuffers();
    void BuildFrameVertices(uint32_t frameIndex, const FrameDraw& frame);
    void WriteConstants(
        uint32_t frameIndex,
        const Camera& camera,
        const FrameDraw& frame,
        bool disableGlow,
        bool showCoreOnly);

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    Vertex* vertexData_ = nullptr;
    std::byte* constantData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    std::array<TextureState, kFrameCount> textures_{};
    uint32_t constantStride_ = 0;
    bool initialized_ = false;
};
