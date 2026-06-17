#include "WinApp.h"
#include <cassert>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
// --------------------
// ウィンドウプロシージャ
// --------------------
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef _DEBUG
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
#endif
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// --------------------
// 初期化
// --------------------
void WinApp::Initialize() {

    // ウィンドウクラス登録
    wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = L"CG2WindowClass";
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // クライアント領域サイズ
    RECT wrc{ 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

    // ウィンドウ生成
    hwnd = CreateWindow(
        wc.lpszClassName,
        L"LE2C_26_ワカマツ_サンガ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr, nullptr,
        wc.hInstance,
        nullptr);

    assert(hwnd != nullptr);

    ShowWindow(hwnd, SW_SHOW);
}

void WinApp::SetFullscreen(bool fullscreen) {
    if (!hwnd || isFullscreen_ == fullscreen) {
        return;
    }

    if (fullscreen) {
        windowedStyle_ = GetWindowLongPtr(hwnd, GWL_STYLE);
        windowedExStyle_ = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        windowedPlacement_.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &windowedPlacement_);

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        if (!GetMonitorInfo(monitor, &monitorInfo)) {
            return;
        }

        const LONG_PTR fullscreenStyle = windowedStyle_ & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
        const LONG_PTR fullscreenExStyle =
            windowedExStyle_ &
            ~static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

        SetWindowLongPtr(hwnd, GWL_STYLE, fullscreenStyle);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, fullscreenExStyle);
        SetWindowPos(
            hwnd,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_SHOW);
        isFullscreen_ = true;
        return;
    }

    SetWindowLongPtr(hwnd, GWL_STYLE, windowedStyle_);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, windowedExStyle_);
    SetWindowPlacement(hwnd, &windowedPlacement_);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    ShowWindow(hwnd, SW_SHOW);
    isFullscreen_ = false;
}

void WinApp::ToggleFullscreen() {
    SetFullscreen(!isFullscreen_);
}

int32_t WinApp::GetClientWidth() const {
    if (!hwnd) {
        return kClientWidth;
    }

    RECT rect{};
    if (!GetClientRect(hwnd, &rect)) {
        return kClientWidth;
    }
    return static_cast<int32_t>(rect.right - rect.left);
}

int32_t WinApp::GetClientHeight() const {
    if (!hwnd) {
        return kClientHeight;
    }

    RECT rect{};
    if (!GetClientRect(hwnd, &rect)) {
        return kClientHeight;
    }
    return static_cast<int32_t>(rect.bottom - rect.top);
}

bool WinApp::ProcessMessage()
{
    MSG msg{};

    // メッセージが来ていたら処理
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // アプリ終了メッセージかどうか
    if (msg.message == WM_QUIT) {
        return true;
    }
    return false;
}



// --------------------
// 終了処理
// --------------------
void WinApp::Finalize() {

    if (hwnd) {
        //CloseWindow(hwnd); // もともと main.cpp の最後にあったやつ
        hwnd = nullptr;
    }

    // CoInitializeEx とペア
    CoUninitialize();
}
