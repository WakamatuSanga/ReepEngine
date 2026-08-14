#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"
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
            value.x * matrix.m[0][0] +
                value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] +
                value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] +
                value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] +
                value.y * matrix.m[1][3] +
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
        const Vector4 clipPosition = TransformPoint(
            worldPosition,
            camera->GetViewProjectionMatrix());
        if (!std::isfinite(clipPosition.x) ||
            !std::isfinite(clipPosition.y) ||
            !std::isfinite(clipPosition.z) ||
            !std::isfinite(clipPosition.w) ||
            clipPosition.w <= 0.0001f) {
            return false;
        }
        const float inverseW = 1.0f / clipPosition.w;
        const float ndcX = clipPosition.x * inverseW;
        const float ndcY = clipPosition.y * inverseW;
        const float ndcZ = clipPosition.z * inverseW;
        if (ndcZ < 0.0f || ndcZ > 1.0f) {
            return false;
        }
        outScreen = {
            viewTopLeft.x + ((ndcX * 0.5f) + 0.5f) * viewSize.x,
            viewTopLeft.y + ((-ndcY * 0.5f) + 0.5f) * viewSize.y,
        };
        return std::isfinite(outScreen.x) && std::isfinite(outScreen.y);
    }

    float ScreenDistance(const ImVec2& lhs, const ImVec2& rhs) {
        const float deltaX = lhs.x - rhs.x;
        const float deltaY = lhs.y - rhs.y;
        return std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    float ProjectRadiusToPixels(
        const Vector3& worldCenter,
        float worldRadius,
        const Camera* camera,
        const ImVec2& viewTopLeft,
        const ImVec2& viewSize,
        const ImVec2& centerScreen) {
        if (!std::isfinite(worldRadius) || worldRadius <= 0.0f) {
            return 0.0f;
        }
        constexpr std::array<Vector3, 3> kAxes = {{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        }};
        float radiusPixels = 0.0f;
        for (const Vector3& axis : kAxes) {
            const Vector3 radiusPoint = {
                worldCenter.x + axis.x * worldRadius,
                worldCenter.y + axis.y * worldRadius,
                worldCenter.z + axis.z * worldRadius,
            };
            ImVec2 radiusScreen{};
            if (ProjectToScreen(
                    radiusPoint,
                    camera,
                    viewTopLeft,
                    viewSize,
                    radiusScreen)) {
                radiusPixels = (std::max)(
                    radiusPixels,
                    ScreenDistance(centerScreen, radiusScreen));
            }
        }
        return std::clamp(radiusPixels, 2.0f, 180.0f);
    }

    struct ColliderDrawStyle {
        ImU32 color = IM_COL32(150, 150, 150, 220);
        float centerThickness = 2.5f;
        float edgeThickness = 2.0f;
    };

    ColliderDrawStyle GetDrawStyle(
        KrakenColliderPreviewRole role,
        bool enabled,
        bool valid,
        bool phaseActive) {
        if (!enabled || !valid) {
            return { IM_COL32(145, 145, 145, 220), 2.5f, 2.0f };
        }
        switch (role) {
        case KrakenColliderPreviewRole::Attack:
            return phaseActive
                ? ColliderDrawStyle{
                    IM_COL32(255, 51, 5, 242), 3.5f, 3.0f }
                : ColliderDrawStyle{
                    IM_COL32(115, 20, 8, 71), 1.5f, 1.25f };
        case KrakenColliderPreviewRole::Damage:
            return { IM_COL32(70, 145, 255, 235), 2.5f, 2.0f };
        case KrakenColliderPreviewRole::WeakPoint:
            return { IM_COL32(255, 220, 55, 240), 2.5f, 2.0f };
        default:
            return {};
        }
    }

    ImU32 GetSelectionOutlineColor(
        KrakenColliderPreviewRole role,
        bool enabled,
        bool valid,
        bool phaseActive) {
        if (!enabled || !valid ||
            (role == KrakenColliderPreviewRole::Attack && !phaseActive)) {
            return IM_COL32(225, 228, 232, 230);
        }
        return IM_COL32(255, 255, 215, 255);
    }

    bool DrawCapsule(
        ImDrawList* drawList,
        const KrakenBoneColliderPreview& collider,
        const Camera* camera,
        const ImVec2& viewTopLeft,
        const ImVec2& viewSize,
        bool selected) {
        ImVec2 startScreen{};
        ImVec2 endScreen{};
        if (!ProjectToScreen(
                collider.worldStart,
                camera,
                viewTopLeft,
                viewSize,
                startScreen) ||
            !ProjectToScreen(
                collider.worldEnd,
                camera,
                viewTopLeft,
                viewSize,
                endScreen)) {
            return false;
        }
        const float startRadius = ProjectRadiusToPixels(
            collider.worldStart,
            collider.worldRadius,
            camera,
            viewTopLeft,
            viewSize,
            startScreen);
        const float endRadius = ProjectRadiusToPixels(
            collider.worldEnd,
            collider.worldRadius,
            camera,
            viewTopLeft,
            viewSize,
            endScreen);
        const float radiusPixels = (startRadius + endRadius) * 0.5f;
        const float deltaX = endScreen.x - startScreen.x;
        const float deltaY = endScreen.y - startScreen.y;
        const float screenLength = std::sqrt(
            deltaX * deltaX + deltaY * deltaY);
        const float inverseLength = screenLength > 0.0001f
            ? 1.0f / screenLength
            : 0.0f;
        const ImVec2 perpendicular = {
            -deltaY * inverseLength * radiusPixels,
            deltaX * inverseLength * radiusPixels,
        };
        const ColliderDrawStyle style = GetDrawStyle(
            collider.role,
            collider.enabled,
            collider.valid,
            collider.phaseActive);
        const ImU32 highlightColor = GetSelectionOutlineColor(
            collider.role,
            collider.enabled,
            collider.valid,
            collider.phaseActive);
        if (selected) {
            drawList->AddLine(
                startScreen,
                endScreen,
                highlightColor,
                style.centerThickness + 3.5f);
            drawList->AddCircle(
                startScreen, startRadius + 3.0f, highlightColor, 24, 3.0f);
            drawList->AddCircle(
                endScreen, endRadius + 3.0f, highlightColor, 24, 3.0f);
        }
        drawList->AddLine(
            startScreen,
            endScreen,
            style.color,
            style.centerThickness);
        drawList->AddLine(
            { startScreen.x + perpendicular.x,
                startScreen.y + perpendicular.y },
            { endScreen.x + perpendicular.x,
                endScreen.y + perpendicular.y },
            style.color,
            style.edgeThickness);
        drawList->AddLine(
            { startScreen.x - perpendicular.x,
                startScreen.y - perpendicular.y },
            { endScreen.x - perpendicular.x,
                endScreen.y - perpendicular.y },
            style.color,
            style.edgeThickness);
        drawList->AddCircle(
            startScreen,
            startRadius,
            style.color,
            24,
            style.edgeThickness);
        drawList->AddCircle(
            endScreen,
            endRadius,
            style.color,
            24,
            style.edgeThickness);
        return true;
    }

    bool DrawTipSphere(
        ImDrawList* drawList,
        const KrakenTipSphereColliderPreview& sphere,
        const Camera* camera,
        const ImVec2& viewTopLeft,
        const ImVec2& viewSize,
        bool selected) {
        ImVec2 centerScreen{};
        if (!ProjectToScreen(
                sphere.worldPosition,
                camera,
                viewTopLeft,
                viewSize,
                centerScreen)) {
            return false;
        }
        const float radiusPixels = ProjectRadiusToPixels(
            sphere.worldPosition,
            sphere.worldRadius,
            camera,
            viewTopLeft,
            viewSize,
            centerScreen);
        const ColliderDrawStyle style = GetDrawStyle(
            sphere.role,
            sphere.enabled,
            sphere.valid,
            sphere.phaseActive);
        if (selected) {
            drawList->AddCircle(
                centerScreen,
                radiusPixels + 4.0f,
                GetSelectionOutlineColor(
                    sphere.role,
                    sphere.enabled,
                    sphere.valid,
                    sphere.phaseActive),
                28,
                4.0f);
        }
        drawList->AddCircleFilled(
            centerScreen,
            (std::max)(radiusPixels * 0.20f, 3.0f),
            style.color);
        drawList->AddCircle(
            centerScreen,
            radiusPixels,
            style.color,
            28,
            style.centerThickness);
        return true;
    }
#endif
}

void SkinningEditorKrakenMotionPreview::DrawBoneColliderDebugOverlay(
    const Camera* camera,
    float viewX,
    float viewY,
    float viewWidth,
    float viewHeight) const {
#ifdef USE_IMGUI
    if (!boneColliderPreview_ ||
        !camera ||
        viewWidth <= 0.0f ||
        viewHeight <= 0.0f) {
        return;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 viewTopLeft = { viewX, viewY };
    const ImVec2 viewSize = { viewWidth, viewHeight };
    drawList->PushClipRect(
        viewTopLeft,
        { viewX + viewWidth, viewY + viewHeight },
        true);
    std::uint32_t shapeCount = 0;
    const auto& chainPreviews =
        boneColliderPreview_->GetChainPreviews();
    for (std::size_t chainIndex = 0;
        chainIndex < chainPreviews.size();
        ++chainIndex) {
        const auto& preview = chainPreviews[chainIndex];
        if (!preview) {
            continue;
        }
        const std::vector<KrakenBoneColliderPreview>& capsules =
            preview->GetCapsules();
        for (std::size_t index = 0; index < capsules.size(); ++index) {
            const KrakenBoneColliderPreview& collider = capsules[index];
            if (!collider.previewVisible || !collider.valid) {
                continue;
            }
            const bool selected = boneColliderPreview_->IsSelected(
                chainIndex,
                index,
                false);
            if (DrawCapsule(
                drawList,
                collider,
                camera,
                viewTopLeft,
                viewSize,
                selected)) {
                ++shapeCount;
            }
        }
        const KrakenTipSphereColliderPreview& tipSphere =
            preview->GetTipSphere();
        if (tipSphere.previewVisible && tipSphere.valid) {
            const bool tipSelected = boneColliderPreview_->IsSelected(
                chainIndex,
                0,
                true);
            if (DrawTipSphere(
                    drawList,
                    tipSphere,
                    camera,
                    viewTopLeft,
                    viewSize,
                    tipSelected)) {
                ++shapeCount;
            }
        }
    }
    drawList->PopClipRect();
    boneColliderPreview_->RecordDebugDraw(shapeCount);
#else
    (void)camera;
    (void)viewX;
    (void)viewY;
    (void)viewWidth;
    (void)viewHeight;
#endif
}
