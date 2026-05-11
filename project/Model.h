#pragma once
#include "ModelCommon.h"
#include "Matrix4x4.h"
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

class Model {
public:
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = 0;
    };

    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
        float alphaReference;
        float padding2[3];
    };

public:
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

    // 生成済みのModelDataを直接渡す用
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);

    void Draw();
    void SetVertices(const std::vector<VertexData>& vertices);

    void SetTextureIndex(uint32_t index) { modelData_.material.textureIndex = index; }

    Material* GetMaterialData() { return materialData_; }

    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

    // 三角形ポリゴンで球のモデルデータを生成する関数
    static ModelData CreateSphereData(uint32_t subdivision = 16);
    static ModelData CreatePlaneData();
    static ModelData CreateCircleData(uint32_t subdivision = 32);
    static ModelData CreateRingData(
        uint32_t subdivision = 32,
        float innerRadius = 0.5f,
        float outerRadius = 1.0f,
        float startAngle = 0.0f,
        float endAngle = 6.2831853f,
        float startRadius = 1.0f,
        float endRadius = 1.0f);
    static ModelData CreateTorusData(uint32_t majorSubdivision = 32, uint32_t minorSubdivision = 16, float majorRadius = 0.7f, float minorRadius = 0.3f);
    static ModelData CreateCylinderData(uint32_t subdivision = 32, float radius = 1.0f, float height = 2.0f);
    static ModelData CreateEffectCylinderData(uint32_t subdivision = 32, float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f);
    static ModelData CreateConeData(uint32_t subdivision = 32, float radius = 1.0f, float height = 2.0f);
    static ModelData CreateTriangleData();
    static ModelData CreateBoxData();

private:
    ModelCommon* modelCommon_ = nullptr;
    ModelData modelData_; // 読み込んだデータを保持

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
