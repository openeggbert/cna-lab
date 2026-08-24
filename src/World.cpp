#include "World.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"

namespace WolfCna
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float WallHeight = 1.0f;
        constexpr float DoorThickness = 0.12f;
        constexpr float DoorOpenSpeed = 1.8f;
        constexpr float DoorPassableAt = 0.78f;
        constexpr float DoorAutoCloseDelay = 4.0f;
        constexpr float DoorBodyHoldRadius = 0.45f;
        constexpr float DoorPlayerClearance = 0.22f;
        constexpr float ActivationRange = 1.5f;
        constexpr float ActivationDotThreshold = 0.5f;
        constexpr float HitScanRange = 12.0f;
        constexpr float HitScanStep = 0.02f;
        constexpr float ImpactHalfSize = 0.075f;
        constexpr float ImpactSurfaceOffset = 0.003f;
        constexpr float EnemyWakeRange = 7.0f;
        constexpr float EnemyCloseAwarenessRange = 0.8f;
        constexpr float EnemySearchSeconds = 2.0f;
        constexpr float HoundAttackRange = 0.85f;
        constexpr float GuardAttackRange = 6.0f;
        constexpr float EnemySpeed = 0.8f;
        constexpr float EnemyPathRefreshSeconds = 0.35f;
        constexpr float GuardProjectileSpeed = 4.5f;
        constexpr float GuardProjectileHitRadius = 0.25f;
        constexpr float GuardProjectileLifetime = 2.0f;
        constexpr float EnemyImpactDuration = 0.28f;
        constexpr std::size_t MaxEnemyImpactCount = 12;
        constexpr float RangedAttackVisualSeconds = 0.18f;
        constexpr float HoundAttackVisualSeconds = 0.24f;
        constexpr float EnemyPainVisualSeconds = 0.16f;
        constexpr float PickupRadius = 0.42f;
        constexpr float ExitRadius = 0.45f;

        [[nodiscard]] int ScalePositiveAmount(int amount, float multiplier)
        {
            return std::max(1, static_cast<int>(std::lround(
                static_cast<float>(amount) * multiplier)));
        }
    }

    World::World(const LevelDefinition& level, Difficulty difficulty)
        : map_(level.Rows())
        , playerStart_(
            static_cast<float>(level.PlayerStartX()) + 0.5f,
            0.62f,
            static_cast<float>(level.PlayerStartZ()) + 0.5f)
        , difficultyProfile_(GetDifficultyProfile(difficulty))
    {
        impacts_.reserve(MaxImpactCount);
        BuildDoors();
        BuildEnemies();
        BuildPickups();
        BuildTerminals();
        BuildRelays();
        BuildExits();
        BuildDecorations();
        BuildMesh();
        RebuildDoorGeometry();
        BuildImpactGeometry();
        BuildBillboardGeometry();
        BuildBloodPoolGeometry();
    }

    Vector3 World::PlayerStart() const
    {
        return playerStart_;
    }

    bool World::IsStaticWallCell(int x, int z) const
    {
        if (z < 0 || z >= static_cast<int>(map_.size()))
            return true;
        if (x < 0 || x >= static_cast<int>(map_[z].size()))
            return true;
        return map_[z][x] == '#';
    }

    bool World::IsBlockedCell(int x, int z) const
    {
        if (IsStaticWallCell(x, z))
            return true;
        if (map_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] == 'Y')
            return true;

        for (const Exit& exit : exits_)
        {
            if (exit.x == x && exit.z == z)
                return exit.openAmount < DoorPassableAt;
        }

        for (const Door& door : doors_)
        {
            if (door.x == x && door.z == z)
                return door.openAmount < DoorPassableAt;
        }

        return false;
    }

    bool World::IsNavigationBlockedCell(
        int x,
        int z,
        bool allowOrdinaryDoors) const
    {
        if (IsStaticWallCell(x, z))
            return true;
        if (map_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] == 'Y' ||
            IsExitCell(x, z))
            return true;

        for (const Door& door : doors_)
        {
            if (door.x != x || door.z != z || door.openAmount >= DoorPassableAt)
                continue;
            if (allowOrdinaryDoors && door.material == Material::Door && !door.isSecret)
                return false;
            return true;
        }

        return false;
    }

    bool World::IsExitCell(int x, int z) const
    {
        if (z < 0 || z >= static_cast<int>(map_.size()))
            return false;
        if (x < 0 || x >= static_cast<int>(map_[static_cast<std::size_t>(z)].size()))
            return false;
        const char symbol = map_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
        return symbol == 'E' || symbol == 'X';
    }

    World::Material World::WallMaterialForCell(int x, int z) const
    {
        constexpr std::array materials = {
            Material::WallStone,
            Material::WallBrick,
            Material::WallSteel,
            Material::WallLab};
        constexpr int materialRegionSize = 8;
        const int regionX = std::max(0, x) / materialRegionSize;
        const int regionZ = std::max(0, z) / materialRegionSize;
        const std::size_t index = static_cast<std::size_t>(
            (regionX * 3 + regionZ * 5) % static_cast<int>(materials.size()));
        return materials[index];
    }

    bool World::HasDeadEnemyInDoorway(const Door& door) const
    {
        const float centerX = static_cast<float>(door.x) + 0.5f;
        const float centerZ = static_cast<float>(door.z) + 0.5f;

        for (const Enemy& enemy : enemies_)
        {
            if (enemy.state != EnemyState::Dead)
                continue;

            const float dx = enemy.position.X - centerX;
            const float dz = enemy.position.Z - centerZ;
            if (dx * dx + dz * dz <= DoorBodyHoldRadius * DoorBodyHoldRadius)
                return true;
        }

        return false;
    }

    bool World::HasPlayerInDoorway(const Door& door, const Vector3& playerPosition) const
    {
        const float centerX = static_cast<float>(door.x) + 0.5f;
        const float centerZ = static_cast<float>(door.z) + 0.5f;
        const float doorwayHalfExtent = 0.5f + DoorPlayerClearance;
        return std::abs(playerPosition.X - centerX) <= doorwayHalfExtent &&
            std::abs(playerPosition.Z - centerZ) <= doorwayHalfExtent;
    }

    bool World::Collides(float worldX, float worldZ, float radius) const
    {
        const std::array<Vector2, 8> probes = {
            Vector2(-radius, -radius),
            Vector2( radius, -radius),
            Vector2(-radius,  radius),
            Vector2( radius,  radius),
            Vector2(-radius, 0.0f),
            Vector2( radius, 0.0f),
            Vector2(0.0f, -radius),
            Vector2(0.0f,  radius)
        };

        for (const Vector2& p : probes)
        {
            const int cellX = static_cast<int>(std::floor(worldX + p.X));
            const int cellZ = static_cast<int>(std::floor(worldZ + p.Y));
            if (IsBlockedCell(cellX, cellZ))
                return true;
        }

        return false;
    }

    World::AttackResult World::FireHitscan(
        const Vector3& playerPosition,
        const Vector3& lookDirection,
        float range,
        bool emitsNoise)
    {
        if (emitsNoise)
            pendingPlayerNoise_ = playerPosition;

        int previousCellX = static_cast<int>(std::floor(playerPosition.X));
        int previousCellZ = static_cast<int>(std::floor(playerPosition.Z));

        for (float distance = HitScanStep; distance <= std::min(HitScanRange, range); distance += HitScanStep)
        {
            const float rayX = playerPosition.X + lookDirection.X * distance;
            const float rayZ = playerPosition.Z + lookDirection.Z * distance;
            const int cellX = static_cast<int>(std::floor(rayX));
            const int cellZ = static_cast<int>(std::floor(rayZ));

            for (Enemy& enemy : enemies_)
            {
                if (enemy.state == EnemyState::Dead)
                    continue;

                const float enemyX = enemy.position.X - rayX;
                const float enemyZ = enemy.position.Z - rayZ;
                if (enemyX * enemyX + enemyZ * enemyZ > 0.08f)
                    continue;

                --enemy.health;
                const bool defeated = enemy.health <= 0;
                enemy.state = defeated ? EnemyState::Dead : EnemyState::Chase;
                enemy.attackVisualSeconds = 0.0f;
                enemy.painVisualSeconds = defeated ? 0.0f : EnemyPainVisualSeconds;
                if (defeated)
                {
                    ++defeatedEnemies_;
                    if (!enemy.melee)
                    {
                        pickups_.push_back({
                            Vector3(enemy.position.X, 0.08f, enemy.position.Z),
                            PickupType::Ammo,
                            false,
                            enemy.ammunitionDrop});
                    }
                }
                return {true, defeated ? enemy.scoreValue : 0};
            }

            if (!IsStaticWallCell(cellX, cellZ))
            {
                previousCellX = cellX;
                previousCellZ = cellZ;
                continue;
            }

            Vector3 normal(0.0f, 0.0f, 0.0f);
            float impactX = rayX;
            float impactZ = rayZ;

            if (cellX != previousCellX)
            {
                normal.X = lookDirection.X > 0.0f ? -1.0f : 1.0f;
                impactX = normal.X < 0.0f ? static_cast<float>(cellX) : static_cast<float>(cellX + 1);
            }
            else if (cellZ != previousCellZ)
            {
                normal.Z = lookDirection.Z > 0.0f ? -1.0f : 1.0f;
                impactZ = normal.Z < 0.0f ? static_cast<float>(cellZ) : static_cast<float>(cellZ + 1);
            }
            else
            {
                return {};
            }

            if (impacts_.size() == MaxImpactCount)
                impacts_.erase(impacts_.begin());
            impacts_.push_back({Vector3(impactX, 0.62f, impactZ), normal});
            BuildImpactGeometry();

            if (impactVertexBuffer_)
            {
                impactVertexBuffer_->SetData(
                    impactVertices_.data(),
                    static_cast<int>(impactVertices_.size()));
            }

            return {true, 0};
        }

        return {};
    }

    World::PickupResult World::CollectPickups(
        const Vector3& playerPosition,
        int currentHealth)
    {
        PickupResult result;

        for (Pickup& pickup : pickups_)
        {
            if (pickup.collected)
                continue;

            const float dx = pickup.position.X - playerPosition.X;
            const float dz = pickup.position.Z - playerPosition.Z;
            if (dx * dx + dz * dz > PickupRadius * PickupRadius)
                continue;

            if (pickup.type == PickupType::Health)
            {
                const int missingHealth = 100 - currentHealth - result.health;
                if (missingHealth <= 0)
                    continue;
                pickup.collected = true;
                result.health += std::min(25, missingHealth);
            }
            else if (pickup.type == PickupType::Ammo)
            {
                pickup.collected = true;
                result.ammo += pickup.amount;
            }
            else if (pickup.type == PickupType::GoldBars ||
                pickup.type == PickupType::GoldenGoblet ||
                pickup.type == PickupType::PeaceMedallion)
            {
                pickup.collected = true;
                result.gold += pickup.type == PickupType::GoldBars
                    ? 100
                    : pickup.type == PickupType::GoldenGoblet ? 250 : 500;
                ++collectedGold_;
            }
            else if (pickup.type == PickupType::AccessCard)
            {
                pickup.collected = true;
                ++result.accessCards;
            }
            else if (pickup.type == PickupType::RepeaterWeapon)
            {
                pickup.collected = true;
                ++result.repeaterWeapons;
                result.ammo += pickup.amount;
            }
            else
            {
                pickup.collected = true;
                ++result.heavyWeapons;
                result.ammo += pickup.amount;
            }
        }

        return result;
    }

    bool World::ReachedExit(const Vector3& playerPosition) const
    {
        return ReachedExitRoute(playerPosition).has_value();
    }

    std::optional<World::ExitRoute> World::ReachedExitRoute(
        const Vector3& playerPosition) const
    {
        const BossStatus boss = GetBossStatus();
        if (boss.present && !boss.defeated)
            return std::nullopt;

        for (const Exit& exit : exits_)
        {
            const float dx = exit.position.X - playerPosition.X;
            const float dz = exit.position.Z - playerPosition.Z;
            if (exit.openAmount >= DoorPassableAt &&
                dx * dx + dz * dz <= ExitRadius * ExitRadius)
                return exit.route;
        }

        return std::nullopt;
    }

    bool World::AreObjectivesComplete() const
    {
        const bool terminalsActivated = terminals_.empty() || std::all_of(
            terminals_.begin(),
            terminals_.end(),
            [](const Terminal& terminal) { return terminal.activated; });
        const bool relaysActivated = relays_.empty() || std::all_of(
            relays_.begin(),
            relays_.end(),
            [](const Relay& relay) { return relay.activated; });
        return terminalsActivated && relaysActivated;
    }

    std::optional<World::ExitApproach> World::GetExitApproach() const
    {
        if (exits_.empty())
            return std::nullopt;

        const auto standardExit = std::find_if(
            exits_.begin(),
            exits_.end(),
            [](const Exit& exit) { return exit.route == ExitRoute::Standard; });
        const Exit& exit = standardExit != exits_.end() ? *standardExit : exits_.front();
        return ExitApproach{
            Vector3(
                exit.position.X + static_cast<float>(exit.approachX),
                playerStart_.Y,
                exit.position.Z + static_cast<float>(exit.approachZ)),
            Vector3(
                -static_cast<float>(exit.approachX),
                0.0f,
                -static_cast<float>(exit.approachZ))};
    }

    World::ObjectiveStatus World::GetObjectiveStatus() const
    {
        return {
            static_cast<int>(std::count_if(
                relays_.begin(),
                relays_.end(),
                [](const Relay& relay) { return relay.activated; })),
            static_cast<int>(relays_.size()),
            static_cast<int>(std::count_if(
                terminals_.begin(),
                terminals_.end(),
                [](const Terminal& terminal) { return terminal.activated; })),
            static_cast<int>(terminals_.size())};
    }

    World::CompletionStats World::GetCompletionStats() const
    {
        return {
            defeatedEnemies_,
            static_cast<int>(enemies_.size()),
            collectedGold_,
            totalGold_,
            foundSecrets_,
            totalSecrets_};
    }

    World::DifficultyBalance World::GetDifficultyBalance() const
    {
        return {
            .activeEnemies = static_cast<int>(enemies_.size()),
            .totalEnemyHealth = totalEnemyHealth_,
            .fixedAmmunition = fixedAmmunition_,
            .potentialDroppedAmmunition = potentialDroppedAmmunition_};
    }

    World::EnemyBehaviorStats World::GetEnemyBehaviorStats() const
    {
        EnemyBehaviorStats result;
        for (const Enemy& enemy : enemies_)
        {
            result.totalTravelDistance += enemy.distanceTravelled;
            result.ambushEnemies += enemy.ambush ? 1 : 0;
            switch (enemy.state)
            {
            case EnemyState::Idle: ++result.idleEnemies; break;
            case EnemyState::Patrol: ++result.patrollingEnemies; break;
            case EnemyState::Alert: ++result.alertingEnemies; break;
            case EnemyState::Chase: ++result.chasingEnemies; break;
            case EnemyState::Attack: ++result.attackingEnemies; break;
            case EnemyState::Search: ++result.searchingEnemies; break;
            case EnemyState::Dead: ++result.deadEnemies; break;
            }
        }
        return result;
    }

    World::BossStatus World::GetBossStatus() const
    {
        for (const Enemy& enemy : enemies_)
        {
            if (enemy.type != Enemy::Type::Boss)
                continue;
            return {
                true,
                enemy.state == EnemyState::Dead,
                std::max(0, enemy.health),
                ScalePositiveAmount(32, difficultyProfile_.enemyHealthMultiplier)};
        }
        return {};
    }

    World::SaveState World::CaptureSaveState() const
    {
        SaveState state;
        state.defeatedEnemies = defeatedEnemies_;
        state.collectedGold = collectedGold_;
        state.foundSecrets = foundSecrets_;
        state.doors.reserve(doors_.size());
        for (const Door& door : doors_)
            state.doors.push_back({door.opening, door.openAmount, door.closeDelay});
        state.enemies.reserve(enemies_.size());
        for (const Enemy& enemy : enemies_)
        {
            state.enemies.push_back({
                .state = static_cast<int>(enemy.state),
                .health = enemy.health,
                .positionX = enemy.position.X,
                .positionZ = enemy.position.Z,
                .facingX = enemy.facing.X,
                .facingZ = enemy.facing.Z,
                .lastKnownX = enemy.lastKnownTarget.X,
                .lastKnownZ = enemy.lastKnownTarget.Z,
                .attackCooldown = enemy.attackCooldown,
                .attackVisualSeconds = enemy.attackVisualSeconds,
                .painVisualSeconds = enemy.painVisualSeconds,
                .visualAnimationSeconds = enemy.visualAnimationSeconds,
                .reactionRemaining = enemy.reactionRemaining,
                .searchRemaining = enemy.searchRemaining,
                .distanceTravelled = enemy.distanceTravelled});
        }
        state.pickups.reserve(pickups_.size());
        for (const Pickup& pickup : pickups_)
        {
            state.pickups.push_back({
                .positionX = pickup.position.X,
                .positionZ = pickup.position.Z,
                .type = static_cast<int>(pickup.type),
                .collected = pickup.collected,
                .amount = pickup.amount});
        }
        state.terminalsActivated.reserve(terminals_.size());
        for (const Terminal& terminal : terminals_)
            state.terminalsActivated.push_back(terminal.activated);
        state.relaysActivated.reserve(relays_.size());
        for (const Relay& relay : relays_)
            state.relaysActivated.push_back(relay.activated);
        state.exitOpenAmounts.reserve(exits_.size());
        for (const Exit& exit : exits_)
            state.exitOpenAmounts.push_back(exit.openAmount);
        state.projectiles.reserve(enemyProjectiles_.size());
        for (const EnemyProjectile& projectile : enemyProjectiles_)
        {
            state.projectiles.push_back({
                .positionX = projectile.position.X,
                .positionY = projectile.position.Y,
                .positionZ = projectile.position.Z,
                .velocityX = projectile.velocity.X,
                .velocityY = projectile.velocity.Y,
                .velocityZ = projectile.velocity.Z,
                .remainingLifetime = projectile.remainingLifetime,
                .damage = projectile.damage});
        }
        return state;
    }

    bool World::RestoreSaveState(const SaveState& state)
    {
        const auto finite = [](float value) { return std::isfinite(value); };
        const auto validPosition = [this, &finite](float x, float z)
        {
            if (!finite(x) || !finite(z))
                return false;
            const int cellX = static_cast<int>(std::floor(x));
            const int cellZ = static_cast<int>(std::floor(z));
            return cellZ >= 0 && cellZ < static_cast<int>(map_.size()) &&
                cellX >= 0 && cellX < static_cast<int>(map_[static_cast<std::size_t>(cellZ)].size()) &&
                !IsStaticWallCell(cellX, cellZ);
        };
        if (state.doors.size() != doors_.size() || state.enemies.size() != enemies_.size() ||
            state.terminalsActivated.size() != terminals_.size() ||
            state.relaysActivated.size() != relays_.size() ||
            state.exitOpenAmounts.size() != exits_.size() ||
            state.pickups.size() < basePickupCount_ ||
            state.pickups.size() > basePickupCount_ + enemies_.size() ||
            state.projectiles.size() > 256 ||
            state.defeatedEnemies < 0 ||
            state.defeatedEnemies > static_cast<int>(enemies_.size()) ||
            state.collectedGold < 0 || state.collectedGold > totalGold_ ||
            state.foundSecrets < 0 || state.foundSecrets > totalSecrets_)
            return false;

        for (const DoorSaveState& door : state.doors)
        {
            if (!finite(door.openAmount) || door.openAmount < 0.0f || door.openAmount > 1.0f ||
                !finite(door.closeDelay) || door.closeDelay < 0.0f || door.closeDelay > 60.0f)
                return false;
        }
        int savedDeadEnemies = 0;
        for (const EnemySaveState& enemy : state.enemies)
        {
            if (enemy.state < static_cast<int>(EnemyState::Idle) ||
                enemy.state > static_cast<int>(EnemyState::Dead) ||
                enemy.health < -1000 || enemy.health > 1000 ||
                !validPosition(enemy.positionX, enemy.positionZ) ||
                !finite(enemy.facingX) || !finite(enemy.facingZ) ||
                !validPosition(enemy.lastKnownX, enemy.lastKnownZ) ||
                !finite(enemy.attackCooldown) || enemy.attackCooldown < -60.0f ||
                enemy.attackCooldown > 60.0f ||
                !finite(enemy.attackVisualSeconds) || enemy.attackVisualSeconds < 0.0f ||
                enemy.attackVisualSeconds > 10.0f ||
                !finite(enemy.painVisualSeconds) || enemy.painVisualSeconds < 0.0f ||
                enemy.painVisualSeconds > 10.0f ||
                !finite(enemy.visualAnimationSeconds) || enemy.visualAnimationSeconds < 0.0f ||
                enemy.visualAnimationSeconds > 1000.0f ||
                !finite(enemy.reactionRemaining) || enemy.reactionRemaining < 0.0f ||
                enemy.reactionRemaining > 60.0f ||
                !finite(enemy.searchRemaining) || enemy.searchRemaining < 0.0f ||
                enemy.searchRemaining > 60.0f ||
                !finite(enemy.distanceTravelled) || enemy.distanceTravelled < 0.0f ||
                enemy.distanceTravelled > 1000000.0f)
                return false;
            const bool dead = enemy.state == static_cast<int>(EnemyState::Dead);
            if ((dead && enemy.health > 0) || (!dead && enemy.health <= 0))
                return false;
            savedDeadEnemies += dead ? 1 : 0;
        }
        if (savedDeadEnemies != state.defeatedEnemies)
            return false;

        int savedCollectedGold = 0;
        for (std::size_t index = 0; index < state.pickups.size(); ++index)
        {
            const PickupSaveState& pickup = state.pickups[index];
            if (!validPosition(pickup.positionX, pickup.positionZ) || pickup.type < 0 ||
                pickup.type > static_cast<int>(PickupType::HeavyWeapon) ||
                pickup.amount < 0 || pickup.amount > 999)
                return false;
            if (index < basePickupCount_)
            {
                const Pickup& authored = pickups_[index];
                if (pickup.type != static_cast<int>(authored.type) ||
                    std::abs(pickup.positionX - authored.position.X) > 0.001f ||
                    std::abs(pickup.positionZ - authored.position.Z) > 0.001f ||
                    pickup.amount != authored.amount)
                    return false;
            }
            else if (pickup.type != static_cast<int>(PickupType::Ammo) || pickup.amount <= 0)
            {
                return false;
            }
            if (pickup.collected &&
                (pickup.type == static_cast<int>(PickupType::GoldBars) ||
                    pickup.type == static_cast<int>(PickupType::GoldenGoblet) ||
                    pickup.type == static_cast<int>(PickupType::PeaceMedallion)))
                ++savedCollectedGold;
        }
        if (savedCollectedGold != state.collectedGold)
            return false;
        int savedFoundSecrets = 0;
        for (std::size_t index = 0; index < doors_.size(); ++index)
        {
            if (doors_[index].isSecret &&
                (state.doors[index].opening || state.doors[index].openAmount > 0.0f))
                ++savedFoundSecrets;
        }
        if (savedFoundSecrets != state.foundSecrets)
            return false;
        for (float openAmount : state.exitOpenAmounts)
        {
            if (!finite(openAmount) || openAmount < 0.0f || openAmount > 1.0f)
                return false;
        }
        for (const ProjectileSaveState& projectile : state.projectiles)
        {
            if (!validPosition(projectile.positionX, projectile.positionZ) ||
                !finite(projectile.positionY) || projectile.positionY < -10.0f ||
                projectile.positionY > 10.0f || !finite(projectile.velocityX) ||
                !finite(projectile.velocityY) || !finite(projectile.velocityZ) ||
                !finite(projectile.remainingLifetime) || projectile.remainingLifetime < 0.0f ||
                projectile.remainingLifetime > 10.0f || projectile.damage < 0 ||
                projectile.damage > 1000)
                return false;
        }

        for (std::size_t index = 0; index < doors_.size(); ++index)
        {
            doors_[index].opening = state.doors[index].opening;
            doors_[index].openAmount = state.doors[index].openAmount;
            doors_[index].closeDelay = state.doors[index].closeDelay;
        }
        for (std::size_t index = 0; index < enemies_.size(); ++index)
        {
            Enemy& enemy = enemies_[index];
            const EnemySaveState& saved = state.enemies[index];
            enemy.state = static_cast<EnemyState>(saved.state);
            enemy.health = saved.health;
            enemy.position.X = saved.positionX;
            enemy.position.Z = saved.positionZ;
            enemy.facing = Vector3(saved.facingX, 0.0f, saved.facingZ);
            enemy.lastKnownTarget = Vector3(saved.lastKnownX, 0.0f, saved.lastKnownZ);
            enemy.attackCooldown = saved.attackCooldown;
            enemy.attackVisualSeconds = saved.attackVisualSeconds;
            enemy.painVisualSeconds = saved.painVisualSeconds;
            enemy.visualAnimationSeconds = saved.visualAnimationSeconds;
            enemy.reactionRemaining = saved.reactionRemaining;
            enemy.searchRemaining = saved.searchRemaining;
            enemy.distanceTravelled = saved.distanceTravelled;
            enemy.path.clear();
            enemy.pathIndex = 0;
            enemy.pathRefreshTime = 0.0f;
        }
        pickups_.clear();
        pickups_.reserve(state.pickups.size());
        for (const PickupSaveState& saved : state.pickups)
        {
            pickups_.push_back({
                Vector3(saved.positionX, 0.08f, saved.positionZ),
                static_cast<PickupType>(saved.type),
                saved.collected,
                saved.amount});
        }
        for (std::size_t index = 0; index < terminals_.size(); ++index)
            terminals_[index].activated = state.terminalsActivated[index];
        for (std::size_t index = 0; index < relays_.size(); ++index)
            relays_[index].activated = state.relaysActivated[index];
        for (std::size_t index = 0; index < exits_.size(); ++index)
            exits_[index].openAmount = state.exitOpenAmounts[index];
        enemyProjectiles_.clear();
        enemyProjectiles_.reserve(state.projectiles.size());
        for (const ProjectileSaveState& saved : state.projectiles)
        {
            enemyProjectiles_.push_back({
                Vector3(saved.positionX, saved.positionY, saved.positionZ),
                Vector3(saved.velocityX, saved.velocityY, saved.velocityZ),
                saved.remainingLifetime,
                saved.damage});
        }
        defeatedEnemies_ = state.defeatedEnemies;
        collectedGold_ = state.collectedGold;
        foundSecrets_ = state.foundSecrets;
        enemyImpacts_.clear();
        impacts_.clear();
        pendingGuardShotCount_ = 0;
        pendingEnemyAudioEvents_ = {};
        pendingPlayerNoise_.reset();
        RebuildDoorGeometry();
        BuildImpactGeometry();
        if (doorVertexBuffer_)
            doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));
        return true;
    }

    int World::ConsumeGuardShotCount()
    {
        const int shotCount = pendingGuardShotCount_;
        pendingGuardShotCount_ = 0;
        return shotCount;
    }

    World::EnemyAudioEvents World::ConsumeEnemyAudioEvents()
    {
        const EnemyAudioEvents events = pendingEnemyAudioEvents_;
        pendingEnemyAudioEvents_ = {};
        return events;
    }

    int World::ActiveEnemyImpactCount() const
    {
        return static_cast<int>(enemyImpacts_.size());
    }

    void World::AddEnemyImpact(const Vector3& position, bool hitPlayer)
    {
        if (enemyImpacts_.size() == MaxEnemyImpactCount)
            enemyImpacts_.erase(enemyImpacts_.begin());
        enemyImpacts_.push_back({position, EnemyImpactDuration, hitPlayer});
        ++pendingEnemyAudioEvents_.projectileImpacts;
    }

    World::InteractionResult World::TryActivate(
        const Vector3& playerPosition,
        const Vector3& lookDirection,
        bool hasSecurityCard)
    {
        Door* target = nullptr;
        Terminal* targetTerminal = nullptr;
        Relay* targetRelay = nullptr;
        Exit* targetExit = nullptr;
        float closestDistanceSquared = ActivationRange * ActivationRange;

        for (Door& door : doors_)
        {
            if (door.opening || door.openAmount >= 1.0f)
                continue;

            const float offsetX = static_cast<float>(door.x) + 0.5f - playerPosition.X;
            const float offsetZ = static_cast<float>(door.z) + 0.5f - playerPosition.Z;
            const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > closestDistanceSquared || distanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
            const float facing = (offsetX * lookDirection.X + offsetZ * lookDirection.Z) * inverseDistance;
            if (facing < ActivationDotThreshold)
                continue;

            target = &door;
            closestDistanceSquared = distanceSquared;
        }

        for (Terminal& terminal : terminals_)
        {
            if (terminal.activated)
                continue;

            const float offsetX = terminal.position.X - playerPosition.X;
            const float offsetZ = terminal.position.Z - playerPosition.Z;
            const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > closestDistanceSquared || distanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
            const float facing = (offsetX * lookDirection.X + offsetZ * lookDirection.Z) * inverseDistance;
            if (facing < ActivationDotThreshold)
                continue;

            targetTerminal = &terminal;
            closestDistanceSquared = distanceSquared;
        }

        for (Relay& relay : relays_)
        {
            if (relay.activated)
                continue;

            const float offsetX = relay.position.X - playerPosition.X;
            const float offsetZ = relay.position.Z - playerPosition.Z;
            const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > closestDistanceSquared || distanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
            const float facing = (offsetX * lookDirection.X + offsetZ * lookDirection.Z) * inverseDistance;
            if (facing < ActivationDotThreshold)
                continue;

            targetRelay = &relay;
            targetTerminal = nullptr;
            closestDistanceSquared = distanceSquared;
        }

        for (Exit& exit : exits_)
        {
            const float offsetX = exit.position.X - playerPosition.X;
            const float offsetZ = exit.position.Z - playerPosition.Z;
            const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > closestDistanceSquared || distanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
            const float facing =
                (offsetX * lookDirection.X + offsetZ * lookDirection.Z) * inverseDistance;
            if (facing < ActivationDotThreshold)
                continue;

            targetExit = &exit;
            targetRelay = nullptr;
            targetTerminal = nullptr;
            target = nullptr;
            closestDistanceSquared = distanceSquared;
        }

        if (targetExit)
        {
            const BossStatus boss = GetBossStatus();
            if (boss.present && !boss.defeated)
                return InteractionResult::ExitSealed;
            return targetExit->route == ExitRoute::Secret
                ? InteractionResult::SecretExitActivated
                : InteractionResult::ExitActivated;
        }

        if (targetRelay)
        {
            targetRelay->activated = true;
            return InteractionResult::RelayActivated;
        }

        if (targetTerminal)
        {
            targetTerminal->activated = true;
            return InteractionResult::TerminalActivated;
        }

        if (!target)
            return InteractionResult::None;

        if (target->material == Material::SecurityDoor && !hasSecurityCard)
            return InteractionResult::DoorLocked;

        target->opening = true;
        target->closeDelay = DoorAutoCloseDelay;
        if (target->isSecret)
        {
            ++foundSecrets_;
            return InteractionResult::SecretRevealed;
        }
        return InteractionResult::DoorOpened;
    }

    int World::Update(
        float elapsedSeconds,
        const Vector3& playerPosition)
    {
        bool changed = false;
        int damage = 0;

        for (Door& door : doors_)
        {
            if (door.opening)
            {
                const float previousAmount = door.openAmount;
                door.openAmount = std::min(1.0f, door.openAmount + DoorOpenSpeed * elapsedSeconds);
                changed = changed || door.openAmount != previousAmount;
                if (door.openAmount >= 1.0f)
                {
                    door.opening = false;
                    door.closeDelay = DoorAutoCloseDelay;
                }
                continue;
            }

            if (door.openAmount <= 0.0f)
                continue;

            if (door.isSecret)
                continue;

            if (HasDeadEnemyInDoorway(door) || HasPlayerInDoorway(door, playerPosition))
            {
                door.closeDelay = DoorAutoCloseDelay;
                continue;
            }

            if (door.closeDelay > 0.0f)
            {
                door.closeDelay = std::max(0.0f, door.closeDelay - elapsedSeconds);
                continue;
            }

            const float previousAmount = door.openAmount;
            door.openAmount = std::max(0.0f, door.openAmount - DoorOpenSpeed * elapsedSeconds);
            changed = changed || door.openAmount != previousAmount;
        }

        constexpr float elevatorTarget = 1.0f;
        for (Exit& exit : exits_)
        {
            const float previousAmount = exit.openAmount;
            if (exit.openAmount < elevatorTarget)
            {
                exit.openAmount = std::min(
                    elevatorTarget,
                    exit.openAmount + DoorOpenSpeed * elapsedSeconds);
            }
            else if (exit.openAmount > elevatorTarget)
            {
                exit.openAmount = std::max(
                    elevatorTarget,
                    exit.openAmount - DoorOpenSpeed * elapsedSeconds);
            }
            changed = changed || exit.openAmount != previousAmount;
        }

        if (changed)
        {
            RebuildDoorGeometry();
            if (doorVertexBuffer_)
                doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));
        }

        const auto moveEnemyToward = [this, elapsedSeconds](Enemy& enemy, const Vector3& target)
        {
            const float moveX = target.X - enemy.position.X;
            const float moveZ = target.Z - enemy.position.Z;
            const float moveDistanceSquared = moveX * moveX + moveZ * moveZ;
            if (moveDistanceSquared <= 0.0001f)
                return;

            const float inverseDistance = 1.0f / std::sqrt(moveDistanceSquared);
            enemy.facing = Vector3(moveX * inverseDistance, 0.0f, moveZ * inverseDistance);
            const float step = std::min(
                enemy.moveSpeed * elapsedSeconds,
                std::sqrt(moveDistanceSquared));
            const Vector3 previous = enemy.position;
            const float nextX = enemy.position.X + enemy.facing.X * step;
            const float nextZ = enemy.position.Z + enemy.facing.Z * step;
            if (!Collides(nextX, enemy.position.Z, 0.2f))
                enemy.position.X = nextX;
            if (!Collides(enemy.position.X, nextZ, 0.2f))
                enemy.position.Z = nextZ;
            const float traveledX = enemy.position.X - previous.X;
            const float traveledZ = enemy.position.Z - previous.Z;
            enemy.distanceTravelled += std::sqrt(
                traveledX * traveledX + traveledZ * traveledZ);
        };

        for (Enemy& enemy : enemies_)
        {
            enemy.attackVisualSeconds = std::max(0.0f, enemy.attackVisualSeconds - elapsedSeconds);
            enemy.painVisualSeconds = std::max(0.0f, enemy.painVisualSeconds - elapsedSeconds);
            if (enemy.state == EnemyState::Dead)
                continue;
            enemy.visualAnimationSeconds = std::fmod(
                enemy.visualAnimationSeconds + elapsedSeconds, 1000.0f);

            const bool passivelyObserving =
                enemy.state == EnemyState::Idle || enemy.state == EnemyState::Patrol;
            const float sightDx = playerPosition.X - enemy.position.X;
            const float sightDz = playerPosition.Z - enemy.position.Z;
            const bool activeSight =
                sightDx * sightDx + sightDz * sightDz <= EnemyWakeRange * EnemyWakeRange &&
                HasLineOfSight(enemy.position, playerPosition);
            const bool canSeePlayer = passivelyObserving
                ? HasDirectionalSight(enemy, playerPosition)
                : activeSight;
            const bool heardPlayer = pendingPlayerNoise_.has_value() &&
                CanHearNoise(enemy, *pendingPlayerNoise_);
            if ((passivelyObserving || enemy.state == EnemyState::Search) &&
                (canSeePlayer || heardPlayer))
            {
                BeginEnemyAlert(
                    enemy,
                    canSeePlayer ? playerPosition : *pendingPlayerNoise_);
            }

            if (enemy.state == EnemyState::Alert)
            {
                if (canSeePlayer)
                    enemy.lastKnownTarget = playerPosition;
                enemy.reactionRemaining = std::max(
                    0.0f,
                    enemy.reactionRemaining - elapsedSeconds);
                if (enemy.reactionRemaining <= 0.0f)
                    enemy.state = EnemyState::Chase;
                continue;
            }

            if ((enemy.state == EnemyState::Chase || enemy.state == EnemyState::Attack) &&
                canSeePlayer)
            {
                enemy.lastKnownTarget = playerPosition;
            }

            if (enemy.state == EnemyState::Search)
            {
                enemy.searchRemaining = std::max(0.0f, enemy.searchRemaining - elapsedSeconds);
                if (enemy.searchRemaining <= 0.0f)
                    enemy.state = enemy.hasPatrolRoute ? EnemyState::Patrol : EnemyState::Idle;
            }
        }

        Enemy* designatedRangedAttacker = nullptr;
        float designatedDistanceSquared = std::numeric_limits<float>::max();
        for (Enemy& enemy : enemies_)
        {
            if ((enemy.state != EnemyState::Chase && enemy.state != EnemyState::Attack) ||
                enemy.melee)
                continue;

            const float dx = playerPosition.X - enemy.position.X;
            const float dz = playerPosition.Z - enemy.position.Z;
            const float distanceSquared = dx * dx + dz * dz;
            if (distanceSquared > enemy.attackRange * enemy.attackRange ||
                distanceSquared > EnemyWakeRange * EnemyWakeRange ||
                !HasLineOfSight(enemy.position, playerPosition))
                continue;

            if (distanceSquared < designatedDistanceSquared)
            {
                designatedRangedAttacker = &enemy;
                designatedDistanceSquared = distanceSquared;
            }
        }

        for (Enemy& enemy : enemies_)
        {
            if (enemy.state == EnemyState::Dead || enemy.state == EnemyState::Idle ||
                enemy.state == EnemyState::Alert || enemy.state == EnemyState::Search)
                continue;

            const float dx = playerPosition.X - enemy.position.X;
            const float dz = playerPosition.Z - enemy.position.Z;
            const float distanceSquared = dx * dx + dz * dz;
            const bool canSeePlayer =
                distanceSquared <= EnemyWakeRange * EnemyWakeRange &&
                HasLineOfSight(enemy.position, playerPosition);

            if (enemy.state == EnemyState::Patrol)
            {
                const int cellX = static_cast<int>(std::floor(enemy.position.X));
                const int cellZ = static_cast<int>(std::floor(enemy.position.Z));
                const float centerX = static_cast<float>(cellX) + 0.5f;
                const float centerZ = static_cast<float>(cellZ) + 0.5f;
                const char routeSymbol = map_[static_cast<std::size_t>(cellZ)]
                    [static_cast<std::size_t>(cellX)];
                const float centerDx = centerX - enemy.position.X;
                const float centerDz = centerZ - enemy.position.Z;
                const bool centered = centerDx * centerDx + centerDz * centerDz <= 0.04f;

                Vector3 patrolTarget;
                if ((routeSymbol == '^' || routeSymbol == '>' ||
                    routeSymbol == 'v' || routeSymbol == '<') && !centered)
                {
                    patrolTarget = Vector3(centerX, 0.0f, centerZ);
                }
                else
                {
                    if (routeSymbol == '^')
                    {
                        enemy.patrolDirectionX = 0;
                        enemy.patrolDirectionZ = -1;
                    }
                    else if (routeSymbol == '>')
                    {
                        enemy.patrolDirectionX = 1;
                        enemy.patrolDirectionZ = 0;
                    }
                    else if (routeSymbol == 'v')
                    {
                        enemy.patrolDirectionX = 0;
                        enemy.patrolDirectionZ = 1;
                    }
                    else if (routeSymbol == '<')
                    {
                        enemy.patrolDirectionX = -1;
                        enemy.patrolDirectionZ = 0;
                    }

                    int targetX = cellX + enemy.patrolDirectionX;
                    int targetZ = cellZ + enemy.patrolDirectionZ;
                    if (IsNavigationBlockedCell(targetX, targetZ, false))
                    {
                        enemy.patrolDirectionX = -enemy.patrolDirectionX;
                        enemy.patrolDirectionZ = -enemy.patrolDirectionZ;
                        targetX = cellX + enemy.patrolDirectionX;
                        targetZ = cellZ + enemy.patrolDirectionZ;
                    }
                    if (IsNavigationBlockedCell(targetX, targetZ, false))
                    {
                        enemy.state = EnemyState::Idle;
                        continue;
                    }
                    patrolTarget = Vector3(
                        static_cast<float>(targetX) + 0.5f,
                        0.0f,
                        static_cast<float>(targetZ) + 0.5f);
                }
                moveEnemyToward(enemy, patrolTarget);
                continue;
            }

            const bool canAttack = distanceSquared <= enemy.attackRange * enemy.attackRange &&
                (enemy.melee || canSeePlayer);

            const bool hasAttackTurn = enemy.melee || &enemy == designatedRangedAttacker;
            if (enemy.state == EnemyState::Chase && canAttack && hasAttackTurn)
                enemy.state = EnemyState::Attack;
            else if (enemy.state == EnemyState::Attack && (!canAttack || !hasAttackTurn))
                enemy.state = EnemyState::Chase;

            if (enemy.state == EnemyState::Attack)
            {
                enemy.attackCooldown -= elapsedSeconds;
                if (enemy.attackCooldown <= 0.0f)
                {
                    if (enemy.melee)
                    {
                        damage += static_cast<int>(std::lround(
                            enemy.attackDamage * difficultyProfile_.incomingDamageMultiplier));
                        ++pendingEnemyAudioEvents_.houndAttacks;
                    }
                    else
                    {
                        const float inverseDistance = distanceSquared > 0.0001f
                            ? 1.0f / std::sqrt(distanceSquared)
                            : 0.0f;
                        const float baseDirectionX = dx * inverseDistance;
                        const float baseDirectionZ = dz * inverseDistance;
                        for (int projectileIndex = 0;
                            projectileIndex < enemy.projectileBurst;
                            ++projectileIndex)
                        {
                            const float spread = enemy.projectileBurst == 1
                                ? 0.0f
                                : (static_cast<float>(projectileIndex) -
                                    static_cast<float>(enemy.projectileBurst - 1) * 0.5f) * 0.16f;
                            const float cosine = std::cos(spread);
                            const float sine = std::sin(spread);
                            const float directionX = baseDirectionX * cosine - baseDirectionZ * sine;
                            const float directionZ = baseDirectionX * sine + baseDirectionZ * cosine;
                            enemyProjectiles_.push_back({
                                enemy.position + Vector3(0.0f, 0.5f, 0.0f),
                                Vector3(
                                    directionX * enemy.projectileSpeed,
                                    0.0f,
                                    directionZ * enemy.projectileSpeed),
                                GuardProjectileLifetime,
                                enemy.attackDamage});
                        }
                        ++pendingGuardShotCount_;
                    }
                    enemy.attackVisualSeconds = enemy.melee
                        ? HoundAttackVisualSeconds
                        : RangedAttackVisualSeconds;
                    enemy.painVisualSeconds = 0.0f;
                    enemy.attackCooldown = enemy.attackInterval;
                }
            }

            if (enemy.state != EnemyState::Chase || distanceSquared <= 0.0f)
                continue;

            Vector3 target = enemy.lastKnownTarget;
            if (!canSeePlayer)
            {
                enemy.pathRefreshTime -= elapsedSeconds;
                if (enemy.pathRefreshTime <= 0.0f)
                {
                    enemy.path = FindPath(
                        static_cast<int>(std::floor(enemy.position.X)),
                        static_cast<int>(std::floor(enemy.position.Z)),
                        static_cast<int>(std::floor(enemy.lastKnownTarget.X)),
                        static_cast<int>(std::floor(enemy.lastKnownTarget.Z)),
                        true);
                    enemy.pathIndex = 0;
                    enemy.pathRefreshTime = EnemyPathRefreshSeconds;
                }

                if (enemy.path.empty())
                {
                    enemy.state = EnemyState::Search;
                    enemy.searchRemaining = EnemySearchSeconds;
                    continue;
                }
                if (enemy.pathIndex < enemy.path.size())
                {
                    const auto [cellX, cellZ] = enemy.path[enemy.pathIndex];
                    target = Vector3(static_cast<float>(cellX) + 0.5f, 0.0f, static_cast<float>(cellZ) + 0.5f);
                    const float targetX = target.X - enemy.position.X;
                    const float targetZ = target.Z - enemy.position.Z;
                    if (targetX * targetX + targetZ * targetZ < 0.04f)
                    {
                        ++enemy.pathIndex;
                        if (enemy.pathIndex >= enemy.path.size())
                        {
                            enemy.state = EnemyState::Search;
                            enemy.searchRemaining = EnemySearchSeconds;
                            continue;
                        }
                    }
                }
            }
            else
            {
                target = playerPosition;
                enemy.path.clear();
                enemy.pathIndex = 0;
            }

            const int targetCellX = static_cast<int>(std::floor(target.X));
            const int targetCellZ = static_cast<int>(std::floor(target.Z));
            if (IsBlockedCell(targetCellX, targetCellZ) &&
                TryOpenOrdinaryDoor(targetCellX, targetCellZ))
                continue;
            moveEnemyToward(enemy, target);
        }

        pendingPlayerNoise_.reset();

        for (EnemyImpact& impact : enemyImpacts_)
            impact.remainingSeconds -= elapsedSeconds;
        std::erase_if(
            enemyImpacts_,
            [](const EnemyImpact& impact) { return impact.remainingSeconds <= 0.0f; });

        for (auto iterator = enemyProjectiles_.begin(); iterator != enemyProjectiles_.end();)
        {
            const Vector3 previousPosition = iterator->position;
            iterator->remainingLifetime -= elapsedSeconds;
            iterator->position.X += iterator->velocity.X * elapsedSeconds;
            iterator->position.Y += iterator->velocity.Y * elapsedSeconds;
            iterator->position.Z += iterator->velocity.Z * elapsedSeconds;

            const int cellX = static_cast<int>(std::floor(iterator->position.X));
            const int cellZ = static_cast<int>(std::floor(iterator->position.Z));
            const float dx = playerPosition.X - iterator->position.X;
            const float dz = playerPosition.Z - iterator->position.Z;
            if (dx * dx + dz * dz <= GuardProjectileHitRadius * GuardProjectileHitRadius)
            {
                damage += static_cast<int>(std::lround(
                    iterator->damage * difficultyProfile_.incomingDamageMultiplier));
                const float speedSquared =
                    iterator->velocity.X * iterator->velocity.X +
                    iterator->velocity.Z * iterator->velocity.Z;
                const float inverseSpeed = speedSquared > 0.0001f
                    ? 1.0f / std::sqrt(speedSquared)
                    : 0.0f;
                AddEnemyImpact(
                    Vector3(
                        playerPosition.X - iterator->velocity.X * inverseSpeed * 0.34f,
                        0.42f,
                        playerPosition.Z - iterator->velocity.Z * inverseSpeed * 0.34f),
                    true);
                iterator = enemyProjectiles_.erase(iterator);
            }
            else if (IsBlockedCell(cellX, cellZ))
            {
                AddEnemyImpact(Vector3(previousPosition.X, 0.4f, previousPosition.Z), false);
                iterator = enemyProjectiles_.erase(iterator);
            }
            else if (iterator->remainingLifetime <= 0.0f)
            {
                iterator = enemyProjectiles_.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }

        return damage;
    }

    void World::AddQuad(
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        const Vector3& d,
        Material material)
    {
        if (vertices_.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max() - 4))
            throw std::runtime_error("Starter world exceeded the 16-bit vertex index limit.");

        const auto base = static_cast<std::uint16_t>(vertices_.size());

        constexpr float panelWidth = 1.0f / static_cast<float>(MaterialPanelCount);
        const float u0 = panelWidth * static_cast<int>(material);
        const float u1 = u0 + panelWidth;

        vertices_.emplace_back(a, Vector2(u0, 1.0f));
        vertices_.emplace_back(b, Vector2(u1, 1.0f));
        vertices_.emplace_back(c, Vector2(u1, 0.0f));
        vertices_.emplace_back(d, Vector2(u0, 0.0f));

        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    }

    void World::AddBox(const Vector3& minimum, const Vector3& maximum, Material material)
    {
        const Vector3 lowerNorthWest(minimum.X, minimum.Y, minimum.Z);
        const Vector3 lowerNorthEast(maximum.X, minimum.Y, minimum.Z);
        const Vector3 lowerSouthWest(minimum.X, minimum.Y, maximum.Z);
        const Vector3 lowerSouthEast(maximum.X, minimum.Y, maximum.Z);
        const Vector3 upperNorthWest(minimum.X, maximum.Y, minimum.Z);
        const Vector3 upperNorthEast(maximum.X, maximum.Y, minimum.Z);
        const Vector3 upperSouthWest(minimum.X, maximum.Y, maximum.Z);
        const Vector3 upperSouthEast(maximum.X, maximum.Y, maximum.Z);

        AddQuad(lowerNorthEast, lowerNorthWest, upperNorthWest, upperNorthEast, material);
        AddQuad(lowerSouthWest, lowerSouthEast, upperSouthEast, upperSouthWest, material);
        AddQuad(lowerNorthWest, lowerSouthWest, upperSouthWest, upperNorthWest, material);
        AddQuad(lowerSouthEast, lowerNorthEast, upperNorthEast, upperSouthEast, material);
        AddQuad(upperNorthWest, upperNorthEast, upperSouthEast, upperSouthWest, material);
        AddQuad(lowerNorthWest, lowerSouthWest, lowerSouthEast, lowerNorthEast, material);
    }

    void World::AddDoorQuad(
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        const Vector3& d,
        Material material)
    {
        if (doorVertices_.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max() - 4))
            throw std::runtime_error("Door geometry exceeded the 16-bit vertex index limit.");

        const auto base = static_cast<std::uint16_t>(doorVertices_.size());
        constexpr float panelWidth = 1.0f / static_cast<float>(MaterialPanelCount);
        const float u0 = panelWidth * static_cast<int>(material);
        const float u1 = u0 + panelWidth;

        doorVertices_.emplace_back(a, Vector2(u0, 1.0f));
        doorVertices_.emplace_back(b, Vector2(u1, 1.0f));
        doorVertices_.emplace_back(c, Vector2(u1, 0.0f));
        doorVertices_.emplace_back(d, Vector2(u0, 0.0f));

        doorIndices_.push_back(base + 0);
        doorIndices_.push_back(base + 1);
        doorIndices_.push_back(base + 2);
        doorIndices_.push_back(base + 0);
        doorIndices_.push_back(base + 2);
        doorIndices_.push_back(base + 3);
    }

    void World::AddDoorBox(const Vector3& minimum, const Vector3& maximum, Material material)
    {
        const Vector3 lowerNorthWest(minimum.X, minimum.Y, minimum.Z);
        const Vector3 lowerNorthEast(maximum.X, minimum.Y, minimum.Z);
        const Vector3 lowerSouthWest(minimum.X, minimum.Y, maximum.Z);
        const Vector3 lowerSouthEast(maximum.X, minimum.Y, maximum.Z);
        const Vector3 upperNorthWest(minimum.X, maximum.Y, minimum.Z);
        const Vector3 upperNorthEast(maximum.X, maximum.Y, minimum.Z);
        const Vector3 upperSouthWest(minimum.X, maximum.Y, maximum.Z);
        const Vector3 upperSouthEast(maximum.X, maximum.Y, maximum.Z);

        AddDoorQuad(lowerNorthEast, lowerNorthWest, upperNorthWest, upperNorthEast, material);
        AddDoorQuad(lowerSouthWest, lowerSouthEast, upperSouthEast, upperSouthWest, material);
        AddDoorQuad(lowerNorthWest, lowerSouthWest, upperSouthWest, upperNorthWest, material);
        AddDoorQuad(lowerSouthEast, lowerNorthEast, upperNorthEast, upperSouthEast, material);
        AddDoorQuad(upperNorthWest, upperNorthEast, upperSouthEast, upperSouthWest, material);
        AddDoorQuad(lowerNorthWest, lowerSouthWest, lowerSouthEast, lowerNorthEast, material);
    }

    void World::BuildDoors()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] != 'D' && map_[z][x] != 'Q' && map_[z][x] != 'S')
                    continue;

                doors_.push_back({
                    x,
                    z,
                    IsStaticWallCell(x, z - 1) && IsStaticWallCell(x, z + 1),
                    map_[z][x] == 'Q'
                        ? Material::SecurityDoor
                        : map_[z][x] == 'S' ? WallMaterialForCell(x, z) : Material::Door,
                    map_[z][x] == 'S'});
                if (map_[z][x] == 'S')
                    ++totalSecrets_;
            }
        }
    }

    void World::RebuildDoorGeometry()
    {
        doorVertices_.clear();
        doorIndices_.clear();

        for (const Door& door : doors_)
        {
            const float centerX = static_cast<float>(door.x) + 0.5f;
            const float centerZ = static_cast<float>(door.z) + 0.5f;
            const float halfThickness = DoorThickness * 0.5f;
            const float minimumX = door.blocksAlongX ? centerX - halfThickness : static_cast<float>(door.x);
            const float maximumX = door.blocksAlongX ? centerX + halfThickness : static_cast<float>(door.x) + 1.0f;
            const float minimumZ = door.blocksAlongX ? static_cast<float>(door.z) : centerZ - halfThickness;
            const float maximumZ = door.blocksAlongX ? static_cast<float>(door.z) + 1.0f : centerZ + halfThickness;
            const float minimumY = door.openAmount * WallHeight;
            const float maximumY = minimumY + WallHeight;

            AddDoorBox(
                Vector3(minimumX, minimumY, minimumZ),
                Vector3(maximumX, maximumY, maximumZ),
                door.material);
        }

        for (const Exit& exit : exits_)
        {
            const float halfThickness = DoorThickness * 0.5f;
            const bool blocksAlongX = exit.approachX != 0;
            const float doorwayX = exit.approachX < 0
                ? static_cast<float>(exit.x)
                : static_cast<float>(exit.x) + 1.0f;
            const float doorwayZ = exit.approachZ < 0
                ? static_cast<float>(exit.z)
                : static_cast<float>(exit.z) + 1.0f;
            const float minimumX = blocksAlongX
                ? doorwayX - halfThickness
                : static_cast<float>(exit.x);
            const float maximumX = blocksAlongX
                ? doorwayX + halfThickness
                : static_cast<float>(exit.x) + 1.0f;
            const float minimumZ = blocksAlongX
                ? static_cast<float>(exit.z)
                : doorwayZ - halfThickness;
            const float maximumZ = blocksAlongX
                ? static_cast<float>(exit.z) + 1.0f
                : doorwayZ + halfThickness;
            const float minimumY = exit.openAmount * WallHeight;
            const float maximumY = minimumY + WallHeight;

            AddDoorBox(
                Vector3(minimumX, minimumY, minimumZ),
                Vector3(maximumX, maximumY, maximumZ),
                Material::SecurityDoor);
        }
    }

    void World::BuildImpactGeometry()
    {
        impactVertices_.resize(MaxImpactCount * 4);
        impactIndices_.resize(MaxImpactCount * 6);

        for (std::size_t index = 0; index < MaxImpactCount; ++index)
        {
            const auto vertexBase = static_cast<std::uint16_t>(index * 4);
            const std::size_t indexBase = index * 6;
            impactIndices_[indexBase + 0] = vertexBase + 0;
            impactIndices_[indexBase + 1] = vertexBase + 1;
            impactIndices_[indexBase + 2] = vertexBase + 2;
            impactIndices_[indexBase + 3] = vertexBase + 0;
            impactIndices_[indexBase + 4] = vertexBase + 2;
            impactIndices_[indexBase + 5] = vertexBase + 3;
        }

        for (std::size_t index = 0; index < impacts_.size(); ++index)
        {
            const Impact& impact = impacts_[index];
            const Vector3 center = impact.position + impact.normal * ImpactSurfaceOffset;
            Vector3 horizontal;

            if (impact.normal.X != 0.0f)
                horizontal = Vector3(0.0f, 0.0f, ImpactHalfSize);
            else
                horizontal = Vector3(ImpactHalfSize, 0.0f, 0.0f);

            const Vector3 vertical(0.0f, ImpactHalfSize, 0.0f);
            const std::size_t vertexBase = index * 4;
            impactVertices_[vertexBase + 0] = VertexPositionTexture(
                center - horizontal - vertical,
                Vector2(0.0f, 1.0f));
            impactVertices_[vertexBase + 1] = VertexPositionTexture(
                center + horizontal - vertical,
                Vector2(1.0f / static_cast<float>(MaterialPanelCount), 1.0f));
            impactVertices_[vertexBase + 2] = VertexPositionTexture(
                center + horizontal + vertical,
                Vector2(1.0f / static_cast<float>(MaterialPanelCount), 0.0f));
            impactVertices_[vertexBase + 3] = VertexPositionTexture(
                center - horizontal + vertical,
                Vector2(0.0f, 0.0f));
        }
    }

    void World::BuildEnemies()
    {
        // Authored row-major encounter order supplies stable difficulty tiers:
        // two encounters per group are available to Scout, one more to Operative,
        // and the fourth is a Veteran reinforcement.
        constexpr std::array spawnTiers{0, 1, 0, 2};
        std::size_t encounterIndex = 0;
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                const char symbol = map_[z][x];
                if (symbol == 'G' || symbol == 'K' || symbol == 'F' || symbol == 'U' ||
                    symbol == 'Z' ||
                    symbol == 'g' || symbol == 'k' || symbol == 'f' || symbol == 'u')
                {
                    const bool ambushSymbol =
                        symbol == 'g' || symbol == 'k' || symbol == 'f' || symbol == 'u';
                    int patrolDirectionX = 0;
                    int patrolDirectionZ = 0;
                    if (!ambushSymbol)
                    {
                        constexpr std::array<std::pair<int, int>, 4> directions = {
                            std::pair{1, 0},
                            std::pair{-1, 0},
                            std::pair{0, 1},
                            std::pair{0, -1}};
                        for (const auto [directionX, directionZ] : directions)
                        {
                            const int routeX = x + directionX;
                            const int routeZ = z + directionZ;
                            if (routeZ < 0 || routeZ >= static_cast<int>(map_.size()) ||
                                routeX < 0 ||
                                routeX >= static_cast<int>(map_[static_cast<std::size_t>(routeZ)].size()))
                                continue;
                            const char routeSymbol = map_[static_cast<std::size_t>(routeZ)]
                                [static_cast<std::size_t>(routeX)];
                            if (routeSymbol == '^' || routeSymbol == '>' ||
                                routeSymbol == 'v' || routeSymbol == '<')
                            {
                                patrolDirectionX = directionX;
                                patrolDirectionZ = directionZ;
                                break;
                            }
                        }
                    }
                    const bool authoredBehaviorEncounter = ambushSymbol || symbol == 'Z' ||
                        patrolDirectionX != 0 || patrolDirectionZ != 0;
                    const int spawnTier = authoredBehaviorEncounter
                        ? 0
                        : spawnTiers[encounterIndex % spawnTiers.size()];
                    ++encounterIndex;
                    if (spawnTier > difficultyProfile_.maximumEnemySpawnTier)
                        continue;

                    Enemy enemy;
                    enemy.position = Vector3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(z) + 0.5f);
                    enemy.lastKnownTarget = enemy.position;
                    enemy.ambush = ambushSymbol;
                    const char archetype = symbol == 'g' ? 'G'
                        : symbol == 'k' ? 'K'
                        : symbol == 'f' ? 'F'
                        : symbol == 'u' ? 'U'
                        : symbol;
                    if (archetype == 'K')
                    {
                        enemy.type = Enemy::Type::Hound;
                        enemy.health = 2;
                        enemy.scoreValue = 200;
                        enemy.attackDamage = 14;
                        enemy.moveSpeed = EnemySpeed * 1.8f;
                        enemy.attackRange = HoundAttackRange;
                        enemy.attackInterval = 1.05f;
                        enemy.melee = true;
                        enemy.ammunitionDrop = 0;
                        enemy.reactionDuration = 0.18f;
                        enemy.viewDotThreshold = 0.0f;
                        enemy.hearingRange = 10.0f;
                    }
                    else if (archetype == 'F')
                    {
                        enemy.type = Enemy::Type::RapidTrooper;
                        enemy.health = 4;
                        enemy.scoreValue = 250;
                        enemy.attackDamage = 5;
                        enemy.moveSpeed = EnemySpeed * 1.2f;
                        enemy.attackRange = GuardAttackRange;
                        enemy.attackInterval = 0.8f;
                        enemy.projectileSpeed = GuardProjectileSpeed * 1.15f;
                        enemy.ammunitionDrop = 5;
                        enemy.reactionDuration = 0.25f;
                        enemy.viewDotThreshold = 0.25f;
                        enemy.hearingRange = 14.0f;
                    }
                    else if (archetype == 'U')
                    {
                        enemy.type = Enemy::Type::HeavyUnit;
                        enemy.health = 8;
                        enemy.scoreValue = 500;
                        enemy.attackDamage = 14;
                        enemy.moveSpeed = EnemySpeed * 0.65f;
                        enemy.attackRange = GuardAttackRange + 1.0f;
                        enemy.attackInterval = 1.7f;
                        enemy.projectileSpeed = GuardProjectileSpeed * 0.85f;
                        enemy.ammunitionDrop = 8;
                        enemy.reactionDuration = 0.55f;
                        enemy.viewDotThreshold = 0.5f;
                        enemy.hearingRange = 10.0f;
                    }
                    else if (archetype == 'Z')
                    {
                        enemy.type = Enemy::Type::Boss;
                        enemy.health = 32;
                        enemy.scoreValue = 5000;
                        enemy.attackDamage = 9;
                        enemy.moveSpeed = EnemySpeed * 0.72f;
                        enemy.attackRange = GuardAttackRange + 1.0f;
                        enemy.attackInterval = 1.25f;
                        enemy.projectileSpeed = GuardProjectileSpeed * 0.95f;
                        enemy.projectileBurst = 3;
                        enemy.ammunitionDrop = 12;
                        enemy.reactionDuration = 0.65f;
                        enemy.viewDotThreshold = 0.55f;
                        enemy.hearingRange = 16.0f;
                    }

                    const float playerDx = playerStart_.X - enemy.position.X;
                    const float playerDz = playerStart_.Z - enemy.position.Z;
                    const float playerDistanceSquared = playerDx * playerDx + playerDz * playerDz;
                    if (playerDistanceSquared > 0.0001f)
                    {
                        const float inverseDistance = 1.0f / std::sqrt(playerDistanceSquared);
                        const float ambushDirection = enemy.ambush ? -1.0f : 1.0f;
                        enemy.facing = Vector3(
                            playerDx * inverseDistance * ambushDirection,
                            0.0f,
                            playerDz * inverseDistance * ambushDirection);
                    }

                    if (patrolDirectionX != 0 || patrolDirectionZ != 0)
                    {
                        enemy.state = EnemyState::Patrol;
                        enemy.hasPatrolRoute = true;
                        enemy.patrolDirectionX = patrolDirectionX;
                        enemy.patrolDirectionZ = patrolDirectionZ;
                        enemy.facing = Vector3(
                            static_cast<float>(patrolDirectionX),
                            0.0f,
                            static_cast<float>(patrolDirectionZ));
                    }

                    enemy.health = ScalePositiveAmount(
                        enemy.health,
                        difficultyProfile_.enemyHealthMultiplier);
                    enemy.moveSpeed *= difficultyProfile_.enemySpeedMultiplier;
                    enemy.attackInterval *= difficultyProfile_.enemyAttackIntervalMultiplier;
                    if (enemy.ammunitionDrop > 0)
                    {
                        enemy.ammunitionDrop = ScalePositiveAmount(
                            enemy.ammunitionDrop,
                            difficultyProfile_.ammunitionMultiplier);
                    }
                    totalEnemyHealth_ += enemy.health;
                    potentialDroppedAmmunition_ += enemy.ammunitionDrop;
                    enemies_.push_back(std::move(enemy));
                }
            }
        }
    }

    void World::BuildPickups()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                const char symbol = map_[z][x];
                if (symbol != 'H' && symbol != 'A' && symbol != 'T' && symbol != 'J' &&
                    symbol != 'N' && symbol != 'C' && symbol != 'W' && symbol != 'V')
                    continue;

                PickupType type = PickupType::Health;
                if (symbol == 'A')
                    type = PickupType::Ammo;
                else if (symbol == 'T')
                    type = PickupType::GoldBars;
                else if (symbol == 'J')
                    type = PickupType::GoldenGoblet;
                else if (symbol == 'N')
                    type = PickupType::PeaceMedallion;
                else if (symbol == 'C')
                    type = PickupType::AccessCard;
                else if (symbol == 'W')
                    type = PickupType::RepeaterWeapon;
                else if (symbol == 'V')
                    type = PickupType::HeavyWeapon;

                int amount = symbol == 'V' ? 10 : 6;
                if (symbol == 'A' || symbol == 'W' || symbol == 'V')
                {
                    amount = ScalePositiveAmount(amount, difficultyProfile_.ammunitionMultiplier);
                    fixedAmmunition_ += amount;
                }
                pickups_.push_back({
                    Vector3(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f),
                    type,
                    false,
                    amount});
                if (symbol == 'T' || symbol == 'J' || symbol == 'N')
                    ++totalGold_;
            }
        }
        basePickupCount_ = pickups_.size();
    }

    void World::BuildExits()
    {
        constexpr std::array<std::pair<int, int>, 4> directions = {
            std::pair{-1, 0},
            std::pair{1, 0},
            std::pair{0, -1},
            std::pair{0, 1}};

        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] != 'E' && map_[z][x] != 'X')
                    continue;

                auto approach = directions.end();
                for (auto candidate = directions.begin(); candidate != directions.end(); ++candidate)
                {
                    if (!IsStaticWallCell(x + candidate->first, z + candidate->second))
                    {
                        approach = candidate;
                        break;
                    }
                }

                if (approach == directions.end())
                    throw std::runtime_error("Sector elevator has no walkable approach cell.");

                exits_.push_back({
                    Vector3(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f),
                    x,
                    z,
                    approach->first,
                    approach->second,
                    map_[z][x] == 'X' ? ExitRoute::Secret : ExitRoute::Standard,
                    1.0f});
            }
        }
    }

    void World::BuildTerminals()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] == 'M')
                    terminals_.push_back({Vector3(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f)});
            }
        }
    }

    void World::BuildRelays()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] == 'O')
                    relays_.push_back({Vector3(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f)});
            }
        }
    }

    void World::BuildDecorations()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                const char symbol = map_[z][x];
                if (symbol == 'L')
                {
                    decorations_.push_back({
                        Vector3(static_cast<float>(x) + 0.5f, 0.985f, static_cast<float>(z) + 0.5f),
                        Decoration::Type::CeilingLamp,
                        0.0f});
                    continue;
                }
                if (symbol == 'I')
                {
                    decorations_.push_back({
                        Vector3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(z) + 0.5f),
                        Decoration::Type::Plant,
                        0.0f});
                    continue;
                }
                if (symbol != 'R' && symbol != 'B')
                    continue;

                Decoration decoration;
                decoration.type = symbol == 'R'
                    ? Decoration::Type::Painting
                    : Decoration::Type::PeaceBanner;
                decoration.position.Y = symbol == 'R' ? 0.22f : 0.1f;

                if (IsStaticWallCell(x, z - 1))
                {
                    decoration.position.X = static_cast<float>(x) + 0.5f;
                    decoration.position.Z = static_cast<float>(z) + 0.012f;
                }
                else if (IsStaticWallCell(x, z + 1))
                {
                    decoration.position.X = static_cast<float>(x) + 0.5f;
                    decoration.position.Z = static_cast<float>(z) + 0.988f;
                    decoration.rotationY = MathHelper::Pi;
                }
                else if (IsStaticWallCell(x - 1, z))
                {
                    decoration.position.X = static_cast<float>(x) + 0.012f;
                    decoration.position.Z = static_cast<float>(z) + 0.5f;
                    decoration.rotationY = MathHelper::PiOver2;
                }
                else
                {
                    decoration.position.X = static_cast<float>(x) + 0.988f;
                    decoration.position.Z = static_cast<float>(z) + 0.5f;
                    decoration.rotationY = -MathHelper::PiOver2;
                }
                decorations_.push_back(decoration);
            }
        }
    }

    bool World::HasLineOfSight(const Vector3& from, const Vector3& to) const
    {
        const float dx = to.X - from.X;
        const float dz = to.Z - from.Z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 0.0f)
            return true;

        const float stepX = dx / distance * HitScanStep;
        const float stepZ = dz / distance * HitScanStep;
        for (float traveled = HitScanStep; traveled < distance; traveled += HitScanStep)
        {
            const int x = static_cast<int>(std::floor(from.X + stepX * (traveled / HitScanStep)));
            const int z = static_cast<int>(std::floor(from.Z + stepZ * (traveled / HitScanStep)));
            if (IsBlockedCell(x, z))
                return false;
        }

        return true;
    }

    bool World::HasDirectionalSight(const Enemy& enemy, const Vector3& target) const
    {
        const float dx = target.X - enemy.position.X;
        const float dz = target.Z - enemy.position.Z;
        const float distanceSquared = dx * dx + dz * dz;
        if (distanceSquared > EnemyWakeRange * EnemyWakeRange ||
            !HasLineOfSight(enemy.position, target))
            return false;
        if (distanceSquared <= EnemyCloseAwarenessRange * EnemyCloseAwarenessRange)
            return true;

        const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
        const float facing =
            (dx * enemy.facing.X + dz * enemy.facing.Z) * inverseDistance;
        return facing >= enemy.viewDotThreshold;
    }

    bool World::CanHearNoise(const Enemy& enemy, const Vector3& noisePosition) const
    {
        if (enemy.ambush)
            return false;

        const float dx = noisePosition.X - enemy.position.X;
        const float dz = noisePosition.Z - enemy.position.Z;
        if (dx * dx + dz * dz > enemy.hearingRange * enemy.hearingRange)
            return false;

        const int startX = static_cast<int>(std::floor(enemy.position.X));
        const int startZ = static_cast<int>(std::floor(enemy.position.Z));
        const int goalX = static_cast<int>(std::floor(noisePosition.X));
        const int goalZ = static_cast<int>(std::floor(noisePosition.Z));
        return (startX == goalX && startZ == goalZ) ||
            !FindPath(startX, startZ, goalX, goalZ, true).empty();
    }

    void World::BeginEnemyAlert(Enemy& enemy, const Vector3& target)
    {
        if (enemy.state == EnemyState::Alert || enemy.state == EnemyState::Chase ||
            enemy.state == EnemyState::Attack)
            return;

        enemy.state = EnemyState::Alert;
        enemy.reactionRemaining = enemy.reactionDuration;
        enemy.lastKnownTarget = target;
        if (enemy.melee)
            ++pendingEnemyAudioEvents_.houndAlerts;
        else
            ++pendingEnemyAudioEvents_.guardAlerts;
    }

    bool World::TryOpenOrdinaryDoor(int x, int z)
    {
        for (Door& door : doors_)
        {
            if (door.x != x || door.z != z || door.material != Material::Door ||
                door.isSecret)
                continue;

            if (!door.opening && door.openAmount < DoorPassableAt)
            {
                door.opening = true;
                door.closeDelay = DoorAutoCloseDelay;
                ++pendingEnemyAudioEvents_.doorsOpened;
            }
            return true;
        }
        return false;
    }

    std::vector<std::pair<int, int>> World::FindPath(
        int startX,
        int startZ,
        int goalX,
        int goalZ,
        bool allowOrdinaryDoors) const
    {
        const int width = static_cast<int>(map_.front().size());
        const int height = static_cast<int>(map_.size());
        const auto toIndex = [width](int x, int z) { return z * width + x; };
        const int start = toIndex(startX, startZ);
        const int goal = toIndex(goalX, goalZ);
        std::vector<int> parent(static_cast<std::size_t>(width * height), -1);
        std::vector<int> cost(static_cast<std::size_t>(width * height), std::numeric_limits<int>::max());
        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> open;

        cost[static_cast<std::size_t>(start)] = 0;
        open.push({0, start});
        constexpr std::array<std::pair<int, int>, 4> neighbors = {
            std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}};

        while (!open.empty())
        {
            const int current = open.top().second;
            open.pop();
            if (current == goal)
                break;

            const int x = current % width;
            const int z = current / width;
            for (const auto [offsetX, offsetZ] : neighbors)
            {
                const int nextX = x + offsetX;
                const int nextZ = z + offsetZ;
                if (nextX < 0 || nextX >= width || nextZ < 0 || nextZ >= height ||
                    IsNavigationBlockedCell(nextX, nextZ, allowOrdinaryDoors))
                    continue;

                const int next = toIndex(nextX, nextZ);
                const int nextCost = cost[static_cast<std::size_t>(current)] + 1;
                if (nextCost >= cost[static_cast<std::size_t>(next)])
                    continue;

                cost[static_cast<std::size_t>(next)] = nextCost;
                parent[static_cast<std::size_t>(next)] = current;
                const int estimate = std::abs(goalX - nextX) + std::abs(goalZ - nextZ);
                open.push({nextCost + estimate, next});
            }
        }

        if (parent[static_cast<std::size_t>(goal)] == -1 && goal != start)
            return {};

        std::vector<std::pair<int, int>> path;
        for (int current = goal; current != start; current = parent[static_cast<std::size_t>(current)])
            path.emplace_back(current % width, current / width);
        std::reverse(path.begin(), path.end());
        return path;
    }

    void World::BuildBillboardGeometry()
    {
        billboardVertices_ = {
            VertexPositionTexture(Vector3(-0.5f, 0.0f, 0.0f), Vector2(0.0f, 1.0f)),
            VertexPositionTexture(Vector3( 0.5f, 0.0f, 0.0f), Vector2(1.0f, 1.0f)),
            VertexPositionTexture(Vector3( 0.5f, 1.0f, 0.0f), Vector2(1.0f, 0.0f)),
            VertexPositionTexture(Vector3(-0.5f, 1.0f, 0.0f), Vector2(0.0f, 0.0f))};
        billboardIndices_ = {0, 1, 2, 0, 2, 3};
    }

    void World::BuildBloodPoolGeometry()
    {
        bloodPoolVertices_ = {
            VertexPositionTexture(Vector3(-0.5f, 0.0f, -0.5f), Vector2(0.0f, 0.0f)),
            VertexPositionTexture(Vector3( 0.5f, 0.0f, -0.5f), Vector2(1.0f, 0.0f)),
            VertexPositionTexture(Vector3( 0.5f, 0.0f,  0.5f), Vector2(1.0f, 1.0f)),
            VertexPositionTexture(Vector3(-0.5f, 0.0f,  0.5f), Vector2(0.0f, 1.0f))};
        bloodPoolIndices_ = {0, 1, 2, 0, 2, 3};
    }

    void World::BuildMesh()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (!IsStaticWallCell(x, z))
                {
                    const float x0 = static_cast<float>(x);
                    const float x1 = x0 + 1.0f;
                    const float z0 = static_cast<float>(z);
                    const float z1 = z0 + 1.0f;
                    const bool isElevator = IsExitCell(x, z);

                    // Floor.
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        isElevator ? Material::WallSteel : Material::Floor);

                    // Ceiling. CullNone is used in the starter, so winding is deliberately
                    // not relied upon yet.
                    AddQuad(
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        isElevator ? Material::WallSteel : Material::Ceiling);

                    if (map_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] == 'Y')
                    {
                        constexpr float topBottom = 0.48f;
                        constexpr float topHeight = 0.1f;
                        constexpr float legWidth = 0.1f;
                        constexpr float legInset = 0.16f;
                        AddBox(
                            Vector3(x0 + 0.12f, topBottom, z0 + 0.18f),
                            Vector3(x1 - 0.12f, topBottom + topHeight, z1 - 0.18f),
                            Material::Wood);
                        for (const float legX : {x0 + legInset, x1 - legInset - legWidth})
                        {
                            for (const float legZ : {z0 + legInset, z1 - legInset - legWidth})
                            {
                                AddBox(
                                    Vector3(legX, 0.0f, legZ),
                                    Vector3(legX + legWidth, topBottom, legZ + legWidth),
                                    Material::Wood);
                            }
                        }
                    }

                    continue;
                }

                const float x0 = static_cast<float>(x);
                const float x1 = x0 + 1.0f;
                const float z0 = static_cast<float>(z);
                const float z1 = z0 + 1.0f;
                const Material wallMaterial = WallMaterialForCell(x, z);

                // Emit only wall faces that border open space.
                if (!IsStaticWallCell(x, z - 1))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        IsExitCell(x, z - 1) ? Material::WallSteel : wallMaterial);
                }

                if (!IsStaticWallCell(x, z + 1))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        IsExitCell(x, z + 1) ? Material::WallSteel : wallMaterial);
                }

                if (!IsStaticWallCell(x - 1, z))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, 0.0f, z1),
                        Vector3(x0, WallHeight, z1),
                        Vector3(x0, WallHeight, z0),
                        IsExitCell(x - 1, z) ? Material::WallSteel : wallMaterial);
                }

                if (!IsStaticWallCell(x + 1, z))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        IsExitCell(x + 1, z) ? Material::WallSteel : wallMaterial);
                }
            }
        }
    }

    void World::Upload(GraphicsDevice& device)
    {
        if (vertexBuffer_ || indexBuffer_)
            return;

        vertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionTexture::getVertexDeclarationStatic(),
            static_cast<int>(vertices_.size()),
            BufferUsage::None);

        vertexBuffer_->SetData(vertices_.data(), static_cast<int>(vertices_.size()));

        indexBuffer_ = std::make_unique<IndexBuffer>(
            device,
            IndexElementSize::SixteenBits,
            static_cast<int>(indices_.size()),
            BufferUsage::None);

        indexBuffer_->SetData(indices_.data(), static_cast<int>(indices_.size()));

        if (!doorVertices_.empty())
        {
            doorVertexBuffer_ = std::make_unique<VertexBuffer>(
                device,
                VertexPositionTexture::getVertexDeclarationStatic(),
                static_cast<int>(doorVertices_.size()),
                BufferUsage::None);
            doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));

            doorIndexBuffer_ = std::make_unique<IndexBuffer>(
                device,
                IndexElementSize::SixteenBits,
                static_cast<int>(doorIndices_.size()),
                BufferUsage::None);
            doorIndexBuffer_->SetData(doorIndices_.data(), static_cast<int>(doorIndices_.size()));
        }

        impactVertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionTexture::getVertexDeclarationStatic(),
            static_cast<int>(impactVertices_.size()),
            BufferUsage::None);
        impactVertexBuffer_->SetData(impactVertices_.data(), static_cast<int>(impactVertices_.size()));

        impactIndexBuffer_ = std::make_unique<IndexBuffer>(
            device,
            IndexElementSize::SixteenBits,
            static_cast<int>(impactIndices_.size()),
            BufferUsage::None);
        impactIndexBuffer_->SetData(impactIndices_.data(), static_cast<int>(impactIndices_.size()));

        if (!enemies_.empty() || !pickups_.empty() || !decorations_.empty() ||
            !terminals_.empty() || !relays_.empty() || !exits_.empty())
        {
            billboardVertexBuffer_ = std::make_unique<VertexBuffer>(
                device,
                VertexPositionTexture::getVertexDeclarationStatic(),
                static_cast<int>(billboardVertices_.size()),
                BufferUsage::None);
            billboardVertexBuffer_->SetData(
                billboardVertices_.data(), static_cast<int>(billboardVertices_.size()));

            billboardIndexBuffer_ = std::make_unique<IndexBuffer>(
                device,
                IndexElementSize::SixteenBits,
                static_cast<int>(billboardIndices_.size()),
                BufferUsage::None);
            billboardIndexBuffer_->SetData(
                billboardIndices_.data(), static_cast<int>(billboardIndices_.size()));

            bloodPoolVertexBuffer_ = std::make_unique<VertexBuffer>(
                device,
                VertexPositionTexture::getVertexDeclarationStatic(),
                static_cast<int>(bloodPoolVertices_.size()),
                BufferUsage::None);
            bloodPoolVertexBuffer_->SetData(
                bloodPoolVertices_.data(), static_cast<int>(bloodPoolVertices_.size()));

            bloodPoolIndexBuffer_ = std::make_unique<IndexBuffer>(
                device,
                IndexElementSize::SixteenBits,
                static_cast<int>(bloodPoolIndices_.size()),
                BufferUsage::None);
            bloodPoolIndexBuffer_->SetData(
                bloodPoolIndices_.data(), static_cast<int>(bloodPoolIndices_.size()));
        }
    }

    void World::Draw(
        GraphicsDevice& device,
        BasicEffect& effect,
        const Matrix& view,
        const Matrix& projection,
        Texture2D& atlas,
        Texture2D& guardSprite,
        Texture2D& houndSprite,
        Texture2D& rapidTrooperSprite,
        Texture2D& heavyUnitSprite,
        Texture2D& bossSprite,
        Texture2D& guardAttackSprite,
        Texture2D& houndAttackSprite,
        Texture2D& rapidTrooperAttackSprite,
        Texture2D& heavyUnitAttackSprite,
        Texture2D& bossAttackSprite,
        Texture2D& guardPainSprite,
        Texture2D& houndPainSprite,
        Texture2D& rapidTrooperPainSprite,
        Texture2D& heavyUnitPainSprite,
        Texture2D& bossPainSprite,
        Texture2D& defeatedGuardSprite,
        Texture2D& defeatedHoundSprite,
        Texture2D& defeatedRapidTrooperSprite,
        Texture2D& defeatedHeavyUnitSprite,
        Texture2D& defeatedBossSprite,
        Texture2D& ammoPickupSprite,
        Texture2D& healthPickupSprite,
        Texture2D& goldBarsSprite,
        Texture2D& goldenGobletSprite,
        Texture2D& peaceMedallionSprite,
        Texture2D& accessCardSprite,
        Texture2D& repeaterPickupSprite,
        Texture2D& heavyWeaponPickupSprite,
        Texture2D& terminalSprite,
        Texture2D& relaySprite,
        Texture2D& exitSprite,
        Texture2D& enemyProjectileSprite,
        Texture2D& enemyImpactSprite,
        Texture2D& bloodDecal,
        Texture2D& paintingTexture,
        Texture2D& peaceBannerTexture,
        Texture2D& ceilingLampTexture,
        Texture2D& lampLightTexture,
        Texture2D& plantSprite,
        const Vector3& cameraPosition)
    {
        if (!vertexBuffer_ || !indexBuffer_ || indices_.empty())
            return;

        effect.setWorldProperty(Matrix::getIdentityProperty());
        effect.setViewProperty(view);
        effect.setProjectionProperty(projection);
        effect.setLightingEnabledProperty(false);
        effect.setTextureEnabledProperty(true);
        effect.setTextureProperty(&atlas);
        effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
        effect.setAlphaProperty(1.0f);

        device.SetVertexBuffer(vertexBuffer_.get());
        device.setIndicesProperty(indexBuffer_.get());

        for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();

            device.DrawIndexedPrimitives(
                PrimitiveType::TriangleList,
                0,
                0,
                static_cast<int>(vertices_.size()),
                0,
                static_cast<int>(indices_.size() / 3));
        }

        if (doorVertexBuffer_ && doorIndexBuffer_ && !doorIndices_.empty())
        {
            device.SetVertexBuffer(doorVertexBuffer_.get());
            device.setIndicesProperty(doorIndexBuffer_.get());

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();

                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(doorVertices_.size()),
                    0,
                    static_cast<int>(doorIndices_.size() / 3));
            }
        }

        if (!decorations_.empty() && billboardVertexBuffer_ && billboardIndexBuffer_ &&
            bloodPoolVertexBuffer_ && bloodPoolIndexBuffer_)
        {
            device.setBlendStateProperty(BlendState::NonPremultiplied);
            device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
            effect.setTextureEnabledProperty(true);
            effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));

            device.SetVertexBuffer(billboardVertexBuffer_.get());
            device.setIndicesProperty(billboardIndexBuffer_.get());
            for (const Decoration& decoration : decorations_)
            {
                if (decoration.type == Decoration::Type::CeilingLamp)
                    continue;

                if (decoration.type == Decoration::Type::Plant)
                {
                    const float plantHeight = 0.86f;
                    const float plantAspect = static_cast<float>(plantSprite.getWidthProperty()) /
                        static_cast<float>(std::max(1, plantSprite.getHeightProperty()));
                    effect.setTextureProperty(&plantSprite);
                    effect.setWorldProperty(
                        Matrix::CreateScale(plantHeight * plantAspect, plantHeight, 1.0f) *
                        Matrix::CreateConstrainedBillboard(
                            decoration.position,
                            cameraPosition,
                            Vector3::Up,
                            std::nullopt,
                            std::nullopt));

                    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                    {
                        pass.Apply();
                        device.DrawIndexedPrimitives(
                            PrimitiveType::TriangleList,
                            0,
                            0,
                            static_cast<int>(billboardVertices_.size()),
                            0,
                            static_cast<int>(billboardIndices_.size() / 3));
                    }
                    continue;
                }

                const bool isPainting = decoration.type == Decoration::Type::Painting;
                effect.setTextureProperty(isPainting ? &paintingTexture : &peaceBannerTexture);
                effect.setWorldProperty(
                    Matrix::CreateScale(isPainting ? 0.58f : 0.56f, isPainting ? 0.56f : 0.78f, 1.0f) *
                    Matrix::CreateRotationY(decoration.rotationY) *
                    Matrix::CreateTranslation(decoration.position));

                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(billboardVertices_.size()),
                        0,
                        static_cast<int>(billboardIndices_.size() / 3));
                }
            }

            device.SetVertexBuffer(bloodPoolVertexBuffer_.get());
            device.setIndicesProperty(bloodPoolIndexBuffer_.get());
            effect.setTextureProperty(&ceilingLampTexture);
            for (const Decoration& decoration : decorations_)
            {
                if (decoration.type != Decoration::Type::CeilingLamp)
                    continue;

                effect.setWorldProperty(
                    Matrix::CreateScale(0.54f, 1.0f, 0.54f) *
                    Matrix::CreateTranslation(decoration.position));
                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(bloodPoolVertices_.size()),
                        0,
                        static_cast<int>(bloodPoolIndices_.size() / 3));
                }
            }

            effect.setTextureProperty(&lampLightTexture);
            effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
            for (const Decoration& decoration : decorations_)
            {
                if (decoration.type != Decoration::Type::CeilingLamp)
                    continue;

                effect.setWorldProperty(
                    Matrix::CreateScale(2.45f, 1.0f, 2.45f) *
                    Matrix::CreateTranslation(decoration.position.X, 0.004f, decoration.position.Z));
                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(bloodPoolVertices_.size()),
                        0,
                        static_cast<int>(bloodPoolIndices_.size() / 3));
                }
            }

            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
        }

        if (!impacts_.empty() && impactVertexBuffer_ && impactIndexBuffer_)
        {
            effect.setDiffuseColorProperty(Vector3(0.9f, 0.16f, 0.1f));
            device.SetVertexBuffer(impactVertexBuffer_.get());
            device.setIndicesProperty(impactIndexBuffer_.get());

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();

                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(impacts_.size() * 4),
                    0,
                    static_cast<int>(impacts_.size() * 2));
            }
        }

        if (bloodPoolVertexBuffer_ && bloodPoolIndexBuffer_)
        {
            device.setBlendStateProperty(BlendState::NonPremultiplied);
            device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
            device.SetVertexBuffer(bloodPoolVertexBuffer_.get());
            device.setIndicesProperty(bloodPoolIndexBuffer_.get());
            effect.setTextureEnabledProperty(true);
            effect.setTextureProperty(&bloodDecal);
            effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));

            for (const Enemy& enemy : enemies_)
            {
                if (enemy.state != EnemyState::Dead)
                    continue;

                const bool isHound = enemy.type == Enemy::Type::Hound;
                const bool isHeavy = enemy.type == Enemy::Type::HeavyUnit;
                const bool isBoss = enemy.type == Enemy::Type::Boss;
                const float width = isHound ? 0.58f : isBoss ? 1.18f : isHeavy ? 0.82f : 0.72f;
                const float depth = isHound ? 0.46f : isBoss ? 0.82f : isHeavy ? 0.62f : 0.54f;
                const float rotation = enemy.position.X * 1.73f + enemy.position.Z * 0.91f;
                effect.setWorldProperty(
                    Matrix::CreateScale(width, 1.0f, depth) *
                    Matrix::CreateRotationY(rotation) *
                    Matrix::CreateTranslation(enemy.position.X, 0.006f, enemy.position.Z));

                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(bloodPoolVertices_.size()),
                        0,
                        static_cast<int>(bloodPoolIndices_.size() / 3));
                }
            }

            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
        }

        if (billboardVertexBuffer_ && billboardIndexBuffer_ &&
            (!enemies_.empty() || !pickups_.empty() || !terminals_.empty() ||
                !relays_.empty() || !exits_.empty() || !enemyProjectiles_.empty() ||
                !enemyImpacts_.empty()))
        {
            std::vector<const Enemy*> sortedEnemies;
            sortedEnemies.reserve(enemies_.size());
            for (const Enemy& enemy : enemies_)
                sortedEnemies.push_back(&enemy);

            std::sort(
                sortedEnemies.begin(),
                sortedEnemies.end(),
                [&cameraPosition](const Enemy* left, const Enemy* right)
                {
                    const float leftX = left->position.X - cameraPosition.X;
                    const float leftZ = left->position.Z - cameraPosition.Z;
                    const float rightX = right->position.X - cameraPosition.X;
                    const float rightZ = right->position.Z - cameraPosition.Z;
                    return leftX * leftX + leftZ * leftZ > rightX * rightX + rightZ * rightZ;
                });

            device.setBlendStateProperty(BlendState::NonPremultiplied);
            device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
            device.SetVertexBuffer(billboardVertexBuffer_.get());
            device.setIndicesProperty(billboardIndexBuffer_.get());
            effect.setTextureEnabledProperty(true);
            effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));

            const auto drawBillboard = [&](Texture2D& texture, const Vector3& position,
                                           float width, float height, const Vector3& tint)
            {
                effect.setTextureProperty(&texture);
                effect.setDiffuseColorProperty(tint);
                effect.setWorldProperty(
                    Matrix::CreateScale(width, height, 1.0f) *
                    Matrix::CreateConstrainedBillboard(
                        position,
                        cameraPosition,
                        Vector3::Up,
                        std::nullopt,
                        std::nullopt));

                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(billboardVertices_.size()),
                        0,
                        static_cast<int>(billboardIndices_.size() / 3));
                }
            };

            for (const Enemy* enemy : sortedEnemies)
            {
                const bool isHound = enemy->type == Enemy::Type::Hound;
                const bool isHeavy = enemy->type == Enemy::Type::HeavyUnit;
                const bool isBoss = enemy->type == Enemy::Type::Boss;
                const bool isDead = enemy->state == EnemyState::Dead;
                const bool isAttacking = !isDead && enemy->attackVisualSeconds > 0.0f;
                const bool isInPain = !isDead && !isAttacking && enemy->painVisualSeconds > 0.0f;
                const bool isRapid = enemy->type == Enemy::Type::RapidTrooper;
                float width = isDead
                    ? (isHound ? 0.95f : isBoss ? 1.58f : isHeavy ? 1.05f : isRapid ? 1.0f : 0.95f)
                    : (isHound ? 0.82f : isBoss ? 1.32f : isHeavy ? 0.9f : 0.72f);
                float height = isDead
                    ? (isHound ? 0.43f : isBoss ? 0.72f : isHeavy ? 0.64f : isRapid ? 0.54f : 0.44f)
                    : (isHound ? 0.72f : isBoss ? 1.42f : isHeavy ? 1.08f : 1.02f);
                Vector3 position = enemy->position;
                position.Y = 0.0f;

                if (!isDead && !isAttacking && !isInPain)
                {
                    const float phaseOffset = enemy->position.X * 0.73f + enemy->position.Z * 0.41f;
                    if (enemy->state == EnemyState::Chase)
                    {
                        const float strideRate = isHound ? 11.0f : isHeavy ? 6.5f : 8.0f;
                        const float stride = std::sin(
                            enemy->visualAnimationSeconds * strideRate + phaseOffset);
                        const float step = std::abs(stride);
                        position.Y += step * (isHound ? 0.018f : isHeavy ? 0.022f : 0.027f);
                        width *= 1.0f + stride * (isHound ? 0.018f : 0.012f);
                        height *= 1.0f - step * 0.012f;
                    }
                    else if (enemy->state == EnemyState::Idle)
                    {
                        const float breath = std::sin(
                            enemy->visualAnimationSeconds * 2.1f + phaseOffset);
                        position.Y += (breath + 1.0f) * 0.002f;
                        height *= 1.0f + breath * 0.006f;
                    }
                }

                Texture2D* enemyTexture = isDead
                    ? &defeatedGuardSprite
                    : isAttacking ? &guardAttackSprite
                    : isInPain ? &guardPainSprite : &guardSprite;
                if (isHound)
                    enemyTexture = isDead
                        ? &defeatedHoundSprite
                        : isAttacking ? &houndAttackSprite
                        : isInPain ? &houndPainSprite : &houndSprite;
                else if (isRapid)
                    enemyTexture = isDead
                        ? &defeatedRapidTrooperSprite
                        : isAttacking ? &rapidTrooperAttackSprite
                        : isInPain ? &rapidTrooperPainSprite : &rapidTrooperSprite;
                else if (isHeavy)
                    enemyTexture = isDead
                        ? &defeatedHeavyUnitSprite
                        : isAttacking ? &heavyUnitAttackSprite
                        : isInPain ? &heavyUnitPainSprite : &heavyUnitSprite;
                else if (isBoss)
                    enemyTexture = isDead
                        ? &defeatedBossSprite
                        : isAttacking ? &bossAttackSprite
                        : isInPain ? &bossPainSprite : &bossSprite;
                effect.setTextureProperty(enemyTexture);
                effect.setWorldProperty(
                    Matrix::CreateScale(width, height, 1.0f) *
                    Matrix::CreateConstrainedBillboard(
                        position,
                        cameraPosition,
                        Vector3::Up,
                        std::nullopt,
                        std::nullopt));
                effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));

                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(billboardVertices_.size()),
                        0,
                        static_cast<int>(billboardIndices_.size() / 3));
                }
            }

            for (const Pickup& pickup : pickups_)
            {
                if (pickup.collected)
                    continue;

                Texture2D* pickupTexture = nullptr;
                float width = 0.58f;
                float height = 0.56f;
                if (pickup.type == PickupType::Ammo)
                {
                    pickupTexture = &ammoPickupSprite;
                    width = 0.66f;
                }
                else if (pickup.type == PickupType::Health)
                {
                    pickupTexture = &healthPickupSprite;
                    width = 0.68f;
                }
                else if (pickup.type == PickupType::GoldBars)
                {
                    pickupTexture = &goldBarsSprite;
                    height = 0.46f;
                }
                else if (pickup.type == PickupType::GoldenGoblet)
                {
                    pickupTexture = &goldenGobletSprite;
                    width = 0.46f;
                    height = 0.64f;
                }
                else if (pickup.type == PickupType::PeaceMedallion)
                {
                    pickupTexture = &peaceMedallionSprite;
                    width = 0.5f;
                    height = 0.64f;
                }
                else if (pickup.type == PickupType::AccessCard)
                {
                    pickupTexture = &accessCardSprite;
                    width = 0.58f;
                    height = 0.4f;
                }
                else if (pickup.type == PickupType::RepeaterWeapon)
                {
                    pickupTexture = &repeaterPickupSprite;
                    width = 0.82f;
                    height = 0.48f;
                }
                else if (pickup.type == PickupType::HeavyWeapon)
                {
                    pickupTexture = &heavyWeaponPickupSprite;
                    width = 0.9f;
                    height = 0.52f;
                }

                if (!pickupTexture)
                    continue;

                effect.setTextureProperty(pickupTexture);
                effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
                effect.setWorldProperty(
                    Matrix::CreateScale(width, height, 1.0f) *
                    Matrix::CreateConstrainedBillboard(
                        Vector3(pickup.position.X, 0.02f, pickup.position.Z),
                        cameraPosition,
                        Vector3::Up,
                        std::nullopt,
                        std::nullopt));

                for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
                {
                    pass.Apply();
                    device.DrawIndexedPrimitives(
                        PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(billboardVertices_.size()),
                        0,
                        static_cast<int>(billboardIndices_.size() / 3));
                }
            }

            for (const Terminal& terminal : terminals_)
            {
                drawBillboard(
                    terminalSprite,
                    Vector3(terminal.position.X, 0.02f, terminal.position.Z),
                    0.58f,
                    0.9f,
                    terminal.activated ? Vector3(0.72f, 1.0f, 1.0f) : Vector3(1.0f, 1.0f, 1.0f));
            }

            for (const Relay& relay : relays_)
            {
                drawBillboard(
                    relaySprite,
                    Vector3(relay.position.X, 0.02f, relay.position.Z),
                    0.72f,
                    0.72f,
                    relay.activated ? Vector3(0.72f, 1.0f, 0.78f) : Vector3(1.0f, 1.0f, 1.0f));
            }

            for (const EnemyProjectile& projectile : enemyProjectiles_)
            {
                drawBillboard(
                    enemyProjectileSprite,
                    Vector3(projectile.position.X, projectile.position.Y - 0.07f, projectile.position.Z),
                    0.3f,
                    0.15f,
                    Vector3(1.0f, 1.0f, 1.0f));
            }

            for (const EnemyImpact& impact : enemyImpacts_)
            {
                const float remaining = std::clamp(
                    impact.remainingSeconds / EnemyImpactDuration,
                    0.0f,
                    1.0f);
                const float progress = 1.0f - remaining;
                const float size = 0.22f + progress * 0.5f;
                effect.setAlphaProperty(remaining * 0.86f);
                drawBillboard(
                    enemyImpactSprite,
                    impact.position,
                    size,
                    size,
                    impact.hitPlayer
                        ? Vector3(1.0f, 0.48f, 0.26f)
                        : Vector3(0.52f, 0.9f, 1.0f));
            }
            effect.setAlphaProperty(1.0f);

            for (const Exit& exit : exits_)
            {
                const Vector3 markerPosition(
                    exit.position.X - static_cast<float>(exit.approachX) * 0.28f,
                    0.01f,
                    exit.position.Z - static_cast<float>(exit.approachZ) * 0.28f);
                drawBillboard(
                    exitSprite,
                    markerPosition,
                    0.54f,
                    0.86f,
                    exit.openAmount >= DoorPassableAt
                        ? Vector3(0.82f, 1.0f, 1.0f)
                        : Vector3(1.0f, 0.48f, 0.42f));
            }

            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
        }
    }
}
