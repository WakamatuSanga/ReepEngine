#include "LevelObjectDebugVisualizer.h"
#include "LevelEventDebugView.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

struct LevelObjectDebugVisualizer::DebugObject {
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Object3d> colliderObject;
    const LevelObject* source = nullptr;
    std::string modelPath;
    bool usedModel = false;
    bool missingModel = false;
};

namespace {
    constexpr int kDrawModeModel = 0;
    constexpr int kDrawModeColliderOnly = 2;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    struct RuntimeTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    Vector3 MultiplyVector3(const Vector3& lhs, const Vector3& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    Vector3 ScaleVector3(const Vector3& value, float scale) { return { value.x * scale, value.y * scale, value.z * scale }; }
    Vector3 DegreesToRadians(const Vector3& value) { return { value.x * kDegToRad, value.y * kDegToRad, value.z * kDegToRad }; }

    RuntimeTransform CombineTransform(const RuntimeTransform& parent, const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }

    std::string FormatVector3(const Vector3& value) {
        char buffer[96]{};
        std::snprintf(buffer, sizeof(buffer), "(%.3f, %.3f, %.3f)", value.x, value.y, value.z);
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

    bool IsRailObjectType(const std::string& type) {
        return ToLowerString(type) == "curve";
    }

    bool HasObjExtension(const std::string& filePath) {
        return ToLowerString(std::filesystem::path(filePath).extension().string()) == ".obj";
    }

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
        const std::string primitiveShape = ToLowerString(object.primitiveShape);
        if (primitiveShape == "sphere") { return modelManager->CreateSphere("LevelDebugSphere", 12); }
        if (primitiveShape == "plane") { return modelManager->CreatePlane("LevelDebugPlane"); }
        return modelManager->CreateBox("LevelDebugBox");
    }

    void ApplyTransform(Object3d& object, const RuntimeTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(DegreesToRadians(transform.rotationDegrees));
        object.SetScale(transform.scaling);
    }

}

LevelObjectDebugVisualizer::LevelObjectDebugVisualizer() = default;

LevelObjectDebugVisualizer::~LevelObjectDebugVisualizer() = default;

void LevelObjectDebugVisualizer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void LevelObjectDebugVisualizer::Clear() {
    debugObjects_.clear();
    missingModelCount_ = 0;
}

void LevelObjectDebugVisualizer::Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled) {
    Clear();
    if (!object3dCommon_ || !camera_) {
        return;
    }

    auto buildRecursive = [this, axisConversionEnabled](auto& self, const LevelObject& object, const RuntimeTransform& parentTransform) -> void {
        const LevelTransform localTransform = axisConversionEnabled
            ? BlenderToEngineTransform(object.transform)
            : object.transform;
        const RuntimeTransform worldTransform = CombineTransform(parentTransform, localTransform);
        const bool isBlenderHelper = IsBlenderHelperType(object.type);
        const bool isRailObject = IsRailObjectType(object.type);
        const bool shouldCreateDebugObject =
            !object.isEventFlag && !isRailObject && (!isBlenderHelper || showBlenderHelpers_);

        auto debugObject = std::make_unique<DebugObject>();
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
                RuntimeTransform objectTransform = worldTransform;
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

        const LevelCollider collider = axisConversionEnabled ? BlenderToEngineCollider(object.collider) : object.collider;
        if (shouldCreateDebugObject && collider.exists) {
            if (Model* colliderModel = ModelManager::GetInstance()->CreateBox("LevelDebugColliderBox")) {
                const Vector3 colliderCenter = collider.hasCenter
                    ? MultiplyVector3(collider.center, worldTransform.scaling)
                    : Vector3{ 0.0f, 0.0f, 0.0f };
                const Vector3 colliderSize = collider.hasSize
                    ? MultiplyVector3(collider.size, worldTransform.scaling)
                    : ScaleVector3(worldTransform.scaling, 2.0f);

                RuntimeTransform colliderTransform = worldTransform;
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
            self(self, child, worldTransform);
        }
        };

    const RuntimeTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        buildRecursive(buildRecursive, object, identity);
    }
}

void LevelObjectDebugVisualizer::Update(uint64_t frameCounter) {
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
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelObjectDebugVisualizer::Draw() {
    for (const auto& debugObject : debugObjects_) {
        if (showLevelObjects_ && drawMode_ != kDrawModeColliderOnly && debugObject->object) {
            debugObject->object->Draw();
        }
        if (showDebugColliders_ && debugObject->colliderObject) {
            debugObject->colliderObject->Draw();
        }
    }
}

bool LevelObjectDebugVisualizer::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    if (ImGui::Button("デバッグ表示を再構築 (Rebuild Debug Objects)")) {
        needsRebuild = true;
    }
    ImGui::Checkbox("レベルオブジェクト表示 (Show Level Objects)", &showLevelObjects_);
    ImGui::Checkbox("コライダー表示 (Show Debug Colliders)", &showDebugColliders_);
    if (ImGui::Checkbox("Blender補助表示 (Show Blender Helpers)", &showBlenderHelpers_)) {
        needsRebuild = true;
    }
    const char* drawModeNames[] = { "モデル", "デバッグ形状", "コライダーのみ" };
    if (ImGui::Combo("描画モード (Draw Mode)", &drawMode_, drawModeNames, IM_ARRAYSIZE(drawModeNames))) {
        needsRebuild = true;
    }
    return needsRebuild;
#else
    return false;
#endif
}

void LevelObjectDebugVisualizer::DrawObjectDetails(const LevelObject& object) const {
#ifdef _DEBUG
    ImGui::Text("名前 (name): %s", object.name.c_str());
    ImGui::Text("種類 (type): %s", object.type.c_str());
    ImGui::Text("プリミティブ形状 (primitive_shape): %s", object.primitiveShape.empty() ? "(none)" : object.primitiveShape.c_str());
    DrawLevelObjectEventDetailsImGui(object);
    ImGui::Text("モデルファイル (file_name): %s", object.hasFileName ? object.fileName.c_str() : "(none)");
    ImGui::Text("位置 (translation): %s", FormatVector3(object.transform.translation).c_str());
    ImGui::Text("回転 (rotation): %s", FormatVector3(object.transform.rotation).c_str());
    ImGui::Text("拡縮 (scaling): %s", FormatVector3(object.transform.scaling).c_str());

    if (object.collider.exists) {
        ImGui::Text("コライダー種類 (collider.type): %s", object.collider.type.c_str());
        ImGui::Text(
            "コライダー中心 (collider.center): %s",
            object.collider.hasCenter ? FormatVector3(object.collider.center).c_str() : "(default)");
        ImGui::Text(
            "コライダーサイズ (collider.size): %s",
            object.collider.hasSize ? FormatVector3(object.collider.size).c_str() : "(default)");
    } else {
        ImGui::TextUnformatted("コライダー (collider): (none)");
    }
#else
    (void)object;
#endif
}

void LevelObjectDebugVisualizer::DrawObjectDebugDetails(const LevelObject& object) const {
#ifdef _DEBUG
    for (const auto& debugObject : debugObjects_) {
        if (debugObject && debugObject->source == &object) {
            DrawObjectDetails(object);
            ImGui::Separator();
            ImGui::Text("描画元 (draw source): %s", debugObject->usedModel ? "file_name model" : "debug primitive");
            ImGui::Text("モデルパス (model path): %s", debugObject->modelPath.empty() ? "(none)" : debugObject->modelPath.c_str());
            ImGui::Text("不明モデル (missing model): %s", debugObject->missingModel ? "true" : "false");
            ImGui::Text("デバッグコライダー (debug collider): %s", debugObject->colliderObject ? "true" : "false");
            return;
        }
    }

    DrawObjectDetails(object);
    ImGui::Separator();
    ImGui::TextUnformatted("描画元 (draw source): hidden / no debug object");
    ImGui::Text("Blender補助 (blender helper): %s", IsBlenderHelper(object) ? "true" : "false");
#else
    (void)object;
#endif
}

size_t LevelObjectDebugVisualizer::GetVisibleObjectCount() const {
    size_t count = 0;
    if (showLevelObjects_ && drawMode_ != kDrawModeColliderOnly) {
        for (const auto& debugObject : debugObjects_) {
            if (debugObject && debugObject->object) {
                ++count;
            }
        }
    }
    if (showDebugColliders_) {
        for (const auto& debugObject : debugObjects_) {
            if (debugObject && debugObject->colliderObject) {
                ++count;
            }
        }
    }
    return count;
}

bool LevelObjectDebugVisualizer::IsBlenderHelper(const LevelObject& object) const {
    return IsBlenderHelperType(object.type);
}
