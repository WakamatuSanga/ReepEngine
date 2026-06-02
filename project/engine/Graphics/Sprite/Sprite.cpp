#include "Sprite.h"
#include "SpriteCommon.h"
#include "Matrix4x4.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cassert>

using namespace MatrixMath;

void Sprite::Initialize(SpriteCommon* spriteCommon) {
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;

    // 位置やサイズ、回転の初期値
    position_ = { 0.0f, 0.0f };
    size_ = { 640.0f, 360.0f };
    rotation_ = 0.0f;
    textureIndex_ = 0;

    // --- テクスチャ範囲指定の初期値 ---
    textureLeftTop_ = { 0.0f, 0.0f };
    textureSize_ = { 100.0f, 100.0f };

    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    // --- 各種バッファリソースの生成とマッピング ---

    // 1. 頂点バッファ (4頂点)
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 2. インデックスバッファ (2ポリゴン = 6インデックス)
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    // インデックスデータの設定 (0,1,2 と 1,3,2 の三角形)
    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 1; indexData_[4] = 3; indexData_[5] = 2;

    // 3. マテリアル用定数バッファ
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = false;
    materialData_->uvTransform = MakeIdentity4x4();

    // 4. トランスフォーム用定数バッファ
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Sprite::SetTexture(const std::string& filePath)
{
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(filePath);
    textureIndex_ = texMan->GetTextureIndexByFilePath(filePath);

    // テクスチャをセットした際に、切り出しサイズを画像本来のサイズにリセットする
    const DirectX::TexMetadata& metadata = texMan->GetMetaData(textureIndex_);
    textureSize_ = { static_cast<float>(metadata.width), static_cast<float>(metadata.height) };
    size_ = textureSize_; // 描画サイズも合わせるのが一般的です
}

void Sprite::Update() {
    // 座標・回転・サイズ → Transform に反映
    transform_.translate = { position_.x, position_.y, 0.0f };
    transform_.rotate = { 0.0f, 0.0f, rotation_ };
    transform_.scale = { size_.x, size_.y, 1.0f };

    Matrix4x4 world = MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
    // スプライトは2D描画なので、基本的には正射影行列等を掛けるか、シェーダー内で調整します。
    // （カメラを使わない場合、ViewProjは単位行列でOK）
    Matrix4x4 vp = MakeIdentity4x4();
    Matrix4x4 wvp = Multipty(world, vp);

    if (transformationMatrixData_) {
        transformationMatrixData_->World = world;
        transformationMatrixData_->WVP = wvp;
    }

    if (vertexData_) {
        // 頂点のローカル座標設定 (Scaleでサイズ調整するため [0,1] の矩形を作る)
        vertexData_[0].position = { 0.0f, 1.0f, 0.0f, 1.0f }; // 左下
        vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
        vertexData_[2].position = { 1.0f, 1.0f, 0.0f, 1.0f }; // 右下
        vertexData_[3].position = { 1.0f, 0.0f, 0.0f, 1.0f }; // 右上

        // --- 追加: ピクセル単位からUV座標への変換処理 ---
        const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
        float texWidth = static_cast<float>(metadata.width);
        float texHeight = static_cast<float>(metadata.height);

        // UV の計算
        float uvLeft = textureLeftTop_.x / texWidth;
        float uvTop = textureLeftTop_.y / texHeight;
        float uvRight = (textureLeftTop_.x + textureSize_.x) / texWidth;
        float uvBottom = (textureLeftTop_.y + textureSize_.y) / texHeight;

        // UVの設定 (0:左下, 1:左上, 2:右下, 3:右上)
        vertexData_[0].texcoord = { uvLeft,  uvBottom };
        vertexData_[1].texcoord = { uvLeft,  uvTop };
        vertexData_[2].texcoord = { uvRight, uvBottom };
        vertexData_[3].texcoord = { uvRight, uvTop };
    }
}

void Sprite::Draw() {
    assert(spriteCommon_);
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();
    assert(dxCommon);

    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // 頂点/インデックスバッファ
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // CBV セット（RootParam 0,1）
    if (transformationMatrixResource_) {
        commandList->SetGraphicsRootConstantBufferView(
            0, transformationMatrixResource_->GetGPUVirtualAddress());
    }
    if (materialResource_) {
        commandList->SetGraphicsRootConstantBufferView(
            1, materialResource_->GetGPUVirtualAddress());
    }

    // テクスチャ SRV セット（RootParam 2）
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    commandList->SetGraphicsRootDescriptorTable(2, srvHandle);

    // 描画
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
