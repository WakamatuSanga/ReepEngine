#include "Engine/Animation/Skeleton.h"
#include <memory>

namespace GameSceneInitializeHelpers {
    std::unique_ptr<Skeleton> MakeHumanoidPreviewSkeleton() {
        auto skeleton = std::make_unique<Skeleton>();
        skeleton->name = "Humanoid Preview";
        skeleton->joints.resize(8);

        skeleton->joints[0].name = "Root";
        skeleton->joints[0].parentIndex = -1;
        skeleton->joints[0].children = { 1 };
        skeleton->joints[0].localTranslate = { 2.0f, 0.0f, 0.0f };

        skeleton->joints[1].name = "Spine";
        skeleton->joints[1].parentIndex = 0;
        skeleton->joints[1].children = { 2, 4, 6 };
        skeleton->joints[1].localTranslate = { 0.0f, 1.2f, 0.0f };

        skeleton->joints[2].name = "Chest";
        skeleton->joints[2].parentIndex = 1;
        skeleton->joints[2].children = { 3 };
        skeleton->joints[2].localTranslate = { 0.0f, 0.9f, 0.0f };

        skeleton->joints[3].name = "Head";
        skeleton->joints[3].parentIndex = 2;
        skeleton->joints[3].localTranslate = { 0.0f, 0.7f, 0.0f };

        skeleton->joints[4].name = "Arm.L";
        skeleton->joints[4].parentIndex = 1;
        skeleton->joints[4].children = { 5 };
        skeleton->joints[4].localTranslate = { -0.8f, 0.6f, 0.0f };

        skeleton->joints[5].name = "Fore.L";
        skeleton->joints[5].parentIndex = 4;
        skeleton->joints[5].localTranslate = { -0.7f, 0.0f, 0.0f };

        skeleton->joints[6].name = "Arm.R";
        skeleton->joints[6].parentIndex = 1;
        skeleton->joints[6].children = { 7 };
        skeleton->joints[6].localTranslate = { 0.8f, 0.6f, 0.0f };

        skeleton->joints[7].name = "Fore.R";
        skeleton->joints[7].parentIndex = 6;
        skeleton->joints[7].localTranslate = { 0.7f, 0.0f, 0.0f };

        UpdateSkeletonWorldTransforms(*skeleton);
        return skeleton;
    }

    std::unique_ptr<Skeleton> MakeChainPreviewSkeleton() {
        auto skeleton = std::make_unique<Skeleton>();
        skeleton->name = "Chain Preview";
        skeleton->joints.resize(5);

        skeleton->joints[0].name = "Root";
        skeleton->joints[0].parentIndex = -1;
        skeleton->joints[0].children = { 1 };
        skeleton->joints[0].localTranslate = { -2.0f, 0.0f, 0.0f };

        skeleton->joints[1].name = "Joint01";
        skeleton->joints[1].parentIndex = 0;
        skeleton->joints[1].children = { 2 };
        skeleton->joints[1].localTranslate = { 0.0f, 1.0f, 0.0f };

        skeleton->joints[2].name = "Joint02";
        skeleton->joints[2].parentIndex = 1;
        skeleton->joints[2].children = { 3 };
        skeleton->joints[2].localTranslate = { 0.6f, 0.8f, 0.0f };

        skeleton->joints[3].name = "Joint03";
        skeleton->joints[3].parentIndex = 2;
        skeleton->joints[3].children = { 4 };
        skeleton->joints[3].localTranslate = { 0.4f, 0.8f, 0.0f };

        skeleton->joints[4].name = "Tip";
        skeleton->joints[4].parentIndex = 3;
        skeleton->joints[4].localTranslate = { 0.2f, 0.6f, 0.0f };

        UpdateSkeletonWorldTransforms(*skeleton);
        return skeleton;
    }
}
