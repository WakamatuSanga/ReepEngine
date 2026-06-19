#include "ParticleManager.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <random>
#include <cassert>
#include <vector>
#include <string>
#include <Windows.h>
#include <algorithm>
#include <cmath>

using namespace MatrixMath;
using namespace Microsoft::WRL;

std::unique_ptr<ParticleManager> ParticleManager::instance_ = nullptr;

ParticleManager* ParticleManager::GetInstance() {
    if (!instance_) {
        instance_.reset(new ParticleManager());
    }
    return instance_.get();
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreatePipelineState();
    CreateModel();

    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kMaxInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndex_, instancingResource_.Get(), kMaxInstance, sizeof(ParticleForGPU));

    textureName_ = "resources/obj/axis/uvChecker.png";
    TextureManager::GetInstance()->LoadTexture(textureName_);
}

void ParticleManager::Finalize() {
    particles_.clear();
}

void ParticleManager::SetTexture(const std::string& texturePath) {
    if (texturePath.empty() || textureName_ == texturePath) {
        return;
    }

    textureName_ = texturePath;
    TextureManager::GetInstance()->LoadTexture(textureName_);
}

void ParticleManager::Update(Camera* camera) {
    const float deltaTime = FrameTimer::GetInstance().GetGameplayDeltaTime();
    const float legacyFrameScale = deltaTime * 60.0f;
    Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
    Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    uint32_t numInstance = 0;
    for (auto it = particles_.begin(); it != particles_.end();) {
        it->currentTime += deltaTime;
        if (it->currentTime >= it->lifeTime) {
            it = particles_.erase(it);
            continue;
        }

        it->transform.translate.x += it->velocity.x * legacyFrameScale;
        it->transform.translate.y += it->velocity.y * legacyFrameScale;
        it->transform.translate.z += it->velocity.z * legacyFrameScale;

        float alpha = 1.0f - (it->currentTime / it->lifeTime);
        it->color.w = alpha;

        if (numInstance < kMaxInstance) {
            Matrix4x4 scaleMat = MakeScale(it->transform.scale);
            Matrix4x4 rotateMat = MakeRotateZ(it->transform.rotate.z);
            Matrix4x4 transMat = MakeTranslate(it->transform.translate);
            Matrix4x4 worldMat = Multipty(Multipty(Multipty(scaleMat, rotateMat), billboardMatrix), transMat);

            instancingData_[numInstance].World = worldMat;
            instancingData_[numInstance].WVP = Multipty(worldMat, viewProj);
            instancingData_[numInstance].color = it->color;
            numInstance++;
        }
        ++it;
    }
}

void ParticleManager::Draw() {
    uint32_t numInstance = static_cast<uint32_t>(particles_.size());
    if (numInstance == 0) return;
    if (numInstance > kMaxInstance) numInstance = kMaxInstance;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(srvIndex_));

    uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureName_);
    commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));

    commandList->DrawInstanced(4, numInstance, 0, 0);
}

void ParticleManager::Emit(const std::string& name, const Vector3& pos, uint32_t count) {
    const bool isEffectName = (name == "Hit" || name == "Fireball" || name == "Wind");
    if (!isEffectName) {
        SetTexture(name);
    }

    std::random_device rd;
    std::mt19937 gen(rd()); // C++標準の正しい乱数エンジン
    auto RandomRange = [&gen](float minValue, float maxValue) {
        if (minValue > maxValue) {
            std::swap(minValue, maxValue);
        }
        std::uniform_real_distribution<float> dist(minValue, maxValue);
        return dist(gen);
        };
    auto Normalize = [](const Vector3& value) {
        float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lengthSq <= 0.0001f) {
            return Vector3{ 0.0f, 0.0f, 1.0f };
        }
        float invLength = 1.0f / std::sqrt(lengthSq);
        return Vector3{ value.x * invLength, value.y * invLength, value.z * invLength };
        };
    auto RandomUnitDirection = [&]() {
        return Normalize({
            RandomRange(-1.0f, 1.0f),
            RandomRange(-1.0f, 1.0f),
            RandomRange(-1.0f, 1.0f)
            });
        };
    auto RandomColor = [&](const EffectParams& params) {
        return Vector4{
            RandomRange(params.colorRRange.x, params.colorRRange.y),
            RandomRange(params.colorGRange.x, params.colorGRange.y),
            RandomRange(params.colorBRange.x, params.colorBRange.y),
            1.0f
        };
        };
    auto PushParticle = [&](const EffectParams& params, const Vector3& spawnPos, const Vector3& velocity, float scaleX, float scaleY, float rotateZ, const Vector4& color) {
        Particle particle;
        particle.transform = {
            { scaleX, scaleY, 1.0f },
            { 0.0f, 0.0f, rotateZ },
            spawnPos
        };
        particle.velocity = velocity;
        particle.color = color;
        particle.lifeTime = RandomRange(params.lifeTimeRange.x, params.lifeTimeRange.y);
        particle.currentTime = 0.0f;
        particles_.push_back(particle);
        };

    if (name == "Hit") {
        uint32_t emitCount = count > 0 ? count : hitEffectParams_.spawnCount;
        for (uint32_t i = 0; i < emitCount; ++i) {
            // Plane billboard particles are stretched into short-lived streaks
            // and emitted radially to form the assignment-style hit effect.
            Vector3 direction = RandomUnitDirection();
            float speed = RandomRange(hitEffectParams_.speedRange.x, hitEffectParams_.speedRange.y);
            PushParticle(
                hitEffectParams_,
                pos,
                { direction.x * speed, direction.y * speed, direction.z * speed },
                RandomRange(hitEffectParams_.scaleXRange.x, hitEffectParams_.scaleXRange.y),
                RandomRange(hitEffectParams_.scaleYRange.x, hitEffectParams_.scaleYRange.y),
                RandomRange(hitEffectParams_.rotateZRange.x, hitEffectParams_.rotateZRange.y),
                RandomColor(hitEffectParams_));
        }
        return;
    }

    if (name == "Fireball") {
        uint32_t emitCount = count > 0 ? count : fireballEffectParams_.spawnCount;
        for (uint32_t i = 0; i < emitCount; ++i) {
            bool isCore = i < (emitCount * 2 / 3);
            Vector3 offset = {
                RandomRange(-0.12f, 0.12f),
                RandomRange(-0.12f, 0.12f),
                RandomRange(-0.08f, 0.08f)
            };
            Vector3 direction = Normalize({
                RandomRange(-0.28f, 0.28f),
                RandomRange(-0.18f, 0.28f),
                1.0f + RandomRange(0.0f, 0.35f)
                });
            float speed = RandomRange(fireballEffectParams_.speedRange.x, fireballEffectParams_.speedRange.y);
            if (isCore) {
                speed *= 0.45f;
                offset.x *= 0.35f;
                offset.y *= 0.35f;
                offset.z *= 0.35f;
            }

            Vector4 color = RandomColor(fireballEffectParams_);
            if (isCore) {
                color.x = 1.0f;
                color.y = (std::min)(1.0f, color.y + 0.2f);
            }

            float scaleX = RandomRange(fireballEffectParams_.scaleXRange.x, fireballEffectParams_.scaleXRange.y);
            float scaleY = RandomRange(fireballEffectParams_.scaleYRange.x, fireballEffectParams_.scaleYRange.y);
            if (!isCore) {
                scaleX *= 0.55f;
                scaleY *= 0.75f;
            }

            PushParticle(
                fireballEffectParams_,
                { pos.x + offset.x, pos.y + offset.y, pos.z + offset.z },
                { direction.x * speed, direction.y * speed, direction.z * speed },
                scaleX,
                scaleY,
                RandomRange(fireballEffectParams_.rotateZRange.x, fireballEffectParams_.rotateZRange.y),
                color);
        }
        return;
    }

    if (name == "Wind") {
        uint32_t emitCount = count > 0 ? count : windEffectParams_.spawnCount;
        for (uint32_t i = 0; i < emitCount; ++i) {
            Vector3 spawnPos = {
                pos.x + RandomRange(-0.25f, 0.25f),
                pos.y + RandomRange(-0.35f, 0.35f),
                pos.z + RandomRange(-0.25f, 0.25f)
            };
            Vector3 direction = Normalize({
                1.0f,
                RandomRange(-0.15f, 0.15f),
                RandomRange(-0.10f, 0.10f)
                });
            float speed = RandomRange(windEffectParams_.speedRange.x, windEffectParams_.speedRange.y);
            PushParticle(
                windEffectParams_,
                spawnPos,
                { direction.x * speed, direction.y * speed, direction.z * speed },
                RandomRange(windEffectParams_.scaleXRange.x, windEffectParams_.scaleXRange.y),
                RandomRange(windEffectParams_.scaleYRange.x, windEffectParams_.scaleYRange.y),
                RandomRange(windEffectParams_.rotateZRange.x, windEffectParams_.rotateZRange.y),
                RandomColor(windEffectParams_));
        }
        return;
    }

    std::uniform_real_distribution<float> distVelocity(-0.05f, 0.05f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle particle;
        particle.transform = { {1.0f, 1.0f, 1.0f}, {0,0,0}, pos };
        particle.velocity = { distVelocity(gen), distVelocity(gen), distVelocity(gen) };
        particle.color = { distColor(gen), distColor(gen), distColor(gen), 1.0f };
        particle.lifeTime = distTime(gen);
        particle.currentTime = 0.0f;
        particles_.push_back(particle);
    }
}

void ParticleManager::CreateRootSignature() {
    HRESULT hr = S_OK;
    D3D12_ROOT_PARAMETER rootParameters[2]{};

    D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_DESCRIPTOR_RANGE descriptorRangeTex[1]{};
    descriptorRangeTex[0].BaseShaderRegister = 1;
    descriptorRangeTex[0].NumDescriptors = 1;
    descriptorRangeTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeTex;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = staticSamplers;
    desc.NumStaticSamplers = _countof(staticSamplers);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePipelineState() {
    ComPtr<IDxcBlob> vs = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> ps = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,   0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.InputLayout = { inputLayout, _countof(inputLayout) };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel() {
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 4);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    // Particles use a procedurally generated plane quad and face the camera in Update().
    vertexData[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0,0,-1} };
    vertexData[1] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0,0,-1} };
    vertexData[2] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0,0,-1} };
    vertexData[3] = { { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0,0,-1} };
}
