#pragma once
#include "AnimationClip.h"
#include <string>

struct Skeleton;

class GltfAnimationLoader {
public:
    static bool LoadFirstClipFromFile(const std::string& filePath, const Skeleton& referenceSkeleton, AnimationClip& outClip);
    static bool LoadFirstNodeClipFromFile(const std::string& filePath, AnimationClip& outClip);
};
