#pragma once
#include "Model.h"

class PrimitiveGenerator {
public:
    static Model::ModelData CreateRingData(
        uint32_t subdivision = 32,
        float innerRadius = 0.5f,
        float outerRadius = 1.0f,
        float startAngle = 0.0f,
        float endAngle = 6.2831853f,
        float startRadius = 1.0f,
        float endRadius = 1.0f);
};
