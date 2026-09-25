#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Player/Player.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace {
    constexpr float kEpsilon = 0.00001f;

    Vector3 Add(const Vector3& a, const Vector3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }
    Vector3 Subtract(const Vector3& a, const Vector3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }
    float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float LengthSquared(const Vector3& value) {
        return Dot(value, value);
    }
    float Length(const Vector3& value) {
        return std::sqrt(LengthSquared(value));
    }

    float Distance(const Vector3& a, const Vector3& b) {
        return Length(Subtract(a, b));
    }
    Vector3 Normalize(const Vector3& value, const Vector3& fallback = {}) {
        const float length = Length(value);
        return std::isfinite(length) && length > kEpsilon
            ? Scale(value, 1.0f / length) : fallback;
    }

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    Vector3 TransformPosition(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
        };
    }

    bool MatricesMatch(const Matrix4x4& a, const Matrix4x4& b) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (std::fabs(a.m[row][column] - b.m[row][column]) >
                    kEpsilon) {
                    return false;
                }
            }
        }
        return true;
    }

    bool VectorsMatch(const Vector3& a, const Vector3& b) {
        return std::fabs(a.x - b.x) <= kEpsilon &&
            std::fabs(a.y - b.y) <= kEpsilon &&
            std::fabs(a.z - b.z) <= kEpsilon;
    }

    Vector3 ClosestPointOnSegment(
        const Vector3& point, const Vector3& start, const Vector3& end) {
        const Vector3 segment = Subtract(end, start);
        const float lengthSquared = LengthSquared(segment);
        if (!std::isfinite(lengthSquared) || lengthSquared <= kEpsilon) {
            return start;
        }
        const float t = std::clamp(
            Dot(Subtract(point, start), segment) / lengthSquared,
            0.0f, 1.0f);
        return Add(start, Scale(segment, t));
    }

    struct RegionDistanceResult {
        Vector3 playerPosition{};
        float distance = std::numeric_limits<float>::infinity();
        bool valid = false;
    };

    Vector3 PointInPlayerRegion(
        const Vector3& center, const Vector3& right, const Vector3& up,
        float limitX, float limitY, const Vector3& point) {
        const Vector3 offset = Subtract(point, center);
        const float x = std::clamp(Dot(offset, right), -limitX, limitX);
        const float y = std::clamp(Dot(offset, up), -limitY, limitY);
        return Add(Add(center, Scale(right, x)), Scale(up, y));
    }

    RegionDistanceResult ClosestSegmentToPlayerRegion(
        const Vector3& start, const Vector3& end, const Vector3& center,
        const Vector3& right, const Vector3& up,
        float limitX, float limitY) {
        RegionDistanceResult result{};
        const Vector3 segment = Subtract(end, start);
        std::array<float, 6> breaks{ 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        std::size_t breakCount = 2;
        const Vector3 startOffset = Subtract(start, center);
        const float starts[2] = { Dot(startOffset, right), Dot(startOffset, up) };
        const float slopes[2] = { Dot(segment, right), Dot(segment, up) };
        const float limits[2] = { limitX, limitY };
        for (int axis = 0; axis < 2; ++axis) {
            if (std::fabs(slopes[axis]) <= kEpsilon) {
                continue;
            }
            for (float sign : { -1.0f, 1.0f }) {
                const float t = (sign * limits[axis] - starts[axis]) /
                    slopes[axis];
                if (t > 0.0f && t < 1.0f && breakCount < breaks.size()) {
                    breaks[breakCount++] = t;
                }
            }
        }
        std::sort(breaks.begin(), breaks.begin() + breakCount);
        breakCount = static_cast<std::size_t>(std::unique(
            breaks.begin(), breaks.begin() + breakCount,
            [](float a, float b) { return std::fabs(a - b) < kEpsilon; }) -
            breaks.begin());

        const auto evaluate = [&](float t) {
            const Vector3 segmentPoint = Add(start, Scale(segment, t));
            const Vector3 playerPoint = PointInPlayerRegion(
                center, right, up, limitX, limitY, segmentPoint);
            const float distance = Distance(segmentPoint, playerPoint);
            if (std::isfinite(distance) && distance < result.distance) {
                result.playerPosition = playerPoint;
                result.distance = distance;
                result.valid = true;
            }
        };

        for (std::size_t interval = 0; interval + 1 < breakCount; ++interval) {
            const float begin = breaks[interval];
            const float endT = breaks[interval + 1];
            evaluate(begin);
            evaluate(endT);
            const float middle = (begin + endT) * 0.5f;
            const Vector3 point = Add(start, Scale(segment, middle));
            const Vector3 closest = PointInPlayerRegion(
                center, right, up, limitX, limitY, point);
            Vector3 velocity = segment;
            const Vector3 offset = Subtract(point, center);
            if (std::fabs(Dot(offset, right)) < limitX - kEpsilon) {
                velocity = Subtract(
                    velocity, Scale(right, Dot(segment, right)));
            }
            if (std::fabs(Dot(offset, up)) < limitY - kEpsilon) {
                velocity = Subtract(
                    velocity, Scale(up, Dot(segment, up)));
            }
            const float velocityLengthSquared = LengthSquared(velocity);
            if (velocityLengthSquared > kEpsilon) {
                const float candidate = std::clamp(
                    middle - Dot(Subtract(point, closest), velocity) /
                        velocityLengthSquared,
                    begin, endT);
                evaluate(candidate);
            }
        }
        return result;
    }

    RegionDistanceResult FarthestPlayerRegionPoint(
        const Vector3& start, const Vector3& end, const Vector3& center,
        const Vector3& right, const Vector3& up,
        float limitX, float limitY) {
        RegionDistanceResult result{};
        result.distance = -1.0f;
        for (float x : { -limitX, limitX }) {
            for (float y : { -limitY, limitY }) {
                const Vector3 point = Add(
                    Add(center, Scale(right, x)), Scale(up, y));
                const float distance = Distance(
                    point, ClosestPointOnSegment(point, start, end));
                if (std::isfinite(distance) && distance > result.distance) {
                    result.playerPosition = point;
                    result.distance = distance;
                    result.valid = true;
                }
            }
        }
        return result;
    }

    const char* SampleLabel(KrakenTentacleReachPoseSample sample) {
        switch (sample) {
        case KrakenTentacleReachPoseSample::Bind: return "バインド";
        case KrakenTentacleReachPoseSample::Windup: return "振りかぶり終端";
        case KrakenTentacleReachPoseSample::SlamStart: return "叩きつけ 0.00";
        case KrakenTentacleReachPoseSample::SlamThreeQuarter: return "叩きつけ 0.75";
        case KrakenTentacleReachPoseSample::SlamThreshold: return "叩きつけ 0.65";
        case KrakenTentacleReachPoseSample::SlamEnd: return "叩きつけ 1.00";
        case KrakenTentacleReachPoseSample::ImpactHold: return "衝撃保持";
        case KrakenTentacleReachPoseSample::Recovery: return "復帰中間";
        case KrakenTentacleReachPoseSample::Count:
        default: return "不明";
        }
    }
}

void KrakenTentacleMidbossController::Impl::
ApplyRecommendedAttackPoseSettings() {
    attackSettings = {};
}

void KrakenTentacleMidbossController::Impl::ResetAttackReachDiagnostics() {
    placementDiagnostics.attackReachChainMask = 0;
    attackReachDiagnostics = {};
}

void KrakenTentacleMidbossController::Impl::RefreshAttackReachDiagnostics() {
    if (!skeleton || bindPose.size() != skeleton->joints.size() ||
        !camera || !collisionPlayer ||
        chains.size() != kKrakenTentacleReachChainCount ||
        colliderDefinitions.size() != chains.size()) {
        attackReachDiagnostics.playerMovementRegionValid = false;
        return;
    }
    const Matrix4x4& cameraWorld = camera->GetWorldMatrix();
    const Vector3 right = Normalize({
        cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
    const Vector3 up = Normalize({
        cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    const Vector3 center = collisionPlayer->GetBasePosition();
    const float limitX = collisionPlayer->GetMoveLimitX();
    const float limitY = collisionPlayer->GetMoveLimitY();
    if (!IsFinite(right) || !IsFinite(up) || !IsFinite(center) ||
        !std::isfinite(limitX) || !std::isfinite(limitY) ||
        limitX <= 0.0f || limitY <= 0.0f) {
        attackReachDiagnostics.playerMovementRegionValid = false;
        return;
    }

    attackReachDiagnostics.cameraRight = right;
    attackReachDiagnostics.cameraUp = up;
    attackReachDiagnostics.playerMovementCenter = center;
    attackReachDiagnostics.playerMoveLimitX = limitX;
    attackReachDiagnostics.playerMoveLimitY = limitY;
    attackReachDiagnostics.playerRadius = collisionPlayer->GetHitRadius();
    attackReachDiagnostics.playerMovementLeft = Add(center, Scale(right, -limitX));
    attackReachDiagnostics.playerMovementRight = Add(center, Scale(right, limitX));
    attackReachDiagnostics.playerMovementTop = Add(center, Scale(up, limitY));
    attackReachDiagnostics.playerMovementBottom = Add(center, Scale(up, -limitY));
    Vector3 minimum = center;
    Vector3 maximum = center;
    for (float x : { -limitX, limitX }) {
        for (float y : { -limitY, limitY }) {
            const Vector3 corner = Add(Add(center, Scale(right, x)), Scale(up, y));
            minimum.x = (std::min)(minimum.x, corner.x);
            minimum.y = (std::min)(minimum.y, corner.y);
            minimum.z = (std::min)(minimum.z, corner.z);
            maximum.x = (std::max)(maximum.x, corner.x);
            maximum.y = (std::max)(maximum.y, corner.y);
            maximum.z = (std::max)(maximum.z, corner.z);
        }
    }
    attackReachDiagnostics.playerMovementMinimum = minimum;
    attackReachDiagnostics.playerMovementMaximum = maximum;
    attackReachDiagnostics.playerMovementRegionValid = true;

    const KrakenTentacleAttackSettings sanitized =
        SanitizeKrakenTentacleAttackSettings(attackSettings);
    const bool predictionChanged = !attackReachDiagnostics.predictionValid ||
        !MatricesMatch(attackReachDiagnostics.predictionWorldMatrix, worldMatrix) ||
        !VectorsMatch(attackReachDiagnostics.predictedAttackTargetPosition, center) ||
        sanitized.primarySign != attackReachDiagnostics.predictedPrimarySign ||
        sanitized.secondarySign != attackReachDiagnostics.predictedSecondarySign ||
        sanitized.slamPrimaryTotalDegrees !=
            attackReachDiagnostics.predictedSlamPrimaryDegrees ||
        sanitized.slamSecondaryTotalDegrees !=
            attackReachDiagnostics.predictedSlamSecondaryDegrees ||
        sanitized.tipBias != attackReachDiagnostics.predictedTipBias ||
        sanitized.fixedLeadingBoneCount !=
            attackReachDiagnostics.predictedFixedLeadingBoneCount;

    if (predictionChanged) {
        attackReachDiagnostics.predictionValid = false;
        attackReachDiagnostics.predictionWorldMatrix = worldMatrix;
        attackReachDiagnostics.predictedAttackTargetPosition = center;
        attackReachDiagnostics.predictedPrimarySign = sanitized.primarySign;
        attackReachDiagnostics.predictedSecondarySign = sanitized.secondarySign;
        attackReachDiagnostics.predictedSlamPrimaryDegrees =
            sanitized.slamPrimaryTotalDegrees;
        attackReachDiagnostics.predictedSlamSecondaryDegrees =
            sanitized.slamSecondaryTotalDegrees;
        attackReachDiagnostics.predictedTipBias = sanitized.tipBias;
        attackReachDiagnostics.predictedFixedLeadingBoneCount =
            sanitized.fixedLeadingBoneCount;

        const std::array<KrakenTentacleAttackPreviewPhase, 8> phases = {
            KrakenTentacleAttackPreviewPhase::Completed,
            KrakenTentacleAttackPreviewPhase::Windup,
            KrakenTentacleAttackPreviewPhase::Slam,
            KrakenTentacleAttackPreviewPhase::Slam,
            KrakenTentacleAttackPreviewPhase::Slam,
            KrakenTentacleAttackPreviewPhase::Slam,
            KrakenTentacleAttackPreviewPhase::ImpactHold,
            KrakenTentacleAttackPreviewPhase::Recovery,
        };
        const std::array<float, 8> ratios = {
            0.0f, 1.0f, 0.0f, 0.75f, 0.65f, 1.0f, 0.0f, 0.5f };
        bool allValid = true;
        for (std::size_t chainIndex = 0; chainIndex < chains.size(); ++chainIndex) {
            KrakenTentacleReachChainDiagnostics& chainDiagnostic =
                attackReachDiagnostics.chains[chainIndex];
            chainDiagnostic.predictionValid = true;
            for (std::size_t sampleIndex = 0; sampleIndex < phases.size(); ++sampleIndex) {
                Skeleton predicted = *skeleton;
                for (std::size_t jointIndex = 0;
                    jointIndex < predicted.joints.size(); ++jointIndex) {
                    predicted.joints[jointIndex].localTranslate = bindPose[jointIndex].translate;
                    predicted.joints[jointIndex].localRotate = bindPose[jointIndex].rotate;
                    predicted.joints[jointIndex].localScale = bindPose[jointIndex].scale;
                }
                bool poseValid = sampleIndex == 0;
                if (sampleIndex != 0) {
                    const float duration = GetKrakenTentacleAttackPhaseDuration(
                        sanitized, phases[sampleIndex]);
                    const KrakenTentacleAttackPoseTotals totals =
                        EvaluateKrakenTentacleAttackPoseTotals(
                            sanitized, phases[sampleIndex],
                            duration * ratios[sampleIndex]);
                    poseValid = totals.finite && ApplyWholeSlamPoseToSkeleton(
                        predicted, chainIndex, phases[sampleIndex], totals,
                        center, nullptr);
                }
                KrakenTentacleReachPhaseDiagnostics& sample =
                    chainDiagnostic.phaseSamples[sampleIndex];
                sample = {};
                if (!poseValid) {
                    chainDiagnostic.predictionValid = false;
                    allValid = false;
                    continue;
                }
                UpdateSkeletonWorldTransforms(predicted);
                const KrakenTentacleChain& chain = chains[chainIndex];
                const auto attack = std::find_if(
                    colliderDefinitions[chainIndex].capsules.begin(),
                    colliderDefinitions[chainIndex].capsules.end(),
                    [](const KrakenTentacleCapsuleColliderDefinition& definition) {
                        return definition.role == KrakenColliderPreviewRole::Attack;
                    });
                if (attack == colliderDefinitions[chainIndex].capsules.end()) {
                    chainDiagnostic.predictionValid = false;
                    allValid = false;
                    continue;
                }
                const KrakenTentacleCapsuleColliderEvaluation capsule =
                    EvaluateKrakenTentacleCapsuleCollider(
                        predicted, worldMatrix, predicted.root,
                        attack->startJointIndex, attack->endJointIndex,
                        attack->recommendedLocalRadius, colliderRadiusScale,
                        colliderGlobalRadiusScale);
                const int tipIndex = chain.joints.back();
                sample.tipWorldPosition = TransformPosition(
                    predicted.joints[static_cast<std::size_t>(tipIndex)].worldTranslate,
                    worldMatrix);
                sample.finalMovableBoneWorldPosition = sample.tipWorldPosition;
                sample.attackCapsuleStart = capsule.worldStart;
                sample.attackCapsuleEnd = capsule.worldEnd;
                sample.attackCapsuleDirection = Normalize(
                    Subtract(capsule.worldEnd, capsule.worldStart));
                sample.valid = capsule.valid && IsFinite(sample.tipWorldPosition);
                chainDiagnostic.attackCapsuleRadius = capsule.worldRadius;
                chainDiagnostic.predictionValid &= sample.valid;
                allValid &= sample.valid;
            }
        }
        attackReachDiagnostics.predictionValid = allValid;
    }

    attackReachDiagnostics.reachableChainMask = 0;
    attackReachDiagnostics.avoidableChainMask = 0;
    for (std::size_t chainIndex = 0; chainIndex < chains.size(); ++chainIndex) {
        KrakenTentacleReachChainDiagnostics& chainDiagnostic =
            attackReachDiagnostics.chains[chainIndex];
        const int rootIndex = chains[chainIndex].joints.front();
        chainDiagnostic.rootWorldPosition = TransformPosition(
            skeleton->joints[static_cast<std::size_t>(rootIndex)].worldTranslate,
            worldMatrix);
        const auto tip = std::find_if(
            tipSnapshots.begin(), tipSnapshots.end(),
            [chainIndex](const KrakenTentacleMidbossTipSnapshot& value) {
                return value.chainIndex == chainIndex && value.valid;
            });
        const auto capsule = std::find_if(
            capsuleSnapshots.begin(), capsuleSnapshots.end(),
            [chainIndex](const KrakenTentacleMidbossCapsuleSnapshot& value) {
                return value.chainIndex == chainIndex && value.valid &&
                    value.role == KrakenColliderPreviewRole::Attack;
            });
        chainDiagnostic.currentValid =
            tip != tipSnapshots.end() && capsule != capsuleSnapshots.end();
        if (chainDiagnostic.currentValid) {
            chainDiagnostic.currentTipWorldPosition = tip->chainTipWorldPosition;
            chainDiagnostic.currentFinalMovableBoneWorldPosition = tip->chainTipWorldPosition;
            chainDiagnostic.currentAttackCapsuleStart = capsule->worldStart;
            chainDiagnostic.currentAttackCapsuleEnd = capsule->worldEnd;
            chainDiagnostic.currentAttackCapsuleDirection = Normalize(
                Subtract(capsule->worldEnd, capsule->worldStart));
            chainDiagnostic.attackCapsuleRadius = capsule->worldRadius;
        }
        chainDiagnostic.combinedRadius = chainDiagnostic.attackCapsuleRadius +
            attackReachDiagnostics.playerRadius;

        for (KrakenTentacleReachPhaseDiagnostics& sample :
            chainDiagnostic.phaseSamples) {
            if (!sample.valid) {
                continue;
            }
            sample.playerCenterDistance = Distance(
                sample.tipWorldPosition, center);
            const RegionDistanceResult closest = ClosestSegmentToPlayerRegion(
                sample.attackCapsuleStart, sample.attackCapsuleEnd,
                center, right, up, limitX, limitY);
            sample.nearestPlayerPosition = closest.playerPosition;
            sample.movementRegionDistance = closest.distance;
        }

        const auto bindIndex = static_cast<std::size_t>(
            KrakenTentacleReachPoseSample::Bind);
        const auto slamIndex = static_cast<std::size_t>(
            KrakenTentacleReachPoseSample::SlamEnd);
        const auto& bind = chainDiagnostic.phaseSamples[bindIndex];
        const auto& slam = chainDiagnostic.phaseSamples[slamIndex];
        chainDiagnostic.bindToSlamTipDirection = Normalize(
            Subtract(slam.tipWorldPosition, bind.tipWorldPosition));
        chainDiagnostic.bindTipToPlayerDirection = Normalize(
            Subtract(center, bind.tipWorldPosition));
        chainDiagnostic.tipPlayerDirectionDot = Dot(
            chainDiagnostic.bindToSlamTipDirection,
            chainDiagnostic.bindTipToPlayerDirection);

        const std::array<std::size_t, 4> activeSamples = {
            static_cast<std::size_t>(KrakenTentacleReachPoseSample::SlamThreshold),
            static_cast<std::size_t>(KrakenTentacleReachPoseSample::SlamThreeQuarter),
            static_cast<std::size_t>(KrakenTentacleReachPoseSample::SlamEnd),
            static_cast<std::size_t>(KrakenTentacleReachPoseSample::ImpactHold) };
        chainDiagnostic.minimumActiveDistance =
            std::numeric_limits<float>::infinity();
        chainDiagnostic.maximumActiveDistance = -1.0f;
        for (std::size_t sampleIndex : activeSamples) {
            const KrakenTentacleReachPhaseDiagnostics& sample =
                chainDiagnostic.phaseSamples[sampleIndex];
            if (!sample.valid || sample.movementRegionDistance >=
                chainDiagnostic.minimumActiveDistance) {
                continue;
            }
            const RegionDistanceResult farthest = FarthestPlayerRegionPoint(
                sample.attackCapsuleStart, sample.attackCapsuleEnd,
                center, right, up, limitX, limitY);
            chainDiagnostic.minimumActiveDistance = sample.movementRegionDistance;
            chainDiagnostic.nearestReachablePlayerPosition =
                sample.nearestPlayerPosition;
            chainDiagnostic.maximumActiveDistance = farthest.distance;
            chainDiagnostic.farthestReachablePlayerPosition =
                farthest.playerPosition;
        }
        chainDiagnostic.reachesMovementRegion =
            std::isfinite(chainDiagnostic.minimumActiveDistance) &&
            chainDiagnostic.minimumActiveDistance <= chainDiagnostic.combinedRadius;
        chainDiagnostic.avoidableInMovementRegion =
            std::isfinite(chainDiagnostic.maximumActiveDistance) &&
            chainDiagnostic.maximumActiveDistance > chainDiagnostic.combinedRadius;
        if (chainDiagnostic.reachesMovementRegion && chainIndex < 32) {
            attackReachDiagnostics.reachableChainMask |=
                std::uint32_t{ 1 } << chainIndex;
        }
        if (chainDiagnostic.avoidableInMovementRegion && chainIndex < 32) {
            attackReachDiagnostics.avoidableChainMask |=
                std::uint32_t{ 1 } << chainIndex;
        }
    }
}

void KrakenTentacleMidbossController::Impl::
DrawAttackReachDiagnosticsImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "攻撃姿勢・戦闘空間診断##AttackReachDiagnostics",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const auto drawVector = [](const char* label, const Vector3& value) {
        ImGui::Text("%s: (%.3f, %.3f, %.3f)",
            label, value.x, value.y, value.z);
    };
    ImGui::Text("診断有効: %s", attackReachDiagnostics.predictionValid ? "はい" : "いいえ");
    ImGui::Text("プレイヤー移動領域有効: %s",
        attackReachDiagnostics.playerMovementRegionValid ? "はい" : "いいえ");
    drawVector("プレイヤー移動範囲最小", attackReachDiagnostics.playerMovementMinimum);
    drawVector("プレイヤー移動範囲最大", attackReachDiagnostics.playerMovementMaximum);
    drawVector("中央プレイヤー位置", attackReachDiagnostics.playerMovementCenter);
    drawVector("左端", attackReachDiagnostics.playerMovementLeft);
    drawVector("右端", attackReachDiagnostics.playerMovementRight);
    drawVector("上端", attackReachDiagnostics.playerMovementTop);
    drawVector("下端", attackReachDiagnostics.playerMovementBottom);
    ImGui::Text("移動上限 横／縦: %.3f／%.3f",
        attackReachDiagnostics.playerMoveLimitX,
        attackReachDiagnostics.playerMoveLimitY);
    ImGui::SeparatorText("攻撃姿勢設定");
    ImGui::Text("主動作／上側しなり: 対象方向ヒンジ／同一平面");
    ImGui::Text("回転平面: 攻撃開始時の保存対象から自動計算");
    ImGui::Text("叩きつけ主ヒンジ上限／上側しなり: %.1f 度／%.1f 度",
        attackSettings.slamPrimaryTotalDegrees,
        attackSettings.slamSecondaryTotalDegrees);
    ImGui::Text("上側しなり配分バイアス: %.2f", attackSettings.tipBias);
    ImGui::Text("固定する先頭ボーン数: %u", attackSettings.fixedLeadingBoneCount);
    ImGui::Text("到達可能チェーンビット: 0x%X",
        attackReachDiagnostics.reachableChainMask);
    ImGui::Text("回避可能チェーンビット: 0x%X",
        attackReachDiagnostics.avoidableChainMask);

    const std::size_t chainIndex = (std::min)(
        selectedAttackChainIndex, attackReachDiagnostics.chains.size() - 1);
    const KrakenTentacleReachChainDiagnostics& chain =
        attackReachDiagnostics.chains[chainIndex];
    ImGui::SeparatorText("選択チェーン");
    ImGui::Text("現在チェーン: %zu", chainIndex);
    drawVector("チェーン根元", chain.rootWorldPosition);
    drawVector("ボス→プレイヤー方向", Normalize(Subtract(
        attackReachDiagnostics.playerMovementCenter, chain.rootWorldPosition)));
    drawVector("現在先端", chain.currentTipWorldPosition);
    drawVector("現在最終可動ボーン", chain.currentFinalMovableBoneWorldPosition);
    drawVector("現在攻撃カプセル始点", chain.currentAttackCapsuleStart);
    drawVector("現在攻撃カプセル終点", chain.currentAttackCapsuleEnd);
    drawVector("現在カプセル方向", chain.currentAttackCapsuleDirection);
    drawVector("バインド→叩きつけ先端方向", chain.bindToSlamTipDirection);
    drawVector("バインド先端→プレイヤー方向", chain.bindTipToPlayerDirection);
    ImGui::Text("プレイヤー方向内積: %.4f", chain.tipPlayerDirectionDot);
    ImGui::Text("攻撃カプセル半径／プレイヤー半径: %.3f／%.3f",
        chain.attackCapsuleRadius, attackReachDiagnostics.playerRadius);
    ImGui::Text("最短距離／半径合計: %.3f／%.3f",
        chain.minimumActiveDistance, chain.combinedRadius);
    drawVector("最短到達プレイヤー位置", chain.nearestReachablePlayerPosition);
    drawVector("回避確認プレイヤー位置", chain.farthestReachablePlayerPosition);
    ImGui::Text("交差可能／到達可能: %s",
        chain.reachesMovementRegion ? "はい" : "いいえ");
    ImGui::Text("回避可能: %s", chain.avoidableInMovementRegion ? "はい" : "いいえ");

    if (ImGui::TreeNode("姿勢別比較##ReachPhaseComparison")) {
        for (std::size_t index = 0; index < chain.phaseSamples.size(); ++index) {
            const auto sampleKind = static_cast<KrakenTentacleReachPoseSample>(index);
            const KrakenTentacleReachPhaseDiagnostics& sample = chain.phaseSamples[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::TreeNode(SampleLabel(sampleKind))) {
                drawVector("先端", sample.tipWorldPosition);
                drawVector("最終可動ボーン", sample.finalMovableBoneWorldPosition);
                drawVector("カプセル始点", sample.attackCapsuleStart);
                drawVector("カプセル終点", sample.attackCapsuleEnd);
                drawVector("カプセル方向", sample.attackCapsuleDirection);
                ImGui::Text("中央への距離: %.3f", sample.playerCenterDistance);
                ImGui::Text("移動範囲への最短距離: %.3f",
                    sample.movementRegionDistance);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::SeparatorText("全チェーン");
    for (std::size_t index = 0; index < attackReachDiagnostics.chains.size(); ++index) {
        const KrakenTentacleReachChainDiagnostics& value =
            attackReachDiagnostics.chains[index];
        const Vector3& slamTip = value.phaseSamples[static_cast<std::size_t>(
            KrakenTentacleReachPoseSample::SlamEnd)].tipWorldPosition;
        ImGui::Text("チェーン %zu 叩きつけ先端: (%.3f, %.3f, %.3f)",
            index, slamTip.x, slamTip.y, slamTip.z);
        ImGui::Text("チェーン %zu: 最短 %.3f／到達 %s／回避 %s",
            index, value.minimumActiveDistance,
            value.reachesMovementRegion ? "はい" : "いいえ",
            value.avoidableInMovementRegion ? "はい" : "いいえ");
    }
    if (ImGui::Button("推奨攻撃姿勢へ戻す##ResetRecommendedAttackPose")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::ApplyRecommendedAttackPose;
    }
    ImGui::SameLine();
    if (ImGui::Button("攻撃到達診断をリセット##ResetDetailedAttackReach")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::ResetAttackReachDiagnostics;
    }
#endif
}
