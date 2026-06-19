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
        Vector3 tangent{ 1.0f, 0.0f, 0.0f };
    };

    struct MaterialData {
        std::string materialName;
        std::string textureFilePath;
        std::string baseColorTexturePath;
        std::string normalTexturePath;
        std::string metallicRoughnessTexturePath;
        std::string specularF0TexturePath;
        uint32_t textureIndex = 0;
        uint32_t baseColorTextureIndex = 0;
        uint32_t normalTextureIndex = 0;
        uint32_t metallicRoughnessTextureIndex = 0;
        uint32_t specularF0TextureIndex = 0;
        bool usePBR = false;
        bool hasNormalMap = false;
        bool hasMetallicRoughnessMap = false;
        bool hasSpecularF0Map = false;
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        float normalScale = 1.0f;
    };

    struct ModelData {
        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;
        MaterialData material;
    };

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
        float alphaReference;
        int32_t usePBR;
        float metallicFactor;
        float roughnessFactor;
        float normalScale;
        int32_t hasNormalMap;
        int32_t hasMetallicRoughnessMap;
        int32_t hasSpecularF0Map;
    };

public:
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

    // 生成済みのModelDataを直接渡す用
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);

    void Draw();
    void SetVertices(const std::vector<VertexData>& vertices);
    void SetVertexBufferViewOverride(const D3D12_VERTEX_BUFFER_VIEW* vertexBufferView);
    void ClearVertexBufferViewOverride();

    void SetTextureIndex(uint32_t index);
    void DrawPbrMaterialImGui();
    size_t GetVertexCount() const { return modelData_.vertices.size(); }
    size_t GetIndexCount() const { return modelData_.indices.size(); }
    size_t GetMaterialCount() const { return 1; }

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
    const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride_ = nullptr;
    VertexData* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t* indexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
