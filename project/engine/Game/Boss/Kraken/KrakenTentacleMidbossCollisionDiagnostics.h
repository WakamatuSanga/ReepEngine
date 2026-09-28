#pragma once

#include "Engine/Game/Player/PlayerBulletManager.h"

#include <cstdint>
#include <vector>

enum class KrakenTentacleCollisionProjectileType : std::uint8_t;
struct KrakenTentacleMidbossDiagnostics;

KrakenTentacleCollisionProjectileType ToKrakenProjectileType(
    PlayerBulletManager::PlayerProjectileType type);

void RefreshKrakenBulletSnapshotDiagnostics(
    KrakenTentacleMidbossDiagnostics& diagnostics,
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>&
        snapshots);

bool AreKrakenBulletSnapshotsEquivalent(
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>& lhs,
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>& rhs);
