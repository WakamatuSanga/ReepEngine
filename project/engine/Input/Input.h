#pragma once
//
// 入力まわりを 1 クラスに集約するヘッダ
//  - キーボード : DirectInput (c_dfDIKeyboard)
//  - マウス     : DirectInput (c_dfDIMouse2) … 相対移動・ホイール・8ボタン
//  - パッド     : DirectInput (DIJOYSTATE2) … 最初に見つかったゲームコントローラ
//
// ※ Xbox 系コントローラは XInput も選択肢ですが、スライドに合わせて DirectInput で実装
//

// --- ヘッダ側のインクルード（型不足を防ぐ） ---
#include <windows.h>          // HINSTANCE, HWND
#include <wrl.h>              // Microsoft::WRL::ComPtr
#include <cstdint>

// --- DirectInput ---
// dinput.h の前にバージョン定義が必要
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "Engine/Core/WinApp.h"

// ------------------------------------------------------------
// 入力クラス
// ------------------------------------------------------------
class Input {
public:
    // ComPtr の短縮形（ヘッダで using namespace は使わない）
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // マウスボタン識別（DIMOUSESTATE2::rgbButtons のインデックス）
    enum MouseButton : int {
        MouseLeft = 0,
        MouseRight = 1,
        MouseMiddle = 2,
        MouseX1 = 3,
        MouseX2 = 4,
        // rgbButtons は 8 要素あるので 5～7 も使えるデバイスあり
    };

public:
    // 初期化：アプリケーションインスタンスとウィンドウハンドルを受け取る
    void Initialize(WinApp* winApp);

    // 毎フレーム更新：全デバイスの状態を取得
    void Update();

    // ------------- キーボード -------------
    // 指定キーが押されているか
    bool PushKey(BYTE keyNumber) const;
    // 指定キーのトリガ（前回離していて今回押した）
    bool TriggerKey(BYTE keyNumber) const;

    // ------------- マウス -------------
    // ボタン押下/トリガ
    bool MouseDown(MouseButton button) const;
    bool MouseTrigger(MouseButton button) const;

    // 移動量（相対値：前フレームからの差分）
    LONG  MouseDeltaX() const;  // +: 右,  -: 左
    LONG  MouseDeltaY() const;  // +: 下,  -: 上（DirectInputは下が+）
    LONG  MouseWheelDelta() const; // ホイール量（Z、上スクロールが +）

    // ------------- コントローラー（DirectInput） -------------
    // デバイスが見つかっているか
    bool  PadAvailable() const;

    // ボタン押下/トリガ（インデックスはデバイス依存：0～127）
    bool  PadButtonDown(int index) const;
    bool  PadButtonTrigger(int index) const;

    // アナログ軸の取得（正規化済み -1.0～+1.0）
    // ※ Initialize で各軸のレンジを [-1000, +1000] に揃えているので、それを 1000 で割って正規化
    float PadLX() const;  // 左スティック X（+右 / -左）
    float PadLY() const;  // 左スティック Y（+下  / -上 ※DirectInputの正方向）
    float PadRX() const;  // 右スティック X
    float PadRY() const;  // 右スティック Y
    float PadLT() const;  // L トリガ（対応デバイスのみ、未対応なら 0）
    float PadRT() const;  // R トリガ（対応デバイスのみ、未対応なら 0）

private:
    // ---- 内部ヘルパ ----
    static BOOL CALLBACK EnumGamepadCallback(const DIDEVICEINSTANCE* inst, VOID* ref);
    void SetupPadAxisProperties(); // レンジ/デッドゾーン設定
    static float NormalizeAxis(LONG v); // [-1000,+1000] → [-1,+1]

private:

    WinApp* winApp_ = nullptr;

    // --- DirectInput 本体 ---
    ComPtr<IDirectInput8> directInput_;

    // --- キーボード ---
    ComPtr<IDirectInputDevice8> keyboard_;
    BYTE key_[256] = {};   // 今回フレーム
    BYTE keyPre_[256] = {};   // 前回フレーム

    // --- マウス ---
    ComPtr<IDirectInputDevice8> mouse_;
    DIMOUSESTATE2 mouseNow_ = {}; // 今回
    DIMOUSESTATE2 mousePrev_ = {}; // 前回

    // --- パッド（最初に見つかった1台） ---
    ComPtr<IDirectInputDevice8> gamepad_; // 無ければ null
    DIJOYSTATE2 padNow_ = {}; // 今回
    DIJOYSTATE2 padPrev_ = {}; // 前回
};

// ライブラリ（プロジェクト設定でもOK）
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
