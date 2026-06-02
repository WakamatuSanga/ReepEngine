#pragma once
#include "Engine/Core/DirectXCommon.h"
#include <wrl.h>
#include <vector>

class Object3dCommon
{
public:
    // ブレンドモード定義
    enum class BlendMode {
        kNormal,   // 通常
        kAdd,      // 加算
        kSubtract, // 減算
        kMultiply, // 乗算
        kScreen,   // スクリーン
        kNone,     // なし
        kCountOf,
    };

public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定
    // blendModeを指定できるように変更
    void CommonDrawSetting(BlendMode blendMode = BlendMode::kNormal);

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    // ルートシグネチャの作成
    void CreateRootSignature();
    // グラフィックスパイプラインの生成
    void CreateGraphicsPipelineStates();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    // ブレンドモードごとのPSOを配列で持つ
    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> graphicsPipelineStates_;
};
