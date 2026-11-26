#include "PlayerBulletManager.h"
#include "Engine/Game/GameState/StageCombatState.h"

#include "Engine/Core/GameViewport.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    float Length(const Vector3& value) {
        return std::sqrt(Dot(value, value));
    }

    bool IsFinite(const Vector2& value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    bool TryNormalize(const Vector3& value, Vector3& normalized) {
        const float length = Length(value);
        if (!IsFinite(value) || !std::isfinite(length) || length <= kMinVectorLength) {
            return false;
        }
        normalized = Scale(value, 1.0f / length);
        return IsFinite(normalized);
    }

    Vector3 ResolveSafeForward(
        const Player* player,
        const Camera* camera,
        std::string& sourceLabel) {
        Vector3 direction{};
        if (player && TryNormalize(player->GetBaseForward(), direction)) {
            sourceLabel = "プレイヤー基準前方";
            return direction;
        }
        if (camera) {
            const Matrix4x4& world = camera->GetWorldMatrix();
            if (TryNormalize({ world.m[2][0], world.m[2][1], world.m[2][2] }, direction)) {
                sourceLabel = "カメラ／レール基準前方";
                return direction;
            }
        }
        sourceLabel = "ゲームワールド前方（+Z）";
        return { 0.0f, 0.0f, 1.0f };
    }

    struct ShotAimContext {
        const Vector3& muzzlePosition;
        const Vector3& cameraDirection;
        float aimDistance;
        const Camera* camera;
        const Player* player;
        const GameViewport* viewport;
        const AimCorridorVisualController* corridor;
        bool gameModeActive;
        bool playerAlive;
    };

    struct ShotAimResult {
        Vector3 direction;
        Vector3 point;
        std::string fallbackReason;
    };

    ShotAimResult MakeForwardAim(const ShotAimContext& context, const Vector3& direction) {
        return { direction, Add(context.muzzlePosition, Scale(direction, context.aimDistance)), {} };
    }

    ShotAimResult MakeFallbackAim(const ShotAimContext& context, const Vector3& direction,
        const char* reason, const char* source) {
        auto result = MakeForwardAim(context, direction);
        result.fallbackReason = std::string(reason) + "。" + source + "へフォールバックしました。";
        return result;
    }

    class ShotAim {
    public:
        virtual ~ShotAim() = default;
        virtual ShotAimResult Resolve(const ShotAimContext& context, Vector3& cursorRayDirection) const = 0;
    };

    class CameraForwardAim final : public ShotAim {
    public:
        ShotAimResult Resolve(const ShotAimContext& context, Vector3&) const override {
            return MakeForwardAim(context, context.cameraDirection);
        }
    };

    class CorridorAim final : public ShotAim {
    public:
        ShotAimResult Resolve(const ShotAimContext& context, Vector3&) const override {
            if (!context.corridor) {
                return Fallback(context, "エイムコリドー管理機能がありません");
            }
            if (!context.gameModeActive || !context.playerAlive || !context.corridor->IsGameModeActive()) {
                return Fallback(context, "ゲームモードまたはプレイヤー生存状態が無効です");
            }
            if (!context.corridor->IsMainReticlePresentationValid()) {
                return Fallback(context, "小型長方形照準が表示可能な状態ではありません");
            }

            const Vector3 targetWorldPosition = context.corridor->GetMainReticleWorldCenter();
            const Vector2 targetScreenUv = context.corridor->GetMainReticleScreenUv();
            if (!IsFinite(targetWorldPosition) || !IsFinite(targetScreenUv)) {
                return Fallback(context, "小型長方形照準の座標が非有限です");
            }
            Vector3 direction{};
            if (!TryNormalize(Subtract(targetWorldPosition, context.muzzlePosition), direction)) {
                return Fallback(context, "小型長方形照準がプレイヤー位置に近すぎます");
            }
            return { direction, targetWorldPosition, {} };
        }

    private:
        static ShotAimResult Fallback(const ShotAimContext& context, const char* reason) {
            std::string source;
            const Vector3 direction = ResolveSafeForward(context.player, context.camera, source);
            return MakeFallbackAim(context, direction, reason, source.c_str());
        }
    };

    class MouseAim : public ShotAim {
    public:
        ShotAimResult Resolve(const ShotAimContext& context, Vector3& cursorRayDirection) const override final {
            if (!context.viewport) {
                return Fallback(context, "ゲーム表示領域がありません");
            }
            if (!context.viewport->IsMouseInGameViewport()) {
                return Fallback(context, "マウスがゲーム表示領域外です");
            }
            const auto mouseRay = context.viewport->GetMouseRayFromCamera(*context.camera);
            if (!mouseRay.valid) {
                return Fallback(context, "カーソル光線が無効です");
            }
            cursorRayDirection = mouseRay.direction;
            return ResolveRay(context, mouseRay);
        }

    protected:
        static ShotAimResult Fallback(const ShotAimContext& context, const char* reason) {
            return MakeFallbackAim(context, context.cameraDirection, reason, "カメラ正面");
        }

    private:
        virtual ShotAimResult ResolveRay(const ShotAimContext& context, const GameViewport::Ray& mouseRay) const = 0;
    };

    class MouseRayAim final : public MouseAim {
        ShotAimResult ResolveRay(const ShotAimContext& context, const GameViewport::Ray& mouseRay) const override {
            Vector3 direction{};
            if (!TryNormalize(mouseRay.direction, direction)) {
                return Fallback(context, "カーソル光線方向が無効です");
            }
            return { direction, Add(mouseRay.origin, Scale(direction, context.aimDistance)), {} };
        }
    };

    class MousePlaneAim final : public MouseAim {
        ShotAimResult ResolveRay(const ShotAimContext& context, const GameViewport::Ray& mouseRay) const override {
            const Vector3 planeCenter = Add(
                context.camera->GetTranslate(), Scale(context.cameraDirection, context.aimDistance));
            const float denominator = Dot(mouseRay.direction, context.cameraDirection);
            if (!std::isfinite(denominator) || std::fabs(denominator) <= kMinVectorLength) {
                return Fallback(context, "カーソル光線が照準平面と平行です");
            }

            const float distanceOnRay =
                Dot(Subtract(planeCenter, mouseRay.origin), context.cameraDirection) / denominator;
            if (!std::isfinite(distanceOnRay) || distanceOnRay <= kMinVectorLength) {
                return Fallback(context, "照準平面との交点がカメラ後方です");
            }

            const Vector3 targetWorldPosition = Add(mouseRay.origin, Scale(mouseRay.direction, distanceOnRay));
            Vector3 direction{};
            if (!IsFinite(targetWorldPosition)
                || !TryNormalize(Subtract(targetWorldPosition, context.muzzlePosition), direction)) {
                return Fallback(context, "カーソル照準のワールド座標が無効です");
            }
            return { direction, targetWorldPosition, {} };
        }
    };

    const ShotAim& GetShotAim(PlayerBulletManager::AimMode mode) {
        static const CorridorAim corridor;
        static const CameraForwardAim cameraForward;
        static const MouseRayAim mouseRay;
        static const MousePlaneAim mousePlane;

        switch (mode) {
        case PlayerBulletManager::AimMode::AimCorridor:
            return corridor;
        case PlayerBulletManager::AimMode::CameraForward:
            return cameraForward;
        case PlayerBulletManager::AimMode::MouseRay:
            return mouseRay;
        case PlayerBulletManager::AimMode::MouseAimPlane:
        default:
            return mousePlane;
        }
    }

    const char* ToJapaneseAimMode(PlayerBulletManager::AimMode mode) {
        switch (mode) {
        case PlayerBulletManager::AimMode::AimCorridor:
            return "エイムコリドー";
        case PlayerBulletManager::AimMode::MouseRay:
            return "カーソル照準（光線比較）";
        case PlayerBulletManager::AimMode::MouseAimPlane:
            return "カーソル照準";
        case PlayerBulletManager::AimMode::CameraForward:
        default:
            return "正面射撃";
        }
    }

    const char* ToJapaneseLockState(AimCorridorTargetingController::AimLockState state) {
        switch (state) {
        case AimCorridorTargetingController::AimLockState::Candidate:
            return "候補";
        case AimCorridorTargetingController::AimLockState::Acquiring:
            return "ロック中";
        case AimCorridorTargetingController::AimLockState::Locked:
            return "ロック完了";
        case AimCorridorTargetingController::AimLockState::None:
        default:
            return "なし";
        }
    }

    const char* ToJapaneseBool(bool value) {
        return value ? "はい" : "いいえ";
    }
}

void PlayerBulletManager::SetAimRuntimeContext(
    AimCorridorVisualController* visualController,
    AimCorridorTargetingController* targetingController,
    const PlayerLockOnTargetProvider* targetProvider,
    bool gameModeActive,
    bool playerAlive) {
    aimCorridorVisualController_ = visualController;
    aimCorridorTargetingController_ = targetingController;
    if (targetingController) targetingController->SetLockAmmoAvailable(lockAmmo_ > 0);
    targetProvider_ = targetProvider;

    const bool enteringGameMode =
        (!aimRuntimeStateInitialized_ && gameModeActive)
        || (aimRuntimeStateInitialized_ && !aimGameModeActive_ && gameModeActive);
    const bool respawned =
        aimRuntimeStateInitialized_ && !aimPlayerAlive_ && playerAlive;
    if (respawned) {
        lockAmmo_ = kMaxLockAmmo;
        if (combatState_) combatState_->Reset();
        if (targetingController) targetingController->SetLockAmmoAvailable(true);
    }
    const bool leavingGameMode =
        aimRuntimeStateInitialized_ && aimGameModeActive_ && !gameModeActive;

    aimGameModeActive_ = gameModeActive;
    aimPlayerAlive_ = playerAlive;
    aimRuntimeStateInitialized_ = true;

    if (enteringGameMode || respawned) {
        normalShotInputArmed_ = false;
        missileInputArmed_ = false;
        aimMode_ = kDefaultNormalShotAimMode;
        debugAimModeForced_ = false;
        ResetAimDiagnostics();
        ResetLockedWingShotState(true);
    }
    if (leavingGameMode) {
        normalShotInputArmed_ = false;
        missileInputArmed_ = false;
        ClearLockedWingShotForceState();
        lockedTargetValidation_ = {};
    }
    if (aimGameModeActive_) {
        aimMode_ = kDefaultNormalShotAimMode;
        debugAimModeForced_ = false;
    }
    if (!playerAlive) {
        normalShotInputArmed_ = false;
        missileInputArmed_ = false;
    }
}

void PlayerBulletManager::ClearAimCorridorContext() {
    normalShotInputArmed_ = false;
    missileInputArmed_ = false;
    aimCorridorVisualController_ = nullptr;
    aimCorridorTargetingController_ = nullptr;
    targetProvider_ = nullptr;
    aimRuntimeStateInitialized_ = false;
    aimGameModeActive_ = false;
    aimPlayerAlive_ = true;
    aimMode_ = kDefaultNormalShotAimMode;
    debugAimModeForced_ = false;
    ResetAimDiagnostics();
    ResetLockedWingShotState(true);
}

void PlayerBulletManager::ResetAimDiagnostics() {
    lastAimPoint_ = {};
    lastAimDirection_ = { 0.0f, 0.0f, 1.0f };
    lastMuzzlePosition_ = {};
    lastFirePosition_ = {};
    lastFireDirection_ = { 0.0f, 0.0f, 1.0f };
    lastShotVelocity_ = {};
    lastShotDirectionLength_ = 0.0f;
    lastUsedAimMode_ = aimMode_;
    lastAimFallbackUsed_ = false;
    lastShotSpawnDataValid_ = false;
    lastShotFallbackReason_ = "なし";
    cursorAimVisualUseCount_ = 0;
    cursorAimStandardShotCount_ = 0;
    aimCorridorShotCount_ = 0;
    straightForwardShotCount_ = 0;
}

Vector3 PlayerBulletManager::ResolveAimDirection(
    const Vector3& muzzleBasePosition,
    const Vector3& cameraForward,
    Vector3& cursorRayDirection) {
    Vector3 cameraDirection{ 0.0f, 0.0f, 1.0f };
    TryNormalize(cameraForward, cameraDirection);

    lastUsedAimMode_ = aimMode_;
    lastShotSpawnDataValid_ = false;
    const ShotAimContext context{
        muzzleBasePosition, cameraDirection, aimDistance_, camera_, player_, gameViewport_,
        aimCorridorVisualController_, aimGameModeActive_, aimPlayerAlive_
    };
    auto result = GetShotAim(aimMode_).Resolve(context, cursorRayDirection);
    lastAimDirection_ = result.direction;
    lastAimPoint_ = result.point;
    lastAimFallbackUsed_ = !result.fallbackReason.empty();
    lastShotFallbackReason_ = lastAimFallbackUsed_ ? std::move(result.fallbackReason) : "なし";
    return result.direction;
}

Vector3 PlayerBulletManager::ResolveFinalShotDirection(
    const Vector3& muzzleWorldPosition,
    const Vector3& fallbackDirection) {
    Vector3 safeFallback{};
    if (!TryNormalize(fallbackDirection, safeFallback)) {
        std::string source;
        safeFallback = ResolveSafeForward(player_, camera_, source);
    }

    lastMuzzlePosition_ = muzzleWorldPosition;
    lastShotSpawnDataValid_ = IsFinite(muzzleWorldPosition) && std::isfinite(normalBulletSpeed_);
    if (!lastShotSpawnDataValid_) {
        lastAimFallbackUsed_ = true;
        lastShotFallbackReason_ = "発射口座標または弾速が非有限のため、弾を生成しません。";
        lastAimDirection_ = safeFallback;
        lastShotDirectionLength_ = Length(safeFallback);
        return safeFallback;
    }

    Vector3 shotDirection{};
    if (lastAimFallbackUsed_) {
        const float fallbackTargetDistance = std::isfinite(aimDistance_)
            ? (std::max)(aimDistance_, 1.0f)
            : 1.0f;
        lastAimPoint_ = Add(muzzleWorldPosition, Scale(safeFallback, fallbackTargetDistance));
        shotDirection = safeFallback;
    } else if (!IsFinite(lastAimPoint_)
        || !TryNormalize(Subtract(lastAimPoint_, muzzleWorldPosition), shotDirection)) {
        std::string source;
        safeFallback = ResolveSafeForward(player_, camera_, source);
        lastAimFallbackUsed_ = true;
        lastShotFallbackReason_ =
            "発射口と目標の差が無効です。" + source + "へフォールバックしました。";
        lastAimPoint_ = Add(muzzleWorldPosition, Scale(safeFallback, 1.0f));
        shotDirection = safeFallback;
    }

    lastAimDirection_ = shotDirection;
    lastShotDirectionLength_ = Length(shotDirection);
    return shotDirection;
}

void PlayerBulletManager::RecordAimShot() {
    switch (lastUsedAimMode_) {
    case AimMode::AimCorridor:
        ++aimCorridorShotCount_;
        break;
    case AimMode::CameraForward:
        ++straightForwardShotCount_;
        break;
    case AimMode::MouseRay:
    case AimMode::MouseAimPlane:
        if (aimGameModeActive_) {
            ++cursorAimStandardShotCount_;
        }
        break;
    }
}

void PlayerBulletManager::DrawAimImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("通常射撃の照準");

    int aimModeIndex = 0;
    if (aimMode_ == AimMode::MouseRay || aimMode_ == AimMode::MouseAimPlane) {
        aimModeIndex = 1;
    } else if (aimMode_ == AimMode::CameraForward) {
        aimModeIndex = 2;
    }
    const char* aimModeItems[] = { "エイムコリドー", "カーソル照準", "正面射撃" };
    ImGui::BeginDisabled(aimGameModeActive_);
    if (ImGui::Combo("照準モード##AimMode", &aimModeIndex, aimModeItems, 3)) {
        const AimMode selectedModes[] = {
            AimMode::AimCorridor,
            AimMode::MouseAimPlane,
            AimMode::CameraForward,
        };
        aimMode_ = selectedModes[aimModeIndex];
        debugAimModeForced_ = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "エイムコリドーは、画面に表示されている小型長方形照準の\n"
            "ワールド座標の中心へ通常弾を直進させます。");
    }
    if (ImGui::Button("エイムコリドーを使用##UseAimCorridor")) {
        aimMode_ = AimMode::AimCorridor;
        debugAimModeForced_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("カーソル照準を使用##UseCursorAim")) {
        aimMode_ = AimMode::MouseAimPlane;
        debugAimModeForced_ = true;
    }
    if (ImGui::Button("正面射撃を使用##UseStraightForward")) {
        aimMode_ = AimMode::CameraForward;
        debugAimModeForced_ = true;
    }
    ImGui::EndDisabled();

    if (ImGui::Button("ゲームモード標準へ戻す##ReturnGameModeDefault")) {
        aimMode_ = kDefaultNormalShotAimMode;
        debugAimModeForced_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("照準診断をリセット##ResetAimDiagnostics")) {
        ResetAimDiagnostics();
    }
    if (ImGui::Button("強制設定をすべて解除##ClearForcedAim")) {
        aimMode_ = kDefaultNormalShotAimMode;
        debugAimModeForced_ = false;
        ClearLockedWingShotForceState();
    }

    const bool controllerActive = aimCorridorVisualController_ != nullptr;
    const bool mainReticleVisible = controllerActive
        && aimCorridorVisualController_->IsMainReticlePresentationValid();
    const Vector3 reticleWorldCenter = controllerActive
        ? aimCorridorVisualController_->GetMainReticleWorldCenter()
        : Vector3{};
    const Vector2 reticleScreenUv = controllerActive
        ? aimCorridorVisualController_->GetMainReticleScreenUv()
        : Vector2{};
    const bool mainReticleCenterValid = controllerActive
        && aimGameModeActive_ && aimPlayerAlive_
        && aimCorridorVisualController_->IsGameModeActive()
        && aimCorridorVisualController_->IsMainReticlePresentationValid()
        && IsFinite(reticleWorldCenter) && IsFinite(reticleScreenUv);

    ImGui::Text("現在の照準モード: %s", ToJapaneseAimMode(aimMode_));
    ImGui::Text("ゲームモード標準照準: %s", ToJapaneseAimMode(kDefaultNormalShotAimMode));
    ImGui::Text("ゲームモード実行中: %s", ToJapaneseBool(aimGameModeActive_));
    ImGui::Text("デバッグ強制設定: %s", ToJapaneseBool(debugAimModeForced_));
    ImGui::Text("エイムコリドー管理機能有効: %s", ToJapaneseBool(controllerActive));
    ImGui::Text("小型長方形照準を表示中: %s", ToJapaneseBool(mainReticleVisible));
    ImGui::Text("小型長方形照準の中心有効: %s", ToJapaneseBool(mainReticleCenterValid));
    ImGui::Text("小型長方形照準のワールド中心: %.2f, %.2f, %.2f",
        reticleWorldCenter.x, reticleWorldCenter.y, reticleWorldCenter.z);
    ImGui::Text("小型長方形照準の画面座標: %.3f, %.3f",
        reticleScreenUv.x, reticleScreenUv.y);
    ImGui::Text("最後の発射口ワールド位置: %.2f, %.2f, %.2f",
        lastMuzzlePosition_.x, lastMuzzlePosition_.y, lastMuzzlePosition_.z);
    ImGui::Text("最後の目標ワールド位置: %.2f, %.2f, %.2f",
        lastAimPoint_.x, lastAimPoint_.y, lastAimPoint_.z);
    ImGui::Text("最後の射撃方向: %.3f, %.3f, %.3f",
        lastFireDirection_.x, lastFireDirection_.y, lastFireDirection_.z);
    ImGui::Text("射撃方向の長さ: %.6f", lastShotDirectionLength_);
    ImGui::Text("最後に使用した照準モード: %s", ToJapaneseAimMode(lastUsedAimMode_));
    ImGui::Text("代替方向を使用: %s", ToJapaneseBool(lastAimFallbackUsed_));
    ImGui::TextWrapped("代替方向の理由: %s", lastShotFallbackReason_.c_str());
    ImGui::Text("カーソル照準の描画使用回数: %zu（専用描画なし）", cursorAimVisualUseCount_);
    ImGui::Text("カーソル照準の標準射撃使用回数: %zu", cursorAimStandardShotCount_);
    ImGui::Text("エイムコリドー射撃回数: %zu", aimCorridorShotCount_);
    ImGui::Text("正面射撃回数: %zu", straightForwardShotCount_);

    const AimCorridorTargetingController::AimLockState lockState =
        aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockState()
        : AimCorridorTargetingController::AimLockState::None;
    const std::string lockedTargetId = aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockedTargetId()
        : std::string{};
    ImGui::Text("ロック状態: %s", ToJapaneseLockState(lockState));
    ImGui::TextWrapped("ロック対象の識別子: %s",
        lockedTargetId.empty() ? "なし" : lockedTargetId.c_str());
    ImGui::Text("ロック完了時の発射方式: 対象有効なら左右翼下弾");
    ImGui::Text(
        "翼下弾の追尾: %s",
        ToJapaneseBool(lockedWingHomingEnabled_));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "ロック完了かつ対象が有効な場合は、左右翼下から交互に発射します。\n"
            "翼下弾は巡航へ移行後、発射時に保存した対象へ追尾します。");
    }

    ImGui::Text("マウスがゲーム表示領域内: %s", ToJapaneseBool(mouseInGameView_));
    ImGui::Text("マウス正規化デバイス座標: %.3f, %.3f", mouseNdc_.x, mouseNdc_.y);
    ImGui::Text("マウス正規化座標: %.3f, %.3f", mouseNormalized_.x, mouseNormalized_.y);
    ImGui::DragFloat("照準距離##AimDistance", &aimDistance_, 0.5f, 0.1f, 1000.0f, "%.2f");
    ImGui::DragFloat("発射口オフセット##MuzzleOffset", &muzzleOffset_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::Checkbox("デバッグ照準でカメラ速度を継承##InheritCameraVelocity", &inheritCameraVelocity_);
    ImGui::DragFloat(
        "カメラ速度の継承率##InheritCameraVelocityFactor",
        &inheritCameraVelocityFactor_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Text("カメラ速度: %.2f, %.2f, %.2f",
        cameraVelocity_.x, cameraVelocity_.y, cameraVelocity_.z);
    ImGui::Text("最後の射撃速度: %.2f, %.2f, %.2f",
        lastShotVelocity_.x, lastShotVelocity_.y, lastShotVelocity_.z);
#endif
}
