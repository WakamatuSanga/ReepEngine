#include "LevelSceneData.h"

namespace {
    size_t CountObjectRecursive(const LevelObject& object) {
        size_t count = 1;
        for (const LevelObject& child : object.children) {
            count += CountObjectRecursive(child);
        }
        return count;
    }
}

void LevelSceneData::Clear() {
    name.clear();
    objects.clear();
    rails.clear();
}

size_t LevelSceneData::GetObjectCount() const {
    size_t count = 0;
    for (const LevelObject& object : objects) {
        count += CountObjectRecursive(object);
    }
    return count;
}

size_t LevelSceneData::GetRailPointCount() const {
    size_t count = 0;
    for (const LevelRail& rail : rails) {
        count += rail.points.size();
    }
    return count;
}
