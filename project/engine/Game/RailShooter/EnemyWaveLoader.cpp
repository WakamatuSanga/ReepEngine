#include "EnemyWaveLoader.h"
#include "Engine/Game/RailShooter/EnemyWaveData.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {
    bool ReadTextFile(const std::filesystem::path& path, std::string& outText) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        outText = stream.str();
        return true;
    }

    class WaveJsonReader {
    public:
        explicit WaveJsonReader(std::string_view source) : source_(source) {}

        bool Parse(EnemyWaveDefinition& wave, std::string& error) {
            SkipWhitespace();
            if (!ParseWaveObject(wave)) {
                error = error_.empty() ? "Invalid wave JSON." : error_;
                return false;
            }
            SkipWhitespace();
            if (position_ != source_.size()) {
                error = "Unexpected text after wave JSON root.";
                return false;
            }
            return true;
        }

    private:
        bool ParseWaveObject(EnemyWaveDefinition& wave) {
            if (!Consume('{')) {
                return Fail("Wave JSON root must be an object.");
            }
            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid wave object member.");
                }

                if (key == "waveId" || key == "wave_id") {
                    if (!ParseString(wave.waveId)) {
                        return Fail("Invalid waveId.");
                    }
                } else if (key == "name") {
                    if (!ParseString(wave.name)) {
                        return Fail("Invalid wave name.");
                    }
                } else if (key == "delay") {
                    if (!ParseFloat(wave.delay)) {
                        return Fail("Invalid wave delay.");
                    }
                } else if (key == "clearCondition" || key == "clear_condition") {
                    if (!ParseString(wave.clearCondition)) {
                        return Fail("Invalid clearCondition.");
                    }
                } else if (key == "nextWaveId" || key == "next_wave_id") {
                    if (!ParseString(wave.nextWaveId)) {
                        return Fail("Invalid nextWaveId.");
                    }
                } else if (key == "clearDelayAfterAllDead" || key == "clear_delay_after_all_dead") {
                    if (!ParseFloat(wave.clearDelayAfterAllDead)) {
                        return Fail("Invalid clearDelayAfterAllDead.");
                    }
                } else if (key == "clearDelayAfterAllEscaped" || key == "clear_delay_after_all_escaped") {
                    if (!ParseFloat(wave.clearDelayAfterAllEscaped)) {
                        return Fail("Invalid clearDelayAfterAllEscaped.");
                    }
                } else if (key == "enemies") {
                    if (!ParseEnemies(wave.enemies)) {
                        return false;
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in wave object.");
                }
            }
        }

        bool ParseEnemies(std::vector<EnemyWaveSpawnEntry>& enemies) {
            enemies.clear();
            if (!Consume('[')) {
                return Fail("Wave enemies must be an array.");
            }
            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                EnemyWaveSpawnEntry entry;
                if (!ParseEnemy(entry)) {
                    return false;
                }
                enemies.push_back(std::move(entry));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in enemies array.");
                }
            }
        }

        bool ParseEnemy(EnemyWaveSpawnEntry& entry) {
            if (!Consume('{')) {
                return Fail("Enemy spawn entry must be an object.");
            }
            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return Fail("Invalid enemy spawn member.");
                }

                if (key == "enemyType" || key == "enemy_type") {
                    if (!ParseString(entry.enemyType)) {
                        return Fail("Invalid enemyType.");
                    }
                } else if (key == "spawnTime" || key == "spawn_time") {
                    if (!ParseFloat(entry.spawnTime)) {
                        return Fail("Invalid spawnTime.");
                    }
                } else if (key == "screenX" || key == "screen_x") {
                    if (!ParseFloat(entry.screenX)) {
                        return Fail("Invalid screenX.");
                    }
                } else if (key == "screenY" || key == "screen_y") {
                    if (!ParseFloat(entry.screenY)) {
                        return Fail("Invalid screenY.");
                    }
                } else if (key == "depth") {
                    if (!ParseFloat(entry.depth)) {
                        return Fail("Invalid depth.");
                    }
                } else if (key == "movePattern" || key == "move_pattern") {
                    if (!ParseString(entry.movePattern)) {
                        return Fail("Invalid movePattern.");
                    }
                } else if (key == "attackPattern" || key == "attack_pattern") {
                    if (!ParseString(entry.attackPattern)) {
                        return Fail("Invalid attackPattern.");
                    }
                } else if (key == "spawnScreenY" || key == "spawn_screen_y") {
                    if (!ParseFloat(entry.spawnScreenY)) {
                        return Fail("Invalid spawnScreenY.");
                    }
                } else if (key == "dropDuration" || key == "drop_duration") {
                    if (!ParseFloat(entry.dropDuration)) {
                        return Fail("Invalid dropDuration.");
                    }
                } else if (key == "enemyScale" || key == "enemy_scale") {
                    if (!ParseFloat(entry.enemyScale)) {
                        return Fail("Invalid enemyScale.");
                    }
                } else if (key == "rotationDuringDrop" || key == "rotation_during_drop") {
                    if (!ParseFloat(entry.rotationDuringDrop)) {
                        return Fail("Invalid rotationDuringDrop.");
                    }
                } else if (!SkipValue()) {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma in enemy spawn entry.");
                }
            }
        }

        bool ParseString(std::string& out) {
            SkipWhitespace();
            if (position_ >= source_.size() || source_[position_] != '"') {
                return false;
            }
            ++position_;
            out.clear();
            while (position_ < source_.size()) {
                const char ch = source_[position_++];
                if (ch == '"') {
                    return true;
                }
                if (ch == '\\' && position_ < source_.size()) {
                    const char escaped = source_[position_++];
                    switch (escaped) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: out.push_back(escaped); break;
                    }
                } else {
                    out.push_back(ch);
                }
            }
            return Fail("Unterminated string.");
        }

        bool ParseFloat(float& out) {
            SkipWhitespace();
            const size_t start = position_;
            while (position_ < source_.size()) {
                const char ch = source_[position_];
                if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
                    ++position_;
                } else {
                    break;
                }
            }
            if (start == position_) {
                return false;
            }
            const std::string numberText(source_.substr(start, position_ - start));
            char* end = nullptr;
            const float value = std::strtof(numberText.c_str(), &end);
            if (end == numberText.c_str() || !std::isfinite(value)) {
                return false;
            }
            out = value;
            return true;
        }

        bool SkipValue() {
            SkipWhitespace();
            if (position_ >= source_.size()) {
                return Fail("Unexpected end of JSON value.");
            }
            if (source_[position_] == '"') {
                std::string ignored;
                return ParseString(ignored);
            }
            if (source_[position_] == '{') {
                return SkipObject();
            }
            if (source_[position_] == '[') {
                return SkipArray();
            }
            while (position_ < source_.size()) {
                const char ch = source_[position_];
                if (ch == ',' || ch == '}' || ch == ']' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
                    break;
                }
                ++position_;
            }
            return true;
        }

        bool SkipObject() {
            if (!Consume('{')) {
                return false;
            }
            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                std::string key;
                if (!ParseString(key) || !Consume(':') || !SkipValue()) {
                    return Fail("Invalid object while skipping JSON value.");
                }
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma while skipping JSON object.");
                }
            }
        }

        bool SkipArray() {
            if (!Consume('[')) {
                return false;
            }
            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!SkipValue()) {
                    return false;
                }
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return Fail("Expected comma while skipping JSON array.");
                }
            }
        }

        bool Consume(char expected) {
            SkipWhitespace();
            if (position_ < source_.size() && source_[position_] == expected) {
                ++position_;
                return true;
            }
            return false;
        }

        void SkipWhitespace() {
            if (position_ == 0 && source_.size() >= 3 &&
                static_cast<unsigned char>(source_[0]) == 0xEF &&
                static_cast<unsigned char>(source_[1]) == 0xBB &&
                static_cast<unsigned char>(source_[2]) == 0xBF) {
                position_ = 3;
            }
            while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
        }

        bool Fail(const std::string& message) {
            error_ = message;
            return false;
        }

        std::string_view source_;
        size_t position_ = 0;
        std::string error_;
    };
}

bool EnemyWaveLoader::LoadFromFile(const std::string& filePath, EnemyWaveDefinition& outWave, std::string& resultMessage) {
    std::string jsonText;
    if (!ReadTextFile(filePath, jsonText)) {
        resultMessage = "Failed to read wave file: " + filePath;
        return false;
    }

    EnemyWaveDefinition wave;
    std::string parseError;
    WaveJsonReader reader(jsonText);
    if (!reader.Parse(wave, parseError)) {
        resultMessage = "Failed to parse wave file: " + filePath + " error=" + parseError;
        return false;
    }

    if (wave.waveId.empty()) {
        wave.waveId = std::filesystem::path(filePath).stem().string();
    }
    if (wave.name.empty()) {
        wave.name = wave.waveId;
    }

    outWave = std::move(wave);
    resultMessage = "Loaded wave file: " + filePath;
    return true;
}