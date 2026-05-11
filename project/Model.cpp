#include "Model.h"
#include "PrimitiveGenerator.h"
#include "TextureManager.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace std;
using namespace MatrixMath;

namespace {
    constexpr float kPi = 3.14159265359f;

    uint32_t GetPrimitiveTextureIndex() {
        return TextureManager::GetInstance()->GetTextureIndexByFilePath("resources/obj/axis/uvChecker.png");
    }

    Vector3 SubtractVector(const Vector3& v1, const Vector3& v2) {
        return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
    }

    float Length(const Vector3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    Vector3 Normalize(const Vector3& v, const Vector3& fallback = { 0.0f, 1.0f, 0.0f }) {
        float length = Length(v);
        if (length <= 0.00001f) {
            return fallback;
        }
        return { v.x / length, v.y / length, v.z / length };
    }

    Model::VertexData MakeVertex(float x, float y, float z, float u, float v, float nx, float ny, float nz) {
        return { {x, y, z, 1.0f}, {u, v}, {nx, ny, nz} };
    }

    void PushTriangle(Model::ModelData& data,
        const Model::VertexData& v0,
        const Model::VertexData& v1,
        const Model::VertexData& v2) {
        data.vertices.push_back(v0);
        data.vertices.push_back(v1);
        data.vertices.push_back(v2);
    }

    void PushQuad(Model::ModelData& data,
        const Model::VertexData& v00,
        const Model::VertexData& v01,
        const Model::VertexData& v10,
        const Model::VertexData& v11) {
        PushTriangle(data, v00, v01, v10);
        PushTriangle(data, v01, v11, v10);
    }
}

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename) {
    ModelData data = LoadObjFile(directoryPath, filename);
    Initialize(modelCommon, data);
}

void Model::Initialize(ModelCommon* modelCommon, const ModelData& modelData) {
    assert(modelCommon);
    modelCommon_ = modelCommon;
    modelData_ = modelData;

    // --- 頂点バッファ作成 ---
    vertexResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(
        sizeof(VertexData) * modelData_.vertices.size());

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

    // --- マテリアルリソース作成 ---
    materialResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 1;
    materialData_->uvTransform = MakeIdentity4x4();
    materialData_->alphaReference = 0.5f;
}

void Model::Draw() {
    ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // ★修正: 最新の textureIndex を使って描画する
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));

    commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
}

void Model::SetVertices(const std::vector<VertexData>& vertices) {
    assert(vertices.size() == modelData_.vertices.size());
    modelData_.vertices = vertices;
    std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

Model::ModelData Model::CreateSphereData(uint32_t subdivision) {
    ModelData data;

    // ★修正: 初期値として、確実に存在する「uvChecker.png」のインデックスを取得してセットしておく
    subdivision = (subdivision < 3) ? 3 : subdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    for (uint32_t lat = 0; lat < subdivision; ++lat) {
        float latAngle0 = kPi * static_cast<float>(lat) / static_cast<float>(subdivision);
        float latAngle1 = kPi * static_cast<float>(lat + 1) / static_cast<float>(subdivision);

        float y0 = std::cos(latAngle0);
        float r0 = std::sin(latAngle0);
        float y1 = std::cos(latAngle1);
        float r1 = std::sin(latAngle1);

        for (uint32_t lon = 0; lon < subdivision; ++lon) {
            float lonAngle0 = 2.0f * kPi * static_cast<float>(lon) / static_cast<float>(subdivision);
            float lonAngle1 = 2.0f * kPi * static_cast<float>(lon + 1) / static_cast<float>(subdivision);

            float u0 = static_cast<float>(lon) / static_cast<float>(subdivision);
            float u1 = static_cast<float>(lon + 1) / static_cast<float>(subdivision);
            float v0 = static_cast<float>(lat) / static_cast<float>(subdivision);
            float v1 = static_cast<float>(lat + 1) / static_cast<float>(subdivision);

            VertexData v00 = { {r0 * std::cos(lonAngle0), y0, r0 * std::sin(lonAngle0), 1.0f}, {u0, v0}, {r0 * std::cos(lonAngle0), y0, r0 * std::sin(lonAngle0)} };
            VertexData v10 = { {r1 * std::cos(lonAngle0), y1, r1 * std::sin(lonAngle0), 1.0f}, {u0, v1}, {r1 * std::cos(lonAngle0), y1, r1 * std::sin(lonAngle0)} };
            VertexData v01 = { {r0 * std::cos(lonAngle1), y0, r0 * std::sin(lonAngle1), 1.0f}, {u1, v0}, {r0 * std::cos(lonAngle1), y0, r0 * std::sin(lonAngle1)} };
            VertexData v11 = { {r1 * std::cos(lonAngle1), y1, r1 * std::sin(lonAngle1), 1.0f}, {u1, v1}, {r1 * std::cos(lonAngle1), y1, r1 * std::sin(lonAngle1)} };

            PushQuad(data, v00, v01, v10, v11);
        }
    }
    return data;
}

Model::ModelData Model::CreatePlaneData() {
    ModelData data;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    const Vector3 normal = { 0.0f, 1.0f, 0.0f };
    PushQuad(data,
        MakeVertex(-1.0f, 0.0f, 1.0f, 0.0f, 0.0f, normal.x, normal.y, normal.z),
        MakeVertex(1.0f, 0.0f, 1.0f, 1.0f, 0.0f, normal.x, normal.y, normal.z),
        MakeVertex(-1.0f, 0.0f, -1.0f, 0.0f, 1.0f, normal.x, normal.y, normal.z),
        MakeVertex(1.0f, 0.0f, -1.0f, 1.0f, 1.0f, normal.x, normal.y, normal.z));

    return data;
}

Model::ModelData Model::CreateCircleData(uint32_t subdivision) {
    ModelData data;
    subdivision = (subdivision < 3) ? 3 : subdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    for (uint32_t i = 0; i < subdivision; ++i) {
        float angle0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(subdivision);
        float angle1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(subdivision);

        float x0 = std::cos(angle0);
        float z0 = std::sin(angle0);
        float x1 = std::cos(angle1);
        float z1 = std::sin(angle1);

        VertexData center = MakeVertex(0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f);
        VertexData v0 = MakeVertex(x0, 0.0f, z0, x0 * 0.5f + 0.5f, -z0 * 0.5f + 0.5f, 0.0f, 1.0f, 0.0f);
        VertexData v1 = MakeVertex(x1, 0.0f, z1, x1 * 0.5f + 0.5f, -z1 * 0.5f + 0.5f, 0.0f, 1.0f, 0.0f);
        PushTriangle(data, center, v1, v0);
    }

    return data;
}

Model::ModelData Model::CreateRingData(
    uint32_t subdivision,
    float innerRadius,
    float outerRadius,
    float startAngle,
    float endAngle,
    float startRadius,
    float endRadius) {
    return PrimitiveGenerator::CreateRingData(subdivision, innerRadius, outerRadius, startAngle, endAngle, startRadius, endRadius);
}

Model::ModelData Model::CreateTorusData(uint32_t majorSubdivision, uint32_t minorSubdivision, float majorRadius, float minorRadius) {
    ModelData data;
    majorSubdivision = (majorSubdivision < 3) ? 3 : majorSubdivision;
    minorSubdivision = (minorSubdivision < 3) ? 3 : minorSubdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    auto MakeTorusVertex = [majorRadius, minorRadius](float theta, float phi, float u, float v) {
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        float cosPhi = std::cos(phi);
        float sinPhi = std::sin(phi);
        float radius = majorRadius + minorRadius * cosPhi;

        float x = radius * cosTheta;
        float y = minorRadius * sinPhi;
        float z = radius * sinTheta;
        Vector3 normal = Normalize({ cosPhi * cosTheta, sinPhi, cosPhi * sinTheta }, { 0.0f, 1.0f, 0.0f });
        return MakeVertex(x, y, z, u, v, normal.x, normal.y, normal.z);
        };

    for (uint32_t major = 0; major < majorSubdivision; ++major) {
        float theta0 = 2.0f * kPi * static_cast<float>(major) / static_cast<float>(majorSubdivision);
        float theta1 = 2.0f * kPi * static_cast<float>(major + 1) / static_cast<float>(majorSubdivision);
        float u0 = static_cast<float>(major) / static_cast<float>(majorSubdivision);
        float u1 = static_cast<float>(major + 1) / static_cast<float>(majorSubdivision);

        for (uint32_t minor = 0; minor < minorSubdivision; ++minor) {
            float phi0 = 2.0f * kPi * static_cast<float>(minor) / static_cast<float>(minorSubdivision);
            float phi1 = 2.0f * kPi * static_cast<float>(minor + 1) / static_cast<float>(minorSubdivision);
            float v0 = static_cast<float>(minor) / static_cast<float>(minorSubdivision);
            float v1 = static_cast<float>(minor + 1) / static_cast<float>(minorSubdivision);

            PushQuad(data,
                MakeTorusVertex(theta0, phi0, u0, v0),
                MakeTorusVertex(theta1, phi0, u1, v0),
                MakeTorusVertex(theta0, phi1, u0, v1),
                MakeTorusVertex(theta1, phi1, u1, v1));
        }
    }

    return data;
}

Model::ModelData Model::CreateCylinderData(uint32_t subdivision, float radius, float height) {
    ModelData data;
    subdivision = (subdivision < 3) ? 3 : subdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    float halfHeight = height * 0.5f;

    for (uint32_t i = 0; i < subdivision; ++i) {
        float angle0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(subdivision);
        float angle1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(subdivision);

        float x0 = std::cos(angle0) * radius;
        float z0 = std::sin(angle0) * radius;
        float x1 = std::cos(angle1) * radius;
        float z1 = std::sin(angle1) * radius;
        float u0 = static_cast<float>(i) / static_cast<float>(subdivision);
        float u1 = static_cast<float>(i + 1) / static_cast<float>(subdivision);

        Vector3 normal0 = Normalize({ std::cos(angle0), 0.0f, std::sin(angle0) }, { 1.0f, 0.0f, 0.0f });
        Vector3 normal1 = Normalize({ std::cos(angle1), 0.0f, std::sin(angle1) }, { 1.0f, 0.0f, 0.0f });

        PushQuad(data,
            MakeVertex(x0, halfHeight, z0, u0, 0.0f, normal0.x, normal0.y, normal0.z),
            MakeVertex(x1, halfHeight, z1, u1, 0.0f, normal1.x, normal1.y, normal1.z),
            MakeVertex(x0, -halfHeight, z0, u0, 1.0f, normal0.x, normal0.y, normal0.z),
            MakeVertex(x1, -halfHeight, z1, u1, 1.0f, normal1.x, normal1.y, normal1.z));

        VertexData topCenter = MakeVertex(0.0f, halfHeight, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f);
        VertexData top0 = MakeVertex(x0, halfHeight, z0, x0 / (radius * 2.0f) + 0.5f, -z0 / (radius * 2.0f) + 0.5f, 0.0f, 1.0f, 0.0f);
        VertexData top1 = MakeVertex(x1, halfHeight, z1, x1 / (radius * 2.0f) + 0.5f, -z1 / (radius * 2.0f) + 0.5f, 0.0f, 1.0f, 0.0f);
        PushTriangle(data, topCenter, top1, top0);

        VertexData bottomCenter = MakeVertex(0.0f, -halfHeight, 0.0f, 0.5f, 0.5f, 0.0f, -1.0f, 0.0f);
        VertexData bottom0 = MakeVertex(x0, -halfHeight, z0, x0 / (radius * 2.0f) + 0.5f, z0 / (radius * 2.0f) + 0.5f, 0.0f, -1.0f, 0.0f);
        VertexData bottom1 = MakeVertex(x1, -halfHeight, z1, x1 / (radius * 2.0f) + 0.5f, z1 / (radius * 2.0f) + 0.5f, 0.0f, -1.0f, 0.0f);
        PushTriangle(data, bottomCenter, bottom0, bottom1);
    }

    return data;
}

Model::ModelData Model::CreateEffectCylinderData(uint32_t subdivision, float topRadius, float bottomRadius, float height) {
    ModelData data;
    subdivision = (subdivision < 3) ? 3 : subdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    float radianPerDivide = 2.0f * kPi / static_cast<float>(subdivision);

    for (uint32_t index = 0; index < subdivision; ++index) {
        float sinVal = std::sin(index * radianPerDivide);
        float cosVal = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = static_cast<float>(index) / static_cast<float>(subdivision);
        float uNext = static_cast<float>(index + 1) / static_cast<float>(subdivision);

        float vTop = 1.0f; // Slide 3: flip v
        float vBottom = 0.0f;

        PushTriangle(data,
            MakeVertex(-sinVal * topRadius, height, cosVal * topRadius, u, vTop, -sinVal, 0.0f, cosVal),
            MakeVertex(-sinNext * topRadius, height, cosNext * topRadius, uNext, vTop, -sinNext, 0.0f, cosNext),
            MakeVertex(-sinVal * bottomRadius, 0.0f, cosVal * bottomRadius, u, vBottom, -sinVal, 0.0f, cosVal));

        PushTriangle(data,
            MakeVertex(-sinVal * bottomRadius, 0.0f, cosVal * bottomRadius, u, vBottom, -sinVal, 0.0f, cosVal),
            MakeVertex(-sinNext * topRadius, height, cosNext * topRadius, uNext, vTop, -sinNext, 0.0f, cosNext),
            MakeVertex(-sinNext * bottomRadius, 0.0f, cosNext * bottomRadius, uNext, vBottom, -sinNext, 0.0f, cosNext));
    }

    return data;
}

Model::ModelData Model::CreateConeData(uint32_t subdivision, float radius, float height) {
    ModelData data;
    subdivision = (subdivision < 3) ? 3 : subdivision;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    float halfHeight = height * 0.5f;
    float slope = radius / height;

    for (uint32_t i = 0; i < subdivision; ++i) {
        float angle0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(subdivision);
        float angle1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(subdivision);
        float midAngle = (angle0 + angle1) * 0.5f;

        float x0 = std::cos(angle0) * radius;
        float z0 = std::sin(angle0) * radius;
        float x1 = std::cos(angle1) * radius;
        float z1 = std::sin(angle1) * radius;
        float u0 = static_cast<float>(i) / static_cast<float>(subdivision);
        float u1 = static_cast<float>(i + 1) / static_cast<float>(subdivision);

        Vector3 normal0 = Normalize({ std::cos(angle0), slope, std::sin(angle0) }, { 1.0f, 0.0f, 0.0f });
        Vector3 normal1 = Normalize({ std::cos(angle1), slope, std::sin(angle1) }, { 1.0f, 0.0f, 0.0f });
        Vector3 normalApex = Normalize({ std::cos(midAngle), slope, std::sin(midAngle) }, { 0.0f, 1.0f, 0.0f });

        PushTriangle(data,
            MakeVertex(0.0f, halfHeight, 0.0f, 0.5f, 0.0f, normalApex.x, normalApex.y, normalApex.z),
            MakeVertex(x1, -halfHeight, z1, u1, 1.0f, normal1.x, normal1.y, normal1.z),
            MakeVertex(x0, -halfHeight, z0, u0, 1.0f, normal0.x, normal0.y, normal0.z));

        VertexData bottomCenter = MakeVertex(0.0f, -halfHeight, 0.0f, 0.5f, 0.5f, 0.0f, -1.0f, 0.0f);
        VertexData bottom0 = MakeVertex(x0, -halfHeight, z0, x0 / (radius * 2.0f) + 0.5f, z0 / (radius * 2.0f) + 0.5f, 0.0f, -1.0f, 0.0f);
        VertexData bottom1 = MakeVertex(x1, -halfHeight, z1, x1 / (radius * 2.0f) + 0.5f, z1 / (radius * 2.0f) + 0.5f, 0.0f, -1.0f, 0.0f);
        PushTriangle(data, bottomCenter, bottom0, bottom1);
    }

    return data;
}

Model::ModelData Model::CreateTriangleData() {
    ModelData data;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    const Vector3 normal = { 0.0f, 1.0f, 0.0f };
    PushTriangle(data,
        MakeVertex(-1.0f, 0.0f, -1.0f, 0.0f, 1.0f, normal.x, normal.y, normal.z),
        MakeVertex(0.0f, 0.0f, 1.0f, 0.5f, 0.0f, normal.x, normal.y, normal.z),
        MakeVertex(1.0f, 0.0f, -1.0f, 1.0f, 1.0f, normal.x, normal.y, normal.z));

    return data;
}

Model::ModelData Model::CreateBoxData() {
    ModelData data;
    data.material.textureIndex = GetPrimitiveTextureIndex();

    const float min = -1.0f;
    const float max = 1.0f;

    PushQuad(data,
        MakeVertex(min, max, max, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        MakeVertex(max, max, max, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        MakeVertex(min, min, max, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        MakeVertex(max, min, max, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f));

    PushQuad(data,
        MakeVertex(max, max, min, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f),
        MakeVertex(min, max, min, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f),
        MakeVertex(max, min, min, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f),
        MakeVertex(min, min, min, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f));

    PushQuad(data,
        MakeVertex(min, max, min, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        MakeVertex(min, max, max, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        MakeVertex(min, min, min, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f),
        MakeVertex(min, min, max, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f));

    PushQuad(data,
        MakeVertex(max, max, max, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),
        MakeVertex(max, max, min, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f),
        MakeVertex(max, min, max, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f),
        MakeVertex(max, min, min, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f));

    PushQuad(data,
        MakeVertex(min, max, min, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        MakeVertex(max, max, min, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        MakeVertex(min, max, max, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
        MakeVertex(max, max, max, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f));

    PushQuad(data,
        MakeVertex(min, min, max, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f),
        MakeVertex(max, min, max, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f),
        MakeVertex(min, min, min, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f),
        MakeVertex(max, min, min, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f));

    return data;
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    MaterialData materialData;
    std::string line;
    std::ifstream file(directoryPath + "/" + filename);
    if (!file.is_open()) return materialData;

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materialData;
}

Model::ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    ModelData modelData;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "v") {
            Vector4 p{};
            s >> p.x >> p.y >> p.z;
            p.w = 1.0f;
            positions.push_back(p);
        } else if (identifier == "vt") {
            Vector2 uv{};
            s >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y;
            texcoords.push_back(uv);
        } else if (identifier == "vn") {
            Vector3 n{};                                                 
            s >> n.x >> n.y >> n.z;
            n.x *= -1.0f;
            normals.push_back(n);
        } else if (identifier == "f") {
            VertexData triangle[3]{};
            for (int i = 0; i < 3; ++i) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                std::istringstream v(vertexDefinition);
                uint32_t idx[3]{};
                for (int e = 0; e < 3; ++e) {
                    std::string indexStr;
                    std::getline(v, indexStr, '/');
                    idx[e] = static_cast<uint32_t>(std::stoi(indexStr));
                }
                Vector4 p = positions[idx[0] - 1];
                Vector2 t = texcoords[idx[1] - 1];
                Vector3 n = normals[idx[2] - 1];
                p.x *= -1.0f;
                triangle[i] = { p, t, n };
            }
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
        } else if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }
    return modelData;
}
