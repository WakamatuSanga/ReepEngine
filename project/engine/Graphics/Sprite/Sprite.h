#pragma once
#include "Matrix4x4.h"
#include <Windows.h>
#include "SpriteCommon.h"

// スプライト1枚分
class Sprite {
public:
    // 初期化（SpriteCommon を受け取る）
    void Initialize(SpriteCommon* spriteCommon);

    // 必要になったら使う
    void Update();
    void Draw();

    //=====================
    //  setter / getter
    //=====================

    // 座標
    const Vector2& GetPosition() const { return position_; }
    void SetPosition(const Vector2& position) { position_ = position; }

    // 回転(Z軸, radian)
    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    // サイズ（ピクセル）
    const Vector2& GetSize() const { return size_; }
    void SetSize(const Vector2& size) { size_ = size; }

    // 色（material の color をそのまま返す）
    const Vector4& GetColor() const { return materialData_->color; }
    void SetColor(const Vector4& color) { materialData_->color = color; }

    // --- テクスチャ関連 ---

   // ファイルパス指定でテクスチャをセット
    void SetTexture(const std::string& filePath);

    // すでに TextureManager に登録済みのインデックスを直接セット
    void SetTextureIndex(uint32_t textureIndex) { textureIndex_ = textureIndex; }
    uint32_t GetTextureIndex() const { return textureIndex_; }
    // テクスチャ切り出し範囲
    const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
    void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

    const Vector2& GetTextureSize() const { return textureSize_; }
    void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }
private:
    // 共通部へのポインタ
    SpriteCommon* spriteCommon_ = nullptr;

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };
    // テクスチャの切り出し左上座標（ピクセル単位）
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    // テクスチャの切り出しサイズ（ピクセル単位）
    Vector2 textureSize_ = { 100.0f, 100.0f };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float   padding[3];
        Matrix4x4 uvTransform;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    Transform transform_;

    // ─ Sprite ごとのパラメータ ─
    Vector2 position_ = { 0.0f, 0.0f };
    Vector2 size_ = { 640.0f, 360.0f };
    float   rotation_ = 0.0f;

    uint32_t textureIndex_ = 0;
};
