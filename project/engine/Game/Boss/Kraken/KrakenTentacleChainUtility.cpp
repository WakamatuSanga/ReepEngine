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
