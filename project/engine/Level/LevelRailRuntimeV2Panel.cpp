#include "LevelRailRuntime.h"

#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"

void LevelRailRuntime::DrawRuntimeV2ImGui() {
    if (railPathRuntimeV2_) {
        railPathRuntimeV2_->DrawImGui();
    }
}

const RailPathRuntimeV2* LevelRailRuntime::GetRailPathRuntimeV2() const {
    return railPathRuntimeV2_.get();
}
