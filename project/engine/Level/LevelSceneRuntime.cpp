#include "LevelSceneRuntime.h"
#include "LevelEventConnectionVisualizer.h"
#include "LevelEventDebugView.h"
#include "LevelEventLabelVisualizer.h"
#include "LevelEventObjectActionVisualizer.h"
#include "LevelEventVisualizer.h"
#include "LevelObjectDebugVisualizer.h"
#include "LevelSceneLoader.h"
#include <algorithm>
#include <cstdint>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    const LevelObject* FindObjectByTreeIndexRecursive(
        const LevelObject& object,
        int targetObjectIndex,
        int& currentObjectIndex) {
        if (currentObjectIndex == targetObjectIndex) {
            return &object;
        }

        ++currentObjectIndex;
        for (const LevelObject& child : object.children) {
            if (const LevelObject* foundObject =
                FindObjectByTreeIndexRecursive(child, targetObjectIndex, currentObjectIndex)) {
                return foundObject;
            }
        }

        return nullptr;
    }

    const LevelObject* FindObjectByTreeIndex(const LevelSceneData& sceneData, int targetObjectIndex) {
        if (targetObjectIndex < 0) {
            return nullptr;
        }

        int currentObjectIndex = 0;
        for (const LevelObject& object : sceneData.objects) {
            if (const LevelObject* foundObject =
                FindObjectByTreeIndexRecursive(object, targetObjectIndex, currentObjectIndex)) {
                return foundObject;
            }
        }

        return nullptr;
    }

#ifdef _DEBUG
    void DrawObjectTreeRecursive(
        const LevelObject& object,
        int& objectIndex,
        int& selectedObjectIndex,
        const LevelObjectDebugVisualizer& objectDebugVisualizer) {
        const int currentIndex = objectIndex++;
        const std::string label = object.name.empty()
            ? "(unnamed) [" + object.type + "]"
            : object.name + " [" + object.type + "]";

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (object.children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selectedObjectIndex == currentIndex) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool isOpen = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<intptr_t>(currentIndex + 1)),
            flags,
            "%s",
            label.c_str());
        if (ImGui::IsItemClicked()) {
            selectedObjectIndex = currentIndex;
        }
        if (isOpen) {
            objectDebugVisualizer.DrawObjectDetails(object);
            for (const LevelObject& child : object.children) {
                DrawObjectTreeRecursive(child, objectIndex, selectedObjectIndex, objectDebugVisualizer);
            }
            ImGui::TreePop();
        }
    }
#endif
}

LevelSceneRuntime::LevelSceneRuntime() = default;

LevelSceneRuntime::~LevelSceneRuntime() = default;

void LevelSceneRuntime::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    loader_ = std::make_unique<LevelSceneLoader>();
    objectDebugVisualizer_ = std::make_unique<LevelObjectDebugVisualizer>();
    eventVisualizer_ = std::make_unique<LevelEventVisualizer>();
    connectionVisualizer_ = std::make_unique<LevelEventConnectionVisualizer>();
    objectActionVisualizer_ = std::make_unique<LevelEventObjectActionVisualizer>();
    labelVisualizer_ = std::make_unique<LevelEventLabelVisualizer>();
    objectDebugVisualizer_->Initialize(object3dCommon, camera);
    eventVisualizer_->Initialize(object3dCommon, camera);
    connectionVisualizer_->Initialize(object3dCommon, camera);
    objectActionVisualizer_->Initialize(object3dCommon, camera);
    labelVisualizer_->Initialize(camera);
    SetPathBufferText(jsonPath_);
    LoadJsonFromBuffer();
}

void LevelSceneRuntime::Update() {
    ++frameCounter_;

    if (rebuildDirty_ && !freezeDebugObjects_) {
        RebuildDebugObjects();
    }

    if (objectDebugVisualizer_) { objectDebugVisualizer_->Update(frameCounter_); }
    if (eventVisualizer_) { eventVisualizer_->Update(frameCounter_); }
    if (connectionVisualizer_) { connectionVisualizer_->Update(frameCounter_); }
    if (objectActionVisualizer_) { objectActionVisualizer_->Update(frameCounter_); }
}

void LevelSceneRuntime::Draw() {
    if (objectDebugVisualizer_) {
        objectDebugVisualizer_->Update(frameCounter_);
        objectDebugVisualizer_->Draw();
    }
    if (eventVisualizer_) { eventVisualizer_->Draw(frameCounter_); }
    if (connectionVisualizer_) { connectionVisualizer_->Draw(frameCounter_); }
    if (objectActionVisualizer_) { objectActionVisualizer_->Draw(frameCounter_); }
}

void LevelSceneRuntime::DrawImGui() {
#ifdef _DEBUG
    if (labelVisualizer_) {
        labelVisualizer_->DrawOverlay();
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("レベルエディタ確認 (Level Editor Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("JSONパス (JSON Path)", jsonPathBuffer_.data(), jsonPathBuffer_.size());
    if (ImGui::Button("JSON読込 (Load JSON)")) {
        LoadJsonFromBuffer();
    }
    ImGui::SameLine();
    if (objectDebugVisualizer_ && objectDebugVisualizer_->DrawImGui()) {
        RequestRebuild("Manual");
    }
    if (ImGui::Checkbox("軸変換 (Axis Conversion)", &axisConversionEnabled_)) {
        RequestRebuild("Manual");
    }
    ImGui::Checkbox("レベルデバッグ表示を固定 (Freeze Level Debug Objects)", &freezeDebugObjects_);
    ImGui::Checkbox("ライブ反映を一時停止 (Pause Live Apply)", &pauseLiveApply_);
    ImGui::Checkbox("JSON変更時のみ再構築 (Rebuild Only When JSON Changed)", &rebuildOnlyWhenJsonChanged_);

    ImGui::Separator();
    ImGui::TextWrapped("最後の読込状態 (Last Load Status): %s", lastLoadStatus_.c_str());
    ImGui::TextWrapped("解決済みパス (Resolved Path): %s", lastResolvedPath_.empty() ? "(none)" : lastResolvedPath_.c_str());
    ImGui::Text("軸変換 (Axis Conversion): %s", axisConversionEnabled_ ? "Blender(x,y,z) -> Engine(x,z,y)" : "OFF");
    ImGui::Text("シーン名 (Scene Name): %s", sceneData_.name.empty() ? "(none)" : sceneData_.name.c_str());
    ImGui::Text("オブジェクト数 (Object Count): %zu", sceneData_.GetObjectCount());
    ImGui::Text("表示オブジェクト数 (Visible Object Count): %zu", objectDebugVisualizer_ ? objectDebugVisualizer_->GetVisibleObjectCount() : 0);
    ImGui::Text("不明モデル数 (Missing Model Count): %zu", objectDebugVisualizer_ ? objectDebugVisualizer_->GetMissingModelCount() : 0);
    ImGui::Text("デバッグ表示数 (Debug Object Count): %zu", objectDebugVisualizer_ ? objectDebugVisualizer_->GetDebugObjectCount() : 0);
    ImGui::Text("再構築回数 (Rebuild Count): %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("最後に再構築したフレーム (Last Rebuild Frame): %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text(
        "最後のデバッグ行列更新フレーム (Last Debug Matrix Update Frame): %llu",
        static_cast<unsigned long long>(objectDebugVisualizer_ ? objectDebugVisualizer_->GetLastMatrixUpdateFrame() : 0));
    ImGui::Text("最後の反映元 (Last Apply Source): %s", lastApplySource_.c_str());
    ImGui::Text("自動反映有効 (Auto Apply Enabled): %s", liveAutoApplyEnabled_ ? "true" : "false");
    ImGui::Text("最後に反映したパケット (Last Packet Applied): %llu", static_cast<unsigned long long>(lastPacketApplied_));
    ImGui::Text("再構築待ち (Rebuild Dirty): %s", rebuildDirty_ ? "true" : "false");

    const LevelObject* selectedObject = FindObjectByTreeIndex(sceneData_, selectedObjectIndex_);
    if (ImGui::TreeNode("選択中オブジェクト情報 (Selected Object Info)")) {
        if (selectedObject) {
            if (objectDebugVisualizer_) {
                objectDebugVisualizer_->DrawObjectDebugDetails(*selectedObject);
            }
        } else {
            ImGui::TextDisabled("オブジェクトツリーでオブジェクトを選択してください。 (Select an object in Object Tree.)");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("イベントフラグ確認 (Event Flag Debug)")) {
        if (DrawLevelEventDebugImGui(
            sceneData_,
            selectedObject,
            eventVisualizer_.get(),
            connectionVisualizer_.get(),
            objectActionVisualizer_.get(),
            labelVisualizer_.get())) {
            RequestRebuild("Manual");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("オブジェクトツリー (Object Tree)")) {
        if (sceneData_.objects.empty()) {
            ImGui::TextDisabled("オブジェクトが読み込まれていません。 (No objects loaded.)");
        } else {
            int objectIndex = 0;
            if (objectDebugVisualizer_) {
                for (const LevelObject& object : sceneData_.objects) {
                    DrawObjectTreeRecursive(object, objectIndex, selectedObjectIndex_, *objectDebugVisualizer_);
                }
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
#endif
}

void LevelSceneRuntime::SetGameViewRect(float x, float y, float width, float height) {
    if (labelVisualizer_) {
        labelVisualizer_->SetViewportRect(x, y, width, height);
    }
}

void LevelSceneRuntime::ClearGameViewRect() {
    if (labelVisualizer_) {
        labelVisualizer_->ClearViewportRect();
    }
}

void LevelSceneRuntime::ApplySceneData(
    const LevelSceneData& sceneData,
    const std::string& statusMessage,
    const std::string& sourceName) {
    sceneData_ = sceneData;
    lastLoadStatus_ = statusMessage;
    lastResolvedPath_ = sourceName;
    selectedObjectIndex_ = -1;
    RequestRebuild(sourceName.find("UDP") != std::string::npos ? "LiveSync" : sourceName);
}

void LevelSceneRuntime::SetLiveSyncDiagnostics(bool autoApplyEnabled, uint64_t lastPacketApplied) {
    liveAutoApplyEnabled_ = autoApplyEnabled;
    lastPacketApplied_ = lastPacketApplied;
}

void LevelSceneRuntime::LoadJsonFromBuffer() {
    if (!loader_) {
        loader_ = std::make_unique<LevelSceneLoader>();
    }

    jsonPath_ = jsonPathBuffer_.data();
    LevelSceneLoader::LoadResult result = loader_->LoadFromFile(jsonPath_, sceneData_);
    lastLoadStatus_ = result.message;
    lastResolvedPath_ = result.resolvedPath;
    selectedObjectIndex_ = -1;

    if (result.success) {
        RequestRebuild("LoadJson");
    } else {
        if (objectDebugVisualizer_) {
            objectDebugVisualizer_->Clear();
        }
        if (eventVisualizer_) {
            eventVisualizer_->Clear();
        }
        if (connectionVisualizer_) {
            connectionVisualizer_->Clear();
        }
        if (objectActionVisualizer_) {
            objectActionVisualizer_->Clear();
        }
        if (labelVisualizer_) {
            labelVisualizer_->Clear();
        }
        rebuildDirty_ = false;
    }
}

void LevelSceneRuntime::SetPathBufferText(const std::string& text) {
    jsonPathBuffer_.fill('\0');
    const size_t copyCount = (std::min)(text.size(), jsonPathBuffer_.size() - 1);
    std::copy_n(text.data(), copyCount, jsonPathBuffer_.data());
}

void LevelSceneRuntime::RequestRebuild(const std::string& applySource) {
    rebuildDirty_ = true;
    pendingRebuildSource_ = applySource;
    lastApplySource_ = applySource;
}

void LevelSceneRuntime::RebuildDebugObjects() {
    rebuildDirty_ = false;
    lastApplySource_ = pendingRebuildSource_.empty() ? lastApplySource_ : pendingRebuildSource_;
    pendingRebuildSource_.clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter_;

    if (objectDebugVisualizer_) {
        objectDebugVisualizer_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    if (eventVisualizer_) {
        eventVisualizer_->Rebuild(sceneData_, axisConversionEnabled_, frameCounter_);
    }
    if (connectionVisualizer_) {
        connectionVisualizer_->Rebuild(sceneData_, axisConversionEnabled_, frameCounter_);
    }
    if (objectActionVisualizer_) {
        objectActionVisualizer_->Rebuild(sceneData_, axisConversionEnabled_, frameCounter_);
    }
    if (labelVisualizer_) {
        labelVisualizer_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    if (objectDebugVisualizer_) {
        objectDebugVisualizer_->Update(frameCounter_);
    }
    if (eventVisualizer_) {
        eventVisualizer_->Update(frameCounter_);
    }
    if (connectionVisualizer_) {
        connectionVisualizer_->Update(frameCounter_);
    }
    if (objectActionVisualizer_) {
        objectActionVisualizer_->Update(frameCounter_);
    }
}
