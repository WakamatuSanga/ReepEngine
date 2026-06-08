#pragma once
#include "Engine/Level/LevelSceneData.h"
#include <string>

class LevelSceneLoader {
public:
    struct LoadResult {
        bool success = false;
        std::string message;
        std::string resolvedPath;
    };

    LoadResult LoadFromFile(const std::string& filePath, LevelSceneData& sceneData);
    LoadResult LoadFromJsonText(
        const std::string& jsonText,
        LevelSceneData& sceneData,
        const std::string& sourceName = "json text");

    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastResolvedPath() const { return lastResolvedPath_; }

private:
    std::string lastError_;
    std::string lastResolvedPath_;
};
