#pragma once

#include "Engine/Graphics/Model/GltfSkinnedModelMaterialData.h"

class GltfSkinnedModel;

struct SkinningEditorSkinnedMaterialDiagnosticsState {
    GltfSkinnedMaterialDiagnostics diagnostics{};
    const GltfSkinnedModel* activeModel = nullptr;
    bool hasDiagnostics = false;
};
