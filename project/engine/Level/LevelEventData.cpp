#include "LevelEventData.h"
#include "LevelSceneData.h"
#include <functional>
#include <unordered_set>

namespace {
    std::string GetEventFlagId(const LevelObject& object) {
        if (!object.eventFlag.id.empty()) {
            return object.eventFlag.id;
        }
        return object.eventFlagId;
    }

    void TraverseObjects(
        const std::vector<LevelObject>& objects,
        const std::function<void(const LevelObject&)>& visitor) {
        for (const LevelObject& object : objects) {
            visitor(object);
            TraverseObjects(object.children, visitor);
        }
    }

    bool ContainsNonEmptyValue(
        const std::unordered_set<std::string>& values,
        const std::string& value) {
        return !value.empty() && values.contains(value);
    }
}

size_t LevelEventValidationResult::GetMissingLinkCount() const {
    return missingFlagLinks.size() + missingObjectLinks.size();
}

size_t CountEventFlags(const LevelSceneData& sceneData) {
    size_t count = 0;
    TraverseObjects(sceneData.objects, [&count](const LevelObject& object) {
        if (object.isEventFlag) {
            ++count;
        }
        });
    return count;
}

std::vector<const LevelObject*> CollectEventFlagObjects(const LevelSceneData& sceneData) {
    std::vector<const LevelObject*> eventFlags;
    TraverseObjects(sceneData.objects, [&eventFlags](const LevelObject& object) {
        if (object.isEventFlag) {
            eventFlags.push_back(&object);
        }
        });
    return eventFlags;
}

std::vector<LevelEventFlagLink> CollectFlagLinks(const LevelSceneData& sceneData) {
    std::vector<LevelEventFlagLink> links;
    TraverseObjects(sceneData.objects, [&links](const LevelObject& object) {
        if (!object.isEventFlag) {
            return;
        }

        const std::string sourceFlagId = GetEventFlagId(object);
        for (const std::string& targetFlagId : object.eventFlag.nextFlagIds) {
            if (!targetFlagId.empty()) {
                links.push_back({ sourceFlagId, targetFlagId });
            }
        }
        });
    return links;
}

std::vector<LevelEventObjectActionLink> CollectObjectActionLinks(const LevelSceneData& sceneData) {
    std::vector<LevelEventObjectActionLink> links;
    TraverseObjects(sceneData.objects, [&links](const LevelObject& object) {
        if (!object.isEventFlag) {
            return;
        }

        const std::string sourceFlagId = GetEventFlagId(object);
        for (const LevelEventObjectAction& action : object.eventFlag.objectActions) {
            if (action.targetObjectId.empty() && action.targetObjectName.empty()) {
                continue;
            }
            links.push_back({
                sourceFlagId,
                action.targetObjectId,
                action.targetObjectName,
                action.actionType,
                action.actionDescription,
                });
        }
        });
    return links;
}

LevelEventValidationResult ValidateLevelEventLinks(const LevelSceneData& sceneData) {
    std::unordered_set<std::string> objectIds;
    std::unordered_set<std::string> objectNames;
    std::unordered_set<std::string> flagIds;

    TraverseObjects(sceneData.objects, [&objectIds, &objectNames, &flagIds](const LevelObject& object) {
        if (!object.objectId.empty()) {
            objectIds.insert(object.objectId);
        }
        if (!object.name.empty()) {
            objectNames.insert(object.name);
        }
        if (object.isEventFlag) {
            const std::string flagId = GetEventFlagId(object);
            if (!flagId.empty()) {
                flagIds.insert(flagId);
            }
        }
        });

    LevelEventValidationResult result;
    for (const LevelEventFlagLink& link : CollectFlagLinks(sceneData)) {
        if (!ContainsNonEmptyValue(flagIds, link.targetFlagId)) {
            result.missingFlagLinks.push_back(link);
        }
    }

    for (const LevelEventObjectActionLink& link : CollectObjectActionLinks(sceneData)) {
        const bool hasTarget = !link.targetObjectId.empty()
            ? ContainsNonEmptyValue(objectIds, link.targetObjectId)
            : ContainsNonEmptyValue(objectNames, link.targetObjectName);
        if (!hasTarget) {
            result.missingObjectLinks.push_back(link);
        }
    }

    return result;
}
