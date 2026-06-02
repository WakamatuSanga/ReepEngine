#pragma once
#include <memory>
#include <string>

struct Skeleton;

class GltfSkeletonLoader {
public:
    static std::unique_ptr<Skeleton> LoadFromFile(const std::string& filePath);
};
