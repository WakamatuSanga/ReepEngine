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
    float spawnScreenY = 0.95f;
    float dropDuration = 0.6f;
    float enemyScale = 1.4f;
    float rotationDuringDrop = 720.0f;
};

struct EnemyWaveDefinition {
    std::string waveId;
    std::string name;
    float delay = 0.0f;
    std::string clearCondition = "AllDead";
    std::string nextWaveId;
    float clearDelayAfterAllDead = 3.5f;
    float clearDelayAfterAllEscaped = 0.0f;
    bool showWarningOnStart = false;
    std::string warningText = "WARNING";
    float warningDuration = 1.5f;
    bool waitForWarningBeforeSpawn = false;
    float postWarningDelay = 0.0f;
    std::vector<EnemyWaveSpawnEntry> enemies;
};
