#pragma once
#include <map>
#include <string>
#include <memory>
#include "Model.h"
#include "ModelCommon.h"

class ModelManager
{
    friend struct std::default_delete<ModelManager>;

private:
    static std::unique_ptr<ModelManager> instance_;

    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

public:
    static ModelManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon);
    void Finalize();

    void LoadModel(const std::string& filePath);
    Model* FindModel(const std::string& filePath);
    ModelCommon* GetModelCommon() const { return modelCommon_.get(); }

    Model* CreateSphere(const std::string& keyName, uint32_t subdivision = 16);
    Model* CreatePlane(const std::string& keyName);
    Model* CreateCircle(const std::string& keyName, uint32_t subdivision = 32);
    Model* CreateRing(
        const std::string& keyName,
        uint32_t subdivision = 32,
        float innerRadius = 0.5f,
        float outerRadius = 1.0f,
        float startAngle = 0.0f,
        float endAngle = 6.2831853f,
        float startRadius = 1.0f,
        float endRadius = 1.0f);
    Model* CreateTorus(const std::string& keyName, uint32_t majorSubdivision = 32, uint32_t minorSubdivision = 16);
    Model* CreateCylinder(const std::string& keyName, uint32_t subdivision = 32);
    Model* CreateEffectCylinder(const std::string& keyName, uint32_t subdivision = 32);
    Model* CreateCone(const std::string& keyName, uint32_t subdivision = 32);
    Model* CreateTriangle(const std::string& keyName);
    Model* CreateBox(const std::string& keyName);

private:
    Model* CreatePrimitive(const std::string& keyName, const Model::ModelData& modelData);

    std::map<std::string, std::unique_ptr<Model>> models_;
    std::unique_ptr<ModelCommon> modelCommon_;
};
