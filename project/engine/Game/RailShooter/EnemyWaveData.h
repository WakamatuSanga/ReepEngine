#pragma once
#include <string>
#include <vector>

struct EnemyWaveSpawnEntry {
    std::string enemyType = "Default";
    float spawnTime = 0.0f;
    float screenX = 0.0f;
    float screenY = 0.0f;
    float depth = 80.0f;
    std::string movePattern = "Straight";
    std::string attackPattern = "SingleShot";
};

struct EnemyWaveDefinition {
    std::string waveId;
    std::string name;
    float delay = 0.0f;
    std::string clearCondition = "AllDead";
    std::vector<EnemyWaveSpawnEntry> enemies;
};