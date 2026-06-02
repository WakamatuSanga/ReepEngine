#include "ModelManager.h"
#include <filesystem>

std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (!instance_) {
        instance_.reset(new ModelManager());
    }
    return instance_.get();
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
    models_.clear();
}

void ModelManager::LoadModel(const std::string& filePath)
{
    if (models_.contains(filePath)) { return; }

    // ファイルが存在しない場合はクラッシュを防ぐためにスキップ
    if (!std::filesystem::exists(filePath)) { return; }

    std::string directoryPath;
    std::string filename;
    size_t pos = filePath.find_last_of('/');
    if (pos != std::string::npos) {
        directoryPath = filePath.substr(0, pos);
        filename = filePath.substr(pos + 1);
    } else {
        directoryPath = "";
        filename = filePath;
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), directoryPath, filename);

    models_.insert(std::make_pair(filePath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models_.contains(filePath)) {
        return models_.at(filePath).get();
    }
    return nullptr;
}

Model* ModelManager::CreateSphere(const std::string& keyName, uint32_t subdivision)
{
    return CreatePrimitive(keyName, Model::CreateSphereData(subdivision));
}

Model* ModelManager::CreatePlane(const std::string& keyName)
{
    return CreatePrimitive(keyName, Model::CreatePlaneData());
}

Model* ModelManager::CreateCircle(const std::string& keyName, uint32_t subdivision)
{
    return CreatePrimitive(keyName, Model::CreateCircleData(subdivision));
}

Model* ModelManager::CreateRing(
    const std::string& keyName,
    uint32_t subdivision,
    float innerRadius,
    float outerRadius,
    float startAngle,
    float endAngle,
    float startRadius,
    float endRadius)
{
    return CreatePrimitive(keyName, Model::CreateRingData(subdivision, innerRadius, outerRadius, startAngle, endAngle, startRadius, endRadius));
}

Model* ModelManager::CreateTorus(const std::string& keyName, uint32_t majorSubdivision, uint32_t minorSubdivision)
{
    return CreatePrimitive(keyName, Model::CreateTorusData(majorSubdivision, minorSubdivision));
}

Model* ModelManager::CreateCylinder(const std::string& keyName, uint32_t subdivision)
{
    return CreatePrimitive(keyName, Model::CreateCylinderData(subdivision));
}

Model* ModelManager::CreateEffectCylinder(const std::string& keyName, uint32_t subdivision)
{
    return CreatePrimitive(keyName, Model::CreateEffectCylinderData(subdivision));
}

Model* ModelManager::CreateCone(const std::string& keyName, uint32_t subdivision)
{
    return CreatePrimitive(keyName, Model::CreateConeData(subdivision));
}

Model* ModelManager::CreateTriangle(const std::string& keyName)
{
    return CreatePrimitive(keyName, Model::CreateTriangleData());
}

Model* ModelManager::CreateBox(const std::string& keyName)
{
    return CreatePrimitive(keyName, Model::CreateBoxData());
}

Model* ModelManager::CreatePrimitive(const std::string& keyName, const Model::ModelData& modelData)
{
    if (models_.contains(keyName)) {
        models_.at(keyName)->Initialize(modelCommon_.get(), modelData);
        return models_.at(keyName).get();
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), modelData);

    models_.insert(std::make_pair(keyName, std::move(model)));
    return models_.at(keyName).get();
}
