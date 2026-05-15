#pragma once

// 2Dベクトル
struct Vector2 {
    float x, y;
};

// 3Dベクトル
struct Vector3 {
    float x, y, z;
};

// 4Dベクトル
struct Vector4 {
    float x, y, z, w;
};

// 4x4行列
// Quaternion rotation. Used by node animation and skeleton joints.
struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Matrix4x4 {
    float m[4][4];
};

// SRT 用 Transform
struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

namespace MatrixMath {
    // 行列の加法
    Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
    // 行列の減法
    Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
    // 行列の積
    Matrix4x4 Multipty(const Matrix4x4& m1, const Matrix4x4& m2);
    // 逆行列
    Matrix4x4 Inverse(const Matrix4x4& m);
    // 転置行列
    Matrix4x4 Transpoce(const Matrix4x4& m);
    // 単位行列
    Matrix4x4 MakeIdentity4x4();
    // 平行移動行列
    Matrix4x4 MakeTranslate(const Vector3& translate);
    // 拡大縮小行列
    Matrix4x4 MakeScale(const Vector3& scale);
    // 回転行列
    Matrix4x4 MakeRotateX(float radian);
    Matrix4x4 MakeRotateY(float radian);
    Matrix4x4 MakeRotateZ(float radian);

    // アフィン変換
    Matrix4x4 MakeAffine(const Vector3& scale,
        const Vector3& rotate,
        const Vector3& translate);

    // 正射影
    Matrix4x4 Orthographic(float left, float top, float right,
        float bottom, float nearClip, float farClip);
    // 透視投影
    Matrix4x4 PerspectiveFov(float fovY, float aspectRatio,
        float nearClip, float farClip);
    // ビューポート
    Matrix4x4 Viewport(float left, float top, float width, float height,
        float minDepth, float maxDepth);

    // クロス積
    Vector3 Cross(const Vector3& v1, const Vector3& v2);
}
