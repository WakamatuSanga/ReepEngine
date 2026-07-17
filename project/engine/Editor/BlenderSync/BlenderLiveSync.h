#pragma once
#include "Engine/Level/LevelSceneData.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class BlenderLauncher;
class BlenderUdpReceiver;
class LevelSceneLoader;
class LevelSceneRuntime;

class BlenderLiveSync {
public:
    BlenderLiveSync();
    ~BlenderLiveSync();

    void Initialize(LevelSceneRuntime* levelSceneRuntime);
    void Update();
    void DrawImGui();

private:
    static constexpr size_t kPathBufferSize = 512;

    void OpenBlender();
    void TestOpenBlenderOnly();
    void TestOpenBlendOnly();
    void ParseLatestPacket();
    void ApplyPendingScene();
    void PublishDiagnostics();
    std::string MakeJsonPreview(const std::string& jsonText) const;
    void SyncLauncherPathsFromBuffers();
    void CopyPathToBuffer(std::array<char, kPathBufferSize>& buffer, const std::string& text);

    std::unique_ptr<BlenderLauncher> launcher_;
    std::unique_ptr<BlenderUdpReceiver> receiver_;
    std::unique_ptr<LevelSceneLoader> loader_;
    LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    LevelSceneData pendingSceneData_;
    std::array<char, kPathBufferSize> blenderExePathBuffer_{};
    std::array<char, kPathBufferSize> blendFilePathBuffer_{};
    std::array<char, kPathBufferSize> startupScriptPathBuffer_{};
    bool hasPendingScene_ = false;
    bool autoApply_ = true;
    uint64_t appliedPacketCount_ = 0;
    uint64_t pendingPacketCount_ = 0;
    uint64_t duplicatePacketSkipCount_ = 0;
    size_t receivedObjectCount_ = 0;
    std::string pendingJsonText_;
    std::string lastAppliedJsonText_;
    std::string lastJsonPreview_;
    std::string lastApplyStatus_;
    std::string lastError_;
};
