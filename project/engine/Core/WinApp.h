#pragma once
#include <Windows.h>
#include <cstdint>

class WinApp {
public:
    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;

public:
    WinApp() = default;
    ~WinApp() = default; 

    // ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    // 初期化 / 終了
    void Initialize();        // WindowsAPI の初期化
    bool ProcessMessage();
    void Finalize();          // WindowsAPI の終了処理 ← 追加

    // getter
    HWND      GetHwnd()      const { return hwnd; }
    HINSTANCE GetHInstance() const { return wc.hInstance; }

private:
    WNDCLASS wc{};    // ウィンドウクラス
    HWND     hwnd = nullptr;
};
