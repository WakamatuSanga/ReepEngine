#include "CloudResolutionComparison.h"

bool CloudResolutionComparison::Begin(VolumetricCloudSettings& settings, uint32_t width, uint32_t height) {
    if (active_ || !width || !height) return false;
    saved_ = settings; locked_ = settings;
    locked_.useLowResolutionCloud_ = true;
    locked_.cloudRenderInterval_ = 1;
    width_ = width; height_ = height; index_ = 0; active_ = true; tooHeavy_ = false; trial_ = false;
    timings_ = {}; lastFrame_ = 0;
    Apply(settings); return true;
}
bool CloudResolutionComparison::BeginTrial(VolumetricCloudSettings& settings, uint32_t width, uint32_t height, VolumetricCloudSettings::AppearanceMode appearance) {
    if (!Begin(settings, width, height)) return false;
    trial_ = true; index_ = 1;
    locked_ = saved_; locked_.ApplyAppearanceCandidate(appearance); Apply(settings);
    return true;
}
void CloudResolutionComparison::Apply(VolumetricCloudSettings& settings) const {
    constexpr float resolutions[]{ .125f, .25f, .5f, 1.0f };
    settings = locked_; settings.cloudResolutionScale_ = resolutions[index_];
}
bool CloudResolutionComparison::Next(VolumetricCloudSettings& settings) {
    if (!active_ || trial_ || tooHeavy_ || index_ >= 3) return false;
    ++index_; Apply(settings); return true;
}
void CloudResolutionComparison::End(VolumetricCloudSettings& settings) {
    if (!active_) return;
    settings = saved_; active_ = false; tooHeavy_ = false; trial_ = false;
}
bool CloudResolutionComparison::Maintain(VolumetricCloudSettings& settings, uint32_t width, uint32_t height) {
    if (!active_) return false;
    if (width != width_ || height != height_) { End(settings); return false; }
    Apply(settings); return true;
}

void CloudResolutionComparison::Observe(uint64_t frame, float resolution, double raymarch, double composite, double whole, bool reallocated) {
    constexpr float resolutions[]{ .125f, .25f, .5f, 1.0f };
    if (!active_ || frame <= lastFrame_ || resolution != resolutions[index_]) return;
    lastFrame_ = frame;
    if (raymarch + composite > 100.0 || whole > 200.0) tooHeavy_ = true;
    auto& timing = timings_[index_];
    if (reallocated) { timing.warmup = 0; return; }
    if (timing.warmup++ < 10 || timing.whole.size() >= 240) return;
    timing.raymarch.push_back(raymarch); timing.composite.push_back(composite); timing.whole.push_back(whole);
}
