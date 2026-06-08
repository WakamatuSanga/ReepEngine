#pragma once
#include "Engine/Level/LevelSceneData.h"

Vector3 BlenderToEnginePosition(const Vector3& blenderPosition);
Vector3 BlenderToEngineRotationDegrees(const Vector3& blenderRotationDegrees);
Vector3 BlenderToEngineScale(const Vector3& blenderScale);
LevelTransform BlenderToEngineTransform(const LevelTransform& blenderTransform);
LevelCollider BlenderToEngineCollider(const LevelCollider& blenderCollider);
