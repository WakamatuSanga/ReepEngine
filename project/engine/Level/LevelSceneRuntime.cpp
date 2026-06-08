#include "LevelSceneRuntime.h"
#include "LevelEventConnectionVisualizer.h"
#include "LevelEventDebugView.h"
#include "LevelEventVisualizer.h"
#include "LevelSceneLoader.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

struct LevelSceneDebugObject {
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Object3d> colliderObject;
    const LevelObject* source = nullptr;
    std::string modelPath;
    bool usedModel = false;
    bool missingModel = false;
};

struct LevelSceneRuntimeTransform {
    Vector3 translation{ 0.0f, 0.0f, 0.0f };
    Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    Vector3 scaling{ 1.0f, 1.0f, 1.0f };
};

namespace {
    constexpr int kDrawModeModel = 0;
    constexpr int kDrawModeDebugPrimitive = 1;
    constexpr int kDrawModeColliderOnly = 2;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    Vector3 MultiplyVector3(const Vector3& lhs, const Vector3& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    Vector3 ScaleVector3(const Vector3& value, float scale) { return { value.x * scale, value.y * scale, value.z * scale }; }
    Vector3 DegreesToRadians(const Vector3& value) { return { value.x * kDegToRad, value.y * kDegToRad, value.z * kDegToRad }; }
    LevelSceneRuntimeTransform CombineTransform(
        const LevelSceneRuntimeTransform& parent,
        const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }
    std::string FormatVector3(const Vector3& value) {
        char buffer[96]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "(%.3f, %.3f, %.3f)",
            value.x,
            value.y,
            value.z);
        return buffer;
    }
    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }
    bool IsBlenderHelperType(const std::string& type) {
        const std::string lowerType = ToLowerString(type);
        return lowerType == "light" || lowerType == "camera";
    }
    bool HasObjExtension(const std::string& filePath) { return ToLowerString(std::filesystem::path(filePath).extension().string()) == ".obj"; }

    void AddModelCandidates(const std::string& fileName, std::vector<std::string>& candidates) {
        if (fileName.empty()) {
            return;
        }

        candidates.push_back(fileName);
        candidates.push_back("resources/" + fileName);
        candidates.push_back("resources/obj/" + fileName);
        candidates.push_back("resources/model/" + fileName);

        const std::filesystem::path path(fileName);
        const std::string stem = path.stem().string();
        if (!stem.empty() && !HasObjExtension(fileName)) {
            candidates.push_back(fileName + ".obj");
            candidates.push_back("resources/obj/" + stem + "/" + stem + ".obj");
            candidates.push_back("resources/model/" + stem + ".obj");
        }
    }
    Model* TryLoadModelFromFileName(const std::string& fileName, std::string& resolvedPath) {
        std::vector<std::string> candidates;
        AddModelCandidates(fileName, candidates);

        auto modelManager = ModelManager::GetInstance();
        for (const std::string& candidate : candidates) {
            if (!HasObjExtension(candidate)) {
                continue;
            }
            modelManager->LoadModel(candidate);
            if (Model* model = modelManager->FindModel(candidate)) {
                resolvedPath = candidate;
                return model;
            }
        }

        resolvedPath.clear();
        return nullptr;
    }
    Model* GetPrimitiveModel(const LevelObject& object) {
        auto modelManager = ModelManager::GetInstance();
        if (IsBlenderHelperType(object.type)) {
            return modelManager->CreateSphere("LevelDebugSphere", 12);
        }
        return modelManager->CreateBox("LevelDebugBox");
    }
    void ApplyTransform(Object3d& object, const LevelSceneRuntimeTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(DegreesToRadians(transform.rotationDegrees));
        object.SetScale(transform.scaling);
    }
    size_t CountDebugColliderObjects(const std::vector<std::unique_ptr<LevelSceneDebugObject>>& debugObjects) {
        size_t count = 0;
        for (const auto& debugObject : debugObjects) {
            if (debugObject && debugObject->colliderObject) {
                ++count;
            }
        }
        return count;
    }
    size_t CountVisibleObjects(
        const std::vector<std::unique_ptr<LevelSceneDebugObject>>& debugObjects,
        int drawMode,
        bool showObjects,
        bool showColliders) {
        size_t count = 0;
        if (showObjects && drawMode != kDrawModeColliderOnly) {
            for (const auto& debugObject : debugObjects) {
                if (debugObject && debugObject->object) {
                    ++count;
                }
            }
        }
        if (showColliders) {
            count += CountDebugColliderObjects(debugObjects);
        }
        return count;
    }
#ifdef _DEBUG
    void DrawObjectDetails(const LevelObject& object) {
        ImGui::Text("name: %s", object.name.c_str());
        ImGui::Text("type: %s", object.type.c_str());
        DrawLevelObjectEventDetailsImGui(object);
        ImGui::Text("file_name: %s", object.hasFileName ? object.fileName.c_str() : "(none)");
        ImGui::Text("translation: %s", FormatVector3(object.transform.translation).c_str());
        ImGui::Text("rotation: %s", FormatVector3(object.transform.rotation).c_str());
        ImGui::Text("scaling: %s", FormatVector3(object.transform.scaling).c_str());

        if (object.collider.exists) {
            ImGui::Text("collider.type: %s", object.collider.type.c_str());
            ImGui::Text(
                "collider.center: %s",
                object.collider.hasCenter ? FormatVector3(object.collider.center).c_str() : "(default)");
            ImGui::Text(
                "collider.size: %s",
                object.collider.hasSize ? FormatVector3(object.collider.size).c_str() : "(default)");
        } else {
            ImGui::TextUnformatted("collider: (none)");
        }
    }

    void DrawDebugObjectDetails(const LevelSceneDebugObject& debugObject) {
        if (!debugObject.source) {
            return;
        }

        DrawObjectDetails(*debugObject.source);
        ImGui::Separator();
        ImGui::Text("draw source: %s", debugObject.usedModel ? "file_name model" : "debug primitive");
        ImGui::Text("model path: %s", debugObject.modelPath.empty() ? "(none)" : debugObject.modelPath.c_str());
        ImGui::Text("missing model: %s", debugObject.missingModel ? "true" : "false");
        ImGui::Text("debug collider: %s", debugObject.colliderObject ? "true" : "false");
    }

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

    const LevelSceneDebugObject* FindDebugObjectBySource(
        const std::vector<std::unique_ptr<LevelSceneDebugObject>>& debugObjects,
        const LevelObject* source) {
        if (!source) {
            return nullptr;
        }

        for (const auto& debugObject : debugObjects) {
            if (debugObject && debugObject->source == source) {
                return debugObject.get();
            }
        }

        return nullptr;
    }

    void DrawObjectTreeRecursive(
        const LevelObject& object,
        int& objectIndex,
        int& selectedObjectIndex) {
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
            DrawObjectDetails(object);
            for (const LevelObject& child : object.children) {
                DrawObjectTreeRecursive(child, objectIndex, selectedObjectIndex);
            }
            ImGui::TreePop();
        }
    }
#endif
}

LevelSceneRuntime::LevelSceneRuntime() = default;

LevelSceneRuntime::~LevelSceneRuntime() = default;

void LevelSceneRuntime::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    loader_ = std::make_unique<LevelSceneLoader>();
    eventVisualizer_ = std::make_unique<LevelEventVisualizer>();
    connectionVisualizer_ = std::make_unique<LevelEventConnectionVisualizer>();
    eventVisualizer_->Initialize(object3dCommon_, camera_);
    connectionVisualizer_->Initialize(object3dCommon_, camera_);
    SetPathBufferText(jsonPath_);
    LoadJsonFromBuffer();
}

void LevelSceneRuntime::Update() {
    ++frameCounter_;

    if (rebuildDirty_ && !freezeDebugObjects_) {
        RebuildDebugObjects();
    }

    UpdateDebugObjectMatrices();
    if (eventVisualizer_) { eventVisualizer_->Update(); }
    if (connectionVisualizer_) { connectionVisualizer_->Update(); }
}

void LevelSceneRuntime::Draw() {
    UpdateDebugObjectMatrices();

    for (const auto& debugObject : debugObjects_) {
        if (showLevelObjects_ && drawMode_ != kDrawModeColliderOnly && debugObject->object) {
            debugObject->object->Draw();
        }
        if (showDebugColliders_ && debugObject->colliderObject) {
            debugObject->colliderObject->Draw();
        }
    }
    if (eventVisualizer_) { eventVisualizer_->Draw(); }
    if (connectionVisualizer_) { connectionVisualizer_->Draw(); }
}

void LevelSceneRuntime::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Level Editor Debug / レベルエディタ確認")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("JSON Path", jsonPathBuffer_.data(), jsonPathBuffer_.size());
    if (ImGui::Button("Load JSON")) {
        LoadJsonFromBuffer();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rebuild Debug Objects")) {
        RequestRebuild("Manual");
    }

    ImGui::Checkbox("Show Level Objects", &showLevelObjects_);
    ImGui::Checkbox("Show Debug Colliders", &showDebugColliders_);
    if (ImGui::Checkbox("Show Blender Helpers", &showBlenderHelpers_)) {
        RequestRebuild("Manual");
    }
    if (ImGui::Checkbox("Axis Conversion ON/OFF", &axisConversionEnabled_)) {
        RequestRebuild("Manual");
    }
    const char* drawModeNames[] = { "Model", "Debug Primitive", "Collider Only" };
    if (ImGui::Combo("Draw Mode", &drawMode_, drawModeNames, IM_ARRAYSIZE(drawModeNames))) {
        RequestRebuild("Manual");
    }
    ImGui::Checkbox("Freeze Level Debug Objects", &freezeDebugObjects_);
    ImGui::Checkbox("Pause Live Apply", &pauseLiveApply_);
    ImGui::Checkbox("Rebuild Only When JSON Changed", &rebuildOnlyWhenJsonChanged_);

    ImGui::Separator();
    ImGui::TextWrapped("Last Load Status: %s", lastLoadStatus_.c_str());
    ImGui::TextWrapped("Resolved Path: %s", lastResolvedPath_.empty() ? "(none)" : lastResolvedPath_.c_str());
    ImGui::Text("Axis Conversion: %s", axisConversionEnabled_ ? "Blender(x,y,z) -> Engine(x,z,y)" : "OFF");
    ImGui::Text("Scene Name: %s", sceneData_.name.empty() ? "(none)" : sceneData_.name.c_str());
    ImGui::Text("Loaded Object Count: %zu", sceneData_.GetObjectCount());
    ImGui::Text(
        "Visible Object Count: %zu",
        CountVisibleObjects(debugObjects_, drawMode_, showLevelObjects_, showDebugColliders_));
    ImGui::Text("Missing Model Count: %zu", missingModelCount_);
    ImGui::Text("Debug Object Count: %zu", debugObjects_.size());
    ImGui::Text("Rebuild Count: %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("Last Rebuild Frame: %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text("Last Debug Matrix Update Frame: %llu", static_cast<unsigned long long>(lastDebugMatrixUpdateFrame_));
    ImGui::Text("Last Apply Source: %s", lastApplySource_.c_str());
    ImGui::Text("Auto Apply Enabled: %s", liveAutoApplyEnabled_ ? "true" : "false");
    ImGui::Text("Last Packet Applied: %llu", static_cast<unsigned long long>(lastPacketApplied_));
    ImGui::Text("Rebuild Dirty: %s", rebuildDirty_ ? "true" : "false");

    const LevelObject* selectedObject = FindObjectByTreeIndex(sceneData_, selectedObjectIndex_);
    if (ImGui::TreeNode("Selected Object Info")) {
        if (selectedObject) {
            if (const LevelSceneDebugObject* debugObject = FindDebugObjectBySource(debugObjects_, selectedObject)) {
                DrawDebugObjectDetails(*debugObject);
            } else {
                DrawObjectDetails(*selectedObject);
                ImGui::Separator();
                ImGui::TextUnformatted("draw source: hidden / no debug object");
                ImGui::Text("blender helper: %s", IsBlenderHelperType(selectedObject->type) ? "true" : "false");
            }
        } else {
            ImGui::TextDisabled("Select an object in Object Tree.");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Event Flag Debug / イベントフラグ確認")) {
        if (DrawLevelEventDebugImGui(
            sceneData_, selectedObject, eventVisualizer_.get(), connectionVisualizer_.get())) {
            RequestRebuild("Manual");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Object Tree")) {
        if (sceneData_.objects.empty()) {
            ImGui::TextDisabled("No objects loaded.");
        } else {
            int objectIndex = 0;
            for (const LevelObject& object : sceneData_.objects) {
                DrawObjectTreeRecursive(object, objectIndex, selectedObjectIndex_);
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
#endif
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
        debugObjects_.clear();
        if (eventVisualizer_) {
            eventVisualizer_->Clear();
        }
        if (connectionVisualizer_) {
            connectionVisualizer_->Clear();
        }
        missingModelCount_ = 0;
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
    debugObjects_.clear();
    missingModelCount_ = 0;
    lastApplySource_ = pendingRebuildSource_.empty() ? lastApplySource_ : pendingRebuildSource_;
    pendingRebuildSource_.clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter_;

    if (!object3dCommon_ || !camera_) {
        if (eventVisualizer_) {
            eventVisualizer_->Clear();
        }
        if (connectionVisualizer_) {
            connectionVisualizer_->Clear();
        }
        lastLoadStatus_ = "Level scene loaded, but debug object context is not ready.";
        return;
    }

    const LevelSceneRuntimeTransform identity{};
    for (const LevelObject& object : sceneData_.objects) {
        BuildDebugObjectsRecursive(object, identity);
    }
    if (eventVisualizer_) {
        eventVisualizer_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    if (connectionVisualizer_) {
        connectionVisualizer_->Rebuild(sceneData_, axisConversionEnabled_);
    }
    UpdateDebugObjectMatrices();
    if (eventVisualizer_) {
        eventVisualizer_->Update();
    }
    if (connectionVisualizer_) {
        connectionVisualizer_->Update();
    }
}

void LevelSceneRuntime::UpdateDebugObjectMatrices() {
    for (const auto& debugObject : debugObjects_) {
        if (!debugObject) {
            continue;
        }
        if (debugObject->object) {
            debugObject->object->Update();
        }
        if (debugObject->colliderObject) {
            debugObject->colliderObject->Update();
        }
    }
    lastDebugMatrixUpdateFrame_ = frameCounter_;
}

void LevelSceneRuntime::BuildDebugObjectsRecursive(
    const LevelObject& object,
    const LevelSceneRuntimeTransform& parentTransform) {
    const LevelTransform localTransform = axisConversionEnabled_
        ? BlenderToEngineTransform(object.transform)
        : object.transform;
    const LevelSceneRuntimeTransform worldTransform = CombineTransform(parentTransform, localTransform);
    const bool isBlenderHelper = IsBlenderHelperType(object.type);
    const bool shouldCreateDebugObject = !object.isEventFlag && (!isBlenderHelper || showBlenderHelpers_);

    auto debugObject = std::make_unique<LevelSceneDebugObject>();
    debugObject->source = &object;

    if (shouldCreateDebugObject) {
        Model* drawModel = nullptr;
        if (!isBlenderHelper && object.hasFileName && drawMode_ == kDrawModeModel) {
            drawModel = TryLoadModelFromFileName(object.fileName, debugObject->modelPath);
            debugObject->usedModel = drawModel != nullptr;
            debugObject->missingModel = drawModel == nullptr;
            if (debugObject->missingModel) {
                ++missingModelCount_;
            }
        }
        if (!drawModel) {
            drawModel = GetPrimitiveModel(object);
        }

        if (drawModel) {
            LevelSceneRuntimeTransform objectTransform = worldTransform;
            if (isBlenderHelper) {
                objectTransform.scaling = { 0.25f, 0.25f, 0.25f };
            }

            debugObject->object = std::make_unique<Object3d>();
            debugObject->object->Initialize(object3dCommon_);
            debugObject->object->SetModel(drawModel);
            debugObject->object->SetCamera(camera_);
            ApplyTransform(*debugObject->object, objectTransform);
            debugObject->object->SetEnvironmentMapEnabled(false);
        }
    }

    const LevelCollider collider = axisConversionEnabled_
        ? BlenderToEngineCollider(object.collider)
        : object.collider;
    if (shouldCreateDebugObject && collider.exists) {
        if (Model* colliderModel = ModelManager::GetInstance()->CreateBox("LevelDebugColliderBox")) {
            const Vector3 colliderCenter = collider.hasCenter
                ? MultiplyVector3(collider.center, worldTransform.scaling)
                : Vector3{ 0.0f, 0.0f, 0.0f };
            const Vector3 colliderSize = collider.hasSize
                ? MultiplyVector3(collider.size, worldTransform.scaling)
                : ScaleVector3(worldTransform.scaling, 2.0f);

            LevelSceneRuntimeTransform colliderTransform = worldTransform;
            colliderTransform.translation = AddVector3(worldTransform.translation, colliderCenter);
            colliderTransform.scaling = ScaleVector3(colliderSize, 0.5f);

            debugObject->colliderObject = std::make_unique<Object3d>();
            debugObject->colliderObject->Initialize(object3dCommon_);
            debugObject->colliderObject->SetModel(colliderModel);
            debugObject->colliderObject->SetCamera(camera_);
            ApplyTransform(*debugObject->colliderObject, colliderTransform);
            debugObject->colliderObject->SetEnvironmentMapEnabled(false);
        }
    }

    if (debugObject->object || debugObject->colliderObject) {
        debugObjects_.push_back(std::move(debugObject));
    }

    for (const LevelObject& child : object.children) {
        BuildDebugObjectsRecursive(child, worldTransform);
    }
}
