#pragma once

struct LevelObject;
struct LevelSceneData;
class LevelEventConnectionVisualizer;
class LevelEventLabelVisualizer;
class LevelEventObjectActionVisualizer;
class LevelEventVisualizer;

void DrawLevelObjectEventDetailsImGui(const LevelObject& object);
bool DrawLevelEventDebugImGui(
    const LevelSceneData& sceneData,
    const LevelObject* selectedObject,
    LevelEventVisualizer* eventVisualizer,
    LevelEventConnectionVisualizer* connectionVisualizer,
    LevelEventObjectActionVisualizer* objectActionVisualizer,
    LevelEventLabelVisualizer* labelVisualizer);
