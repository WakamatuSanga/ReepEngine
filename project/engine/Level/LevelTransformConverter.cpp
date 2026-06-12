#include "LevelTransformConverter.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;
    constexpr float kRadToDeg = 180.0f / kPi;
    constexpr float kEpsilon = 0.00001f;

    struct Matrix3x3 {
        float m[3][3]{};
    };

    float AbsFloat(float value) {
        return std::fabs(value);
    }

    Vector3 AbsVector3(const Vector3& value) {
        return {
            AbsFloat(value.x),
            AbsFloat(value.y),
            AbsFloat(value.z),
        };
    }

    Matrix3x3 MultiplyMatrix3x3(const Matrix3x3& lhs, const Matrix3x3& rhs) {
        Matrix3x3 result{};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                for (int index = 0; index < 3; ++index) {
                    result.m[row][column] += lhs.m[row][index] * rhs.m[index][column];
                }
            }
        }
        return result;
    }

    Matrix3x3 MakeEngineRotationMatrixFromEulerDegrees(const Vector3& degrees) {
        const float x = degrees.x * kDegToRad;
        const float y = degrees.y * kDegToRad;
        const float z = degrees.z * kDegToRad;

        const float cx = std::cos(x);
        const float sx = std::sin(x);
        const float cy = std::cos(y);
        const float sy = std::sin(y);
        const float cz = std::cos(z);
        const float sz = std::sin(z);

        Matrix3x3 matrix{};
        matrix.m[0][0] = cy * cz;
        matrix.m[0][1] = cy * sz;
        matrix.m[0][2] = -sy;
        matrix.m[1][0] = (sx * sy * cz) - (cx * sz);
        matrix.m[1][1] = (sx * sy * sz) + (cx * cz);
        matrix.m[1][2] = sx * cy;
        matrix.m[2][0] = (cx * sy * cz) + (sx * sz);
        matrix.m[2][1] = (cx * sy * sz) - (sx * cz);
        matrix.m[2][2] = cx * cy;
        return matrix;
    }

    Vector3 ExtractEngineEulerDegreesFromRotationMatrix(const Matrix3x3& matrix) {
        const float clampedSinY = std::clamp(-matrix.m[0][2], -1.0f, 1.0f);
        const float y = std::asin(clampedSinY);
        const float cosY = std::cos(y);

        float x = 0.0f;
        float z = 0.0f;
        if (std::fabs(cosY) > kEpsilon) {
            x = std::atan2(matrix.m[1][2], matrix.m[2][2]);
            z = std::atan2(matrix.m[0][1], matrix.m[0][0]);
        } else {
            x = 0.0f;
            z = std::atan2(-matrix.m[1][0], matrix.m[1][1]);
        }

        return {
            x * kRadToDeg,
            y * kRadToDeg,
            z * kRadToDeg,
        };
    }

    Matrix3x3 MakeBlenderToEngineAxisMatrix() {
        Matrix3x3 matrix{};
        matrix.m[0][0] = 1.0f;
        matrix.m[1][2] = 1.0f;
        matrix.m[2][1] = 1.0f;
        return matrix;
    }
}

Vector3 BlenderToEnginePosition(const Vector3& blenderPosition) {
    return {
        blenderPosition.x,
        blenderPosition.z,
        blenderPosition.y,
    };
}

Vector3 BlenderToEngineRotationDegrees(const Vector3& blenderRotationDegrees) {
    const Matrix3x3 blenderRotationMatrix = MakeEngineRotationMatrixFromEulerDegrees(blenderRotationDegrees);
    const Matrix3x3 axisMatrix = MakeBlenderToEngineAxisMatrix();
    const Matrix3x3 engineRotationMatrix =
        MultiplyMatrix3x3(MultiplyMatrix3x3(axisMatrix, blenderRotationMatrix), axisMatrix);
    return ExtractEngineEulerDegreesFromRotationMatrix(engineRotationMatrix);
}

Vector3 BlenderToEngineScale(const Vector3& blenderScale) {
    return {
        blenderScale.x,
        blenderScale.z,
        blenderScale.y,
    };
}

LevelTransform BlenderToEngineTransform(const LevelTransform& blenderTransform) {
    return {
        BlenderToEnginePosition(blenderTransform.translation),
        BlenderToEngineRotationDegrees(blenderTransform.rotation),
        BlenderToEngineScale(blenderTransform.scaling),
    };
}

LevelCollider BlenderToEngineCollider(const LevelCollider& blenderCollider) {
    LevelCollider engineCollider = blenderCollider;
    if (engineCollider.hasCenter) {
        engineCollider.center = BlenderToEnginePosition(blenderCollider.center);
    }
    if (engineCollider.hasSize) {
        engineCollider.size = AbsVector3(BlenderToEngineScale(blenderCollider.size));
    }
    return engineCollider;
}
