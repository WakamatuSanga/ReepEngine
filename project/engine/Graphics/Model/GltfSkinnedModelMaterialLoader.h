#pragma once

#include "GltfSkinnedModelMaterialData.h"
#include "GltfSkinnedModelPrimitiveData.h"
#include "Model.h"

#include <string>
#include <vector>

class ModelCommon;

std::string ResolveGltfSkinnedRelativePath(
    const std::string& baseFilePath,
    const std::string& relativePath);

bool LoadGltfSkinnedModelMaterials(
    const std::string& gltfPath,
    const std::string& gltfJson,
    const std::vector<SkinnedPrimitiveRange>& primitiveRanges,
    GltfSkinnedMaterialState& outState);

bool ApplyPrimaryGltfSkinnedMaterial(
    const GltfSkinnedMaterialState& state,
    Model::ModelData& modelData);

bool InitializeGltfSkinnedMaterialBindings(
    ModelCommon* modelCommon,
    Model& model,
    const std::vector<SkinnedPrimitiveRange>& primitiveRanges,
    GltfSkinnedMaterialState& state);
