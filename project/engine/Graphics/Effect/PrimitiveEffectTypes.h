#pragma once
#include "Matrix4x4.h"

struct RotatingPlaneHitEffectSettings {
    Vector3 position = { 0.0f, 1.2f, 3.0f };
    Vector3 baseRotation = { 1.5708f, 0.0f, 0.0f };
    Vector3 startScale = { 0.35f, 1.25f, 1.0f };
    Vector3 endScale = { 1.4f, 0.25f, 1.0f };
    Vector4 color = { 1.0f, 0.8f, 0.25f, 0.85f };
    float lifetime = 0.35f;
    float rotationStep = 0.55f;
    float rotationSpeed = 5.0f;
    int planeCount = 4;
};

struct RingEffectSettings {
    Vector3 position = { 0.0f, 1.2f, 3.0f };
    Vector3 rotation = { 1.25f, 0.0f, 0.0f };
    Vector4 color = { 1.0f, 1.0f, 1.0f, 0.9f };
    float lifetime = 0.75f;
    float startScale = 0.45f;
    float endScale = 2.1f;
    float thickness = 0.35f;
    float startAlpha = 0.9f;
    float endAlpha = 0.0f;
    bool useBillboard = false;
};

struct CylinderEffectSettings {
    Vector3 position = { 3.0f, -1.0f, 3.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector4 color = { 0.45f, 0.65f, 1.0f, 0.75f };
    float lifetime = 1.0f;
    float startRadius = 0.65f;
    float endRadius = 1.65f;
    float startHeight = 0.35f;
    float endHeight = 1.8f;
    float uvScrollSpeedX = 0.5f;
    float uvScrollSpeedY = 0.0f;
    bool uvScrollEnabled = true;
};
