#pragma once

#include "KrakenPreviewAssetMode.h"
#include "SkinningEditorSkeletonComparison.h"
#include "Engine/Graphics/Model/GltfNodeMatrixDiagnostics.h"

struct SkinningEditorGltfMatrixDiagnosticsState {
    KrakenPreviewAssetMode assetMode =
        KrakenPreviewAssetMode::OriginalMatrix;
    KrakenPreviewAssetMode requestedAssetMode =
        KrakenPreviewAssetMode::OriginalMatrix;
    GltfNodeMatrixDiagnostics nodeDiagnostics{};
    SkinningEditorSkeletonSnapshot originalSnapshot{};
    SkinningEditorSkeletonSnapshot compatibleSnapshot{};
    SkinningEditorSkeletonComparisonResult comparison{};
    bool hasNodeDiagnostics = false;
    bool hasPendingLoadRequest = false;
};
