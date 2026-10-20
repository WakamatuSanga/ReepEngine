#include "VolumetricCloudPass.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/GpuTimestampProfiler.h"
#include "Engine/Core/RuntimeModeController.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    using Profiler = GpuTimestampProfiler;
    uint64_t HashBytes(const void* data, size_t size) {
        uint64_t hash = 14695981039346656037ull;
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
        return hash;
    }
}

VolumetricCloudPass::~VolumetricCloudPass() { EndResolutionComparison(); }

void VolumetricCloudPass::MarkGpuScene() {
    if (!dxCommon_) return;
    dxCommon_->GetGpuProfiler().SetScene(passKind_ == PassKind::Game ? Profiler::Role::Game : Profiler::Role::TitleBackground);
}

uint32_t VolumetricCloudPass::BeginGpuMeasurement(bool composite, int omission, bool direct) {
    auto& profiler = dxCommon_->GetGpuProfiler();
    if (!profiler.IsEnabled()) return Profiler::kInvalidToken;
    Profiler::Context context;
    context.role = passKind_ == PassKind::Game ? Profiler::Role::Game :
        (passKind_ == PassKind::TitleBackground ? Profiler::Role::TitleBackground : Profiler::Role::TitleForeground);
    context.invocation = diagnosticInvocation_;
    direct = direct || !settings_.useLowResolutionCloud_;
    context.path = direct ? Profiler::Path::Direct : Profiler::Path::LowResolution;
    context.outputWidth = dxCommon_->GetRenderTextureWidth(); context.outputHeight = dxCommon_->GetRenderTextureHeight();
    context.cloudWidth = direct ? context.outputWidth : cloudBufferWidth_;
    context.cloudHeight = direct ? context.outputHeight : cloudBufferHeight_;
    context.resolution = context.outputWidth ? static_cast<float>(context.cloudWidth) / context.outputWidth : 0;
    context.reallocated = diagnosticReallocated_;
    if (constantData_ && omission < 2) {
        context.viewSteps = constantData_->viewStepCount; context.lightSteps = constantData_->lightStepCount;
        context.distanceLod = constantData_->disableDistanceLod == 0; context.cloudTime = constantData_->cloudRawTime;
        context.constantsHash = HashBytes(constantData_, sizeof(*constantData_));
    }
    if (composite && compositeConstantData_ && omission == 0) context.compositeConstantsHash = HashBytes(compositeConstantData_, sizeof(*compositeConstantData_));
    const auto stage = composite ? Profiler::Stage::Composite : Profiler::Stage::Raymarch;
    if (omission) {
        auto state = Profiler::State::Reused;
        if (omission == 2) { state = Profiler::State::Disabled; if (!composite) context.path = Profiler::Path::Disabled; }
        if (omission == 3) { state = Profiler::State::Culled; context.path = Profiler::Path::Culled; }
        if (omission == 1 && composite && direct) state = Profiler::State::NotApplicable;
        profiler.Record(stage, context, state);
        return Profiler::kInvalidToken;
    }
    return profiler.Begin(dxCommon_->GetCommandList(), stage, context);
}
void VolumetricCloudPass::EndGpuMeasurement(uint32_t token) {
    dxCommon_->GetGpuProfiler().End(dxCommon_->GetCommandList(), token);
}
void VolumetricCloudPass::RecordGpuReuse() { BeginGpuMeasurement(!settings_.useLowResolutionCloud_, 1); }
void VolumetricCloudPass::RecordGpuSkipped(bool disabled) {
    if (!dxCommon_) return;
    ++diagnosticInvocation_;
    BeginGpuMeasurement(false, disabled ? 2 : 3);
    BeginGpuMeasurement(true, disabled ? 2 : 3);
}

bool VolumetricCloudPass::BeginResolutionComparison(bool trial, AppearanceMode appearance) {
    if (!dxCommon_ || passKind_ != PassKind::Game || !settings_.isEnabled_ || !IsCloudCompositeEnabled() || settings_.forceMode_ == ForceMode::ForceSkip || !dxCommon_->GetGpuProfiler().IsAvailable()) return false;
    const auto width = dxCommon_->GetRenderTextureWidth(), height = dxCommon_->GetRenderTextureHeight();
    if (!(trial ? comparison_.BeginTrial(settings_, width, height, appearance) : comparison_.Begin(settings_, width, height))) return false;
    if (auto* runtime = RuntimeModeController::GetActiveController()) runtime->SetCloudComparisonActive(true);
    comparisonSavedProfilerEnabled_ = dxCommon_->GetGpuProfiler().IsEnabled();
    dxCommon_->GetGpuProfiler().SetEnabled(true);
    hasValidCloudBuffer_ = false;
    return true;
}
void VolumetricCloudPass::EndResolutionComparison() {
    if (!comparison_.IsActive()) return;
    comparison_.End(settings_); hasValidCloudBuffer_ = false;
    if (auto* runtime = RuntimeModeController::GetActiveController()) runtime->SetCloudComparisonActive(false);
    if (dxCommon_) dxCommon_->GetGpuProfiler().SetEnabled(comparisonSavedProfilerEnabled_);
}

bool VolumetricCloudPass::UpdateResolutionComparison() {
    if (!comparison_.IsActive()) return false;

    if (!comparison_.Maintain(settings_, dxCommon_->GetRenderTextureWidth(), dxCommon_->GetRenderTextureHeight())) {
        hasValidCloudBuffer_ = false;
        if (auto* runtime = RuntimeModeController::GetActiveController()) runtime->SetCloudComparisonActive(false);
        dxCommon_->GetGpuProfiler().SetEnabled(comparisonSavedProfilerEnabled_);
        return false;
    }

    const auto& result = dxCommon_->GetGpuProfiler().GetLatest();
    double whole = -1, ray = 0, composite = 0; uint32_t rayCount = 0, compositeCount = 0;
    const Profiler::Sample* lastGameRay = nullptr;

    for (const auto& sample : result.samples) 
    {
        if (sample.stage == Profiler::Stage::Frame && sample.state == Profiler::State::Measured) whole = sample.milliseconds;
        if (sample.context.role != Profiler::Role::Game || sample.state != Profiler::State::Measured) continue;
        if (sample.stage == Profiler::Stage::Raymarch) { ray += sample.milliseconds; ++rayCount; lastGameRay = &sample; }
        if (sample.stage == Profiler::Stage::Composite) { composite += sample.milliseconds; ++compositeCount; }
    }

    if (lastGameRay && rayCount == 1 && compositeCount == 1 && whole >= 0 && result.scene == Profiler::Role::Game) 
    {
        comparison_.Observe(result.frame, lastGameRay->context.resolution, ray, composite, whole, lastGameRay->context.reallocated);
    }
#ifdef USE_IMGUI
    auto drawStatus = [this] {
        DebugUiWorkspace::TrackWindow("cloudStatus", "雲の調整候補");
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("雲の調整候補", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
        const char* label = settings_.appearanceMode_ == AppearanceMode::ShapeAndLighting ? "形＋光" : settings_.appearanceMode_ == AppearanceMode::ShapeOnly ? "形のみ" : "サンプリング0.25";
        if (comparison_.IsTrial()) ImGui::Text("通常更新しながら試用 / %s", label);
        else ImGui::Text("固定解像度比較 %.3f / ゲーム更新停止", settings_.cloudResolutionScale_);
        if (ImGui::Button("元に戻す / 試用・比較を終了")) EndResolutionComparison();
        ImGui::End();
    };
#endif
    if (comparison_.IsTrial()) {
#ifdef USE_IMGUI
        auto* runtime = RuntimeModeController::GetActiveController();
        if (ImGui::GetCurrentContext() && ((runtime && runtime->IsGameMode()) || !DebugUiWorkspace::IsVisible())) drawStatus();
#endif
        return false;
    }
#ifdef USE_IMGUI
    const DebugUiWorkspace::Panel panels[]{ { "cloud", "雲の固定解像度比較（ゲーム更新停止）", DebugUiWorkspace::Category::Diagnostics, [this] {
        DrawGpuDiagnosticsImGui(); DrawComparisonControlsImGui();
        const float width = (std::max)(ImGui::GetContentRegionAvail().x, 160.0f);
        const float aspect = static_cast<float>(dxCommon_->GetRenderTextureHeight()) / dxCommon_->GetRenderTextureWidth();
        ImGui::Image(static_cast<ImTextureID>(dxCommon_->GetFinalOutputTextureSRVGPUHandle().ptr), ImVec2(width, width * aspect));
    } } };
    DebugUiWorkspace::Draw(panels);
    if (ImGui::GetCurrentContext() && comparison_.IsActive() && !DebugUiWorkspace::IsVisible()) drawStatus();
#endif
    // Resume on the NEXT update after cancellation: this frame's UI already
    // owns the presentation and should not also run ordinary editor widgets.
    return true;
}

void VolumetricCloudPass::DrawGpuDiagnosticsImGui() {
#ifdef USE_IMGUI
    auto& profiler = dxCommon_->GetGpuProfiler();
    bool enabled = profiler.IsEnabled();
    ImGui::BeginDisabled(comparison_.IsActive());
    if (ImGui::Checkbox("GPU Timestamp 計測", &enabled)) profiler.SetEnabled(enabled);
    ImGui::EndDisabled();
    ImGui::Text("DIRECT queue / available: %s / pending: %u / dropped: %llu", profiler.IsAvailable() ? "yes" : "no",
        profiler.GetPendingCount(), static_cast<unsigned long long>(profiler.GetDroppedFrames()));
    const auto& result = profiler.GetLatest();
    if (!result.frame) {
        if (!profiler.IsAvailable()) ImGui::TextUnformatted("GPU結果: 計測不可 (Queue / Timestamp初期化を確認)");
        else if (profiler.GetPendingCount()) ImGui::TextUnformatted("GPU結果: 未完了 (Fenceを待たず次フレームで取得)");
        else if (!enabled) ImGui::TextUnformatted("GPU結果: 未計測 (OFF)");
        else ImGui::TextUnformatted("GPU結果: 計測開始、次の描画結果を待機中");
        return;
    }
    ImGui::Text("結果 Frame %llu / Fence %llu / %s / %.3f MHz%s", static_cast<unsigned long long>(result.frame),
        static_cast<unsigned long long>(result.fence), Profiler::RoleName(result.scene), static_cast<double>(result.frequency) / 1e6,
        enabled ? "" : " (OFF: 過去の結果)");
    double whole = -1, ray = 0, composite = 0; uint32_t rayCount = 0, compositeCount = 0;
    for (const auto& sample : result.samples) {
        if (sample.stage == Profiler::Stage::Frame && sample.state == Profiler::State::Measured) whole = sample.milliseconds;
        if (sample.context.role != Profiler::Role::Game ||
            (sample.stage != Profiler::Stage::Raymarch && sample.stage != Profiler::Stage::Composite)) continue;
        const auto& context = sample.context;
        ImGui::Text("Game #%llu %s %s: %s", static_cast<unsigned long long>(context.invocation), Profiler::PathName(context.path),
            sample.stage == Profiler::Stage::Raymarch ? "Raymarch" : "Composite", Profiler::StateName(sample.state));
        if (sample.state != Profiler::State::Measured) continue;
        ImGui::Text("  %.4f ms / RT %ux%u -> %ux%u (%.3f) / View %u Light %u LOD %s", sample.milliseconds,
            context.cloudWidth, context.cloudHeight, context.outputWidth, context.outputHeight, context.resolution,
            context.viewSteps, context.lightSteps, context.distanceLod ? "ON" : "OFF");
        ImGui::Text("  CB hash %016llX / time %.4f%s", static_cast<unsigned long long>(context.constantsHash), context.cloudTime,
            context.reallocated ? " / RT再確保" : "");
        if (sample.stage == Profiler::Stage::Composite) ImGui::Text("  Composite CB hash %016llX", static_cast<unsigned long long>(context.compositeConstantsHash));
        if (sample.stage == Profiler::Stage::Raymarch) { ray += sample.milliseconds; ++rayCount; }
        else { composite += sample.milliseconds; ++compositeCount; }
    }
    if (whole >= 0) ImGui::Text("描画全体 GPU: %.4f ms (UpdateのGPU処理〜最終Barrier、UI/遷移/Post含む)", whole);
    if (rayCount || compositeCount) ImGui::Text("雲合計 GPU: %.4f ms (GameのRaymarch+Compositeのみ)", ray + composite);
    ImGui::TextWrapped("全体と雲合計は重複区間です。加算しません。Present / Fence待ち / 60fps待機 / CPU更新は含みません。View/Lightは距離LOD前のCB値です。");

#endif
}

void VolumetricCloudPass::DrawComparisonControlsImGui() {
#ifdef USE_IMGUI
    if (comparison_.IsTrial()) {
        const bool appearance = settings_.appearanceMode_ != AppearanceMode::Legacy;
        ImGui::TextUnformatted(appearance ? "雲の形状改善候補を試用中（通常既定は未変更）" : "調整候補（0.25）を試用中 / 通常ゲーム更新・Flow・Boostは継続");
        if (appearance) {
            ImGui::TextUnformatted(settings_.appearanceMode_ == AppearanceMode::ShapeAndLighting ? "形＋光 / Volumeのみの試作" : "形だけ / 光は従来値（切り分け）");
            ImGui::TextWrapped("既存FBM評価数は同じ。丸い中心と薄い外周を別に密度へ変換。Volumeの基礎設定・時刻・Flowは書き換えず、描画時だけ物理値を上書きします。");
            ImGui::TextWrapped("Auto Applyは一時抑止。終了・サイズ変更・シーン終了で元に戻します。JSONへ自動保存しません。");
            if (constantData_) ImGui::Text("描画CB: noise %.3f / detail %.3f / weight %.3f / ambient %.2f / sun %.2f / lightAbs %.2f", constantData_->noiseScale, constantData_->detailNoiseScale, constantData_->detailWeight, constantData_->ambientLighting, constantData_->sunIntensity, constantData_->lightAbsorption);
        }
        ImGui::Text("Resolution %.3f / Interval %d / View x%.2f / Light x%.2f", settings_.cloudResolutionScale_,
            settings_.cloudRenderInterval_, settings_.viewStepScale_, settings_.lightStepScale_);
        if (constantData_) ImGui::Text("実際のCB: View %u / Light %u (距離LOD前)", constantData_->viewStepCount, constantData_->lightStepCount);
        if (!appearance) ImGui::TextWrapped("標準Volume基準256/8では192/4。変更は上記サンプリングのみ。形・密度・光・合成は開始時の値を保持。Auto Applyは一時抑止し、終了・サイズ変更・シーン終了で元の設定へ復元。自動保存しません。");
        ImGui::TextUnformatted("F1でGame / Debug切替。候補は切替中も保持します。");
        if (ImGui::Button("候補試用終了 / 元に戻す")) EndResolutionComparison();
        return;
    }
    ImGui::TextWrapped("ゲーム更新のみ固定中 (Pause設定は変更しません)。Auto Applyは適用されません。解像度以外は開始時の設定、Interval=1。終了すると元の設定・更新へ戻ります。Boost/Postの状態も開始時で固定します。");
    ImGui::Text("比較 %u / 4 / Resolution %.3f / Interval 1", comparison_.GetIndex() + 1, settings_.cloudResolutionScale_);
    const auto& timings = comparison_.GetTimings(comparison_.GetIndex());
    ImGui::Text("Warmup %u / samples %zu (上限240)", (std::min)(timings.warmup, 10u), timings.whole.size());
    auto percentile = [](std::vector<double> values, double p) {
        std::sort(values.begin(), values.end());
        return values[static_cast<size_t>(std::ceil(p * static_cast<double>(values.size()))) - 1];
    };
    if (!timings.whole.empty()) {
        ImGui::Text("全体 median/p95: %.4f / %.4f ms", percentile(timings.whole, .5), percentile(timings.whole, .95));
        ImGui::Text("Ray median/p95: %.4f / %.4f ms", percentile(timings.raymarch, .5), percentile(timings.raymarch, .95));
        ImGui::Text("合成 median/p95: %.4f / %.4f ms", percentile(timings.composite, .5), percentile(timings.composite, .95));
    }
    if (comparison_.IsTooHeavy()) ImGui::TextWrapped("上位比較を停止: 雲>100ms または全体>200msの実測。これは保護用の上限で、自動画質制御ではありません。");
    ImGui::BeginDisabled(comparison_.IsTooHeavy() || timings.whole.size() < 30 || comparison_.GetIndex() >= 3);
    if (ImGui::Button("次の解像度へ")) { comparison_.Next(settings_); hasValidCloudBuffer_ = false; }
    ImGui::EndDisabled();
    if (ImGui::Button("比較終了 / キャンセル (元に戻す)")) EndResolutionComparison();
#endif
}
