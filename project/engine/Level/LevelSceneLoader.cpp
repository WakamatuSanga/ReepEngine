#include "LevelSceneLoader.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    std::string ToGenericString(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }

    bool IsAbsolutePath(const std::string& filePath) {
        return std::filesystem::path(filePath).is_absolute();
    }

    std::string ResolveExistingPath(const std::string& filePath) {
        const std::filesystem::path requestedPath(filePath);
        if (std::filesystem::exists(requestedPath)) {
            return ToGenericString(requestedPath);
        }

        if (IsAbsolutePath(filePath)) {
            return {};
        }

        const std::array<std::filesystem::path, 5> basePaths = {
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath / requestedPath;
            if (std::filesystem::exists(candidate)) {
                return ToGenericString(candidate);
            }
        }

        return {};
    }

    bool ReadTextFile(const std::string& filePath, std::string& text) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        text = buffer.str();
        return true;
    }

    std::string TrimCopy(std::string text) {
        auto isSpace = [](unsigned char ch) {
            return std::isspace(ch) != 0;
            };

        text.erase(
            text.begin(),
            std::find_if(text.begin(), text.end(), [isSpace](unsigned char ch) {
                return !isSpace(ch);
                }));
        text.erase(
            std::find_if(text.rbegin(), text.rend(), [isSpace](unsigned char ch) {
                return !isSpace(ch);
                }).base(),
            text.end());
        return text;
    }

    std::vector<std::string> SplitString(const std::string& text, char delimiter) {
        std::vector<std::string> values;
        std::stringstream stream(text);
        std::string item;
        while (std::getline(stream, item, delimiter)) {
            item = TrimCopy(item);
            if (!item.empty()) {
                values.push_back(std::move(item));
            }
        }
        return values;
    }

    std::vector<LevelEventObjectAction> ParseObjectActionsText(const std::string& text) {
        std::vector<LevelEventObjectAction> actions;
        for (const std::string& actionText : SplitString(text, ';')) {
            const std::vector<std::string> parts = SplitString(actionText, '|');
            if (parts.empty()) {
                continue;
            }

            LevelEventObjectAction action;
            action.targetObjectName = parts[0];
            if (parts.size() >= 2) {
                action.actionType = parts[1];
            }
            if (parts.size() >= 3) {
                action.actionDescription = parts[2];
            }
            if (!action.targetObjectName.empty() || !action.actionType.empty()) {
                actions.push_back(std::move(action));
            }
        }
        return actions;
    }

    class LevelJsonReader {
    public:
        explicit LevelJsonReader(std::string_view source)
            : source_(source) {
        }

        bool Parse(LevelSceneData& sceneData) {
            sceneData.Clear();
            SkipWhitespace();
            if (!Consume('{')) {
                return Fail("Root must be a JSON object.");
            }

            bool hasObjects = false;
            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    SkipWhitespace();
                    if (position_ != source_.size()) {
                        return Fail("Unexpected text after root object.");
                    }
                    if (!hasObjects) {
                        return Fail("Root object must contain objects array.");
                    }
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid root member.");
                }

                if (key == "name") {
                    if (!ParseString(sceneData.name)) {
                        return Fail("Invalid scene name.");
                    }
                } else if (key == "objects") {
                    if (!ParseObjectArray(sceneData.objects)) {
                        return false;
                    }
                    hasObjects = true;
                } else if (key == "rails") {
                    if (!ParseRailArray(sceneData.rails)) {
                        return false;
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    SkipWhitespace();
                    if (position_ != source_.size()) {
                        return Fail("Unexpected text after root object.");
                    }
                    return hasObjects || Fail("Root object must contain objects array.");
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in root object.");
                }
            }
        }

        const std::string& GetError() const { return error_; }

    private:
        bool ParseObjectArray(std::vector<LevelObject>& objects) {
            objects.clear();
            if (!Consume('[')) {
                return Fail("Expected object array.");
            }

            while (true) {
                if (Consume(']')) {
                    return true;
                }

                LevelObject object;
                if (!ParseLevelObject(object)) {
                    return false;
                }
                objects.push_back(std::move(object));

                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in object array.");
                }
            }
        }

        bool ParseRailArray(std::vector<LevelRail>& rails) {
            rails.clear();
            if (!Consume('[')) {
                return Fail("Expected rail array.");
            }

            while (true) {
                if (Consume(']')) {
                    return true;
                }

                LevelRail rail;
                if (!ParseRail(rail)) {
                    return false;
                }
                if (!rail.railId.empty() || !rail.name.empty() || !rail.points.empty()) {
                    rails.push_back(std::move(rail));
                }

                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in rail array.");
                }
            }
        }

        bool ParseRail(LevelRail& rail) {
            if (!Consume('{')) {
                return Fail("Expected rail object.");
            }

            while (true) {
                if (Consume('}')) {
                    if (rail.railId.empty()) {
                        rail.railId = rail.name;
                    }
                    if (rail.name.empty()) {
                        rail.name = rail.railId;
                    }
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid rail member.");
                }

                if (key == "rail_id" || key == "railId" || key == "id") {
                    if (!ParseString(rail.railId)) {
                        return Fail("Invalid rail_id.");
                    }
                } else if (key == "name" || key == "rail_name" || key == "railName") {
                    if (!ParseString(rail.name)) {
                        return Fail("Invalid rail name.");
                    }
                } else if (key == "rail_type" || key == "railType" || key == "type") {
                    if (!ParseString(rail.railType)) {
                        return Fail("Invalid rail type.");
                    }
                } else if (key == "loop" || key == "rail_loop" || key == "railLoop") {
                    if (!ParseBoolValue(rail.loop)) {
                        return Fail("Invalid rail loop.");
                    }
                } else if (key == "visible" || key == "visibleInEditor" ||
                    key == "visible_in_editor" || key == "rail_visible_in_editor") {
                    if (!ParseBoolValue(rail.visibleInEditor)) {
                        return Fail("Invalid rail visibleInEditor.");
                    }
                } else if (key == "speed" || key == "rail_speed" || key == "railSpeed") {
                    double speed = 0.0;
                    if (!ParseNumber(speed)) {
                        return Fail("Invalid rail speed.");
                    }
                    rail.speed = static_cast<float>(speed);
                } else if (key == "points") {
                    if (!ParseRailPointArray(rail.points)) {
                        return Fail("Invalid rail points.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    if (rail.railId.empty()) {
                        rail.railId = rail.name;
                    }
                    if (rail.name.empty()) {
                        rail.name = rail.railId;
                    }
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in rail.");
                }
            }
        }

        bool ParseRailPointArray(std::vector<Vector3>& points) {
            points.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                if (Consume(']')) {
                    return true;
                }

                Vector3 point{};
                if (!ParseRailPointValue(point)) {
                    return false;
                }
                points.push_back(point);

                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in rail point array.");
                }
            }
        }

        bool ParseRailPointValue(Vector3& point) {
            SkipWhitespace();
            if (Peek() == '[') {
                return ParseVector3(point);
            }

            if (!Consume('{')) {
                return false;
            }

            bool hasX = false;
            bool hasY = false;
            bool hasZ = false;
            while (true) {
                if (Consume('}')) {
                    return hasX && hasY && hasZ;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid rail point member.");
                }

                double value = 0.0;
                if (key == "x") {
                    if (!ParseNumber(value)) {
                        return Fail("Invalid rail point x.");
                    }
                    point.x = static_cast<float>(value);
                    hasX = true;
                } else if (key == "y") {
                    if (!ParseNumber(value)) {
                        return Fail("Invalid rail point y.");
                    }
                    point.y = static_cast<float>(value);
                    hasY = true;
                } else if (key == "z") {
                    if (!ParseNumber(value)) {
                        return Fail("Invalid rail point z.");
                    }
                    point.z = static_cast<float>(value);
                    hasZ = true;
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    return hasX && hasY && hasZ;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in rail point.");
                }
            }
        }

        bool ParseLevelObject(LevelObject& object) {
            if (!Consume('{')) {
                return Fail("Expected level object.");
            }

            while (true) {
                if (Consume('}')) {
                    FinalizeLevelObject(object);
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid level object member.");
                }

                if (key == "object_id" || key == "objectId") {
                    if (!ParseString(object.objectId)) {
                        return Fail("Invalid object_id.");
                    }
                } else if (key == "name") {
                    if (!ParseString(object.name)) {
                        return Fail("Invalid object name.");
                    }
                } else if (key == "type") {
                    if (!ParseString(object.type)) {
                        return Fail("Invalid object type.");
                    }
                } else if (key == "editor_label" || key == "editorLabel") {
                    if (!ParseString(object.editorLabel)) {
                        return Fail("Invalid editor_label.");
                    }
                } else if (key == "editor_description" || key == "editorDescription") {
                    if (!ParseString(object.editorDescription)) {
                        return Fail("Invalid editor_description.");
                    }
                } else if (key == "primitive_shape" || key == "primitiveShape") {
                    if (!ParseString(object.primitiveShape)) {
                        return Fail("Invalid primitive_shape.");
                    }
                } else if (key == "is_event_flag" || key == "isEventFlag") {
                    if (!ParseBoolValue(object.isEventFlag)) {
                        return Fail("Invalid is_event_flag.");
                    }
                } else if (key == "event_flag_id" || key == "eventFlagId") {
                    object.isEventFlag = true;
                    if (!ParseString(object.eventFlagId)) {
                        return Fail("Invalid event_flag_id.");
                    }
                    object.eventFlag.id = object.eventFlagId;
                } else if (key == "event_flag" || key == "eventFlag") {
                    object.isEventFlag = true;
                    if (!ParseEventFlag(object.eventFlag)) {
                        return false;
                    }
                } else if (key == "event_display_name") {
                    object.isEventFlag = true;
                    if (!ParseString(object.eventFlag.displayName)) {
                        return Fail("Invalid event_display_name.");
                    }
                } else if (key == "event_description") {
                    object.isEventFlag = true;
                    if (!ParseString(object.eventFlag.description)) {
                        return Fail("Invalid event_description.");
                    }
                } else if (key == "event_trigger_type") {
                    object.isEventFlag = true;
                    if (!ParseString(object.eventFlag.triggerType)) {
                        return Fail("Invalid event_trigger_type.");
                    }
                } else if (key == "event_shape_type") {
                    object.isEventFlag = true;
                    if (!ParseString(object.eventFlag.shapeType)) {
                        return Fail("Invalid event_shape_type.");
                    }
                } else if (key == "event_one_shot") {
                    object.isEventFlag = true;
                    if (!ParseBoolValue(object.eventFlag.oneShot)) {
                        return Fail("Invalid event_one_shot.");
                    }
                } else if (key == "event_initially_enabled") {
                    object.isEventFlag = true;
                    if (!ParseBoolValue(object.eventFlag.initiallyEnabled)) {
                        return Fail("Invalid event_initially_enabled.");
                    }
                } else if (key == "event_visible_in_editor") {
                    object.isEventFlag = true;
                    if (!ParseBoolValue(object.eventFlag.visibleInEditor)) {
                        return Fail("Invalid event_visible_in_editor.");
                    }
                } else if (key == "event_next_flag_ids") {
                    object.isEventFlag = true;
                    if (!ParseStringListValue(object.eventFlag.nextFlagIds)) {
                        return Fail("Invalid event_next_flag_ids.");
                    }
                } else if (key == "event_object_actions") {
                    object.isEventFlag = true;
                    if (!ParseObjectActionsValue(object.eventFlag.objectActions)) {
                        return Fail("Invalid event_object_actions.");
                    }
                } else if (key == "file_name") {
                    object.hasFileName = true;
                    if (!ParseString(object.fileName)) {
                        return Fail("Invalid object file_name.");
                    }
                } else if (key == "transform") {
                    if (!ParseTransform(object.transform)) {
                        return false;
                    }
                } else if (key == "collider") {
                    if (!ParseCollider(object.collider)) {
                        return false;
                    }
                } else if (key == "children") {
                    if (!ParseObjectArray(object.children)) {
                        return false;
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    FinalizeLevelObject(object);
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in level object.");
                }
            }
        }

        bool ParseEventFlag(LevelEventFlag& eventFlag) {
            if (!Consume('{')) {
                return Fail("Expected event_flag object.");
            }

            while (true) {
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid event_flag member.");
                }

                if (key == "id" || key == "eventFlagId" || key == "event_flag_id") {
                    if (!ParseString(eventFlag.id)) {
                        return Fail("Invalid event flag id.");
                    }
                } else if (key == "displayName" || key == "display_name") {
                    if (!ParseString(eventFlag.displayName)) {
                        return Fail("Invalid event flag displayName.");
                    }
                } else if (key == "description") {
                    if (!ParseString(eventFlag.description)) {
                        return Fail("Invalid event flag description.");
                    }
                } else if (key == "triggerType" || key == "trigger_type") {
                    if (!ParseString(eventFlag.triggerType)) {
                        return Fail("Invalid event flag triggerType.");
                    }
                } else if (key == "shapeType" || key == "shape_type") {
                    if (!ParseString(eventFlag.shapeType)) {
                        return Fail("Invalid event flag shapeType.");
                    }
                } else if (key == "position") {
                    if (!ParseVector3(eventFlag.position)) {
                        return Fail("Invalid event flag position.");
                    }
                } else if (key == "rotation") {
                    if (!ParseVector3(eventFlag.rotation)) {
                        return Fail("Invalid event flag rotation.");
                    }
                } else if (key == "scale" || key == "scaling") {
                    if (!ParseVector3(eventFlag.scale)) {
                        return Fail("Invalid event flag scale.");
                    }
                } else if (key == "size") {
                    if (!ParseVector3(eventFlag.size)) {
                        return Fail("Invalid event flag size.");
                    }
                } else if (key == "oneShot" || key == "one_shot") {
                    if (!ParseBoolValue(eventFlag.oneShot)) {
                        return Fail("Invalid event flag oneShot.");
                    }
                } else if (key == "initiallyEnabled" || key == "initially_enabled") {
                    if (!ParseBoolValue(eventFlag.initiallyEnabled)) {
                        return Fail("Invalid event flag initiallyEnabled.");
                    }
                } else if (key == "visibleInEditor" || key == "visible_in_editor") {
                    if (!ParseBoolValue(eventFlag.visibleInEditor)) {
                        return Fail("Invalid event flag visibleInEditor.");
                    }
                } else if (key == "nextFlagIds" || key == "next_flag_ids") {
                    if (!ParseStringListValue(eventFlag.nextFlagIds)) {
                        return Fail("Invalid event flag nextFlagIds.");
                    }
                } else if (key == "objectActions" || key == "object_actions") {
                    if (!ParseObjectActionsValue(eventFlag.objectActions)) {
                        return Fail("Invalid event flag objectActions.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in event_flag.");
                }
            }
        }

        bool ParseStringListValue(std::vector<std::string>& values) {
            SkipWhitespace();
            if (Peek() == '[') {
                return ParseStringArray(values);
            }

            std::string text;
            if (!ParseString(text)) {
                return false;
            }
            values = SplitString(text, ',');
            return true;
        }

        bool ParseStringArray(std::vector<std::string>& values) {
            values.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                if (Consume(']')) {
                    return true;
                }

                std::string value;
                if (!ParseString(value)) {
                    return false;
                }
                value = TrimCopy(value);
                if (!value.empty()) {
                    values.push_back(std::move(value));
                }

                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in string array.");
                }
            }
        }

        bool ParseObjectActionsValue(std::vector<LevelEventObjectAction>& actions) {
            SkipWhitespace();
            if (Peek() == '[') {
                return ParseObjectActionArray(actions);
            }

            std::string text;
            if (!ParseString(text)) {
                return false;
            }
            actions = ParseObjectActionsText(text);
            return true;
        }

        bool ParseObjectActionArray(std::vector<LevelEventObjectAction>& actions) {
            actions.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                if (Consume(']')) {
                    return true;
                }

                LevelEventObjectAction action;
                if (!ParseObjectAction(action)) {
                    return false;
                }
                actions.push_back(std::move(action));

                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in object action array.");
                }
            }
        }

        bool ParseObjectAction(LevelEventObjectAction& action) {
            if (!Consume('{')) {
                return Fail("Expected object action.");
            }

            while (true) {
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid object action member.");
                }

                if (key == "targetObjectId" || key == "target_object_id") {
                    if (!ParseString(action.targetObjectId)) {
                        return Fail("Invalid targetObjectId.");
                    }
                } else if (key == "targetObjectName" || key == "target_object_name") {
                    if (!ParseString(action.targetObjectName)) {
                        return Fail("Invalid targetObjectName.");
                    }
                } else if (key == "actionType" || key == "action_type") {
                    if (!ParseString(action.actionType)) {
                        return Fail("Invalid actionType.");
                    }
                } else if (key == "actionDescription" || key == "action_description") {
                    if (!ParseString(action.actionDescription)) {
                        return Fail("Invalid actionDescription.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in object action.");
                }
            }
        }

        bool ParseTransform(LevelTransform& transform) {
            if (!Consume('{')) {
                return Fail("Expected transform object.");
            }

            while (true) {
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid transform member.");
                }

                if (key == "translation") {
                    if (!ParseVector3(transform.translation)) {
                        return Fail("Invalid transform translation.");
                    }
                } else if (key == "rotation") {
                    if (!ParseVector3(transform.rotation)) {
                        return Fail("Invalid transform rotation.");
                    }
                } else if (key == "scaling") {
                    if (!ParseVector3(transform.scaling)) {
                        return Fail("Invalid transform scaling.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in transform.");
                }
            }
        }

        bool ParseCollider(LevelCollider& collider) {
            collider.exists = true;
            if (!Consume('{')) {
                return Fail("Expected collider object.");
            }

            while (true) {
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid collider member.");
                }

                if (key == "type") {
                    if (!ParseString(collider.type)) {
                        return Fail("Invalid collider type.");
                    }
                } else if (key == "center") {
                    collider.hasCenter = true;
                    if (!ParseVector3(collider.center)) {
                        return Fail("Invalid collider center.");
                    }
                } else if (key == "size") {
                    collider.hasSize = true;
                    if (!ParseVector3(collider.size)) {
                        return Fail("Invalid collider size.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in collider.");
                }
            }
        }

        bool ParseVector3(Vector3& value) {
            if (!Consume('[')) {
                return false;
            }

            double components[3]{};
            for (int index = 0; index < 3; ++index) {
                if (!ParseNumber(components[index])) {
                    return false;
                }
                if (index < 2 && !Consume(',')) {
                    return false;
                }
            }

            if (!Consume(']')) {
                return false;
            }

            value = {
                static_cast<float>(components[0]),
                static_cast<float>(components[1]),
                static_cast<float>(components[2])
            };
            return true;
        }

        bool ParseBoolValue(bool& value) {
            SkipWhitespace();
            if (ConsumeKeyword("true")) {
                value = true;
                return true;
            }
            if (ConsumeKeyword("false")) {
                value = false;
                return true;
            }

            std::string text;
            if (!ParseString(text)) {
                return false;
            }

            text = TrimCopy(text);
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
                });
            value = text == "true" || text == "1" || text == "yes" || text == "on";
            return true;
        }

        void FinalizeLevelObject(LevelObject& object) {
            if (object.objectId.empty()) {
                object.objectId = object.name;
            }

            if (!object.eventFlag.id.empty()) {
                object.eventFlagId = object.eventFlag.id;
            }

            if (object.isEventFlag) {
                if (object.eventFlagId.empty()) {
                    object.eventFlagId = object.objectId.empty() ? object.name : object.objectId;
                }
                if (object.eventFlag.id.empty()) {
                    object.eventFlag.id = object.eventFlagId;
                }
                if (object.eventFlag.displayName.empty()) {
                    object.eventFlag.displayName =
                        object.editorLabel.empty() ? object.name : object.editorLabel;
                }
                if (object.eventFlag.description.empty()) {
                    object.eventFlag.description = object.editorDescription;
                }
                if (object.eventFlag.shapeType.empty()) {
                    object.eventFlag.shapeType = object.collider.type.empty() ? "Box" : object.collider.type;
                }

                object.eventFlag.position = object.transform.translation;
                object.eventFlag.rotation = object.transform.rotation;
                object.eventFlag.scale = object.transform.scaling;
                object.eventFlag.size = object.collider.hasSize
                    ? object.collider.size
                    : Vector3{ 1.0f, 1.0f, 1.0f };
            }
        }

        bool SkipValue() {
            SkipWhitespace();
            if (position_ >= source_.size()) {
                return Fail("Unexpected end of JSON.");
            }

            const char ch = source_[position_];
            if (ch == '"') {
                std::string ignored;
                return ParseString(ignored);
            }
            if (ch == '{') {
                return SkipObject();
            }
            if (ch == '[') {
                return SkipArray();
            }
            if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
                double ignored = 0.0;
                return ParseNumber(ignored);
            }
            if (ConsumeKeyword("true") || ConsumeKeyword("false") || ConsumeKeyword("null")) {
                return true;
            }
            return Fail("Unknown JSON value.");
        }

        char Peek() {
            SkipWhitespace();
            if (position_ >= source_.size()) {
                return '\0';
            }
            return source_[position_];
        }

        bool SkipObject() {
            if (!Consume('{')) {
                return false;
            }
            while (true) {
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':') || !SkipValue()) {
                    return Fail("Invalid skipped object.");
                }

                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in skipped object.");
                }
            }
        }

        bool SkipArray() {
            if (!Consume('[')) {
                return false;
            }
            while (true) {
                if (Consume(']')) {
                    return true;
                }
                if (!SkipValue()) {
                    return false;
                }
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in skipped array.");
                }
            }
        }

        void SkipWhitespace() {
            while (position_ < source_.size() &&
                std::isspace(static_cast<unsigned char>(source_[position_]))) {
                ++position_;
            }
        }

        bool Consume(char expected) {
            SkipWhitespace();
            if (position_ >= source_.size() || source_[position_] != expected) {
                return false;
            }
            ++position_;
            return true;
        }

        bool ConsumeKeyword(std::string_view keyword) {
            SkipWhitespace();
            if (source_.substr(position_, keyword.size()) != keyword) {
                return false;
            }
            position_ += keyword.size();
            return true;
        }

        bool ParseString(std::string& value) {
            value.clear();
            if (!Consume('"')) {
                return false;
            }

            while (position_ < source_.size()) {
                const char ch = source_[position_++];
                if (ch == '"') {
                    return true;
                }
                if (ch != '\\') {
                    value.push_back(ch);
                    continue;
                }
                if (position_ >= source_.size()) {
                    return false;
                }

                const char escaped = source_[position_++];
                switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'u':
                    if (!SkipUnicodeEscape()) {
                        return false;
                    }
                    value.push_back('?');
                    break;
                default:
                    return false;
                }
            }
            return false;
        }

        bool SkipUnicodeEscape() {
            if (position_ + 4 > source_.size()) {
                return false;
            }
            for (int count = 0; count < 4; ++count) {
                if (!std::isxdigit(static_cast<unsigned char>(source_[position_ + count]))) {
                    return false;
                }
            }
            position_ += 4;
            return true;
        }

        bool ParseNumber(double& value) {
            SkipWhitespace();
            const size_t begin = position_;
            if (position_ < source_.size() && source_[position_] == '-') {
                ++position_;
            }

            bool hasDigits = ConsumeDigits();
            if (position_ < source_.size() && source_[position_] == '.') {
                ++position_;
                hasDigits = ConsumeDigits() || hasDigits;
            }
            if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
                ++position_;
                if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) {
                    ++position_;
                }
                if (!ConsumeDigits()) {
                    return false;
                }
            }
            if (!hasDigits) {
                return false;
            }

            const std::string numberText(source_.substr(begin, position_ - begin));
            char* end = nullptr;
            value = std::strtod(numberText.c_str(), &end);
            return end == numberText.c_str() + numberText.size() && std::isfinite(value);
        }

        bool ConsumeDigits() {
            const size_t begin = position_;
            while (position_ < source_.size() &&
                std::isdigit(static_cast<unsigned char>(source_[position_]))) {
                ++position_;
            }
            return position_ > begin;
        }

        bool Fail(const std::string& message) {
            if (error_.empty()) {
                error_ = message + " position=" + std::to_string(position_);
            }
            return false;
        }

        std::string_view source_;
        size_t position_ = 0;
        std::string error_;
    };
}

LevelSceneLoader::LoadResult LevelSceneLoader::LoadFromFile(
    const std::string& filePath,
    LevelSceneData& sceneData) {
    lastError_.clear();
    lastResolvedPath_.clear();

    const std::string resolvedPath = ResolveExistingPath(filePath);
    if (resolvedPath.empty()) {
        lastError_ = "File not found: " + filePath;
        return { false, lastError_, {} };
    }

    std::string jsonText;
    if (!ReadTextFile(resolvedPath, jsonText)) {
        lastError_ = "Failed to read file: " + resolvedPath;
        return { false, lastError_, resolvedPath };
    }

    LoadResult result = LoadFromJsonText(jsonText, sceneData, resolvedPath);
    if (!result.success) {
        result.resolvedPath = resolvedPath;
        return result;
    }

    lastResolvedPath_ = resolvedPath;
    result.resolvedPath = resolvedPath;
    return result;
}

LevelSceneLoader::LoadResult LevelSceneLoader::LoadFromJsonText(
    const std::string& jsonText,
    LevelSceneData& sceneData,
    const std::string& sourceName) {
    lastError_.clear();
    lastResolvedPath_.clear();

    LevelSceneData loadedData;
    LevelJsonReader reader(jsonText);
    if (!reader.Parse(loadedData)) {
        lastError_ = "Failed to parse JSON: " + reader.GetError();
        return { false, lastError_, sourceName };
    }

    sceneData = std::move(loadedData);
    lastResolvedPath_ = sourceName;
    return {
        true,
        "Loaded " + std::to_string(sceneData.GetObjectCount()) + " objects.",
        sourceName
    };
}
