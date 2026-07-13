#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>
#include <vector>

struct LevelObject;
struct LevelSceneData;

struct LevelEventObjectAction {
    std::string targetObjectId;
    std::string targetObjectName;
    std::string actionType;
    std::string actionDescription;
    std::string postEffectType;
    std::string waveId;
    std::string warningText;
    float warningDuration = 0.0f;
};

struct LevelEventFlag {
    std::string id;
    std::string displayName;
    std::string description;
    std::string triggerType = "Enter";
    std::string shapeType = "Box";
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Vector3 size{ 1.0f, 1.0f, 1.0f };
    bool oneShot = true;
    bool initiallyEnabled = true;
    bool visibleInEditor = true;
    std::vector<std::string> nextFlagIds;
    std::vector<LevelEventObjectAction> objectActions;
};

struct LevelEventFlagLink {
    std::string sourceFlagId;
    std::string targetFlagId;
};

struct LevelEventObjectActionLink {
    std::string sourceFlagId;
    std::string targetObjectId;
    std::string targetObjectName;
    std::string actionType;
    std::string actionDescription;
    std::string postEffectType;
    std::string waveId;
};

struct LevelEventValidationResult {
    std::vector<LevelEventFlagLink> missingFlagLinks;
    std::vector<LevelEventObjectActionLink> missingObjectLinks;

    size_t GetMissingLinkCount() const;
};

size_t CountEventFlags(const LevelSceneData& sceneData);
std::vector<const LevelObject*> CollectEventFlagObjects(const LevelSceneData& sceneData);
std::vector<LevelEventFlagLink> CollectFlagLinks(const LevelSceneData& sceneData);
std::vector<LevelEventObjectActionLink> CollectObjectActionLinks(const LevelSceneData& sceneData);
LevelEventValidationResult ValidateLevelEventLinks(const LevelSceneData& sceneData);
