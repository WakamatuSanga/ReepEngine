#include "AimCorridorVisualController.h"

#include <algorithm>
#include <cmath>

namespace {
    Vector2 Add(const Vector2& lhs, const Vector2& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y };
    }

    Vector2 Subtract(const Vector2& lhs, const Vector2& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y };
    }

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    float Length(const Vector2& value) {
        const float lengthSquared = value.x * value.x + value.y * value.y;
        return std::isfinite(lengthSquared) ? std::sqrt(lengthSquared) : 0.0f;
    }

    float ClampFinite(float value, float minimum, float maximum, float fallback) {
        if (!std::isfinite(value)) {
            return std::clamp(fallback, minimum, maximum);
        }
        return std::clamp(value, minimum, maximum);
    }
}

void AimCorridorVisualController::UpdateBaseScreenOffsetLayout() {
    const Vector2 nearBaseOffset = baseScreenOffsetEnabled_ ? nearBaseScreenOffset_ : Vector2{};
    const Vector2 farBaseOffset = baseScreenOffsetEnabled_ ? farBaseScreenOffset_ : Vector2{};
    const Vector2 nearLeadOffset = leadLagEnabled_ ? nearLeadCurrentScreen_ : Vector2{};
    const Vector2 farLeadOffset = leadLagEnabled_ ? farLeadCurrentScreen_ : Vector2{};

    nearFinalScreenOffset_ = Add(nearBaseOffset, nearLeadOffset);
    farFinalScreenOffset_ = Add(farBaseOffset, farLeadOffset);
    baseScreenOffsetConversionValid_ = std::isfinite(leadFovY_)
        && leadFovY_ > 0.0f
        && leadFovY_ < 3.14159265358979323846f
        && std::isfinite(leadAspectRatio_)
        && leadAspectRatio_ > 0.0f
        && std::isfinite(nearLeadDepth_)
        && nearLeadDepth_ > 0.0f
        && std::isfinite(farLeadDepth_)
        && farLeadDepth_ > 0.0f;

    nearBaseScreenWorldOffset_ = ConvertScreenOffsetToWorld(nearBaseOffset, nearLeadDepth_);
    farBaseScreenWorldOffset_ = ConvertScreenOffsetToWorld(farBaseOffset, farLeadDepth_);
    nearLeadWorldOffset_ = ConvertScreenOffsetToWorld(nearLeadOffset, nearLeadDepth_);
    farLeadWorldOffset_ = ConvertScreenOffsetToWorld(farLeadOffset, farLeadDepth_);
    nearFinalScreenWorldOffset_ = ConvertScreenOffsetToWorld(nearFinalScreenOffset_, nearLeadDepth_);
    farFinalScreenWorldOffset_ = ConvertScreenOffsetToWorld(farFinalScreenOffset_, farLeadDepth_);
}

void AimCorridorVisualController::UpdateBaseScreenOffsetDebug() {
    playerProjectionValid_ = ProjectWorldToScreenUv(playerRenderPosition_, playerScreenUv_);
    playerToNearScreenDistance_ = playerProjectionValid_ && nearProjectionValid_
        ? Length(Subtract(nearScreenUv_, playerScreenUv_))
        : 0.0f;
    playerToFarScreenDistance_ = playerProjectionValid_ && farProjectionValid_
        ? Length(Subtract(farScreenUv_, playerScreenUv_))
        : 0.0f;
    nearToFarScreenDistance_ = nearProjectionValid_ && farProjectionValid_
        ? Length(Subtract(farScreenUv_, nearScreenUv_))
        : 0.0f;

    forwardPlacementActive_ = baseScreenOffsetEnabled_
        && baseScreenOffsetConversionValid_
        && playerProjectionValid_
        && nearProjectionValid_
        && farProjectionValid_
        && playerToNearScreenDistance_ > 0.0001f
        && nearToFarScreenDistance_ > 0.0001f;

    Vector2 nearBaseScreenUv{};
    Vector2 farBaseScreenUv{};
    const bool nearBaseProjectionValid = ProjectWorldToScreenUv(
        Add(nearBaseCenter_, nearBaseScreenWorldOffset_), nearBaseScreenUv);
    const bool farBaseProjectionValid = ProjectWorldToScreenUv(
        Add(farBaseCenter_, farBaseScreenWorldOffset_), farBaseScreenUv);
    verticalDirectionNormal_ = baseScreenOffsetEnabled_
        && aimOriginProjectionValid_
        && nearBaseProjectionValid
        && farBaseProjectionValid
        && nearBaseScreenUv.y < aimOriginScreenUv_.y - 0.0001f
        && farBaseScreenUv.y < nearBaseScreenUv.y - 0.0001f;
}

void AimCorridorVisualController::ClampBaseScreenOffsetParameters() {
    nearBaseScreenOffset_.x = ClampFinite(nearBaseScreenOffset_.x, -0.15f, 0.15f, 0.0f);
    nearBaseScreenOffset_.y = ClampFinite(nearBaseScreenOffset_.y, -0.20f, 0.25f, 0.09f);
    farBaseScreenOffset_.x = ClampFinite(farBaseScreenOffset_.x, -0.20f, 0.20f, 0.0f);
    farBaseScreenOffset_.y = ClampFinite(farBaseScreenOffset_.y, -0.25f, 0.35f, 0.16f);
}

void AimCorridorVisualController::ResetBaseScreenOffsetParameters() {
    baseScreenOffsetEnabled_ = true;
    nearBaseScreenOffset_ = { 0.0f, 0.09f };
    farBaseScreenOffset_ = { 0.0f, 0.16f };
    ClampBaseScreenOffsetParameters();
    ResetBaseScreenOffsetRuntimeState();
}

void AimCorridorVisualController::ResetBaseScreenOffsetRuntimeState() {
    nearBaseScreenWorldOffset_ = {};
    farBaseScreenWorldOffset_ = {};
    nearFinalScreenWorldOffset_ = {};
    farFinalScreenWorldOffset_ = {};
    nearLeadWorldOffset_ = {};
    farLeadWorldOffset_ = {};
    nearFinalScreenOffset_ = {};
    farFinalScreenOffset_ = {};
    playerScreenUv_ = {};
    playerToNearScreenDistance_ = 0.0f;
    playerToFarScreenDistance_ = 0.0f;
    nearToFarScreenDistance_ = 0.0f;
    playerProjectionValid_ = false;
    baseScreenOffsetConversionValid_ = false;
    forwardPlacementActive_ = false;
    verticalDirectionNormal_ = false;
}
