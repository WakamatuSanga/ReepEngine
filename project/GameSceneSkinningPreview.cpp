#include "Engine/Animation/Skeleton.h"
#include <memory>
#include <utility>

namespace GameSceneInitializeHelpers {
    std::unique_ptr<Skeleton> MakeHumanoidPreviewSkeleton() {
        std::vector<Joint> joints(8);

        joints[0].name = "Root";
        joints[0].parentIndex = -1;
        joints[0].localTranslate = { 2.0f, 0.0f, 0.0f };

        joints[1].name = "Spine";
        joints[1].parentIndex = 0;
        joints[1].localTranslate = { 0.0f, 1.2f, 0.0f };

        joints[2].name = "Chest";
        joints[2].parentIndex = 1;
        joints[2].localTranslate = { 0.0f, 0.9f, 0.0f };

        joints[3].name = "Head";
        joints[3].parentIndex = 2;
        joints[3].localTranslate = { 0.0f, 0.7f, 0.0f };

        joints[4].name = "Arm.L";
        joints[4].parentIndex = 1;
        joints[4].localTranslate = { -0.8f, 0.6f, 0.0f };

        joints[5].name = "Fore.L";
        joints[5].parentIndex = 4;
        joints[5].localTranslate = { -0.7f, 0.0f, 0.0f };

        joints[6].name = "Arm.R";
        joints[6].parentIndex = 1;
        joints[6].localTranslate = { 0.8f, 0.6f, 0.0f };

        joints[7].name = "Fore.R";
        joints[7].parentIndex = 6;
        joints[7].localTranslate = { 0.7f, 0.0f, 0.0f };

        return std::make_unique<Skeleton>("Humanoid Preview", std::move(joints));
    }

    std::unique_ptr<Skeleton> MakeChainPreviewSkeleton() {
        std::vector<Joint> joints(5);

        joints[0].name = "Root";
        joints[0].parentIndex = -1;
        joints[0].localTranslate = { -2.0f, 0.0f, 0.0f };

        joints[1].name = "Joint01";
        joints[1].parentIndex = 0;
        joints[1].localTranslate = { 0.0f, 1.0f, 0.0f };

        joints[2].name = "Joint02";
        joints[2].parentIndex = 1;
        joints[2].localTranslate = { 0.6f, 0.8f, 0.0f };

        joints[3].name = "Joint03";
        joints[3].parentIndex = 2;
        joints[3].localTranslate = { 0.4f, 0.8f, 0.0f };

        joints[4].name = "Tip";
        joints[4].parentIndex = 3;
        joints[4].localTranslate = { 0.2f, 0.6f, 0.0f };

        return std::make_unique<Skeleton>("Chain Preview", std::move(joints));
    }
}
