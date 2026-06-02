#include "Input.h"
#include <cassert>
#include <cstring>   // memcpy, memset
#include <algorithm> // std::clamp

// ========== ヘルパ ==========

// DirectInput の列挙コールバック：最初に見つかったコントローラを確保して終了
BOOL CALLBACK Input::EnumGamepadCallback(const DIDEVICEINSTANCE* inst, VOID* ref)
{
    auto* self = reinterpret_cast<Input*>(ref);
    if (!self || self->gamepad_) return DIENUM_STOP;

    HRESULT hr = self->directInput_->CreateDevice(inst->guidInstance, self->gamepad_.GetAddressOf(), nullptr);
    if (SUCCEEDED(hr)) {
        return DIENUM_STOP; // 1台確保したら終了
    }
    return DIENUM_CONTINUE;
}

// 各アナログ軸にレンジ/デッドゾーンを設定
void Input::SetupPadAxisProperties()
{
    if (!gamepad_) return;

    // レンジを [-1000, +1000] に統一（正規化都合）
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwHow = DIPH_BYOFFSET;
    range.lMin = -1000;
    range.lMax = +1000;

    // デッドゾーン（0～10000 の 1/100％表記）。25% を例示
    DIPROPDWORD deadzone{};
    deadzone.diph.dwSize = sizeof(DIPROPDWORD);
    deadzone.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    deadzone.diph.dwHow = DIPH_BYOFFSET;
    deadzone.dwData = 2500;

    // 対象軸（必要十分）：X,Y,Z,Rx,Ry,Rz
    const DWORD axes[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ };
    for (DWORD ofs : axes) {
        range.diph.dwObj = ofs;
        deadzone.diph.dwObj = ofs;
        gamepad_->SetProperty(DIPROP_RANGE, &range.diph);
        gamepad_->SetProperty(DIPROP_DEADZONE, &deadzone.diph);
    }
}

float Input::NormalizeAxis(LONG v)
{
    // [-1000,+1000] → [-1,+1] に正規化（オーバーは丸め）
    return std::clamp(v / 1000.0f, -1.0f, 1.0f);
}

// ========== 初期化 ==========

void Input::Initialize(WinApp* winApp)
{
    assert(winApp);
    winApp_ = winApp;

    // DirectInput インスタンス生成
    HRESULT hr = DirectInput8Create(
        winApp_->GetHInstance(), DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()),
        nullptr);
    assert(SUCCEEDED(hr));

    // ------ キーボード ------
    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
    assert(SUCCEEDED(hr));
    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));
    hr = keyboard_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
    std::memset(key_,    0, sizeof(key_));
    std::memset(keyPre_, 0, sizeof(keyPre_));

    // ------ マウス ------
    hr = directInput_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
    assert(SUCCEEDED(hr));
    hr = mouse_->SetDataFormat(&c_dfDIMouse2);
    assert(SUCCEEDED(hr));
    hr = mouse_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(hr));
    std::memset(&mouseNow_,  0, sizeof(mouseNow_));
    std::memset(&mousePrev_, 0, sizeof(mousePrev_));

    // ------ パッド ------
    gamepad_.Reset();
    directInput_->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumGamepadCallback, this, DIEDFL_ATTACHEDONLY);
    if (gamepad_) {
        hr = gamepad_->SetDataFormat(&c_dfDIJoystick2);
        assert(SUCCEEDED(hr));
        hr = gamepad_->SetCooperativeLevel(
            winApp_->GetHwnd(),
            DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
        assert(SUCCEEDED(hr));

        SetupPadAxisProperties();
        std::memset(&padNow_,  0, sizeof(padNow_));
        std::memset(&padPrev_, 0, sizeof(padPrev_));
    }
}
// ========== 更新 ==========

void Input::Update()
{
    // ---- キーボード ----
    std::memcpy(keyPre_, key_, sizeof(key_));
    if (keyboard_) {
        keyboard_->Acquire(); // フォーカス復帰時のため
        HRESULT hr = keyboard_->GetDeviceState(sizeof(key_), key_);
        if (FAILED(hr)) { std::memset(key_, 0, sizeof(key_)); }
    }

    // ---- マウス ----
    mousePrev_ = mouseNow_;
    if (mouse_) {
        mouse_->Acquire();
        HRESULT hr = mouse_->GetDeviceState(sizeof(mouseNow_), &mouseNow_);
        if (FAILED(hr)) { std::memset(&mouseNow_, 0, sizeof(mouseNow_)); }
    }

    // ---- パッド ----
    padPrev_ = padNow_;
    if (gamepad_) {
        // ジョイスティックは Poll → GetDeviceState が必要なデバイスが多い
        HRESULT hr = gamepad_->Poll();
        if (FAILED(hr)) { gamepad_->Acquire(); /* 再取得 */ gamepad_->Poll(); }
        hr = gamepad_->GetDeviceState(sizeof(padNow_), &padNow_);
        if (FAILED(hr)) { std::memset(&padNow_, 0, sizeof(padNow_)); }
    }
}

// ========== キーボード ==========

bool Input::PushKey(BYTE keyNumber) const
{
    // DirectInput の押下フラグは 0x80（最上位ビット）
    // スライド準拠で「0 以外」を押下扱いにしている
    return key_[keyNumber] != 0;     // 厳密にするなら：(key_[keyNumber] & 0x80) != 0
}

bool Input::TriggerKey(BYTE keyNumber) const
{
    const bool prev = keyPre_[keyNumber] != 0;
    const bool curr = key_[keyNumber] != 0;
    return (!prev && curr);
}

// ========== マウス ==========

bool Input::MouseDown(MouseButton button) const
{
    return (mouseNow_.rgbButtons[button] & 0x80) != 0;
}

bool Input::MouseTrigger(MouseButton button) const
{
    const bool prev = (mousePrev_.rgbButtons[button] & 0x80) != 0;
    const bool curr = (mouseNow_.rgbButtons[button] & 0x80) != 0;
    return (!prev && curr);
}

LONG Input::MouseDeltaX() const { return mouseNow_.lX; }
LONG Input::MouseDeltaY() const { return mouseNow_.lY; }
LONG Input::MouseWheelDelta() const { return mouseNow_.lZ; }

// ========== パッド ==========

bool Input::PadAvailable() const { return gamepad_ != nullptr; }

bool Input::PadButtonDown(int index) const
{
    if (!gamepad_ || index < 0 || index >= 128) return false;
    return (padNow_.rgbButtons[index] & 0x80) != 0;
}

bool Input::PadButtonTrigger(int index) const
{
    if (!gamepad_ || index < 0 || index >= 128) return false;
    const bool prev = (padPrev_.rgbButtons[index] & 0x80) != 0;
    const bool curr = (padNow_.rgbButtons[index] & 0x80) != 0;
    return (!prev && curr);
}

// 軸の正規化（存在しない軸は 0 を返す前提の簡易実装）
float Input::PadLX() const { return NormalizeAxis(padNow_.lX); }
float Input::PadLY() const { return NormalizeAxis(padNow_.lY); }
float Input::PadRX() const { return NormalizeAxis(padNow_.lRx); }
float Input::PadRY() const { return NormalizeAxis(padNow_.lRy); }

// トリガは対応デバイスにより Z / Rz / Slider 等に割り当てが違うため簡易対応
// ここでは Z を L、Rz を R として扱う（異なる場合は適宜差し替えてください）
float Input::PadLT() const { return NormalizeAxis(padNow_.lZ); }
float Input::PadRT() const { return NormalizeAxis(padNow_.lRz); }
