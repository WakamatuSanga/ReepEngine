#pragma once
#include <array>
#include <memory>

class Sprite;

// SceneManager owns the wipe so it survives TitleScene::Finalize.
class TitleCloudTransition {
public:
    TitleCloudTransition();
    ~TitleCloudTransition();
    void Initialize();
    void Update(float dt);
    void Draw();
    void OnFramePresented();
    void OnSceneChanged();
    bool CanSwitch() const;
    bool IsCovering() const;
    bool IsFinished() const;
private:
    enum class Phase { Cover, Covered, Loading, Reveal, Finished };
    Phase phase_ = Phase::Cover;
    float elapsed_ = 0.0f;
    bool presented_ = false;
    static constexpr size_t kLobes = 9;
    std::array<std::unique_ptr<Sprite>, kLobes * 4> lobes_;
    std::unique_ptr<Sprite> body_;
};
