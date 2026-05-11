#pragma once
#include "AnimationClip.h"
#include <string>

struct Skeleton;

class GltfAnimationLoader {
public:
    static bool LoadFirstClipFromFile(const std::string& filePath, const Skeleton& referenceSkeleton, AnimationClip& outClip);
};
