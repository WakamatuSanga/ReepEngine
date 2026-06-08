#include "BlenderLiveSync.h"
#include "BlenderLauncher.h"
#include "BlenderUdpReceiver.h"
#include "Engine/Level/LevelSceneLoader.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <utility>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr uint16_t kDefaultPort = 50000;
    constexpr size_t kJsonPreviewMaxLength = 2048;
}

BlenderLiveSync::BlenderLiveSync() = default;

BlenderLiveSync::~BlenderLiveSync() = default;

void BlenderLiveSync::Initialize(LevelSceneRuntime* levelSceneRuntime) {
    levelSceneRuntime_ = levelSceneRuntime;
    launcher_ = std::make_unique<BlenderLauncher>();
    receiver_ = std::make_unique<BlenderUdpReceiver>();
    loader_ = std::make_unique<LevelSceneLoader>();
    CopyPathToBuffer(blenderExePathBuffer_, launcher_->GetBlenderExePath());
    CopyPathToBuffer(blendFilePathBuffer_, launcher_->GetBlendFilePath());
    CopyPathToBuffer(startupScriptPathBuffer_, launcher_->GetStartupScriptPath());
    lastApplyStatus_ = "Receiver stopped.";
}

void BlenderLiveSync::Update() {
    if (!receiver_) {
        return;
    }

    receiver_->Update();
    if (!receiver_->HasUnreadPacket()) {
        return;
    }

    ParseLatestPacket();
    receiver_->MarkPacketRead();

    if (autoApply_) {
        ApplyPendingScene();
    }
    if (levelSceneRuntime_) {
        levelSceneRuntime_->SetLiveSyncDiagnostics(autoApply_, appliedPacketCount_);
    }
}

void BlenderLiveSync::DrawImGui() {
#ifdef _DEBUG
    if (!receiver_) {
        return;
    }

    const BlenderUdpReceiverStatus& status = receiver_->GetStatus();
    ImGui::SetNextWindowSize(ImVec2(680.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blender Live Sync / ブレンダーライブ同期")) {
        ImGui::End();
        return;
    }

    bool pathChanged = false;
    pathChanged |= ImGui::InputText("Blender Exe Path", blenderExePathBuffer_.data(), blenderExePathBuffer_.size());
    pathChanged |= ImGui::InputText("Blend File Path", blendFilePathBuffer_.data(), blendFilePathBuffer_.size());
    pathChanged |= ImGui::InputText("Startup Script Path", startupScriptPathBuffer_.data(), startupScriptPathBuffer_.size());
    if (pathChanged && launcher_) {
        SyncLauncherPathsFromBuffers();
        launcher_->RefreshDiagnostics();
    }

    if (ImGui::Button("Test Open Blender Only")) {
        TestOpenBlenderOnly();
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Open Blend Only")) {
        TestOpenBlendOnly();
    }
    if (ImGui::Button("BlenderOpen")) {
        OpenBlender();
    }
    const bool blenderRunning = launcher_ && launcher_->IsRunning();
    ImGui::Text("Blender Running: %s", blenderRunning ? "Running" : "Stopped");
    if (launcher_) {
        const BlenderLaunchDiagnostics& diagnostics = launcher_->GetDiagnostics();
        ImGui::TextWrapped(
            "Last Launch Error: %s",
            launcher_->GetLastError().empty() ? "(none)" : launcher_->GetLastError().c_str());
        ImGui::Text("Last Win32 Error Code: %u", diagnostics.win32ErrorCode);
        ImGui::TextWrapped(
            "Last Win32 Error Message: %s",
            diagnostics.win32ErrorMessage.empty() ? "(none)" : diagnostics.win32ErrorMessage.c_str());
        ImGui::TextWrapped(
            "Last Command Line: %s",
            launcher_->GetLastCommandLine().empty() ? "(none)" : launcher_->GetLastCommandLine().c_str());
        ImGui::Text("CreateProcessW Result: %s", diagnostics.createProcessSucceeded ? "success" : "not succeeded / not run");
        ImGui::Text("Blender Exe Exists: %s", diagnostics.blenderExeExists ? "true" : "false");
        ImGui::Text("Blend File Exists: %s", diagnostics.blendFileExists ? "true" : "false");
        ImGui::Text("Startup Script Exists: %s", diagnostics.startupScriptExists ? "true" : "false");
        ImGui::TextWrapped(
            "Resolved Blender Exe Path: %s",
            diagnostics.resolvedBlenderExePath.empty() ? "(none)" : diagnostics.resolvedBlenderExePath.c_str());
        ImGui::TextWrapped(
            "Resolved Blend File Path: %s",
            diagnostics.resolvedBlendFilePath.empty() ? "(none)" : diagnostics.resolvedBlendFilePath.c_str());
        ImGui::TextWrapped(
            "Resolved Startup Script Path: %s",
            diagnostics.resolvedStartupScriptPath.empty() ? "(none)" : diagnostics.resolvedStartupScriptPath.c_str());
        ImGui::TextWrapped(
            "Working Directory: %s",
            diagnostics.workingDirectory.empty() ? "(none)" : diagnostics.workingDirectory.c_str());
        ImGui::TextWrapped(
            "Final Command Line: %s",
            diagnostics.finalCommandLine.empty() ? "(none)" : diagnostics.finalCommandLine.c_str());
    }
    ImGui::Separator();

    ImGui::Text("Receiver Status: %s", status.isRunning ? "Running" : "Stopped");
    if (ImGui::Button("Start Receiver")) {
        if (receiver_->Start(kDefaultPort)) {
            lastApplyStatus_ = "Receiver started.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Receiver")) {
        receiver_->Stop();
        lastApplyStatus_ = "Receiver stopped.";
    }

    ImGui::Checkbox("Auto Apply", &autoApply_);
    if (levelSceneRuntime_) {
        levelSceneRuntime_->SetLiveSyncDiagnostics(autoApply_, appliedPacketCount_);
    }
    if (ImGui::Button("Apply Last Packet")) {
        if (!hasPendingScene_ && !receiver_->GetLastPacket().empty()) {
            ParseLatestPacket();
        }
        ApplyPendingScene();
    }

    const std::string lastError = status.lastError.empty() ? lastError_ : status.lastError;
    ImGui::Separator();
    ImGui::Text("Packet Count: %llu", static_cast<unsigned long long>(status.packetCount));
    ImGui::Text("Last Receive Time: %s", status.lastReceiveTime.empty() ? "(none)" : status.lastReceiveTime.c_str());
    ImGui::Text("Last Packet Size: %zu", status.lastPacketSize);
    ImGui::TextWrapped("Last Error: %s", lastError.empty() ? "(none)" : lastError.c_str());
    ImGui::Text("Received Object Count: %zu", receivedObjectCount_);
    ImGui::Text("Applied Packet Count: %llu", static_cast<unsigned long long>(appliedPacketCount_));
    ImGui::Text("Duplicate Packet Skip Count: %llu", static_cast<unsigned long long>(duplicatePacketSkipCount_));
    ImGui::TextWrapped("Last Apply Status: %s", lastApplyStatus_.c_str());

    ImGui::SeparatorText("Last JSON Preview");
    ImGui::BeginChild("BlenderLiveSyncJsonPreview", ImVec2(0.0f, 150.0f), true);
    ImGui::TextUnformatted(lastJsonPreview_.empty() ? "(none)" : lastJsonPreview_.c_str());
    ImGui::EndChild();

    ImGui::End();
#endif
}

void BlenderLiveSync::OpenBlender() {
    if (!receiver_ || !launcher_) {
        lastApplyStatus_ = "BlenderOpen failed. Live Sync is not initialized.";
        return;
    }

    SyncLauncherPathsFromBuffers();
    autoApply_ = true;

    if (!receiver_->IsRunning() && !receiver_->Start(kDefaultPort)) {
        lastError_ = receiver_->GetStatus().lastError;
        lastApplyStatus_ = "BlenderOpen failed. Receiver could not start.";
        return;
    }

    if (!launcher_->Launch(BlenderLaunchMode::BlendWithStartupScript)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "BlenderOpen failed.";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "BlenderOpen requested. Receiver running and Auto Apply enabled.";
}

void BlenderLiveSync::TestOpenBlenderOnly() {
    if (!launcher_) {
        lastApplyStatus_ = "Test Open Blender Only failed. BlenderLauncher is not initialized.";
        return;
    }

    SyncLauncherPathsFromBuffers();
    if (!launcher_->Launch(BlenderLaunchMode::BlenderOnly)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "Test Open Blender Only failed.";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "Test Open Blender Only requested.";
}

void BlenderLiveSync::TestOpenBlendOnly() {
    if (!launcher_) {
        lastApplyStatus_ = "Test Open Blend Only failed. BlenderLauncher is not initialized.";
        return;
    }

    SyncLauncherPathsFromBuffers();
    if (!launcher_->Launch(BlenderLaunchMode::BlendOnly)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "Test Open Blend Only failed.";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "Test Open Blend Only requested.";
}

void BlenderLiveSync::ParseLatestPacket() {
    if (!receiver_ || !loader_) {
        return;
    }

    const std::string& packet = receiver_->GetLastPacket();
    pendingJsonText_ = packet;
    lastJsonPreview_ = MakeJsonPreview(packet);
    pendingPacketCount_ = receiver_->GetStatus().packetCount;

    LevelSceneData parsedScene;
    LevelSceneLoader::LoadResult result = loader_->LoadFromJsonText(packet, parsedScene, "UDP packet");
    if (!result.success) {
        hasPendingScene_ = false;
        receivedObjectCount_ = 0;
        lastError_ = result.message;
        lastApplyStatus_ = "Received packet, but JSON parse failed.";
        return;
    }

    pendingSceneData_ = std::move(parsedScene);
    hasPendingScene_ = true;
    receivedObjectCount_ = pendingSceneData_.GetObjectCount();
    lastError_.clear();
    lastApplyStatus_ = "Received packet. Waiting for apply.";
}

void BlenderLiveSync::ApplyPendingScene() {
    if (!hasPendingScene_) {
        lastApplyStatus_ = "No valid packet to apply.";
        return;
    }
    if (!levelSceneRuntime_) {
        lastApplyStatus_ = "LevelSceneRuntime is not ready.";
        return;
    }
    if (levelSceneRuntime_->IsLiveApplyPaused()) {
        lastApplyStatus_ = "Live Apply paused by LevelSceneRuntime.";
        levelSceneRuntime_->SetLiveSyncDiagnostics(autoApply_, appliedPacketCount_);
        return;
    }
    if (levelSceneRuntime_->IsRebuildOnlyWhenJsonChangedEnabled() &&
        !lastAppliedJsonText_.empty() &&
        pendingJsonText_ == lastAppliedJsonText_) {
        ++duplicatePacketSkipCount_;
        lastApplyStatus_ = "Skipped duplicate JSON packet.";
        levelSceneRuntime_->SetLiveSyncDiagnostics(autoApply_, appliedPacketCount_);
        return;
    }

    const std::string status =
        "Live Sync applied " + std::to_string(pendingSceneData_.GetObjectCount()) + " objects.";
    levelSceneRuntime_->ApplySceneData(
        pendingSceneData_,
        status,
        "UDP 127.0.0.1:50000 packet=" + std::to_string(pendingPacketCount_));
    lastAppliedJsonText_ = pendingJsonText_;
    appliedPacketCount_ = pendingPacketCount_;
    levelSceneRuntime_->SetLiveSyncDiagnostics(autoApply_, appliedPacketCount_);
    lastApplyStatus_ = status;
}

std::string BlenderLiveSync::MakeJsonPreview(const std::string& jsonText) const {
    if (jsonText.size() <= kJsonPreviewMaxLength) {
        return jsonText;
    }
    return jsonText.substr(0, kJsonPreviewMaxLength) + "\n... truncated ...";
}

void BlenderLiveSync::SyncLauncherPathsFromBuffers() {
    if (!launcher_) {
        return;
    }

    launcher_->SetBlenderExePath(std::string(blenderExePathBuffer_.data()));
    launcher_->SetBlendFilePath(std::string(blendFilePathBuffer_.data()));
    launcher_->SetStartupScriptPath(std::string(startupScriptPathBuffer_.data()));
}

void BlenderLiveSync::CopyPathToBuffer(
    std::array<char, BlenderLiveSync::kPathBufferSize>& buffer,
    const std::string& text) {
    buffer.fill('\0');
    const size_t copyLength = std::min(text.size(), buffer.size() - 1);
    std::copy_n(text.c_str(), copyLength, buffer.data());
}
