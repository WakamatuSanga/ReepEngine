#pragma once
#include "VolumetricCloudSettings.h"
#include <cstdint>
#include <array>
#include <vector>

// Transient diagnostic settings transaction; never changes Pause, input, timers,
// JSON, presets, or RuntimeModeController's Auto Apply preference.
class CloudResolutionComparison {
public:
    bool Begin(VolumetricCloudSettings& settings, uint32_t width, uint32_t height);
    bool BeginTrial(VolumetricCloudSettings& settings, uint32_t width, uint32_t height,
        VolumetricCloudSettings::AppearanceMode appearance = VolumetricCloudSettings::AppearanceMode::Legacy);
    bool IsTrial() const { return active_ && trial_; }
    bool Next(VolumetricCloudSettings& settings);
    void End(VolumetricCloudSettings& settings);
    bool Maintain(VolumetricCloudSettings& settings, uint32_t width, uint32_t height);
    bool IsActive() const { return active_; }
    uint32_t GetIndex() const { return index_; }
    void SetTooHeavy(bool heavy) { tooHeavy_ = heavy; }
    bool IsTooHeavy() const { return tooHeavy_; }
    struct Timings { std::vector<double> raymarch, composite, whole; uint32_t warmup = 0; };
    void Observe(uint64_t frame, float resolution, double raymarch, double composite, double whole, bool reallocated);
    const Timings& GetTimings(uint32_t index) const { return timings_[index]; }
private:
    void Apply(VolumetricCloudSettings& settings) const;
    VolumetricCloudSettings saved_, locked_;
    uint32_t width_ = 0, height_ = 0, index_ = 0;
    bool active_ = false, tooHeavy_ = false, trial_ = false;
    uint64_t lastFrame_ = 0;
    std::array<Timings, 4> timings_;
};
