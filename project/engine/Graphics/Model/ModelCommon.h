#pragma once
#include "Engine/Core/DirectXCommon.h"

class ModelCommon {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
};
