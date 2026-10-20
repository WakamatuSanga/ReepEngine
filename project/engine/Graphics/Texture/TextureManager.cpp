#include "TextureManager.h"
#include <cassert>
#include <cctype>
#include <filesystem>
#include <stdexcept>

std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance() {
    if (!instance_) {
        instance_.reset(new TextureManager());
    }
    return instance_.get();
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    textureDatas.clear();
    textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::Finalize() {
    textureDatas.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

bool TextureManager::IsLoaded(const std::string& filePath) const {
    return std::any_of(textureDatas.begin(), textureDatas.end(),
        [&](const TextureData& data) { return data.filePath == filePath; });
}

void TextureManager::LoadTexture(const std::string& filePath) {
    assert(dxCommon_);

    auto it = std::find_if(
        textureDatas.begin(), textureDatas.end(),
        [&](TextureData& data) { return data.filePath == filePath; });
    if (it != textureDatas.end()) {
        return;
    }

    if (!srvManager_->CanAllocate()) throw std::runtime_error("Texture descriptor capacity: " + filePath);

    TextureData textureData{};
    textureData.filePath = filePath;

    std::wstring wFilePath = StringUtility::ConvertString(filePath);
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extension == ".dds") {
        DirectX::ScratchImage ddsImage{};
        HRESULT hr = DirectX::LoadFromDDSFile(
            wFilePath.c_str(),
            DirectX::DDS_FLAGS_NONE,
            &textureData.metadata,
            ddsImage);
        if (FAILED(hr)) throw std::runtime_error("Texture decode/mipmap failed: " + filePath);

        textureData.metadata = ddsImage.GetMetadata();
        textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
        textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource, ddsImage);
    } else {
        DirectX::ScratchImage image{};
        HRESULT hr = DirectX::LoadFromWICFile(
            wFilePath.c_str(),
            DirectX::WIC_FLAGS_FORCE_SRGB,
            &textureData.metadata,
            image);
        if (FAILED(hr)) throw std::runtime_error("Texture decode/mipmap failed: " + filePath);

        DirectX::ScratchImage mipImages{};
        hr = DirectX::GenerateMipMaps(
            image.GetImages(),
            image.GetImageCount(),
            image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB,
            0,
            mipImages);
        if (FAILED(hr)) throw std::runtime_error("Texture decode/mipmap failed: " + filePath);

        textureData.metadata = mipImages.GetMetadata();
        textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
        textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);
    }

    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    if (textureData.metadata.IsCubemap()) {
        srvManager_->CreateSRVforTextureCube(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            static_cast<UINT>(textureData.metadata.mipLevels)
        );
    } else {
        srvManager_->CreateSRVforTexture2D(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            static_cast<UINT>(textureData.metadata.mipLevels)
        );
    }
    textureDatas.push_back(std::move(textureData));
}

void TextureManager::ReleaseIntermediateResources() {
    for (auto& data : textureDatas) {
        data.intermediateResource.Reset();
    }
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) {
    auto it = std::find_if(
        textureDatas.begin(), textureDatas.end(),
        [&](TextureData& data) { return data.filePath == filePath; });

    if (it != textureDatas.end()) {
        return static_cast<uint32_t>(std::distance(textureDatas.begin(), it));
    }
    assert(false);
    return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex) {
    assert(textureIndex < textureDatas.size());
    return textureDatas[textureIndex].srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t textureIndex) {
    assert(textureIndex < textureDatas.size());
    return textureDatas[textureIndex].metadata;
}
