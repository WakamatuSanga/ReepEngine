#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    Vector4 TransformPoint(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] +
                value.z * matrix.m[2][3] + matrix.m[3][3],
        };
    }

    bool ProjectToScreen(
        const Vector3& worldPosition,
        const Camera* camera,
        const ImVec2& viewTopLeft,
        const ImVec2& viewSize,
        ImVec2& outScreen) {
        if (!camera || viewSize.x <= 0.0f || viewSize.y <= 0.0f) {
            return false;
        }
        const Vector4 clip = TransformPoint(
            worldPosition, camera->GetViewProjectionMatrix());
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
            !std::isfinite(clip.z) || !std::isfinite(clip.w) ||
            clip.w <= 0.0001f) {
            return false;
        }
        const float inverseW = 1.0f / clip.w;
        const float ndcX = clip.x * inverseW;
        const float ndcY = clip.y * inverseW;
        const float ndcZ = clip.z * inverseW;
        if (ndcZ < 0.0f || ndcZ > 1.0f) {
            return false;
        }
        outScreen = {
            viewTopLeft.x + (ndcX * 0.5f + 0.5f) * viewSize.x,
            viewTopLeft.y + (-ndcY * 0.5f + 0.5f) * viewSize.y,
        };
        return std::isfinite(outScreen.x) && std::isfinite(outScreen.y);
    }

    float ScreenDistance(const ImVec2& lhs, const ImVec2& rhs) {
        const float x = lhs.x - rhs.x;
        const float y = lhs.y - rhs.y;
        return std::sqrt(x * x + y * y);
    }

    float ProjectRadius(
        const Vector3& center,
        float radius,
        const Camera* camera,
        const ImVec2& viewTopLeft,
        const ImVec2& viewSize,
        const ImVec2& centerScreen) {
        if (!std::isfinite(radius) || radius <= 0.0f) {
            return 2.0f;
        }
        constexpr std::array<Vector3, 3> axes = {{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        }};
        float pixels = 0.0f;
        for (const Vector3& axis : axes) {
            const Vector3 point = {
                center.x + axis.x * radius,
                center.y + axis.y * radius,
                center.z + axis.z * radius,
            };
            ImVec2 screen{};
            if (ProjectToScreen(
                    point, camera, viewTopLeft, viewSize, screen)) {
                pixels = (std::max)(
                    pixels, ScreenDistance(centerScreen, screen));
            }
        }
        return std::clamp(pixels, 2.0f, 180.0f);
    }

    struct DrawStyle {
        ImU32 color = IM_COL32(145, 145, 145, 220);
        float centerThickness = 2.5f;
        float edgeThickness = 2.0f;
    };

    DrawStyle GetStyle(
        KrakenColliderPreviewRole role,
        bool enabled,
        bool valid,
        bool phaseActive) {
        if (!enabled || !valid) {
            return {};
        }
        switch (role) {
        case KrakenColliderPreviewRole::Attack:
            return phaseActive
                ? DrawStyle{ IM_COL32(255, 51, 5, 242), 3.5f, 3.0f }
                : DrawStyle{ IM_COL32(115, 20, 8, 90), 1.5f, 1.25f };
        case KrakenColliderPreviewRole::Damage:
            return { IM_COL32(70, 145, 255, 235), 2.5f, 2.0f };
        case KrakenColliderPreviewRole::WeakPoint:
            return { IM_COL32(255, 220, 55, 240), 2.5f, 2.0f };
        default:
            return {};
        }
    }

    bool ShouldDrawRole(
        KrakenColliderPreviewRole role,
        bool showAttack,
        bool showDamage,
        bool showWeakPoint) {
        switch (role) {
        case KrakenColliderPreviewRole::Attack:
            return showAttack;
        case KrakenColliderPreviewRole::Damage:
            return showDamage;
        case KrakenColliderPreviewRole::WeakPoint:
            return showWeakPoint;
        default:
            return false;
        }
    }

    void DrawCapsule(
        ImDrawList* drawList,
        const KrakenTentacleMidbossCapsuleSnapshot& collider,
        const Camera* camera,
        const ImVec2& topLeft,
        const ImVec2& size) {
        ImVec2 start{};
        ImVec2 end{};
        if (!ProjectToScreen(collider.worldStart, camera, topLeft, size, start) ||
            !ProjectToScreen(collider.worldEnd, camera, topLeft, size, end)) {
            return;
        }
        const float startRadius = ProjectRadius(
            collider.worldStart, collider.worldRadius,
            camera, topLeft, size, start);
        const float endRadius = ProjectRadius(
            collider.worldEnd, collider.worldRadius,
            camera, topLeft, size, end);
        const float radius = (startRadius + endRadius) * 0.5f;
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        const float inverseLength = length > 0.0001f ? 1.0f / length : 0.0f;
        const ImVec2 perpendicular = {
            -deltaY * inverseLength * radius,
            deltaX * inverseLength * radius,
        };
        const DrawStyle style = GetStyle(
            collider.role, collider.enabled, collider.valid,
            collider.phaseActive);
        drawList->AddLine(start, end, style.color, style.centerThickness);
        drawList->AddLine(
            { start.x + perpendicular.x, start.y + perpendicular.y },
            { end.x + perpendicular.x, end.y + perpendicular.y },
            style.color, style.edgeThickness);
        drawList->AddLine(
            { start.x - perpendicular.x, start.y - perpendicular.y },
            { end.x - perpendicular.x, end.y - perpendicular.y },
            style.color, style.edgeThickness);
        drawList->AddCircle(
            start, startRadius, style.color, 24, style.edgeThickness);
        drawList->AddCircle(
            end, endRadius, style.color, 24, style.edgeThickness);
    }

    void DrawSphere(
        ImDrawList* drawList,
        const KrakenTentacleMidbossTipSnapshot& sphere,
        const Camera* camera,
        const ImVec2& topLeft,
        const ImVec2& size) {
        ImVec2 center{};
        if (!ProjectToScreen(
                sphere.worldPosition, camera, topLeft, size, center)) {
            return;
        }
        const float radius = ProjectRadius(
            sphere.worldPosition, sphere.worldRadius,
            camera, topLeft, size, center);
        const DrawStyle style = GetStyle(
            sphere.role, sphere.enabled, sphere.valid, sphere.phaseActive);
        drawList->AddCircleFilled(
            center, (std::max)(radius * 0.2f, 3.0f), style.color);
        drawList->AddCircle(
            center, radius, style.color, 28, style.centerThickness);
    }
#endif
}

void KrakenTentacleMidbossController::Impl::DrawDebug(
    float viewX,
    float viewY,
    float viewWidth,
    float viewHeight) const {
#ifdef USE_IMGUI
    if (!IsVisible() || !camera || viewWidth <= 0.0f ||
        viewHeight <= 0.0f ||
        (!showBones && !showJoints && !showColliders)) {
        return;
    }
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) {
        return;
    }
    const ImVec2 topLeft{ viewX, viewY };
    const ImVec2 size{ viewWidth, viewHeight };
    drawList->PushClipRect(
        topLeft, { viewX + viewWidth, viewY + viewHeight }, true);

    for (const KrakenTentacleMidbossBoneSnapshot& bone : boneSnapshots) {
        const bool selected =
            bone.chainIndex == selectedAttackChainIndex;
        if (!bone.valid ||
            (showSelectedChainOnly && !selected && !bone.isRoot)) {
            continue;
        }
        ImVec2 screen{};
        if (!ProjectToScreen(
                bone.worldPosition, camera, topLeft, size, screen)) {
            continue;
        }
        if (showBones && bone.parentIndex >= 0 &&
            static_cast<std::size_t>(bone.parentIndex) <
                boneSnapshots.size()) {
            const KrakenTentacleMidbossBoneSnapshot& parent =
                boneSnapshots[static_cast<std::size_t>(bone.parentIndex)];
            ImVec2 parentScreen{};
            if (parent.valid && ProjectToScreen(
                    parent.worldPosition, camera, topLeft, size,
                    parentScreen)) {
                drawList->AddLine(
                    parentScreen,
                    screen,
                    selected
                        ? IM_COL32(255, 220, 65, 235)
                        : IM_COL32(120, 225, 255, 190),
                    selected ? 3.0f : 1.5f);
            }
        }
        if (showJoints) {
            const ImU32 color = bone.isRoot
                ? IM_COL32(80, 255, 100, 255)
                : (selected
                    ? IM_COL32(255, 220, 65, 255)
                    : IM_COL32(220, 250, 255, 225));
            drawList->AddCircleFilled(
                screen, bone.isRoot ? 6.0f : 3.5f, color);
        }
    }

    if (showColliders) {
        for (const KrakenTentacleMidbossCapsuleSnapshot& collider :
            capsuleSnapshots) {
            if ((showSelectedChainOnly &&
                    collider.chainIndex != selectedAttackChainIndex) ||
                !ShouldDrawRole(
                    collider.role,
                    showAttackColliders,
                    showDamageColliders,
                    showWeakPoints)) {
                continue;
            }
            DrawCapsule(drawList, collider, camera, topLeft, size);
        }
        for (const KrakenTentacleMidbossTipSnapshot& sphere : tipSnapshots) {
            if ((showSelectedChainOnly &&
                    sphere.chainIndex != selectedAttackChainIndex) ||
                !ShouldDrawRole(
                    sphere.role,
                    showAttackColliders,
                    showDamageColliders,
                    showWeakPoints)) {
                continue;
            }
            DrawSphere(drawList, sphere, camera, topLeft, size);
        }
    }
    drawList->PopClipRect();
#else
    (void)viewX;
    (void)viewY;
    (void)viewWidth;
    (void)viewHeight;
#endif
}
