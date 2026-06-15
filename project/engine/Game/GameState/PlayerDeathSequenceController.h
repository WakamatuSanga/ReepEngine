#pragma once
#include <memory>

class CameraShakeController;
class DirectXCommon;
class Sprite;
class SpriteCommon;

class PlayerDeathSequenceController {
public:
    PlayerDeathSequenceController();
    ~PlayerDeathSequenceController();

    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, CameraShakeController* cameraShake);
    void Finalize();
    void StartDeathSequence();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();
    void Reset();
    bool ConsumeFinished();

    bool IsPlaying() const { return isPlaying_; }
    bool IsFinished() const { return isFinished_; }
    bool IsActiveOrFinished() const { return isPlaying_ || isFinished_; }

private:
    void ApplyPostEffect();
    void RestorePostEffect();
    float CalculateFadeAlpha() const;

    DirectXCommon* dxCommon_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    CameraShakeController* cameraShake_ = nullptr;
    std::unique_ptr<Sprite> fadeSprite_;

    bool isPlaying_ = false;
    bool isFinished_ = false;
    bool finishConsumed_ = false;
    bool enableGrayscale_ = true;
    bool savedPostEffect_ = false;
    unsigned int previousGrayscaleEnabled_ = 0;
    float previousGrayscaleIntensity_ = 1.0f;
    float elapsedTime_ = 0.0f;
    float deathSequenceDuration_ = 2.5f;
    float grayscaleDuration_ = 2.0f;
    float fadeStartTime_ = 1.0f;
    float fadeDuration_ = 1.0f;
    float shakeDuration_ = 0.7f;
    float shakeAmplitude_ = 0.08f;
    float shakeFrequency_ = 24.0f;
    float fadeAlpha_ = 0.0f;
};
