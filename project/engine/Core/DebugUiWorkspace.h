#pragma once

#ifdef USE_IMGUI
#include <functional>
#include <span>

// Presentation state only. Controllers continue to own settings and actions.
namespace DebugUiWorkspace {
    enum class Category { Game, Player, Enemy, Rendering, Stage, Diagnostics, Count };
    struct Panel {
        const char* id;
        const char* label;
        Category category;
        std::function<void()> draw;
        bool independent = false;
    };
    void Initialize();
    void Shutdown();
    void DrawMenu(bool debugMode);
    void Draw(std::span<const Panel> panels);
    void RequestLayoutReset();
    void TrackWindow(const char* id, const char* name);
    bool IsVisible();
    void SelectPanel(const char* id);
    void PrepareGameView();
    bool BeginPanel(const char* name, bool* open = nullptr, int flags = 0);
    void EndPanel();
}
#endif
