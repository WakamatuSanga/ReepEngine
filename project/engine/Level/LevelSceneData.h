#pragma once
#include "Engine/Level/LevelEventData.h"
#include "Engine/Level/LevelRailData.h"
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>
#include <vector>

struct LevelTransform {
    Vector3 translation{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scaling{ 1.0f, 1.0f, 1.0f };
};

struct LevelCollider {
    bool exists = false;
    bool hasCenter = false;
    bool hasSize = false;
    std::string type;
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    Vector3 size{ 2.0f, 2.0f, 2.0f };
};

struct LevelObject {
    std::string objectId;
    std::string name;
    std::string type;
    std::string editorLabel;
    std::string editorDescription;
    std::string primitiveShape;
    bool isEventFlag = false;
    std::string eventFlagId;
    LevelEventFlag eventFlag;
    bool hasFileName = false;
    std::string fileName;
    LevelTransform transform;
    LevelCollider collider;
    std::vector<LevelObject> children;
};

struct LevelSceneData {
    std::string name;
    std::vector<LevelObject> objects;
    std::vector<LevelRail> rails;

    void Clear();
    size_t GetObjectCount() const;
    size_t GetRailPointCount() const;
};
