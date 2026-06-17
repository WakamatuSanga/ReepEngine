#pragma once
#include <cstddef>
#include <string>
#include <vector>

class PostEffectController;
struct FiredEventAction;

class PostEffectActionBridge {
public:
    PostEffectActionBridge();
    ~PostEffectActionBridge();

    void Initialize(PostEffectController* postEffectController);
    void Finalize();
    void DrawImGui();
    bool HandleAction(const FiredEventAction& action, std::string& resultMessage);

private:
    void AddLog(const std::string& message);
    static bool IsPlayPostEffectAction(const std::string& actionType);

    PostEffectController* postEffectController_ = nullptr;
    std::vector<std::string> actionLog_;
    std::string lastPostEffectType_ = "(none)";
    std::string lastResult_ = "(none)";
    bool enablePlayPostEffect_ = true;
    size_t playCount_ = 0;
    size_t failedCount_ = 0;
};
