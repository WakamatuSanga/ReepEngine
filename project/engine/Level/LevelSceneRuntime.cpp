#include "LevelSceneRuntime.h"
#include "LevelEventConnectionVisualizer.h"
#include "LevelEventDebugView.h"
#include "LevelEventLabelVisualizer.h"
#include "LevelEventObjectActionVisualizer.h"
#include "LevelEventRuntime.h"
#include "LevelEventVisualizer.h"
#include "LevelObjectDebugVisualizer.h"
#include "LevelRailDebugVisualizer.h"
#include "LevelRailRuntime.h"
#include "LevelSceneLoader.h"
#include "LevelTransformConverter.h"
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

    struct LevelSceneQueryTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 MultiplyVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
    }

    LevelSceneQueryTransform CombineTransform(
        const LevelSceneQueryTransform& parent,
        const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }

    bool MatchesTargetObject(const LevelObject& object, const std::string& objectId, const std::string& objectName) {
        if (!objectId.empty()) {
            return object.objectId == objectId;
        }
        return !objectName.empty() && object.name == objectName;
    }

    bool TryFindObjectWorldPositionRecursive(
        const LevelObject& object,
        const LevelSceneQueryTransform& parentTransform,
        bool axisConversionEnabled,
        const std::string& objectId,
        const std::string& objectName,
        Vector3& outPosition) {
        const LevelTransform localTransform = axisConversionEnabled
            ? BlenderToEngineTransform(object.transform)
            : object.transform;
        const LevelSceneQueryTransform objectWorld = CombineTransform(parentTransform, localTransform);

        if (MatchesTargetObject(object, objectId, objectName)) {
            outPosition = objectWorld.translation;
            return true;
        }

        for (const LevelObject& child : object.children) {
            if (TryFindObjectWorldPositionRecursive(
                child,
                objectWorld,
                axisConversionEnabled,
                objectId,
                objectName,
                outPosition)) {
                return true;
            }
        }

        return false;
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
    eventRuntime_ = std::make_unique<LevelEventRuntime>();
    railDebugVisualizer_ = std::make_unique<LevelRailDebugVisualizer>();
    railRuntime_ = std::make_unique<LevelRailRuntime>();
    objectDebugVisualizer_->Initialize(object3dCommon, camera);
    eventVisualizer_->Initialize(object3dCommon, camera);
    connectionVisualizer_->Initialize(object3dCommon, camera);
    objectActionVisualizer_->Initialize(object3dCommon, camera);
    labelVisualizer_->Initialize(camera);
    eventRuntime_->Initialize(object3dCommon, camera);
    railDebugVisualizer_->Initialize(object3dCommon, camera);
    railRuntime_->Initialize(object3dCommon, camera);
    eventVisualizer_->SetRuntimeStateProvider(eventRuntime_.get());
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
    if (eventRuntime_) { eventRuntime_->Update(frameCounter_); }
    if (railDebugVisualizer_) { railDebugVisualizer_->Update(frameCounter_); }
    if (railRuntime_) { railRuntime_->Update(1.0f / 60.0f, frameCounter_); }
}

void LevelSceneRuntime::Draw() {
    if (objectDebugVisualizer_) {
        objectDebugVisualizer_->Update(frameCounter_);
        objectDebugVisualizer_->Draw();
    }
    const bool hideEventDebug = ShouldHideEventDebug();
    const bool hideRailDebug = ShouldHideRailDebug();
    const bool hideRailPoints = ShouldHideRailPoints();

    if (eventVisualizer_) {
        eventVisualizer_->SetExternalDebugHidden(hideEventDebug);
    }
    if (!hideEventDebug) {
        if (eventVisualizer_) { eventVisualizer_->Draw(frameCounter_); }
        if (connectionVisualizer_) { connectionVisualizer_->Draw(frameCounter_); }
        if (objectActionVisualizer_) { objectActionVisualizer_->Draw(frameCounter_); }
        if (eventRuntime_) { eventRuntime_->Draw(frameCounter_); }
    } else if (eventVisualizer_) {
        eventVisualizer_->Draw(frameCounter_);
    }

    if (railDebugVisualizer_) {
        railDebugVisualizer_->SetExternalDebugVisibility(hideRailDebug, hideRailPoints);
        railDebugVisualizer_->Draw(frameCounter_);
    }
    if (railRuntime_) {
        railRuntime_->SetExternalDebugActorHidden(hideRailDebug || gameplayPreviewMode_);
        railRuntime_->Draw(frameCounter_);
    }
}

void LevelSceneRuntime::DrawImGui() {
#ifdef _DEBUG
    if (labelVisualizer_ && !ShouldHideEventDebug()) {
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
    ImGui::Text("初期カメラあり (Camera Start Exists): %s", hasEngineCameraStart_ ? "true" : "false");
    if (hasEngineCameraStart_) {
        ImGui::Text("初期カメラ位置 (Camera Start Position): %.3f, %.3f, %.3f",
            engineCameraStart_.transform.translation.x,
            engineCameraStart_.transform.translation.y,
            engineCameraStart_.transform.translation.z);
    }
    ImGui::Text("オブジェクト数 (Object Count): %zu", sceneData_.GetObjectCount());
    ImGui::Text("レール数 (Rail Count): %zu", sceneData_.rails.size());
    ImGui::Text("レール点数 (Rail Point Count): %zu", sceneData_.GetRailPointCount());
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
    ImGui::Text("CameraRig Active For Debug: %s", cameraRigActiveForDebug_ ? "true" : "false");
    ImGui::Text("Gameplay Preview Mode: %s", gameplayPreviewMode_ ? "true" : "false");
    ImGui::Text("Rail Debug Hidden By CameraRig: %s", ShouldHideRailDebug() ? "true" : "false");
    ImGui::Text("Rail Points Hidden By CameraRig: %s", ShouldHideRailPoints() ? "true" : "false");
    ImGui::Text("Event Debug Hidden By CameraRig: %s", ShouldHideEventDebug() ? "true" : "false");

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

    if (eventRuntime_ && ImGui::TreeNode("イベント実行確認 (Event Runtime Debug)")) {
        eventRuntime_->DrawImGui();
        ImGui::TreePop();
    }

    if (railDebugVisualizer_ && ImGui::TreeNode("レール確認 (Rail Debug)")) {
        if (railDebugVisualizer_->DrawImGui()) {
            RequestRebuild("Manual");
        }
        ImGui::TreePop();
    }

    if (railRuntime_ && ImGui::TreeNode("レール実行確認 (Rail Runtime Debug)")) {
        if (railRuntime_->DrawImGui()) {
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

void LevelSceneRuntime::SetCameraRigPreviewState(
    bool cameraRigActive,
    bool hideRailDebug,
    bool hideRailPoints,
    bool hideEventDebug,
    bool gameplayPreviewMode) {
    cameraRigActiveForDebug_ = cameraRigActive;
    hideRailDebugWhileCameraRigActive_ = hideRailDebug;
    hideRailPointsWhileCameraRigActive_ = hideRailPoints;
    hideEventDebugWhileCameraRigActive_ = hideEventDebug;
    gameplayPreviewMode_ = gameplayPreviewMode;
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

bool LevelSceneRuntime::TryFindObjectWorldPosition(
    const std::string& objectId,
    const std::string& objectName,
    Vector3& outPosition) const {
    if (objectId.empty() && objectName.empty()) {
        return false;
    }

    const LevelSceneQueryTransform identity{};
    for (const LevelObject& object : sceneData_.objects) {
        if (TryFindObjectWorldPositionRecursive(
            object,
            identity,
            axisConversionEnabled_,
            objectId,
            objectName,
            outPosition)) {
            return true;
        }
    }

    return false;
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
        if (eventRuntime_) {
            eventRuntime_->Clear();
        }
        if (railDebugVisualizer_) {
            railDebugVisualizer_->Clear();
        }
        if (railRuntime_) {
            railRuntime_->Clear();
        }
        engineCameraStart_ = {};
        hasEngineCameraStart_ = false;
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
    engineCameraStart_ = sceneData_.cameraStart;
    hasEngineCameraStart_ = sceneData_.cameraStart.exists;
    if (hasEngineCameraStart_ && axisConversionEnabled_) {
        engineCameraStart_.transform = BlenderToEngineTransform(sceneData_.cameraStart.transform);
    }

    if (objectDebugVisualizer_) {
        objectDebugVisualizer_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    if (eventRuntime_) {
        eventRuntime_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    if (railDebugVisualizer_) {
        railDebugVisualizer_->Rebuild(sceneData_, axisConversionEnabled_, frameCounter_);
    }
    if (railRuntime_) {
        railRuntime_->Rebuild(sceneData_, axisConversionEnabled_, frameCounter_);
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
    if (eventRuntime_) {
        eventRuntime_->Update(frameCounter_);
    }
    if (railDebugVisualizer_) {
        railDebugVisualizer_->Update(frameCounter_);
    }
    if (railRuntime_) {
        railRuntime_->Update(1.0f / 60.0f, frameCounter_);
    }
}

bool LevelSceneRuntime::ShouldHideRailDebug() const {
    return cameraRigActiveForDebug_ && (gameplayPreviewMode_ || hideRailDebugWhileCameraRigActive_);
}

bool LevelSceneRuntime::ShouldHideRailPoints() const {
    return cameraRigActiveForDebug_ && (gameplayPreviewMode_ || hideRailPointsWhileCameraRigActive_);
}

bool LevelSceneRuntime::ShouldHideEventDebug() const {
    return cameraRigActiveForDebug_ && (gameplayPreviewMode_ || hideEventDebugWhileCameraRigActive_);
}
