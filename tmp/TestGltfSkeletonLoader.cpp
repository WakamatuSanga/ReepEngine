#include "GltfSkeletonLoader.h"
#include "Skeleton.h"
#include <iostream>

int main() {
    const char* files[] = {
        "project/resources/human/walk.gltf",
        "project/resources/human/sneakWalk.gltf",
    };

    for (const char* file : files) {
        auto skeleton = GltfSkeletonLoader::LoadFromFile(file);
        if (!skeleton) {
            std::cout << file << " failed\n";
            continue;
        }
        std::cout << file << " joints=" << skeleton->joints.size()
            << " root=" << skeleton->root
            << " name=" << skeleton->name << "\n";
    }
    return 0;
}
