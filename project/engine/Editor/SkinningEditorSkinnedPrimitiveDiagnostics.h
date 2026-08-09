#pragma once

#include "Engine/Graphics/Model/GltfSkinnedModelPrimitiveData.h"

class GltfSkinnedModel;

struct SkinningEditorSkinnedPrimitiveDiagnosticsState {
    GltfSkinnedPrimitiveDiagnostics diagnostics{};
    const GltfSkinnedModel* activeModel = nullptr;
    bool hasDiagnostics = false;
};
