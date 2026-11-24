#include "LockedWingSmokePass.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/GpuTimestampProfiler.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kPuffCapacity = 640;
constexpr UINT kTileSize = 16;
constexpr UINT kTileCapacity = 48;
constexpr UINT kTileStride = kTileCapacity + 1;
constexpr DXGI_FORMAT kSmokeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr float kClear[4]{};
constexpr std::array<Vector3, 6> kBurstDirections{{
    {0.0f, 0.12f, 0.0f},
    {-0.88f, 0.15f, 0.20f},
    {0.88f, -0.12f, -0.15f},
    {-0.28f, 0.75f, 0.46f},
    {0.29f, 0.50f, -0.62f},
    {0.14f, -0.61f, 0.42f},
}};

struct GpuPuff {
    Vector4 centerAndRadius;
    Vector2 opacityAndSeed;
};

struct SmokeConstants {
    Matrix4x4 viewToWorld;
    Vector4 projection;
    UINT outputWidth, outputHeight, width, height;
    UINT tileColumns;
    float density, nearPlane, padding;
};
static_assert(sizeof(GpuPuff) == 24);
static_assert(sizeof(SmokeConstants) == 112);

void Transition(ID3D12GraphicsCommandList& commands, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    commands.ResourceBarrier(1, &barrier);
}

Vector3 TransformPoint(const Matrix4x4& view, const Vector3& world) {
    return {
        world.x * view.m[0][0] + world.y * view.m[1][0] + world.z * view.m[2][0] + view.m[3][0],
        world.x * view.m[0][1] + world.y * view.m[1][1] + world.z * view.m[2][1] + view.m[3][1],
        world.x * view.m[0][2] + world.y * view.m[1][2] + world.z * view.m[2][2] + view.m[3][2],
    };
}

D3D12_RECT ProjectPuff(const Vector3& center, float radius, const SmokeConstants& c) {
    const LONG width = static_cast<LONG>(c.width);
    const LONG height = static_cast<LONG>(c.height);
    if (center.z + radius <= c.nearPlane) {
        return {};
    }
    const float front = center.z - radius;
    if (front <= c.nearPlane) {
        return {0, 0, width, height};
    }

    const float x = center.x / center.z;
    const float y = center.y / center.z;
    const float halfX = radius * (1.0f + std::abs(x)) / front;
    const float halfY = radius * (1.0f + std::abs(y)) / front;
    const auto pixelX = [&](float ndc) { return std::clamp(0.5f + 0.5f * ndc * c.projection.x, 0.0f, 1.0f) * c.width; };
    const auto pixelY = [&](float ndc) { return std::clamp(0.5f - 0.5f * ndc * c.projection.y, 0.0f, 1.0f) * c.height; };
    return {
        static_cast<LONG>(std::floor(pixelX(x - halfX))),
        static_cast<LONG>(std::floor(pixelY(y + halfY))),
        static_cast<LONG>(std::ceil(pixelX(x + halfX))),
        static_cast<LONG>(std::ceil(pixelY(y - halfY))),
    };
}

float Smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

struct LockedWingSmokePass::RenderResources {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> raymarch, composite;
    ComPtr<ID3D12Resource> color, constants, puffBuffer, tileBuffer;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    std::vector<GpuPuff> visiblePuffs;
    std::vector<UINT> tileEntries;
    UINT width = 0, height = 0;
    UINT srvIndex = UINT_MAX;
    uint64_t srvGeneration = 0;

    bool Initialize(DirectXCommon& dx);
    bool EnsureBuffer(DirectXCommon& dx, SrvManager& srv, UINT w, UINT h);
};

bool LockedWingSmokePass::RenderResources::Initialize(DirectXCommon& dx) {
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    D3D12_ROOT_PARAMETER params[5]{};
    for (UINT i = 0; i < 2; ++i) {
        ranges[i] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, 0};
        params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i].DescriptorTable = {1, &ranges[i]};
    }
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[3].ParameterType = params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[3].Descriptor.ShaderRegister = 2;
    params[4].Descriptor.ShaderRegister = 3;
    for (auto& param : params) param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = _countof(params);
    rootDesc.pParameters = params;
    ComPtr<ID3DBlob> signature;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr)) ||
        FAILED(dx.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&root)))) {
        return false;
    }

    const auto vs = dx.CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    const auto smokePs = dx.CompileShader(L"resources/shaders/LockedWingSmoke.PS.hlsl", L"ps_6_0");
    const auto compositePs = dx.CompileShader(L"resources/shaders/LockedWingSmokeComposite.PS.hlsl", L"ps_6_0");
    if (!vs || !smokePs || !compositePs) {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = root.Get();
    pipeline.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pipeline.PS = {smokePs->GetBufferPointer(), smokePs->GetBufferSize()};
    pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = kSmokeFormat;
    pipeline.SampleDesc.Count = 1;
    pipeline.SampleMask = UINT_MAX;
    if (FAILED(dx.GetDevice()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&raymarch)))) {
        return false;
    }

    auto& blend = pipeline.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeline.PS = {compositePs->GetBufferPointer(), compositePs->GetBufferSize()};
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (FAILED(dx.GetDevice()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&composite)))) {
        return false;
    }

    const D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
    if (FAILED(dx.GetDevice()->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap)))) {
        return false;
    }
    constants = dx.CreateBufferResource(256);
    puffBuffer = dx.CreateBufferResource(sizeof(GpuPuff) * kPuffCapacity);
    visiblePuffs.reserve(kPuffCapacity);
    return constants && puffBuffer;
}

bool LockedWingSmokePass::RenderResources::EnsureBuffer(DirectXCommon& dx, SrvManager& srv, UINT w, UINT h) {
    if (color && tileBuffer && width == w && height == h) {
        return true;
    }

    D3D12_RESOURCE_DESC texture{};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = w;
    texture.Height = h;
    texture.DepthOrArraySize = texture.MipLevels = 1;
    texture.Format = kSmokeFormat;
    texture.SampleDesc.Count = 1;
    texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = kSmokeFormat;
    ComPtr<ID3D12Resource> newColor;
    if (FAILED(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&newColor)))) {
        return false;
    }

    const size_t entries = static_cast<size_t>((w + kTileSize - 1) / kTileSize)
        * ((h + kTileSize - 1) / kTileSize) * kTileStride;
    auto newTiles = dx.CreateBufferResource(entries * sizeof(UINT));
    if (!newTiles) {
        return false;
    }
    if (srvIndex == UINT_MAX) {
        if (!srv.CanAllocate()) {
            return false;
        }
        srvIndex = srv.Allocate();
        srvGeneration = srv.GetGeneration();
    }
    color = std::move(newColor);
    tileBuffer = std::move(newTiles);
    tileEntries.resize(entries);
    width = w;
    height = h;
    dx.GetDevice()->CreateRenderTargetView(color.Get(), nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());
    srv.CreateSRVforTexture2D(srvIndex, color.Get(), kSmokeFormat, 1);
    return true;
}

LockedWingSmokePass::LockedWingSmokePass(DirectXCommon& dx, SrvManager& srv, Style style)
    : dx_(dx), srv_(srv), style_(style), render_(std::make_unique<RenderResources>()) {
    if (style_ == Style::Burst) {
        density_ = 1.55f;
        expansion_ = 1.2f;
        lifetime_ = 0.8f;
    }
    renderInitFailed_ = !render_->Initialize(dx_);
}

LockedWingSmokePass::~LockedWingSmokePass() {
    if (render_->srvIndex != UINT_MAX) {
        srv_.Release(render_->srvIndex, render_->srvGeneration);
    }
}

void LockedWingSmokePass::Emit(const Vector3& nozzle, const Vector3& direction) {
    if (!IsEnabled()) {
        return;
    }
    const float variation = NextVariation();
    const float angle = variation * 6.2831853f;
    AppendPuff(nozzle,
        {direction.x * 0.8f + std::cos(angle) * 0.24f,
            direction.y * 0.8f + 0.20f + 0.17f * variation,
            direction.z * 0.8f + std::sin(angle) * 0.24f},
        variation, 1.0f);
}

bool LockedWingSmokePass::EmitBurst(const Vector3& center, float scale) {
    if (style_ != Style::Burst || !IsEnabled()) {
        return false;
    }
    const float safeScale = std::clamp(scale, 0.65f, 1.5f);
    const float offset = 0.24f * safeScale;
    const float speed = 1.6f * safeScale;
    for (const auto& direction : kBurstDirections) {
        AppendPuff({center.x + direction.x * offset, center.y + direction.y * offset, center.z + direction.z * offset},
            {direction.x * speed, direction.y * speed + 0.18f, direction.z * speed},
            NextVariation(), safeScale);
    }
    return true;
}

float LockedWingSmokePass::NextVariation() {
    const unsigned seed = ++sequence_ * 747796405u + 2891336453u;
    return static_cast<float>((seed >> 15) & 1023u) / 1023.0f;
}

void LockedWingSmokePass::AppendPuff(const Vector3& origin, const Vector3& drift, float variation, float scale) {
    if (puffs_.size() == kPuffCapacity) {
        puffs_.pop_front();
    }
    puffs_.push_back({origin, drift, time_, 17.0f * variation, scale});
}

void LockedWingSmokePass::Update(float deltaTime) {
    time_ += deltaTime;
    while (!puffs_.empty() && time_ - puffs_.front().birthTime >= lifetime_) {
        puffs_.pop_front();
    }
}

void LockedWingSmokePass::Reset() {
    puffs_.clear();
    time_ = 0.0f;
    sequence_ = 0;
}

void LockedWingSmokePass::Draw(const Camera& camera) {
    if (!enabled_ || renderInitFailed_ || puffs_.empty() || density_ <= 0.0f) {
        return;
    }
    const UINT outputWidth = dx_.GetRenderTextureWidth(), outputHeight = dx_.GetRenderTextureHeight();
    const UINT width = (outputWidth + 1) / 2, height = (outputHeight + 1) / 2;
    auto& gpu = *render_;
    if (!width || !height || !gpu.EnsureBuffer(dx_, srv_, width, height)) {
        return;
    }

    const bool burst = style_ == Style::Burst;
    const auto& projection = camera.GetProjectionMatrix();
    const SmokeConstants settings{camera.GetViewWorldMatrix(),
        {projection.m[0][0], projection.m[1][1], projection.m[2][2], projection.m[3][2]},
        outputWidth, outputHeight, width, height, (width + kTileSize - 1) / kTileSize,
        density_, camera.GetNearClip(), 0.0f};
    gpu.visiblePuffs.clear();
    std::fill(gpu.tileEntries.begin(), gpu.tileEntries.end(), 0u);
    D3D12_RECT bounds{static_cast<LONG>(width), static_cast<LONG>(height), 0, 0};
    // Newer puffs get priority when the screen tiles are full.
    for (auto it = puffs_.rbegin(); it != puffs_.rend(); ++it) {
        const auto& puff = *it;
        const float age = time_ - puff.birthTime;
        const float fade = 1.0f - Smooth(0.38f * lifetime_, lifetime_, age);
        if (fade < 0.004f) continue;
        const float bloom = Smooth(0.0f, burst ? 0.18f : 0.28f, age);
        const float radius = ((burst ? 0.32f : 0.095f)
            + expansion_ * (bloom * (burst ? 0.40f : 0.34f) + age * 0.12f))
            * (0.88f + puff.seed * (0.24f / 17.0f)) * puff.scale;
        const float drift = age / (1.0f + 0.9f * age);
        const Vector3 center = TransformPoint(camera.GetViewMatrix(),
            {puff.origin.x + puff.drift.x * drift, puff.origin.y + puff.drift.y * drift,
                puff.origin.z + puff.drift.z * drift});
        // Residual smoke can drift past the camera as the rail advances. Fade it
        // before it occupies the full screen, while leaving the narrow nozzle visible.
        const float screenRadius = radius * settings.projection.y / (std::max)(center.z, settings.nearPlane);
        const float nearFade = 1.0f - Smooth(burst ? 0.27f : 0.13f, burst ? 0.58f : 0.29f, screenRadius);
        if (fade * nearFade < 0.004f) continue;
        const auto rect = ProjectPuff(center, radius, settings);
        if (rect.left >= rect.right || rect.top >= rect.bottom) continue;
        const UINT id = static_cast<UINT>(gpu.visiblePuffs.size());
        const float opacity = burst ? 1.0f : 0.36f + 0.64f * bloom;
        gpu.visiblePuffs.push_back({{center.x, center.y, center.z, radius}, {fade * nearFade * opacity, puff.seed}});
        bounds.left = (std::min)(bounds.left, rect.left);
        bounds.top = (std::min)(bounds.top, rect.top);
        bounds.right = (std::max)(bounds.right, rect.right);
        bounds.bottom = (std::max)(bounds.bottom, rect.bottom);
        for (LONG y = rect.top / static_cast<LONG>(kTileSize); y <= (rect.bottom - 1) / static_cast<LONG>(kTileSize); ++y) {
            for (LONG x = rect.left / static_cast<LONG>(kTileSize); x <= (rect.right - 1) / static_cast<LONG>(kTileSize); ++x) {
                UINT* tile = gpu.tileEntries.data() + (y * settings.tileColumns + x) * kTileStride;
                if (tile[0] < kTileCapacity) {
                    ++tile[0];
                    tile[tile[0]] = id;
                }
            }
        }
    }
    if (gpu.visiblePuffs.empty()) {
        return;
    }

    const auto upload = [](ID3D12Resource& buffer, const void* data, size_t bytes) {
        void* mapped = nullptr;
        const D3D12_RANGE readRange{0, 0};
        if (FAILED(buffer.Map(0, &readRange, &mapped))) return false;
        std::memcpy(mapped, data, bytes);
        const D3D12_RANGE writeRange{0, bytes};
        buffer.Unmap(0, &writeRange);
        return true;
    };
    if (!upload(*gpu.constants.Get(), &settings, sizeof(settings)) ||
        !upload(*gpu.puffBuffer.Get(), gpu.visiblePuffs.data(), gpu.visiblePuffs.size() * sizeof(GpuPuff)) ||
        !upload(*gpu.tileBuffer.Get(), gpu.tileEntries.data(), gpu.tileEntries.size() * sizeof(UINT))) {
        return;
    }

    auto* commands = dx_.GetCommandList();
    auto& profiler = dx_.GetGpuProfiler();
    GpuTimestampProfiler::Context context{};
    context.path = GpuTimestampProfiler::Path::LowResolution;
    context.role = GpuTimestampProfiler::Role::Game;
    context.outputWidth = outputWidth;
    context.outputHeight = outputHeight;
    context.cloudWidth = width;
    context.cloudHeight = height;
    context.viewSteps = 6;
    context.resolution = 0.5f;
    // The burst is included in the enclosing AfterCloud span.
    const auto token = burst ? GpuTimestampProfiler::kInvalidToken
        : profiler.Begin(commands, GpuTimestampProfiler::Stage::MissileSmoke, context);
    const D3D12_VIEWPORT lowViewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
    const D3D12_VIEWPORT fullViewport{0, 0, static_cast<float>(outputWidth), static_cast<float>(outputHeight), 0, 1};
    const D3D12_RECT fullScissor{0, 0, static_cast<LONG>(outputWidth), static_cast<LONG>(outputHeight)};
    const D3D12_RECT compositeScissor{(std::max)(0L, bounds.left * 2 - 2), (std::max)(0L, bounds.top * 2 - 2),
        (std::min)(fullScissor.right, bounds.right * 2 + 2), (std::min)(fullScissor.bottom, bounds.bottom * 2 + 2)};
    const auto lowRtv = gpu.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    const auto sceneRtv = dx_.GetRenderTextureRTV();
    const auto dsv = dx_.GetDepthStencilView();
    commands->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    dx_.TransitionDepthBuffer(D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(*commands, gpu.color.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands->ClearRenderTargetView(lowRtv, kClear, 0, nullptr);
    commands->OMSetRenderTargets(1, &lowRtv, FALSE, nullptr);
    commands->RSSetViewports(1, &lowViewport);
    commands->RSSetScissorRects(1, &bounds);
    ID3D12DescriptorHeap* heaps[]{dx_.GetSrvDescriptorHeap()};
    commands->SetDescriptorHeaps(1, heaps);
    commands->SetGraphicsRootSignature(gpu.root.Get());
    commands->SetGraphicsRootDescriptorTable(0, dx_.GetDepthTextureSRVGPUHandle());
    commands->SetGraphicsRootConstantBufferView(2, gpu.constants->GetGPUVirtualAddress());
    commands->SetGraphicsRootShaderResourceView(3, gpu.puffBuffer->GetGPUVirtualAddress());
    commands->SetGraphicsRootShaderResourceView(4, gpu.tileBuffer->GetGPUVirtualAddress());
    commands->SetPipelineState(gpu.raymarch.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3, 1, 0, 0);

    commands->OMSetRenderTargets(1, &sceneRtv, FALSE, nullptr);
    Transition(*commands, gpu.color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commands->RSSetViewports(1, &fullViewport);
    commands->RSSetScissorRects(1, &compositeScissor);
    commands->SetGraphicsRootDescriptorTable(1, srv_.GetGPUDescriptorHandle(gpu.srvIndex));
    commands->SetPipelineState(gpu.composite.Get());
    commands->DrawInstanced(3, 1, 0, 0);

    dx_.TransitionDepthBuffer(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    commands->RSSetScissorRects(1, &fullScissor);
    commands->OMSetRenderTargets(1, &sceneRtv, FALSE, &dsv);
    profiler.End(commands, token);
}

void LockedWingSmokePass::DrawImGui() {
#ifdef USE_IMGUI
    const bool burst = style_ == Style::Burst;
    ImGui::SeparatorText(burst ? "ステージ1 撃破の立体煙" : "立体排気煙");
    if (ImGui::Checkbox(burst ? "撃破の煙を描画" : "排気煙を描画", &enabled_) && !enabled_) Reset();
    ImGui::SliderFloat("煙の濃さ", &density_, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("煙の膨らみ", &expansion_, 0.3f, 1.8f, "%.2f");
    ImGui::SliderFloat("煙の寿命", &lifetime_, burst ? 0.48f : 0.4f, burst ? 1.2f : 2.5f, "%.2f 秒");
    ImGui::Text("煙の点数 %zu / %u", puffs_.size(), kPuffCapacity);
    if (renderInitFailed_) ImGui::TextDisabled(burst ? "撃破の煙を初期化できませんでした" : "排気煙を初期化できませんでした");
#endif
}
