#include "Player.h"

#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "PlayerActionController.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumVectorLength = 0.00001f;
constexpr float kPi = 3.14159265358979323846f;

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
    const float length = Length(value);
    if (length <= kMinimumVectorLength) return fallback;
    return { value.x / length, value.y / length, value.z / length };
}

Vector3 MakeRotationFromForward(const Vector3& forward) {
    const Vector3 normalized = Normalize(forward, { 0.0f, 0.0f, 1.0f });
    const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
    return { std::atan2(-normalized.y, horizontal), std::atan2(normalized.x, normalized.z), 0.0f };
}

Vector3 GetModelForwardAxisOffset(Player::ModelForwardAxis axis) {
    switch (axis) {
    case Player::ModelForwardAxis::NegativeZ:
        return { 0.0f, kPi, 0.0f };
    case Player::ModelForwardAxis::PositiveX:
        return { 0.0f, -kPi * 0.5f, 0.0f };
    case Player::ModelForwardAxis::NegativeX:
        return { 0.0f, kPi * 0.5f, 0.0f };
    case Player::ModelForwardAxis::PositiveZ:
    default:
        return {};
    }
}

Vector3 GetModelForwardDirection(Player::ModelForwardAxis axis) {
    switch (axis) {
    case Player::ModelForwardAxis::NegativeZ:
        return { 0.0f, 0.0f, -1.0f };
    case Player::ModelForwardAxis::PositiveX:
        return { 1.0f, 0.0f, 0.0f };
    case Player::ModelForwardAxis::NegativeX:
        return { -1.0f, 0.0f, 0.0f };
    case Player::ModelForwardAxis::PositiveZ:
    default:
        return { 0.0f, 0.0f, 1.0f };
    }
}

Matrix4x4 MakeEulerRotation(const Vector3& rotation) {
    return MatrixMath::Multipty(
        MatrixMath::Multipty(
            MatrixMath::MakeRotateX(rotation.x),
            MatrixMath::MakeRotateY(rotation.y)),
        MatrixMath::MakeRotateZ(rotation.z));
}

Matrix4x4 MakeAxisAngleRotation(const Vector3& axis, float radians) {
    const Vector3 normalizedAxis = Normalize(axis, { 0.0f, 0.0f, 1.0f });
    const float x = normalizedAxis.x;
    const float y = normalizedAxis.y;
    const float z = normalizedAxis.z;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float oneMinusCosine = 1.0f - cosine;

    Matrix4x4 result = MatrixMath::MakeIdentity4x4();
    result.m[0][0] = oneMinusCosine * x * x + cosine;
    result.m[0][1] = oneMinusCosine * x * y + sine * z;
    result.m[0][2] = oneMinusCosine * x * z - sine * y;
    result.m[1][0] = oneMinusCosine * x * y - sine * z;
    result.m[1][1] = oneMinusCosine * y * y + cosine;
    result.m[1][2] = oneMinusCosine * y * z + sine * x;
    result.m[2][0] = oneMinusCosine * x * z + sine * y;
    result.m[2][1] = oneMinusCosine * y * z - sine * x;
    result.m[2][2] = oneMinusCosine * z * z + cosine;
    return result;
}

Vector3 ExtractEulerXYZ(const Matrix4x4& matrix) {
    const float sinY = std::clamp(-matrix.m[0][2], -1.0f, 1.0f);
    const float rotateY = std::asin(sinY);
    const float cosY = std::cos(rotateY);

    float rotateX = 0.0f;
    float rotateZ = 0.0f;
    if (std::fabs(cosY) > 0.0001f) {
        rotateX = std::atan2(matrix.m[1][2], matrix.m[2][2]);
        rotateZ = std::atan2(matrix.m[0][1], matrix.m[0][0]);
    } else {
        rotateX = std::atan2(-matrix.m[2][1], matrix.m[1][1]);
    }
    return { rotateX, rotateY, rotateZ };
}

Vector3 TransformDirection(const Vector3& direction, const Matrix4x4& matrix) {
    return {
        direction.x * matrix.m[0][0] +
            direction.y * matrix.m[1][0] +
            direction.z * matrix.m[2][0],
        direction.x * matrix.m[0][1] +
            direction.y * matrix.m[1][1] +
            direction.z * matrix.m[2][1],
        direction.x * matrix.m[0][2] +
            direction.y * matrix.m[1][2] +
            direction.z * matrix.m[2][2],
    };
}

Vector3 GetCameraRight(const Camera& camera) {
    const Matrix4x4& matrix = camera.GetWorldMatrix();
    return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
}

Vector3 GetCameraUp(const Camera& camera) {
    const Matrix4x4& matrix = camera.GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
}

Vector3 GetCameraForward(const Camera& camera) {
    const Matrix4x4& matrix = camera.GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
}

float Lerp(float start, float end, float t) {
    return start + (end - start) * t;
}
}

void Player::SetRailFlightVisualBankRadians(float radians) {
    railFlightVisualBankRadians_ = std::isfinite(radians) ? radians : 0.0f;
}

Vector3 Player::GetVisualDisplayForward() const {
    const Vector3 modelForward = GetModelForwardDirection(modelForwardAxis_);
    const Vector3 displayForward = TransformDirection(
        modelForward, MakeEulerRotation(visualFinalRotation_));
    return Normalize(displayForward, baseForward_);
}

Vector3 Player::GetActionVisualRotationOffset() const {
    return actionController_ ? actionController_->GetVisualRotationOffset() : Vector3{};
}

void Player::ResetPosition() {
    localOffsetX_ = 0.0f;
    localOffsetY_ = 0.0f;
}

void Player::UpdateWorldPosition() {
    if (!camera_) return;

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraRight = GetCameraRight(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);
    const Vector3 cameraForward = GetCameraForward(*camera_);

    if (baseMode_ == BaseMode::Rail && hasExternalBase_) {
        basePosition_ = externalBasePosition_;
        baseForward_ = Normalize(externalBaseForward_, cameraForward);
    } else {
        basePosition_ = Add(cameraPosition, Scale(cameraForward, distanceFromCamera_));
        baseForward_ = cameraForward;
    }

    worldPosition_ = Add(
        Add(basePosition_, Scale(cameraRight, localOffsetX_)),
        Scale(cameraUp, localOffsetY_));
}

void Player::UpdateCenterVisibilityAssist(float deltaTime) {
    if (!camera_ || !model_) return;

    centerProjectionValid_ = false;
    float targetAlpha = 1.0f;
    currentScreenDistance_ = 1.0f;
    if (enableCenterFade_ && centerFadeOuterRadius_ > centerFadeInnerRadius_) {
        const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
        const Vector3& p = worldPosition_;
        const float clipX = p.x * viewProjection.m[0][0] + p.y * viewProjection.m[1][0] + p.z * viewProjection.m[2][0] + viewProjection.m[3][0];
        const float clipY = p.x * viewProjection.m[0][1] + p.y * viewProjection.m[1][1] + p.z * viewProjection.m[2][1] + viewProjection.m[3][1];
        const float clipW = p.x * viewProjection.m[0][3] + p.y * viewProjection.m[1][3] + p.z * viewProjection.m[2][3] + viewProjection.m[3][3];
        if (clipW > 0.0001f && std::isfinite(clipW)) {
            const float ndcX = clipX / clipW;
            const float ndcY = clipY / clipW;
            if (std::isfinite(ndcX) && std::isfinite(ndcY)) {
                const float normalizedX = ndcX * 0.5f + 0.5f;
                const float normalizedY = -ndcY * 0.5f + 0.5f;
                const float dx = normalizedX - 0.5f;
                const float dy = normalizedY - 0.5f;
                currentScreenDistance_ = std::sqrt(dx * dx + dy * dy);
                const float fadeT = std::clamp(
                    (currentScreenDistance_ - centerFadeInnerRadius_) /
                    (centerFadeOuterRadius_ - centerFadeInnerRadius_),
                    0.0f, 1.0f);
                const float smoothT = fadeT * fadeT * (3.0f - 2.0f * fadeT);
                targetAlpha = Lerp(std::clamp(centerFadeMinAlpha_, 0.0f, 1.0f), 1.0f, smoothT);
                centerProjectionValid_ = true;
            }
        }
    }

    const float alphaT = std::clamp(deltaTime * centerFadeSpeed_, 0.0f, 1.0f);
    currentPlayerAlpha_ = Lerp(currentPlayerAlpha_, targetAlpha, alphaT);
    ApplyModelAlpha(currentPlayerAlpha_ * damageFeedbackAlpha_);
}

void Player::ApplyModelAlpha(float alpha) {
    if (!model_) return;
    if (Model::Material* material = model_->GetMaterialData()) {
        material->color.w = std::clamp(alpha, 0.0f, 1.0f);
    }
}

void Player::UpdateObjectTransform() {
    if (!object_) return;

    visualBaseRotation_ = MakeRotationFromForward(baseForward_);
    const Vector3 existingFinalRotation = Add(
        Add(
            Add(
                Add(visualBaseRotation_, GetModelForwardAxisOffset(modelForwardAxis_)),
                modelRotationOffset_),
            currentVisualTilt_),
        GetActionVisualRotationOffset());
    visualFinalRotation_ = existingFinalRotation;
    if (std::fabs(railFlightVisualBankRadians_) > 0.000001f) {
        const Matrix4x4 existingRotation = MakeEulerRotation(existingFinalRotation);
        const Matrix4x4 railBankRotation =
            MakeAxisAngleRotation(baseForward_, railFlightVisualBankRadians_);
        visualFinalRotation_ = ExtractEulerXYZ(
            MatrixMath::Multipty(existingRotation, railBankRotation));
    }

    object_->SetTranslate(worldPosition_);
    object_->SetRotate(visualFinalRotation_);
    object_->SetScale(modelScale_);
    if (hitRadiusObject_) {
        hitRadiusObject_->SetTranslate(worldPosition_);
        hitRadiusObject_->SetRotate({});
        hitRadiusObject_->SetScale({ hitRadius_, hitRadius_, hitRadius_ });
    }
}

void Player::UpdateVisualTilt(float deltaTime) {
    Vector3 targetTilt{};
    if (enableVisualTilt_ && lastInputApplied_) {
        targetTilt.x = -lastRawMoveInput_.y * visualPitchTiltAmount_;
        targetTilt.z = -lastRawMoveInput_.x * visualRollTiltAmount_;
    }

    const float t = std::clamp(deltaTime * visualTiltSmoothSpeed_, 0.0f, 1.0f);
    currentVisualTilt_ = {
        Lerp(currentVisualTilt_.x, targetTilt.x, t),
        Lerp(currentVisualTilt_.y, targetTilt.y, t),
        Lerp(currentVisualTilt_.z, targetTilt.z, t),
    };
}
