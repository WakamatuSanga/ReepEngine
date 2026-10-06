#include "WinApp.h"
#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
// --------------------
// ウィンドウプロシージャ
// --------------------
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = static_cast<WinApp*>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    // Reuse dispatched input: DirectInput reacquisition alone cannot distinguish
    // an activation click from a button held in another window.
    if (app) {
        if (msg == WM_LBUTTONDOWN && GetForegroundWindow() == hwnd) {
            app->clientLeftClick_ = true;
            app->clientLeftClickPosition_ = {
                static_cast<SHORT>(LOWORD(lparam)), static_cast<SHORT>(HIWORD(lparam)) };
        } else if (msg == WM_KILLFOCUS ||
            (msg == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE) ||
            (msg == WM_ACTIVATEAPP && !wparam) || msg == WM_NCDESTROY) {
            app->clientLeftClick_ = false;
        }
    }
    if (msg == WM_NCDESTROY) SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);

#ifdef USE_IMGUI
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
        this);

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
    clientLeftClick_ = false;
    MSG msg{};

    // Drain activation and click messages before ImGui/Input/scene update.
    // Processing just one message could defer the activation click for frames.
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return false;
}

bool WinApp::WasLeftClickInClient() const {
    RECT client{};
    return clientLeftClick_ && hwnd && GetForegroundWindow() == hwnd &&
        GetClientRect(hwnd, &client) && PtInRect(&client, clientLeftClickPosition_);
}



// --------------------
// 終了処理
// --------------------
void WinApp::Finalize() {

    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        clientLeftClick_ = false;
        //CloseWindow(hwnd); // もともと main.cpp の最後にあったやつ
        hwnd = nullptr;
    }

    // CoInitializeEx とペア
    CoUninitialize();
}
