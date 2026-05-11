#pragma once
#include "Matrix4x4.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Model;
class ModelCommon;
struct Skeleton;

class GltfSkinnedModel {
public:
    GltfSkinnedModel() = default;
    ~GltfSkinnedModel();

    GltfSkinnedModel(const GltfSkinnedModel&) = delete;
    GltfSkinnedModel& operator=(const GltfSkinnedModel&) = delete;

    bool Initialize(ModelCommon* modelCommon, Skeleton* skeleton, const std::string& gltfPath);
    void UpdateSkinning();

    Model* GetModel() const { return model_.get(); }
    bool IsValid() const { return model_ != nullptr; }

private:
    struct SourceVertex {
        Vector3 position;
        Vector3 normal;
        Vector2 texcoord;
        std::array<uint32_t, 4> joints;
        std::array<float, 4> weights;
    };

private:
    Skeleton* skeleton_ = nullptr;
    std::unique_ptr<Model> model_;
    std::vector<SourceVertex> sourceVertices_;
    std::vector<Matrix4x4> inverseBindMatrices_;
    std::vector<Matrix4x4> jointPalette_;
};
