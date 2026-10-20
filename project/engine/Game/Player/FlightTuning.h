#pragma once

// Gameplay-only settings. Missing JSON members retain these defaults.
struct FlightTuning {
    float cameraDistance = 3.30f;
    float viewExpansion = 1.28f;
    float cameraRoll = 0.052f;
    float cameraPitch = 0.010f;
    float cameraInputDelay = 0.060f;
    float cameraRollTime = 0.120f;
    float cameraPitchTime = 0.18f;
    float bank = 0.30f;
    float bankTime = 0.07f;
    float windRate = 70.0f;
    float windSpeed = 28.0f;
    float windLength = 0.75f;
    float windWidth = 0.025f;
    float windAlpha = 0.70f;
    float windBoost = 1.65f;
    float rollDuration = 0.30f;
    float ghostLifetime = 0.135f;
    float ghostAlpha = 0.27f;
    float boostDuration = 1.8f;
    float boostTimeScale = 0.35f;
    float timeResponse = 0.055f;
    float orbitExtra = 0.35f;
    float orbitVertical = 0.45f;
    float depthShrink = 0.32f;
    float vignette = 0.48f;
    float cloudSpeed = 60.0f;
    float defeatAmplitude = 0.60f;
    float defeatDuration = 0.28f;
    float cancelAmplitude = 0.30f;
    float cancelDuration = 0.18f;
    float shakeLimit = 0.65f;

    static FlightTuning& Get();
    static void LoadOnce();
    static void DrawImGui();
    static bool Load(const char* path);
    static bool Save(const char* path);
};
