#include "FlightTuning.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr const char* kPath = "resources/gameplay/flight_tuning.json";
struct Field { const char* key; const char* label; float FlightTuning::* value; float low; float high; };
const Field fields[] = {
    {"cameraDistance", "カメラと機体の距離", &FlightTuning::cameraDistance, 2.5f, 8},
    {"viewExpansion", "表示範囲の倍率", &FlightTuning::viewExpansion, 1.10f, 1.6f},
    {"cameraRoll", "操縦カメラ・ロール(rad)", &FlightTuning::cameraRoll, 0, .08f},
    {"cameraPitch", "操縦カメラ・上下(rad)", &FlightTuning::cameraPitch, 0, .025f},
    {"cameraInputDelay", "操縦カメラ・入力遅延(実秒)", &FlightTuning::cameraInputDelay, 0, .2f},
    {"cameraRollTime", "左右応答(実秒)", &FlightTuning::cameraRollTime, .01f, .3f},
    {"cameraPitchTime", "上下応答(実秒)", &FlightTuning::cameraPitchTime, .01f, .5f},
    {"bank", "機体バンク(rad)", &FlightTuning::bank, 0, .5f},
    {"bankTime", "機体応答(ゲーム秒)", &FlightTuning::bankTime, .01f, .3f},
    {"windRate", "通常風・生成/ゲーム秒", &FlightTuning::windRate, 10, 120},
    {"windSpeed", "通常風・速度", &FlightTuning::windSpeed, 15, 60},
    {"windLength", "通常風・線の長さ", &FlightTuning::windLength, .1f, 2},
    {"windWidth", "通常風・線の太さ", &FlightTuning::windWidth, .005f, .06f},
    {"windAlpha", "通常風・明るさ", &FlightTuning::windAlpha, .1f, 1},
    {"windBoost", "風のBoost倍率", &FlightTuning::windBoost, 1, 2},
    {"rollDuration", "一回転(ゲーム秒)", &FlightTuning::rollDuration, .15f, .8f},
    {"ghostLifetime", "残像寿命(ゲーム秒)", &FlightTuning::ghostLifetime, .03f, .25f},
    {"ghostAlpha", "残像の最大Alpha", &FlightTuning::ghostAlpha, 0, .4f},
    {"boostDuration", "旋回Boost(ゲーム秒)", &FlightTuning::boostDuration, .5f, 4},
    {"boostTimeScale", "Boost共通時間倍率", &FlightTuning::boostTimeScale, .1f, 1},
    {"timeResponse", "時間倍率の応答(実秒)", &FlightTuning::timeResponse, .01f, .2f},
    {"orbitExtra", "横旋回の追加量", &FlightTuning::orbitExtra, 0, .6f},
    {"orbitVertical", "上下旋回量", &FlightTuning::orbitVertical, 0, .8f},
    {"depthShrink", "奥への見かけの縮小率", &FlightTuning::depthShrink, 0, .4f},
    {"vignette", "画面中央ビネット", &FlightTuning::vignette, 0, 1},
    {"cloudSpeed", "Gameplay雲・基準流速(上限120)", &FlightTuning::cloudSpeed, 0, 120},
    {"defeatAmplitude", "撃破シェイク(度)", &FlightTuning::defeatAmplitude, 0, .65f},
    {"defeatDuration", "撃破シェイク(ゲーム秒)", &FlightTuning::defeatDuration, .04f, .5f},
    {"cancelAmplitude", "弾無効化シェイク(度)", &FlightTuning::cancelAmplitude, 0, .65f},
    {"cancelDuration", "弾無効化シェイク(ゲーム秒)", &FlightTuning::cancelDuration, .04f, .5f},
    {"shakeLimit", "イベント合成振幅の上限(度)", &FlightTuning::shakeLimit, .5f, .8f},
};
const char* status = "未保存の調整はRestartでも保持します。保存で次回起動にも適用します。";
}
FlightTuning& FlightTuning::Get() { static FlightTuning settings; return settings; }
void FlightTuning::LoadOnce() { static const bool loaded = Load(kPath); (void)loaded; }
bool FlightTuning::Load(const char* path) {
    std::ifstream file(path);
    if (!file) return false;
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    FlightTuning next; // optional fields always have a fallback, including old files
    for (const auto& field : fields) {
        const std::regex expression(std::string("\"") + field.key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
        std::smatch match;
        if (std::regex_search(json, match, expression)) {
            try {
                const float value = std::stof(match[1].str());
                if (std::isfinite(value)) next.*(field.value) = std::clamp(value, field.low, field.high);
            } catch (...) { return false; }
        }
    }
    Get() = next;
    return true;
}
bool FlightTuning::Save(const char* path) {
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
    if (error) return false;
    std::ofstream file(path);
    if (!file) return false;
    file << "{\n  \"version\": 1";
    for (const auto& field : fields) file << ",\n  \"" << field.key << "\": " << Get().*(field.value);
    file << "\n}\n";
    return static_cast<bool>(file);
}
void FlightTuning::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("飛行・採用プレビュー調整")) {
        if (ImGui::Button("設定を保存")) status = Save(kPath) ? "保存しました。" : "保存できませんでした。";
        ImGui::SameLine();
        if (ImGui::Button("設定を読み込む")) status = Load(kPath) ? "読み込みました。" : "ファイルがないか、読み込めませんでした。";
        ImGui::SameLine();
        if (ImGui::Button("採用値へ戻す")) Get() = FlightTuning{};
        ImGui::TextWrapped("%s", status);
        for (const auto& field : fields) {
            ImGui::SliderFloat(field.label, &(Get().*(field.value)), field.low, field.high, "%.3f");
        }
        ImGui::Text("残像は最大6描画、風は最大128粒・8粒/フレームです。");
    }
    ImGui::End();
#endif
}
