#include "GameScene.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterConfig.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"

void GameScene::InitializeKrakenWaveFoundation() {
    if (!enemyWaveManager_) {
        return;
    }
    if (enemyWaveManager_->ConfigureExternalWaveObjective(
        KrakenTentacleWaveEncounterConfig::kTargetWaveId)) {
        enemyWaveManager_->SetExternalWaveObjectiveCompleted(false);
    }
}

void GameScene::FinalizeKrakenWaveFoundation() {
    if (enemyWaveManager_) {
        enemyWaveManager_->ClearExternalWaveObjective();
    }
}
