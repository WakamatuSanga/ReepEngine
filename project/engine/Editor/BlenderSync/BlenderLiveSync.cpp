#include "BlenderLiveSync.h"
#include "BlenderLauncher.h"
#include "BlenderUdpReceiver.h"
#include "Engine/Level/LevelSceneLoader.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
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
    lastApplyStatus_ = "受信停止中です。 (Receiver stopped.)";
    PublishDiagnostics();
}

void BlenderLiveSync::Update() {
    if (!receiver_) {
        return;
    }

    receiver_->Update();
    PublishDiagnostics();
    if (!receiver_->HasUnreadPacket()) {
        return;
    }

    ParseLatestPacket();
    receiver_->MarkPacketRead();

    if (autoApply_) {
        ApplyPendingScene();
    }
    PublishDiagnostics();
}

void BlenderLiveSync::DrawImGui() {
#ifdef USE_IMGUI
    if (!receiver_) {
        return;
    }

    const BlenderUdpReceiverStatus& status = receiver_->GetStatus();
    ImGui::SetNextWindowSize(ImVec2(680.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ブレンダーライブ同期 (Blender Live Sync)")) {
        ImGui::End();
        return;
    }

    bool pathChanged = false;
    pathChanged |= ImGui::InputText("ブレンダー実行ファイルパス (Blender Exe Path)", blenderExePathBuffer_.data(), blenderExePathBuffer_.size());
    pathChanged |= ImGui::InputText("ブレンドファイルパス (Blend File Path)", blendFilePathBuffer_.data(), blendFilePathBuffer_.size());
    pathChanged |= ImGui::InputText("起動スクリプトパス (Startup Script Path)", startupScriptPathBuffer_.data(), startupScriptPathBuffer_.size());
    if (pathChanged && launcher_) {
        SyncLauncherPathsFromBuffers();
        launcher_->RefreshDiagnostics();
    }

    if (ImGui::Button("Blender単体起動テスト (Test Open Blender Only)")) {
        TestOpenBlenderOnly();
    }
    ImGui::SameLine();
    if (ImGui::Button("Blendファイル起動テスト (Test Open Blend Only)")) {
        TestOpenBlendOnly();
    }
    const bool blenderExeMissing = !launcher_ || !launcher_->GetDiagnostics().blenderExeExists;
    const ImVec4 openButton = blenderExeMissing
        ? ImVec4(0.80f, 0.10f, 0.08f, 1.0f)
        : ImVec4(0.95f, 0.35f, 0.05f, 1.0f);
    const ImVec4 openButtonHovered = blenderExeMissing
        ? ImVec4(1.00f, 0.15f, 0.10f, 1.0f)
        : ImVec4(1.00f, 0.45f, 0.10f, 1.0f);
    const ImVec4 openButtonActive = blenderExeMissing
        ? ImVec4(0.60f, 0.05f, 0.04f, 1.0f)
        : ImVec4(0.85f, 0.20f, 0.05f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, openButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, openButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, openButtonActive);
    const bool openBlenderClicked = ImGui::Button("BlenderOpen");
    ImGui::PopStyleColor(3);
    if (openBlenderClicked) {
        OpenBlender();
    }
    const bool blenderRunning = launcher_ && launcher_->IsRunning();
    ImGui::Text("Blender起動状態 (Blender Running): %s", blenderRunning ? "Running" : "Stopped");
    if (launcher_) {
        const BlenderLaunchDiagnostics& diagnostics = launcher_->GetDiagnostics();
        ImGui::TextWrapped(
            "最後の起動エラー (Last Launch Error): %s",
            launcher_->GetLastError().empty() ? "(none)" : launcher_->GetLastError().c_str());
        ImGui::Text("Win32エラーコード (Last Win32 Error Code): %u", diagnostics.win32ErrorCode);
        ImGui::TextWrapped(
            "Win32エラーメッセージ (Last Win32 Error Message): %s",
            diagnostics.win32ErrorMessage.empty() ? "(none)" : diagnostics.win32ErrorMessage.c_str());
        ImGui::TextWrapped(
            "起動コマンド (Command Line): %s",
            launcher_->GetLastCommandLine().empty() ? "(none)" : launcher_->GetLastCommandLine().c_str());
        ImGui::Text("CreateProcessW結果 (CreateProcessW Result): %s", diagnostics.createProcessSucceeded ? "success" : "not succeeded / not run");
        ImGui::Text("ブレンダー実行ファイル存在 (Blender Exe Exists): %s", diagnostics.blenderExeExists ? "true" : "false");
        ImGui::Text("ブレンドファイル存在 (Blend File Exists): %s", diagnostics.blendFileExists ? "true" : "false");
        ImGui::Text("起動スクリプト存在 (Startup Script Exists): %s", diagnostics.startupScriptExists ? "true" : "false");
        ImGui::TextWrapped(
            "解決済みブレンダー実行ファイルパス (Resolved Blender Exe Path): %s",
            diagnostics.resolvedBlenderExePath.empty() ? "(none)" : diagnostics.resolvedBlenderExePath.c_str());
        ImGui::TextWrapped(
            "解決済みブレンドファイルパス (Resolved Blend File Path): %s",
            diagnostics.resolvedBlendFilePath.empty() ? "(none)" : diagnostics.resolvedBlendFilePath.c_str());
        ImGui::TextWrapped(
            "解決済み起動スクリプトパス (Resolved Startup Script Path): %s",
            diagnostics.resolvedStartupScriptPath.empty() ? "(none)" : diagnostics.resolvedStartupScriptPath.c_str());
        ImGui::TextWrapped(
            "作業ディレクトリ (Working Directory): %s",
            diagnostics.workingDirectory.empty() ? "(none)" : diagnostics.workingDirectory.c_str());
        ImGui::TextWrapped(
            "最終起動コマンド (Final Command Line): %s",
            diagnostics.finalCommandLine.empty() ? "(none)" : diagnostics.finalCommandLine.c_str());
    }
    ImGui::Separator();

    ImGui::Text("受信状態 (Receiver Status): %s", status.isRunning ? "Running" : "Stopped");
    if (ImGui::Button("受信開始 (Start Receiver)")) {
        if (receiver_->Start(kDefaultPort)) {
            lastApplyStatus_ = "受信を開始しました。 (Receiver started.)";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("受信停止 (Stop Receiver)")) {
        receiver_->Stop();
        lastApplyStatus_ = "受信を停止しました。 (Receiver stopped.)";
    }

    ImGui::Checkbox("自動反映 (Auto Apply)", &autoApply_);
    PublishDiagnostics();
    if (ImGui::Button("最後の受信データを反映 (Apply Last Packet)")) {
        if (!hasPendingScene_ && !receiver_->GetLastPacket().empty()) {
            ParseLatestPacket();
        }
        ApplyPendingScene();
    }

    const std::string lastError = status.lastError.empty() ? lastError_ : status.lastError;
    ImGui::Separator();
    ImGui::Text("受信パケット数 (Packet Count): %llu", static_cast<unsigned long long>(status.packetCount));
    ImGui::Text("最後の受信時刻 (Last Receive Time): %s", status.lastReceiveTime.empty() ? "(none)" : status.lastReceiveTime.c_str());
    ImGui::Text("最後のパケットサイズ (Last Packet Size): %zu", status.lastPacketSize);
    ImGui::TextWrapped("最後のエラー (Last Error): %s", lastError.empty() ? "(none)" : lastError.c_str());
    ImGui::Text("受信オブジェクト数 (Received Object Count): %zu", receivedObjectCount_);
    ImGui::Text("反映済みパケット数 (Applied Packet Count): %llu", static_cast<unsigned long long>(appliedPacketCount_));
    ImGui::Text("重複パケットスキップ数 (Duplicate Packet Skip Count): %llu", static_cast<unsigned long long>(duplicatePacketSkipCount_));
    ImGui::TextWrapped("最後の反映状態 (Last Apply Status): %s", lastApplyStatus_.c_str());

    ImGui::SeparatorText("最後のJSONプレビュー (Last JSON Preview)");
    ImGui::BeginChild("BlenderLiveSyncJsonPreview", ImVec2(0.0f, 150.0f), true);
    ImGui::TextUnformatted(lastJsonPreview_.empty() ? "(none)" : lastJsonPreview_.c_str());
    ImGui::EndChild();

    ImGui::End();
#endif
}

void BlenderLiveSync::OpenBlender() {
    if (!receiver_ || !launcher_) {
        lastApplyStatus_ = "BlenderOpenに失敗しました。Live Syncが初期化されていません。 (Live Sync is not initialized.)";
        return;
    }

    SyncLauncherPathsFromBuffers();
    autoApply_ = true;

    if (!receiver_->IsRunning() && !receiver_->Start(kDefaultPort)) {
        lastError_ = receiver_->GetStatus().lastError;
        lastApplyStatus_ = "BlenderOpenに失敗しました。受信を開始できません。 (Receiver could not start.)";
        return;
    }

    if (!launcher_->Launch(BlenderLaunchMode::BlendWithStartupScript)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "BlenderOpenに失敗しました。 (BlenderOpen failed.)";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "BlenderOpenを実行しました。受信中 / 自動反映ONです。 (Receiver running and Auto Apply enabled.)";
}

void BlenderLiveSync::TestOpenBlenderOnly() {
    if (!launcher_) {
        lastApplyStatus_ = "Blender単体起動テストに失敗しました。BlenderLauncherが初期化されていません。";
        return;
    }

    SyncLauncherPathsFromBuffers();
    if (!launcher_->Launch(BlenderLaunchMode::BlenderOnly)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "Blender単体起動テストに失敗しました。 (Test Open Blender Only failed.)";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "Blender単体起動テストを実行しました。 (Test Open Blender Only requested.)";
}

void BlenderLiveSync::TestOpenBlendOnly() {
    if (!launcher_) {
        lastApplyStatus_ = "Blendファイル起動テストに失敗しました。BlenderLauncherが初期化されていません。";
        return;
    }

    SyncLauncherPathsFromBuffers();
    if (!launcher_->Launch(BlenderLaunchMode::BlendOnly)) {
        lastError_ = launcher_->GetLastError();
        lastApplyStatus_ = "Blendファイル起動テストに失敗しました。 (Test Open Blend Only failed.)";
        return;
    }

    lastError_.clear();
    lastApplyStatus_ = "Blendファイル起動テストを実行しました。 (Test Open Blend Only requested.)";
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
        lastApplyStatus_ = "受信しましたがJSON解析に失敗しました。 (JSON parse failed.)";
        return;
    }

    pendingSceneData_ = std::move(parsedScene);
    hasPendingScene_ = true;
    receivedObjectCount_ = pendingSceneData_.GetObjectCount();
    lastError_.clear();
    lastApplyStatus_ = "受信しました。反映待ちです。 (Waiting for apply.)";
}

void BlenderLiveSync::ApplyPendingScene() {
    if (!hasPendingScene_) {
        lastApplyStatus_ = "反映できる有効なパケットがありません。 (No valid packet to apply.)";
        return;
    }
    if (!levelSceneRuntime_) {
        lastApplyStatus_ = "LevelSceneRuntimeの準備ができていません。";
        return;
    }
    if (levelSceneRuntime_->IsLiveApplyPaused()) {
        lastApplyStatus_ = "ライブ反映は一時停止中です。 (Live Apply paused.)";
        PublishDiagnostics();
        return;
    }
    if (levelSceneRuntime_->IsRebuildOnlyWhenJsonChangedEnabled() &&
        !lastAppliedJsonText_.empty() &&
        pendingJsonText_ == lastAppliedJsonText_) {
        ++duplicatePacketSkipCount_;
        lastApplyStatus_ = "同一JSONパケットのためスキップしました。 (Skipped duplicate JSON packet.)";
        PublishDiagnostics();
        return;
    }

    const std::string status =
        "Live Syncを反映しました: " + std::to_string(pendingSceneData_.GetObjectCount()) + " objects.";
    levelSceneRuntime_->ApplySceneData(
        pendingSceneData_,
        status,
        "UDP 127.0.0.1:50000 packet=" + std::to_string(pendingPacketCount_));
    lastAppliedJsonText_ = pendingJsonText_;
    appliedPacketCount_ = pendingPacketCount_;
    lastApplyStatus_ = status;
    PublishDiagnostics();
}

void BlenderLiveSync::PublishDiagnostics() {
    if (!levelSceneRuntime_) return;
    bool running = false;
    uint64_t receivedPacketCount = 0;
    std::string lastReceiveTime;
    std::string receiverError;
    if (receiver_) {
        const BlenderUdpReceiverStatus& status = receiver_->GetStatus();
        running = status.isRunning;
        receivedPacketCount = status.packetCount;
        lastReceiveTime = status.lastReceiveTime;
        receiverError = status.lastError;
    }
    levelSceneRuntime_->SetLiveSyncDiagnostics(
        autoApply_, appliedPacketCount_, running, receivedPacketCount, lastReceiveTime,
        lastApplyStatus_, receiverError.empty() ? lastError_ : receiverError);
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

