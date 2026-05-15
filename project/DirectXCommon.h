#pragma once

// DirectX12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <string>
#include <chrono>
#include <thread>
#include <cstdint>

#include "WinApp.h"
class SrvManager;
#include "externals/DirectXTex/DirectXTex.h" // TexMetadata / ScratchImage 用

// DirectX 基盤クラス
class DirectXCommon {
public:
    struct PostEffectParameters {
        uint32_t gaussianEnabled = 0;
        float gaussianIntensity = 1.0f;
        uint32_t smoothingEnabled = 0;
        float smoothingIntensity = 1.0f;
        uint32_t grayscaleEnabled = 0;
        float grayscaleIntensity = 1.0f;
        uint32_t sepiaEnabled = 0;
        float sepiaIntensity = 1.0f;
        uint32_t invertEnabled = 0;
        float invertIntensity = 1.0f;
        uint32_t vignetteEnabled = 0;
        float vignetteIntensity = 1.0f;
        uint32_t radialBlurEnabled = 0;
        float radialBlurStrength = 0.02f;
        std::array<float, 2> radialBlurCenter = { 0.5f, 0.5f };
        uint32_t radialBlurSampleCount = 8;
        uint32_t dissolveEnabled = 0;
        float dissolveThreshold = 0.0f;
        float dissolveEdgeWidth = 0.05f;
        uint32_t outlineMode = 0;
        float outlineIntensity = 1.0f;
        float outlineThickness = 1.0f;
        float outlineThreshold = 0.1f;
        float outlineSoftness = 0.05f;
        float outlineDepthThreshold = 0.002f;
        float outlineDepthStrength = 10.0f;
        float outlineNormalThreshold = 0.1f;
        float outlineNormalStrength = 4.0f;
        uint32_t hybridColorSource = 2;
        float hybridColorWeight = 1.0f;
        float hybridDepthWeight = 1.0f;
        float hybridNormalWeight = 1.0f;
        float depthNear = 0.1f;
        float depthFar = 100.0f;
        float postEffectPadding = 0.0f;
        std::array<float, 4> dissolveEdgeColor = { 1.0f, 0.5f, 0.1f, 1.0f };
        std::array<float, 4> outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    DirectXCommon() = default;
    ~DirectXCommon() = default;

    // DirectX の初期化 / 終了
    void Initialize(WinApp* winApp);
    void Finalize();

    // 毎フレームの前処理 / 後処理
    void PreDraw();
    void PrepareSwapChainForImGui();
    void CopyRenderTextureToSwapChain();
    void PrepareRenderTextureForImGui();
    void RestoreRenderTextureAfterImGui();
    void PostDraw();
    void CreateRenderTexture(SrvManager* srvManager);

    // 最大SRV数（最大テクスチャ枚数）
    static const uint32_t kMaxSRVCount;

    // --- getter ---
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
    IDXGISwapChain4* GetSwapChain() const { return swapChain.Get(); }

    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap.Get(); }
    ID3D12DescriptorHeap* GetRtvDescriptorHeap() const { return rtvDescriptorHeap.Get(); }
    ID3D12DescriptorHeap* GetDsvDescriptorHeap() const { return dsvDescriptorHeap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const { return dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); }

    UINT GetSrvDescriptorSize() const { return descriptorSizeSRV; }

    // ★追加: バックバッファの数を取得 (ImGui初期化などで使用)
    size_t GetBackBufferCount() const { return swapChainResources.size(); }

    // バックバッファ RTV を直接欲しい場合
    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) const {
        return rtvHandles[index];
    }

    // SRV 用ディスクリプタハンドル
    ID3D12Resource* GetRenderTextureResource() const { return renderTextureResource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTextureRTV() const { return renderTextureRTVHandle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTextureSRVCPUHandle() const { return renderTextureSRVHandleCPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTextureSRVGPUHandle() const { return renderTextureSRVHandleGPU_; }
    uint32_t GetRenderTextureSRVIndex() const { return renderTextureSRVIndex_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthTextureSRVCPUHandle() const { return depthTextureSRVHandleCPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthTextureSRVGPUHandle() const { return depthTextureSRVHandleGPU_; }
    uint32_t GetDepthTextureSRVIndex() const { return depthTextureSRVIndex_; }
    const std::array<float, 4>& GetRenderTextureClearColor() const { return renderTextureClearColor_; }
    void SetDissolveNoiseTextureIndex(uint32_t textureIndex);
    PostEffectParameters& GetPostEffectParameters() { return postEffectParameters_; }
    const PostEffectParameters& GetPostEffectParameters() const { return postEffectParameters_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);
    void TransitionDepthBuffer(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    // ============================
    // 追加関数
    // ============================

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,   // HLSL ファイルパス
        const wchar_t* profile);        // "vs_6_0" など

    /// <summary>バッファリソースの生成（アップロードヒープ）</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    /// <summary>テクスチャリソースの生成（Default ヒープ）</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        const DirectX::TexMetadata& metadata);

    /// <summary>テクスチャデータの転送（ScratchImage → GPU）</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    /// <summary>テクスチャファイルの読み込み（静的関数）</summary>
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // ★ DXC 関連 Getter
    IDxcUtils* GetDxcUtils() const { return dxcUtils.Get(); }
    IDxcCompiler3* GetDxcCompiler() const { return dxcCompiler.Get(); }
    IDxcIncludeHandler* GetDxcIncludeHandler() const { return dxcIncludeHandler.Get(); }

private:
    // デバイスの生成
    void CreateDevice();
    // コマンド関連の初期化
    void InitializeCommand();
    // スワップチェーンの生成
    void CreateSwapChain();
    // 深度バッファの生成
    void CreateDepthBuffer();
    // 各種デスクリプタヒープの生成
    void CreateDescriptorHeaps();
    // レンダーターゲットビューの初期化
    void InitializeRenderTargetView();
    // 深度ステンシルビューの初期化
    void InitializeDepthStencilView();
    // フェンスの生成
    void CreateFence();
    // ビューポート矩形の初期化
    void InitializeViewport();
    // シザー矩形の初期化
    void InitializeScissorRect();
    // DXC コンパイラの生成
    void CreateDXCCompiler();
    void CreateCopyRootSignature();
    void CreateCopyPipelineState();
    // ImGui の初期化
    void InitializeImGui();

    // 内部用：指定番号の CPU ハンドル取得
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    // 内部用：指定番号の GPU ハンドル取得
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    // FPS固定初期化
    void InitializeFixFPS();
    // FPS固定更新
    void UpdateFixFPS();

    // 記録時間（FPS固定用）
    std::chrono::steady_clock::time_point reference_;

private:
    // WindowsAPI
    WinApp* winApp = nullptr;

    // デバイス / ファクトリ
    Microsoft::WRL::ComPtr<ID3D12Device>   device;
    Microsoft::WRL::ComPtr<IDXGIFactory7>  dxgiFactory;

    // コマンドまわり
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

    // スワップチェーンとバックバッファ
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;

    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE depthTextureSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthTextureSRVHandleGPU_{};
    uint32_t depthTextureSRVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE dissolveNoiseTextureSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE dissolveNoiseTextureSRVHandleGPU_{};
    uint32_t dissolveNoiseTextureSRVIndex_ = 0;

    // デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

    // デスクリプタサイズ
    uint32_t descriptorSizeRTV = 0;
    uint32_t descriptorSizeSRV = 0;
    uint32_t descriptorSizeDSV = 0;

    // フェンス
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    uint64_t fenceValue = 0;
    HANDLE   fenceEvent = nullptr;

    // ビューポート / シザー
    D3D12_VIEWPORT viewport{};
    D3D12_RECT     scissorRect{};

    // DXC コンパイラ関連
    Microsoft::WRL::ComPtr<IDxcUtils>          dxcUtils;
    Microsoft::WRL::ComPtr<IDxcCompiler3>      dxcCompiler;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler;

    // バックバッファ 2 枚分の RTV
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE renderTextureSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSRVHandleGPU_{};
    uint32_t renderTextureSRVIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> normalTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE normalTextureRTVHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE normalTextureSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE normalTextureSRVHandleGPU_{};
    uint32_t normalTextureSRVIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> gaussianIntermediateResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE gaussianIntermediateRTVHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE gaussianIntermediateSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE gaussianIntermediateSRVHandleGPU_{};
    uint32_t gaussianIntermediateSRVIndex_ = 0;
    std::array<float, 4> renderTextureClearColor_ = { 0.05f, 0.05f, 0.1f, 1.0f };
    std::array<float, 4> normalTextureClearColor_ = { 0.5f, 0.5f, 0.5f, 1.0f };
    Microsoft::WRL::ComPtr<ID3D12RootSignature> copyRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianBlurXPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianBlurYPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> postEffectResource_;
    PostEffectParameters* postEffectData_ = nullptr;
    PostEffectParameters postEffectParameters_{};
};
