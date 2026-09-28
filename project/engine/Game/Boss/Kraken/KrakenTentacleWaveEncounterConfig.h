#pragma once

#include <string_view>

namespace KrakenTentacleWaveEncounterConfig {
inline constexpr std::string_view kTargetWaveId = "wave_004";
inline constexpr std::string_view kNextWaveId = "wave_005";
inline constexpr float kEntranceShakeDuration = 0.60f;
inline constexpr float kEntranceShakeAmplitude = 0.12f;
inline constexpr float kEntranceShakeFrequency = 24.0f;
inline constexpr float kEntranceRiseDuration = 0.35f;
inline constexpr float kEntranceRiseDistance = 18.0f;
struct EntranceSettings {
    float shakeDuration = kEntranceShakeDuration;
    float shakeAmplitude = kEntranceShakeAmplitude;
    float cameraBlendDuration = 0.45f;
    float riseDuration = kEntranceRiseDuration;
    float riseDistance = kEntranceRiseDistance;
};
}
