#pragma once
#include "Engine/math/Matrix4x4.h"
#include <string>
#include <vector>

struct LevelRail {
    std::string railId;
    std::string name;
    std::string railType;
    bool loop = false;
    bool reverseDirection = false;
    bool visibleInEditor = true;
    float speed = 1.0f;
    std::vector<Vector3> points;
};
