#include "DebugUiWorkspace.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    using namespace DebugUiWorkspace;
    constexpr const char* kWindow = "デバッグ###DebugWorkspace";
    constexpr const char* kTabs[]{ "ゲーム", "機体", "敵", "描画", "レール", "診断" };
    constexpr const char* kCategories[]{ "ゲーム", "プレイヤー・カメラ", "敵・中ボス", "描画・演出", "ステージ・レール", "計測・診断" };
    struct State {
        bool visible = true;
        std::string selected = "game";
        std::unordered_set<std::string> open;
        std::unordered_set<std::string> resetPending;
        std::unordered_set<std::string> workspaces{ kWindow };
        std::unordered_map<std::string, std::vector<std::string>> windows;
        Category category = Category::Game;
        const Panel* current = nullptr;
        bool rootConsumed = false;
        bool revealSelection = true;
        bool resetWorkspace = false;
        bool resetGameView = true;
        int drawnFrame = -1;
        std::vector<bool> embedded;
    } state;
    void Dirty() { ImGui::MarkIniSettingsDirty(); }
    void Place(bool gameView, ImGuiCond condition) {
        const auto* viewport = ImGui::GetMainViewport();
        const float width = viewport->WorkSize.x;
        const float side = std::clamp(width * .38f, 300.f, 520.f);
        const float top = (std::max)(viewport->WorkPos.y, viewport->Pos.y + ImGui::GetFrameHeight());
        ImGui::SetNextWindowPos({ viewport->WorkPos.x + (gameView ? side + 8 : 8), top + 8 }, condition);
        ImGui::SetNextWindowSize({ gameView ? (std::max)(240.f, width - side - 16) : (std::min)(side - 8, width - 16), (std::max)(160.f, viewport->WorkSize.y - (top - viewport->WorkPos.y) - 16) }, condition);
#ifdef IMGUI_HAS_DOCK
        if (condition == ImGuiCond_Always) ImGui::SetNextWindowDockID(0);
#endif
        if (condition == ImGuiCond_Always) ImGui::SetNextWindowCollapsed(false);
    }
    void Invoke(const Panel& panel) {
        state.current = &panel; state.rootConsumed = false;
        std::unordered_map<ImGuiWindow*, int> counts;
        auto& windows = ImGui::GetCurrentContext()->Windows;
        for (auto* window : windows) counts.emplace(window, window->BeginCount);
        if (panel.independent) {
            const auto condition = state.resetPending.contains(panel.id) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
            Place(true, condition);
        }
        panel.draw();
        IM_ASSERT(state.embedded.empty());
        // Remember only windows actually submitted by this controller, including
        // secondary editor windows. Reset never clears the application's ini.
        if (panel.independent) {
            auto& names = state.windows[panel.id];
            for (int i = 0; i < windows.Size; ++i) {
                if (windows[i]->Flags & ImGuiWindowFlags_ChildWindow) continue;
                if (counts.contains(windows[i]) && windows[i]->BeginCount == counts[windows[i]]) continue;
                if (std::find(names.begin(), names.end(), windows[i]->Name) == names.end()) names.emplace_back(windows[i]->Name);
            }
            state.resetPending.erase(panel.id);
        }
        state.current = nullptr;
    }
}
namespace DebugUiWorkspace {
    void Initialize() {
        state = {};
        ImGuiSettingsHandler handler{};
        handler.TypeName = "DebugWorkspace"; handler.TypeHash = ImHashStr(handler.TypeName);
        handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* { state.resetGameView = false; state.revealSelection = true; state.open.clear(); return &state; };
        handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
            if (std::string_view(line).starts_with("Selected=")) state.selected = line + 9;
            else if (std::string_view(line).starts_with("Open=")) state.open.emplace(line + 5);
            else if (std::string_view(line).starts_with("Visible=")) state.visible = line[8] == '1';
        };
        handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buffer) {
            buffer->appendf("[DebugWorkspace][State]\nVisible=%d\nSelected=%s\n", state.visible, state.selected.c_str());
            std::vector<std::string> open(state.open.begin(), state.open.end()); std::sort(open.begin(), open.end());
            for (const auto& id : open) buffer->appendf("Open=%s\n", id.c_str());
            buffer->append("\n");
        };
        ImGui::AddSettingsHandler(&handler);
    }
    void Shutdown() { state = {}; }
    bool IsVisible() {
        if (!ImGui::GetCurrentContext()) return state.visible;
        const auto* window = ImGui::FindWindowByName(kWindow);
        return state.visible && (!window || !window->Collapsed);
    }
    void SelectPanel(const char* id) { state.selected = id; state.visible = true; state.revealSelection = true; Dirty(); }
    void TrackWindow(const char* id, const char* name) {
        auto& names = state.windows[id];
        if (std::find(names.begin(), names.end(), name) == names.end()) names.emplace_back(name);
    }
    void RequestLayoutReset() {
        state.visible = true; state.resetWorkspace = state.resetGameView = true;
        for (const auto& name : state.workspaces) state.resetPending.emplace(name);
        const auto* viewport = ImGui::GetMainViewport();
        for (const auto& [id, names] : state.windows) {
            state.resetPending.emplace(id);
            for (const auto& name : names) {
                auto* window = ImGui::FindWindowByName(name.c_str());
                if (!window) continue;
#ifdef IMGUI_HAS_DOCK
                ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
#endif
                const float side = std::clamp(viewport->WorkSize.x * .38f, 300.f, 520.f);
                ImGui::SetWindowPos(window, { viewport->WorkPos.x + side + 16, (std::max)(viewport->WorkPos.y, viewport->Pos.y + ImGui::GetFrameHeight()) + 16 });
                ImGui::SetWindowSize(window, { (std::max)(200.f, viewport->WorkSize.x * .62f - 32), (std::max)(180.f, viewport->WorkSize.y - 48) });
                ImGui::SetWindowCollapsed(window, false);
            }
        }
        Dirty();
    }
    void DrawMenu(bool debugMode) {
        if (!ImGui::BeginMenu("デバッグ")) return;
        if (ImGui::MenuItem("デバッグ画面", nullptr, &state.visible, debugMode)) Dirty();
        if (ImGui::MenuItem("デバッグ配置を戻す", nullptr, false, debugMode)) RequestLayoutReset();
        if (!debugMode) ImGui::TextDisabled("F1でDebugへ切り替えます");
        ImGui::EndMenu();
    }
    void PrepareGameView() {
        Place(true, state.resetGameView ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
        state.resetGameView = false;
    }
    void Draw(std::span<const Panel> panels) {
        if (!ImGui::GetCurrentContext()) return;
        if (state.drawnFrame == ImGui::GetFrameCount()) return;
        state.drawnFrame = ImGui::GetFrameCount();
        const Panel* selected = nullptr;
        for (const auto& panel : panels) if (state.selected == panel.id) selected = &panel;
        if (!selected && !panels.empty()) { selected = &panels.front(); }
        if (selected) state.category = selected->category;
        // A single scene-information panel gets a separate frame. Fullscreen
        // scenes keep the gameplay DockSpace alive but intentionally invisible.
        // Never undock/rewrite the user's gameplay layout just to show that UI.
        const bool sceneFrame = panels.size() == 1 && panels.front().category == Category::Game;
        const std::string windowName = sceneFrame ? std::string("デバッグ###DebugWorkspace_") + panels.front().id : kWindow;
        state.workspaces.emplace(windowName);
        if (state.visible) {
            const bool reset = state.resetWorkspace || state.resetPending.contains(windowName);
            Place(false, reset ? ImGuiCond_Always : ImGuiCond_FirstUseEver); state.resetWorkspace = false; state.resetPending.erase(windowName);
            if (ImGui::Begin(windowName.c_str(), &state.visible, sceneFrame ? ImGuiWindowFlags_NoDocking : ImGuiWindowFlags_None)) {
                if (ImGui::SmallButton("デバッグ配置を戻す")) RequestLayoutReset();
                ImGui::SameLine(); ImGui::TextDisabled("F1: Game / Debug");
                const Category requestedCategory = state.category;
                if (!sceneFrame && ImGui::BeginTabBar("Categories", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
                    for (int category = 0; category < static_cast<int>(Category::Count); ++category) {
                        const auto group = static_cast<Category>(category);
                        bool exists = false; for (const auto& panel : panels) if (panel.category == group) exists = true;
                        if (!exists) continue;
                        const auto flags = state.revealSelection && requestedCategory == group ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                        if (ImGui::BeginTabItem(kTabs[category], nullptr, flags)) {
                            // SetSelected may take effect after the formerly active
                            // tab has begun this frame. Do not overwrite a request.
                            if (state.revealSelection && group != requestedCategory) { ImGui::EndTabItem(); continue; }
                            if (state.category != group) {
                                state.category = group;
                                for (const auto& panel : panels) if (panel.category == group) { selected = &panel; state.selected = panel.id; break; }
                                Dirty();
                            }
                            ImGui::TextDisabled("%s", kCategories[category]);
                            ImGui::BeginChild("Navigation", { 0, 140 }, ImGuiChildFlags_Borders);
                            for (const auto& panel : panels) if (panel.category == group) {
                                ImGui::PushID(panel.id);
                                if (ImGui::Selectable(panel.label, state.selected == panel.id)) { state.selected = panel.id; selected = &panel; Dirty(); }
                                ImGui::PopID();
                            }
                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
                state.revealSelection = false;
                if (selected) {
                    ImGui::SeparatorText(selected->label);
                    if (selected->independent) {
                        bool open = state.open.contains(selected->id);
                        if (ImGui::Checkbox("独立エディタを開く", &open)) { if (open) state.open.emplace(selected->id); else state.open.erase(selected->id); Dirty(); }
                        ImGui::TextWrapped("このエディタは独立窓で操作します。値は担当クラスが保持します。");
                    } else {
                        ImGui::BeginChild("Contents", { 0, 0 }); ImGui::PushID(selected->id); Invoke(*selected); ImGui::PopID(); ImGui::EndChild();
                    }
                }
            }
            ImGui::End();
        }
        for (const auto& panel : panels) if (panel.independent && state.open.contains(panel.id)) Invoke(panel);
    }
    bool BeginPanel(const char* name, bool* open, int flags) {
        const bool root = state.current && !state.rootConsumed;
        if (root) state.rootConsumed = true;
        const bool embedded = root && !state.current->independent;
        state.embedded.push_back(embedded);
        if (embedded) {
            ImGui::GetCurrentContext()->NextWindowData.ClearFlags();
            return ImGui::BeginChild(name, { 0, 0 }, ImGuiChildFlags_None, flags & ~ImGuiWindowFlags_AlwaysAutoResize);
        }
        if (root) {
            const std::string title = std::string(state.current->label) + "###DebugPanel_" + state.current->id;
            bool visible = true;
            bool* actualOpen = open ? open : &visible;
            const bool result = ImGui::Begin(title.c_str(), actualOpen, flags);
            if (!*actualOpen) { state.open.erase(state.current->id); Dirty(); }
            return result;
        }
        return ImGui::Begin(name, open, flags);
    }
    void EndPanel() {
        IM_ASSERT(!state.embedded.empty());
        const bool embedded = state.embedded.back(); state.embedded.pop_back();
        if (embedded) ImGui::EndChild(); else ImGui::End();
    }
}
#endif
