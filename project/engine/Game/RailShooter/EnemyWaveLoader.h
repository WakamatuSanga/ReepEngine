#pragma once
#include <string>

struct EnemyWaveDefinition;

class EnemyWaveLoader {
public:
    static bool LoadFromFile(const std::string& filePath, EnemyWaveDefinition& outWave, std::string& resultMessage);
};