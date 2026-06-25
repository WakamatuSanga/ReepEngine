#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>

struct InfluenceField {
    enum class Type : uint32_t { Player = 0, Enemy = 1 };
    Type type = Type::Player;
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    float radius = 1.0f;
    float strength = 1.0f;
    float cloudClearStrength = 0.0f;
    float particleRepulsionStrength = 0.0f;
    float falloffPower = 2.0f;
    bool enabled = true;
    bool affectCloud = true;
    bool affectGpuParticle = true;
};

inline constexpr uint32_t kMaxInfluenceFields = 16;
inline constexpr uint32_t kInfluenceFieldFlagPlayer = 1u << 0;
inline constexpr uint32_t kInfluenceFieldFlagEnemy = 1u << 1;
inline constexpr uint32_t kInfluenceFieldFlagAffectCloud = 1u << 2;
inline constexpr uint32_t kInfluenceFieldFlagAffectGpuParticle = 1u << 3;