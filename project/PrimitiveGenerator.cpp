#include "PrimitiveGenerator.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265359f;

    uint32_t GetPrimitiveTextureIndex() {
        return TextureManager::GetInstance()->GetTextureIndexByFilePath("resources/obj/axis/uvChecker.png");
    }

    Model::VertexData MakeVertex(float x, float y, float z, float u, float v, float nx, float ny, float nz) {
        return { {x, y, z, 1.0f}, {u, v}, {nx, ny, nz} };
    }

    void PushTriangle(Model::ModelData& data,
        const Model::VertexData& v0,
        const Model::VertexData& v1,
        const Model::VertexData& v2) {
        const uint32_t firstIndex = static_cast<uint32_t>(data.vertices.size());
        data.vertices.push_back(v0);
        data.vertices.push_back(v1);
        data.vertices.push_back(v2);
        data.indices.push_back(firstIndex);
        data.indices.push_back(firstIndex + 1);
        data.indices.push_back(firstIndex + 2);
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

Model::ModelData PrimitiveGenerator::CreateRingData(
    uint32_t subdivision,
    float innerRadius,
    float outerRadius,
    float startAngle,
    float endAngle,
    float startRadius,
    float endRadius) {
    Model::ModelData data;
    subdivision = (subdivision < 3) ? 3 : subdivision;
    if (innerRadius < 0.0f) {
        innerRadius = 0.0f;
    }
    if (outerRadius <= innerRadius) {
        outerRadius = innerRadius + 0.5f;
    }
    if (endAngle < startAngle) {
        std::swap(startAngle, endAngle);
    }
    startRadius = (std::max)(0.0f, startRadius);
    endRadius = (std::max)(0.0f, endRadius);

    data.material.textureIndex = GetPrimitiveTextureIndex();

    for (uint32_t i = 0; i < subdivision; ++i) {
        float t0 = static_cast<float>(i) / static_cast<float>(subdivision);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(subdivision);
        float angle0 = std::lerp(startAngle, endAngle, t0);
        float angle1 = std::lerp(startAngle, endAngle, t1);
        float radiusScale0 = std::lerp(startRadius, endRadius, t0);
        float radiusScale1 = std::lerp(startRadius, endRadius, t1);

        float outerX0 = -std::sin(angle0) * (outerRadius * radiusScale0);
        float outerY0 = std::cos(angle0) * (outerRadius * radiusScale0);
        float outerX1 = -std::sin(angle1) * (outerRadius * radiusScale1);
        float outerY1 = std::cos(angle1) * (outerRadius * radiusScale1);
        float innerX0 = -std::sin(angle0) * (innerRadius * radiusScale0);
        float innerY0 = std::cos(angle0) * (innerRadius * radiusScale0);
        float innerX1 = -std::sin(angle1) * (innerRadius * radiusScale1);
        float innerY1 = std::cos(angle1) * (innerRadius * radiusScale1);

        auto MakeRingVertex = [](float x, float y, float u, float v) {
            return MakeVertex(
                x, y, 0.0f,
                u, v,
                0.0f, 0.0f, -1.0f);
            };

        PushQuad(data,
            MakeRingVertex(innerX0, innerY0, t0, 1.0f),
            MakeRingVertex(outerX0, outerY0, t0, 0.0f),
            MakeRingVertex(innerX1, innerY1, t1, 1.0f),
            MakeRingVertex(outerX1, outerY1, t1, 0.0f));
    }

    return data;
}
