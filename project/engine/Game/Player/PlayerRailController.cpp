#include "PlayerRailController.h"
#include "Player.h"
#include "Engine/Level/LevelRailRuntime.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinRailLength = 0.0001f;

    float WrapDistance(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        float wrapped = std::fmod(distance, totalLength);
        if (wrapped < 0.0f) {
            wrapped += totalLength;
        }
        return wrapped;
    }

    float ClampDistance(float distance, float totalLength) {
        return std::clamp(distance, 0.0f, (std::max)(0.0f, totalLength));
    }

    float NormalizeT(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }
}

PlayerRailController::PlayerRailController() = default;

PlayerRailController::~PlayerRailController() = default;

void PlayerRailController::Initialize(Player* player, LevelRailRuntime* railRuntime) {
    player_ = player;
    railRuntime_ = railRuntime;
}

void PlayerRailController::Finalize() {
    if (player_) {
        player_->SetBaseMode(Player::BaseMode::CameraFront);
    }
    player_ = nullptr;
    railRuntime_ = nullptr;
}

void PlayerRailController::Update(float deltaTime) {
    railCount_ = railRuntime_ ? railRuntime_->GetRailCount() : 0;
    if (!enableRailControl_ || !player_ || !railRuntime_ || railCount_ == 0) {
        currentEvaluationValid_ = false;
        if (player_) {
            player_->SetBaseMode(Player::BaseMode::CameraFront);
        }
        return;
    }

    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(railCount_) - 1));
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        player_->SetBaseMode(Player::BaseMode::CameraFront);
        return;
    }

    SyncSelectedRailDefaults();
    UpdateEvaluation(deltaTime);
    ApplyToPlayer();
}

void PlayerRailController::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(420.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("プレイヤーレール確認 / Debug")) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Rail評価確認用です。本番のレールシューティング制御ではありません。");
    ImGui::Checkbox("Rail制御有効 (Enable Rail Control)", &enableRailControl_);
    ImGui::Checkbox("Auto Play", &autoPlay_);
    ImGui::Checkbox("ループ (Loop)", &debugLoop_);
    ImGui::Text("Rail Count: %zu", railCount_);

    if (!railRuntime_ || railCount_ == 0) {
        ImGui::TextDisabled("レールがありません。 (No rails.)");
        ImGui::Text("Current Evaluation Valid: %s", currentEvaluationValid_ ? "true" : "false");
        ImGui::End();
        return;
    }

    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(railCount_) - 1));
    LevelRailRuntimeRailInfo selectedInfo;
    const bool hasSelectedInfo = railRuntime_->GetRailInfo(static_cast<size_t>(selectedRailIndex_), selectedInfo);
    const std::string currentLabel = hasSelectedInfo
        ? ((selectedInfo.name.empty() ? selectedInfo.railId : selectedInfo.name) + "##player_rail_current")
        : "(none)";
    if (ImGui::BeginCombo("選択中レール (Selected Rail)", currentLabel.c_str())) {
        for (size_t index = 0; index < railCount_; ++index) {
            LevelRailRuntimeRailInfo info;
            if (!railRuntime_->GetRailInfo(index, info)) {
                continue;
            }
            const std::string label =
                (info.name.empty() ? info.railId : info.name) +
                " [" + std::to_string(info.pointCount) + " pts]##player_rail_" + std::to_string(index);
            const bool selected = selectedRailIndex_ == static_cast<int>(index);
            if (ImGui::Selectable(label.c_str(), selected)) {
                selectedRailIndex_ = static_cast<int>(index);
                previousRailId_.clear();
                FetchSelectedRailInfo();
                SyncSelectedRailDefaults();
                UpdateEvaluation(0.0f);
                ApplyToPlayer();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    FetchSelectedRailInfo();
    const char* moveModeNames[] = { "T", "Distance" };
    ImGui::Combo("Move Mode", &moveMode_, moveModeNames, 2);
    if (moveMode_ == 0) {
        if (ImGui::SliderFloat("Rail T", &railT_, 0.0f, 1.0f, "%.3f")) {
            railDistance_ = railT_ * selectedRailTotalLength_;
            UpdateEvaluation(0.0f);
            ApplyToPlayer();
        }
    } else {
        const float maxDistance = (std::max)(selectedRailTotalLength_, 0.001f);
        if (ImGui::SliderFloat("Rail Distance", &railDistance_, 0.0f, maxDistance, "%.3f")) {
            railDistance_ = ClampDistance(railDistance_, selectedRailTotalLength_);
            railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);
            UpdateEvaluation(0.0f);
            ApplyToPlayer();
        }
    }
    ImGui::DragFloat("Play Speed", &playSpeed_, 0.05f, -50.0f, 50.0f, "%.2f");
    if (ImGui::Button("Reset Rail Position")) {
        ResetRailPosition();
        ApplyToPlayer();
    }

    ImGui::SeparatorText("Rail State");
    ImGui::Text("Selected Rail ID: %s", selectedRailId_.empty() ? "(none)" : selectedRailId_.c_str());
    ImGui::Text("Selected Rail Type: %s", selectedRailType_.empty() ? "(none)" : selectedRailType_.c_str());
    ImGui::Text("Selected Rail Point Count: %zu", selectedRailPointCount_);
    ImGui::Text("Selected Rail Total Length: %.3f", selectedRailTotalLength_);
    ImGui::Text("Rail Loop: %s", selectedRailLoop_ ? "true" : "false");
    ImGui::Text("Current Segment Index: %zu", currentSegmentIndex_);
    ImGui::Text("Current Rail Position: %.3f, %.3f, %.3f",
        currentRailPosition_.x,
        currentRailPosition_.y,
        currentRailPosition_.z);
    ImGui::Text("Current Rail Forward: %.3f, %.3f, %.3f",
        currentRailForward_.x,
        currentRailForward_.y,
        currentRailForward_.z);
    ImGui::Text("Current Evaluation Valid: %s", currentEvaluationValid_ ? "true" : "false");

    if (player_) {
        const Vector3& basePosition = player_->GetBasePosition();
        const Vector3& worldPosition = player_->GetWorldPosition();
        ImGui::SeparatorText("Player State");
        ImGui::Text("Player Base Position: %.3f, %.3f, %.3f", basePosition.x, basePosition.y, basePosition.z);
        ImGui::Text("Player Local Offset: %.3f, %.3f", player_->GetLocalOffsetX(), player_->GetLocalOffsetY());
        ImGui::Text("Player World Position: %.3f, %.3f, %.3f", worldPosition.x, worldPosition.y, worldPosition.z);
        ImGui::Text("Player Base Mode: %s", player_->GetBaseMode() == Player::BaseMode::Rail ? "Rail" : "CameraFront");
    }
    ImGui::End();
#endif
}

void PlayerRailController::SyncSelectedRailDefaults() {
    if (selectedRailId_ == previousRailId_) {
        return;
    }

    railT_ = 0.0f;
    railDistance_ = 0.0f;
    debugLoop_ = selectedRailLoop_;
    autoPlay_ = false;
    previousRailId_ = selectedRailId_;
}

bool PlayerRailController::FetchSelectedRailInfo() {
    if (!railRuntime_ || railCount_ == 0) {
        return false;
    }

    LevelRailRuntimeRailInfo info;
    if (!railRuntime_->GetRailInfo(static_cast<size_t>(selectedRailIndex_), info)) {
        return false;
    }

    selectedRailId_ = info.railId;
    selectedRailName_ = info.name;
    selectedRailType_ = info.railType;
    selectedRailLoop_ = info.loop;
    selectedRailPointCount_ = info.pointCount;
    selectedRailTotalLength_ = info.totalLength;
    if (previousRailId_ != selectedRailId_) {
        playSpeed_ = info.speed;
    }
    return true;
}

void PlayerRailController::UpdateEvaluation(float deltaTime) {
    const bool shouldLoop = debugLoop_ || selectedRailLoop_;
    if (autoPlay_ && selectedRailTotalLength_ > kMinRailLength) {
        railDistance_ += deltaTime * playSpeed_;
        if (shouldLoop) {
            railDistance_ = WrapDistance(railDistance_, selectedRailTotalLength_);
        } else {
            const float clampedDistance = ClampDistance(railDistance_, selectedRailTotalLength_);
            if (clampedDistance != railDistance_) {
                railDistance_ = clampedDistance;
                autoPlay_ = false;
            }
        }
        railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);
    }

    if (moveMode_ == 0) {
        railT_ = std::clamp(railT_, 0.0f, 1.0f);
        railDistance_ = railT_ * selectedRailTotalLength_;
    } else {
        railDistance_ = shouldLoop
            ? WrapDistance(railDistance_, selectedRailTotalLength_)
            : ClampDistance(railDistance_, selectedRailTotalLength_);
        railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);
    }

    const LevelRailEvaluation evaluation = railRuntime_
        ? railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, shouldLoop)
        : LevelRailEvaluation{};
    currentEvaluationValid_ = evaluation.valid;
    if (!evaluation.valid) {
        return;
    }

    currentRailPosition_ = evaluation.position;
    currentRailForward_ = evaluation.forward;
    currentSegmentIndex_ = evaluation.segmentIndex;
    railDistance_ = evaluation.distance;
    railT_ = evaluation.t;
}

void PlayerRailController::ApplyToPlayer() {
    if (!player_) {
        return;
    }

    if (!enableRailControl_ || !currentEvaluationValid_) {
        player_->SetBaseMode(Player::BaseMode::CameraFront);
        return;
    }

    player_->SetBaseMode(Player::BaseMode::Rail);
    player_->SetExternalBasePosition(currentRailPosition_);
    player_->SetExternalBaseForward(currentRailForward_);
    player_->SetExternalBaseUp({ 0.0f, 1.0f, 0.0f });
}

void PlayerRailController::ResetRailPosition() {
    railT_ = 0.0f;
    railDistance_ = 0.0f;
    autoPlay_ = false;
    UpdateEvaluation(0.0f);
}
