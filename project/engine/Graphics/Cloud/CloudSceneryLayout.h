#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
class Camera;
class RailShooterCameraRig;
namespace RenderStyleComparison {struct CloudCompositionSettings;}
// Stateless shared layout: absolute rail stations and reproducible seeds.
struct CloudSceneryLayout {
    static constexpr size_t kBankCount=16;
    std::array<Vector4,kBankCount> centers{},radii{};
    float referenceY=0;
    static CloudSceneryLayout Build(const Camera*,const RailShooterCameraRig*,const RenderStyleComparison::CloudCompositionSettings&,const Vector3& integratedFlow = {});
};
