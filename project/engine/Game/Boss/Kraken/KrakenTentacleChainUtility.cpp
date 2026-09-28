#include "Engine/Game/Boss/Kraken/KrakenTentacleChainUtility.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {
    bool Fail(
        std::vector<KrakenTentacleChain>& chains,
        std::string& errorMessage,
        const char* message) {
        chains.clear();
        errorMessage = message;
        return false;
    }
}

bool DetectKrakenTentacleChains(
    const Skeleton& skeleton,
    std::vector<KrakenTentacleChain>& outChains,
    std::string& outErrorMessage) {
    outChains.clear();
    outErrorMessage.clear();
    if (skeleton.root < 0 ||
        skeleton.root >= static_cast<int32_t>(skeleton.joints.size())) {
        return Fail(
            outChains,
            outErrorMessage,
            "ルートジョイントが不正です。");
    }

    const int jointCount = static_cast<int>(skeleton.joints.size());
    const int rootIndex = skeleton.root;
    std::vector<int> incoming(static_cast<std::size_t>(jointCount), 0);
    for (int parentIndex = 0; parentIndex < jointCount; ++parentIndex) {
        const Joint& parent =
            skeleton.joints[static_cast<std::size_t>(parentIndex)];
        for (int childIndex : parent.children) {
            if (childIndex < 0 || childIndex >= jointCount) {
                return Fail(
                    outChains,
                    outErrorMessage,
                    "範囲外の子ジョイントを検出しました。");
            }
            ++incoming[static_cast<std::size_t>(childIndex)];
            if (skeleton.joints[
                static_cast<std::size_t>(childIndex)].parentIndex !=
                parentIndex) {
                return Fail(
                    outChains,
                    outErrorMessage,
                    "親子ジョイントの対応が一致しません。");
            }
        }
    }

    for (int jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        const int expectedIncoming = jointIndex == rootIndex ? 0 : 1;
        if (incoming[static_cast<std::size_t>(jointIndex)] !=
            expectedIncoming) {
            return Fail(
                outChains,
                outErrorMessage,
                jointIndex == rootIndex
                ? "ルートに親参照があります。"
                : "複数の親または親なしジョイントを検出しました。");
        }
    }

    const Joint& root =
        skeleton.joints[static_cast<std::size_t>(rootIndex)];
    if (root.children.empty()) {
        return Fail(
            outChains,
            outErrorMessage,
            "触手チェーンが0本です。");
    }

    std::vector<bool> visited(
        static_cast<std::size_t>(jointCount),
        false);
    visited[static_cast<std::size_t>(rootIndex)] = true;
    for (int startIndex : root.children) {
        KrakenTentacleChain chain{};
        int jointIndex = startIndex;
        while (true) {
            if (jointIndex < 0 ||
                jointIndex >= jointCount ||
                visited[static_cast<std::size_t>(jointIndex)]) {
                return Fail(
                    outChains,
                    outErrorMessage,
                    "循環または重複ジョイントを検出しました。");
            }

            visited[static_cast<std::size_t>(jointIndex)] = true;
            chain.joints.push_back(jointIndex);
            const Joint& joint =
                skeleton.joints[static_cast<std::size_t>(jointIndex)];
            if (joint.children.empty()) {
                break;
            }
            if (joint.children.size() != 1) {
                return Fail(
                    outChains,
                    outErrorMessage,
                    "触手チェーン途中の分岐を検出しました。");
            }
            jointIndex = joint.children.front();
        }
        if (chain.joints.empty()) {
            return Fail(
                outChains,
                outErrorMessage,
                "空の触手チェーンを検出しました。");
        }
        outChains.push_back(std::move(chain));
    }

    if (!std::all_of(
        visited.begin(),
        visited.end(),
        [](bool value) { return value; })) {
        return Fail(
            outChains,
            outErrorMessage,
            "ルート配下でないジョイントを検出しました。");
    }
    return !outChains.empty();
}

bool ApplyKrakenTentaclePlacementToRestoredPose(
    Skeleton& skeleton,
    const std::vector<KrakenTentacleChain>& chains) {
    // Idle screen order is 04, 03, 02, 01. Keep that mapping while attacking.
    // Model +X moves 01 inward; model -Z moves 02/03 away from the Player.
    constexpr const char* rootNames[] = {
        "Tentacle_01_Root", "Tentacle_02_Root",
        "Tentacle_03_Root", "Tentacle_04_Root" };
    const Vector3 offsets[] = {
        {1.75f, 0.0f, 0.0f}, {1.40f, 0.0f, -0.10f},
        {-0.50f, 0.0f, -0.10f}, {-1.10f, 0.0f, 0.0f} };
    int roots[] = {-1, -1, -1, -1};
    if (chains.size() != 4) {
        return false;
    }
    for (const KrakenTentacleChain& chain : chains) {
        if (chain.joints.empty()) {
            return false;
        }
        const int index = chain.joints.front();
        if (index < 0 || static_cast<std::size_t>(index) >= skeleton.joints.size() ||
            skeleton.joints[index].parentIndex != skeleton.root) {
            return false;
        }
        for (int slot = 0; slot < 4; ++slot) {
            if (skeleton.joints[index].name == rootNames[slot]) {
                if (roots[slot] != -1) {
                    return false;
                }
                roots[slot] = index;
            }
        }
    }
    for (int root : roots) {
        if (root == -1) {
            return false;
        }
    }
    for (int slot = 0; slot < 4; ++slot) {
        Vector3& position = skeleton.joints[roots[slot]].localTranslate;
        position.x += offsets[slot].x;
        position.z += offsets[slot].z;
    }
    return true;
}
