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
        constexpr float HoundAttackRange = 0.85f;
        constexpr float GuardAttackRange = 6.0f;
        constexpr float EnemySpeed = 0.8f;
        constexpr float EnemyPathRefreshSeconds = 0.35f;
        constexpr float GuardProjectileSpeed = 4.5f;
        constexpr float GuardProjectileHitRadius = 0.25f;
        constexpr float GuardProjectileLifetime = 2.0f;
        constexpr float RangedAttackVisualSeconds = 0.18f;
        constexpr float HoundAttackVisualSeconds = 0.24f;
        constexpr float PickupRadius = 0.42f;
        constexpr float ExitRadius = 0.45f;
    }

    World::World(const LevelDefinition& level)
        : map_(level.Rows())
        , playerStart_(
            static_cast<float>(level.PlayerStartX()) + 0.5f,
            0.62f,
            static_cast<float>(level.PlayerStartZ()) + 0.5f)
    {
        impacts_.reserve(MaxImpactCount);
        BuildDoors();
        BuildEnemies();
        BuildPickups();
        BuildTerminals();
        BuildExits();
        BuildDecorations();
        BuildMesh();
        RebuildDoorGeometry();
        BuildImpactGeometry();
        BuildEnemyGeometry();
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

        for (const Door& door : doors_)
        {
            if (door.x == x && door.z == z)
                return door.openAmount < DoorPassableAt;
        }

        return false;
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
        float range)
    {
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
                if (defeated)
                {
                    ++defeatedEnemies_;
                    if (!enemy.melee)
                    {
                        const int ammoDrop = enemy.type == Enemy::Type::HeavyUnit
                            ? 8
                            : enemy.type == Enemy::Type::RapidTrooper ? 5 : 3;
                        pickups_.push_back({
                            Vector3(enemy.position.X, 0.08f, enemy.position.Z),
                            PickupType::Ammo,
                            false,
                            ammoDrop});
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
                result.ammo += 6;
            }
            else
            {
                pickup.collected = true;
                ++result.heavyWeapons;
                result.ammo += 10;
            }
        }

        return result;
    }

    bool World::ReachedExit(const Vector3& playerPosition) const
    {
        if (!IsExitUnlocked())
            return false;

        for (const Vector3& exit : exits_)
        {
            const float dx = exit.X - playerPosition.X;
            const float dz = exit.Z - playerPosition.Z;
            if (dx * dx + dz * dz <= ExitRadius * ExitRadius)
                return true;
        }

        return false;
    }

    bool World::IsExitUnlocked() const
    {
        return terminals_.empty() || std::all_of(
            terminals_.begin(),
            terminals_.end(),
            [](const Terminal& terminal) { return terminal.activated; });
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

    World::InteractionResult World::TryActivate(
        const Vector3& playerPosition,
        const Vector3& lookDirection,
        bool hasSecurityCard)
    {
        Door* target = nullptr;
        Terminal* targetTerminal = nullptr;
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
        const Vector3& playerPosition,
        float damageMultiplier)
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

        if (changed)
        {
            RebuildDoorGeometry();
            if (doorVertexBuffer_)
                doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));
        }

        Enemy* designatedRangedAttacker = nullptr;
        float designatedDistanceSquared = std::numeric_limits<float>::max();
        for (Enemy& enemy : enemies_)
        {
            if (enemy.state == EnemyState::Dead || enemy.melee)
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
            enemy.attackVisualSeconds = std::max(0.0f, enemy.attackVisualSeconds - elapsedSeconds);
            if (enemy.state == EnemyState::Dead)
                continue;

            const float dx = playerPosition.X - enemy.position.X;
            const float dz = playerPosition.Z - enemy.position.Z;
            const float distanceSquared = dx * dx + dz * dz;
            const bool canSeePlayer =
                distanceSquared <= EnemyWakeRange * EnemyWakeRange &&
                HasLineOfSight(enemy.position, playerPosition);

            const bool canAttack = distanceSquared <= enemy.attackRange * enemy.attackRange &&
                (enemy.melee || canSeePlayer);

            if (enemy.state == EnemyState::Idle && canSeePlayer)
            {
                enemy.state = EnemyState::Chase;
                if (enemy.melee)
                    ++pendingEnemyAudioEvents_.houndAlerts;
                else
                    ++pendingEnemyAudioEvents_.guardAlerts;
            }
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
                        damage += static_cast<int>(std::lround(enemy.attackDamage * damageMultiplier));
                        ++pendingEnemyAudioEvents_.houndAttacks;
                    }
                    else
                    {
                        const float inverseDistance = distanceSquared > 0.0001f
                            ? 1.0f / std::sqrt(distanceSquared)
                            : 0.0f;
                        enemyProjectiles_.push_back({
                            enemy.position + Vector3(0.0f, 0.5f, 0.0f),
                            Vector3(dx * inverseDistance * enemy.projectileSpeed, 0.0f, dz * inverseDistance * enemy.projectileSpeed),
                            GuardProjectileLifetime,
                            enemy.attackDamage});
                        ++pendingGuardShotCount_;
                    }
                    enemy.attackVisualSeconds = enemy.melee
                        ? HoundAttackVisualSeconds
                        : RangedAttackVisualSeconds;
                    enemy.attackCooldown = enemy.attackInterval;
                }
            }

            if (enemy.state != EnemyState::Chase || distanceSquared <= 0.0f)
                continue;

            Vector3 target = playerPosition;
            if (!canSeePlayer)
            {
                enemy.pathRefreshTime -= elapsedSeconds;
                if (enemy.pathRefreshTime <= 0.0f)
                {
                    enemy.path = FindPath(
                        static_cast<int>(std::floor(enemy.position.X)),
                        static_cast<int>(std::floor(enemy.position.Z)),
                        static_cast<int>(std::floor(playerPosition.X)),
                        static_cast<int>(std::floor(playerPosition.Z)));
                    enemy.pathIndex = 0;
                    enemy.pathRefreshTime = EnemyPathRefreshSeconds;
                }

                if (enemy.pathIndex < enemy.path.size())
                {
                    const auto [cellX, cellZ] = enemy.path[enemy.pathIndex];
                    target = Vector3(static_cast<float>(cellX) + 0.5f, 0.0f, static_cast<float>(cellZ) + 0.5f);
                    const float targetX = target.X - enemy.position.X;
                    const float targetZ = target.Z - enemy.position.Z;
                    if (targetX * targetX + targetZ * targetZ < 0.04f)
                        ++enemy.pathIndex;
                }
            }

            const float moveX = target.X - enemy.position.X;
            const float moveZ = target.Z - enemy.position.Z;
            const float moveDistanceSquared = moveX * moveX + moveZ * moveZ;
            if (moveDistanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(moveDistanceSquared);
            const float step = enemy.moveSpeed * elapsedSeconds;
            const float nextX = enemy.position.X + moveX * inverseDistance * step;
            const float nextZ = enemy.position.Z + moveZ * inverseDistance * step;
            if (!Collides(nextX, enemy.position.Z, 0.2f))
                enemy.position.X = nextX;
            if (!Collides(enemy.position.X, nextZ, 0.2f))
                enemy.position.Z = nextZ;
        }

        for (auto iterator = enemyProjectiles_.begin(); iterator != enemyProjectiles_.end();)
        {
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
                damage += static_cast<int>(std::lround(iterator->damage * damageMultiplier));
                iterator = enemyProjectiles_.erase(iterator);
            }
            else if (iterator->remainingLifetime <= 0.0f || IsBlockedCell(cellX, cellZ))
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

        constexpr float panelWidth = 1.0f / 5.0f;
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
        constexpr float panelWidth = 1.0f / 5.0f;
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
                        : map_[z][x] == 'S' ? Material::Wall : Material::Door,
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

            const Vector3 lowerNorthWest(minimumX, minimumY, minimumZ);
            const Vector3 lowerNorthEast(maximumX, minimumY, minimumZ);
            const Vector3 lowerSouthWest(minimumX, minimumY, maximumZ);
            const Vector3 lowerSouthEast(maximumX, minimumY, maximumZ);
            const Vector3 upperNorthWest(minimumX, maximumY, minimumZ);
            const Vector3 upperNorthEast(maximumX, maximumY, minimumZ);
            const Vector3 upperSouthWest(minimumX, maximumY, maximumZ);
            const Vector3 upperSouthEast(maximumX, maximumY, maximumZ);

            AddDoorQuad(lowerNorthEast, lowerNorthWest, upperNorthWest, upperNorthEast, door.material);
            AddDoorQuad(lowerSouthWest, lowerSouthEast, upperSouthEast, upperSouthWest, door.material);
            AddDoorQuad(lowerNorthWest, lowerSouthWest, upperSouthWest, upperNorthWest, door.material);
            AddDoorQuad(lowerSouthEast, lowerNorthEast, upperNorthEast, upperSouthEast, door.material);
            AddDoorQuad(upperNorthWest, upperNorthEast, upperSouthEast, upperSouthWest, door.material);
            AddDoorQuad(lowerNorthWest, lowerSouthWest, lowerSouthEast, lowerNorthEast, door.material);
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
                Vector2(1.0f / 5.0f, 1.0f));
            impactVertices_[vertexBase + 2] = VertexPositionTexture(
                center + horizontal + vertical,
                Vector2(1.0f / 5.0f, 0.0f));
            impactVertices_[vertexBase + 3] = VertexPositionTexture(
                center - horizontal + vertical,
                Vector2(0.0f, 0.0f));
        }
    }

    void World::BuildEnemies()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                const char symbol = map_[z][x];
                if (symbol == 'G' || symbol == 'K' || symbol == 'F' || symbol == 'U')
                {
                    Enemy enemy;
                    enemy.position = Vector3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(z) + 0.5f);
                    if (symbol == 'K')
                    {
                        enemy.type = Enemy::Type::Hound;
                        enemy.health = 2;
                        enemy.scoreValue = 200;
                        enemy.attackDamage = 14;
                        enemy.moveSpeed = EnemySpeed * 1.8f;
                        enemy.attackRange = HoundAttackRange;
                        enemy.attackInterval = 1.05f;
                        enemy.melee = true;
                    }
                    else if (symbol == 'F')
                    {
                        enemy.type = Enemy::Type::RapidTrooper;
                        enemy.health = 4;
                        enemy.scoreValue = 250;
                        enemy.attackDamage = 5;
                        enemy.moveSpeed = EnemySpeed * 1.2f;
                        enemy.attackRange = GuardAttackRange;
                        enemy.attackInterval = 0.8f;
                        enemy.projectileSpeed = GuardProjectileSpeed * 1.15f;
                    }
                    else if (symbol == 'U')
                    {
                        enemy.type = Enemy::Type::HeavyUnit;
                        enemy.health = 8;
                        enemy.scoreValue = 500;
                        enemy.attackDamage = 14;
                        enemy.moveSpeed = EnemySpeed * 0.65f;
                        enemy.attackRange = GuardAttackRange + 1.0f;
                        enemy.attackInterval = 1.7f;
                        enemy.projectileSpeed = GuardProjectileSpeed * 0.85f;
                    }
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

                pickups_.push_back({
                    Vector3(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f),
                    type});
                if (symbol == 'T' || symbol == 'J' || symbol == 'N')
                    ++totalGold_;
            }
        }
    }

    void World::BuildExits()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] == 'E')
                    exits_.emplace_back(static_cast<float>(x) + 0.5f, 0.08f, static_cast<float>(z) + 0.5f);
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

    std::vector<std::pair<int, int>> World::FindPath(
        int startX,
        int startZ,
        int goalX,
        int goalZ) const
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
                    IsBlockedCell(nextX, nextZ))
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

    void World::AddEnemyQuad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d)
    {
        const auto base = static_cast<std::uint16_t>(enemyVertices_.size());
        enemyVertices_.emplace_back(a, Vector2(0.0f, 1.0f));
        enemyVertices_.emplace_back(b, Vector2(1.0f / 5.0f, 1.0f));
        enemyVertices_.emplace_back(c, Vector2(1.0f / 5.0f, 0.0f));
        enemyVertices_.emplace_back(d, Vector2(0.0f, 0.0f));
        enemyIndices_.insert(enemyIndices_.end(), {base, static_cast<std::uint16_t>(base + 1), static_cast<std::uint16_t>(base + 2), base, static_cast<std::uint16_t>(base + 2), static_cast<std::uint16_t>(base + 3)});
    }

    void World::BuildEnemyGeometry()
    {
        constexpr float half = 0.22f;
        constexpr float height = 0.82f;
        AddEnemyQuad(Vector3(-half, 0, -half), Vector3(half, 0, -half), Vector3(half, height, -half), Vector3(-half, height, -half));
        AddEnemyQuad(Vector3(half, 0, half), Vector3(-half, 0, half), Vector3(-half, height, half), Vector3(half, height, half));
        AddEnemyQuad(Vector3(-half, 0, half), Vector3(-half, 0, -half), Vector3(-half, height, -half), Vector3(-half, height, half));
        AddEnemyQuad(Vector3(half, 0, -half), Vector3(half, 0, half), Vector3(half, height, half), Vector3(half, height, -half));
        AddEnemyQuad(Vector3(-half, height, -half), Vector3(half, height, -half), Vector3(half, height, half), Vector3(-half, height, half));
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

                    // Floor.
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Material::Floor);

                    // Ceiling. CullNone is used in the starter, so winding is deliberately
                    // not relied upon yet.
                    AddQuad(
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        Material::Ceiling);

                    continue;
                }

                const float x0 = static_cast<float>(x);
                const float x1 = x0 + 1.0f;
                const float z0 = static_cast<float>(z);
                const float z1 = z0 + 1.0f;

                // Emit only wall faces that border open space.
                if (!IsStaticWallCell(x, z - 1))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x, z + 1))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x - 1, z))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, 0.0f, z1),
                        Vector3(x0, WallHeight, z1),
                        Vector3(x0, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x + 1, z))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        Material::Wall);
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

        if (!enemies_.empty() || !pickups_.empty() || !terminals_.empty() || !exits_.empty())
        {
            enemyVertexBuffer_ = std::make_unique<VertexBuffer>(
                device,
                VertexPositionTexture::getVertexDeclarationStatic(),
                static_cast<int>(enemyVertices_.size()),
                BufferUsage::None);
            enemyVertexBuffer_->SetData(enemyVertices_.data(), static_cast<int>(enemyVertices_.size()));
            enemyIndexBuffer_ = std::make_unique<IndexBuffer>(
                device,
                IndexElementSize::SixteenBits,
                static_cast<int>(enemyIndices_.size()),
                BufferUsage::None);
            enemyIndexBuffer_->SetData(enemyIndices_.data(), static_cast<int>(enemyIndices_.size()));
        }

        if (!enemies_.empty() || !pickups_.empty() || !decorations_.empty())
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
        Texture2D& guardAttackSprite,
        Texture2D& houndAttackSprite,
        Texture2D& rapidTrooperAttackSprite,
        Texture2D& heavyUnitAttackSprite,
        Texture2D& defeatedGuardSprite,
        Texture2D& defeatedHoundSprite,
        Texture2D& defeatedRapidTrooperSprite,
        Texture2D& defeatedHeavyUnitSprite,
        Texture2D& ammoPickupSprite,
        Texture2D& healthPickupSprite,
        Texture2D& goldBarsSprite,
        Texture2D& goldenGobletSprite,
        Texture2D& peaceMedallionSprite,
        Texture2D& bloodDecal,
        Texture2D& paintingTexture,
        Texture2D& peaceBannerTexture,
        Texture2D& ceilingLampTexture,
        Texture2D& lampLightTexture,
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
                const float width = isHound ? 0.58f : isHeavy ? 0.82f : 0.72f;
                const float depth = isHound ? 0.46f : isHeavy ? 0.62f : 0.54f;
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
            (!enemies_.empty() || !pickups_.empty()))
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

            for (const Enemy* enemy : sortedEnemies)
            {
                const bool isHound = enemy->type == Enemy::Type::Hound;
                const bool isHeavy = enemy->type == Enemy::Type::HeavyUnit;
                const bool isDead = enemy->state == EnemyState::Dead;
                const bool isAttacking = !isDead && enemy->attackVisualSeconds > 0.0f;
                const bool isRapid = enemy->type == Enemy::Type::RapidTrooper;
                const float width = isDead
                    ? (isHound ? 0.95f : isHeavy ? 1.05f : isRapid ? 1.0f : 0.95f)
                    : (isHound ? 0.82f : isHeavy ? 0.9f : 0.72f);
                const float height = isDead
                    ? (isHound ? 0.43f : isHeavy ? 0.64f : isRapid ? 0.54f : 0.44f)
                    : (isHound ? 0.72f : isHeavy ? 1.08f : 1.02f);
                Vector3 position = enemy->position;
                position.Y = 0.0f;

                Texture2D* enemyTexture = isDead
                    ? &defeatedGuardSprite
                    : isAttacking ? &guardAttackSprite : &guardSprite;
                if (isHound)
                    enemyTexture = isDead
                        ? &defeatedHoundSprite
                        : isAttacking ? &houndAttackSprite : &houndSprite;
                else if (isRapid)
                    enemyTexture = isDead
                        ? &defeatedRapidTrooperSprite
                        : isAttacking ? &rapidTrooperAttackSprite : &rapidTrooperSprite;
                else if (isHeavy)
                    enemyTexture = isDead
                        ? &defeatedHeavyUnitSprite
                        : isAttacking ? &heavyUnitAttackSprite : &heavyUnitSprite;
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

            device.setBlendStateProperty(BlendState::Opaque);
            device.setDepthStencilStateProperty(DepthStencilState::Default);
        }

        if (!enemyVertexBuffer_ || !enemyIndexBuffer_)
            return;

        device.SetVertexBuffer(enemyVertexBuffer_.get());
        device.setIndicesProperty(enemyIndexBuffer_.get());
        effect.setTextureEnabledProperty(false);

        for (const Pickup& pickup : pickups_)
        {
            if (pickup.collected)
                continue;
            if (pickup.type == PickupType::Health || pickup.type == PickupType::Ammo ||
                pickup.type == PickupType::GoldBars || pickup.type == PickupType::GoldenGoblet ||
                pickup.type == PickupType::PeaceMedallion)
                continue;

            effect.setWorldProperty(
                Matrix::CreateScale(0.38f) * Matrix::CreateTranslation(pickup.position));
            effect.setDiffuseColorProperty(
                pickup.type == PickupType::AccessCard
                    ? Vector3(0.28f, 0.72f, 0.94f)
                    : pickup.type == PickupType::RepeaterWeapon
                        ? Vector3(0.66f, 0.36f, 0.92f)
                        : Vector3(0.94f, 0.22f, 0.2f));

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();
                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(enemyVertices_.size()),
                    0,
                    static_cast<int>(enemyIndices_.size() / 3));
            }
        }

        for (const Terminal& terminal : terminals_)
        {
            effect.setWorldProperty(
                Matrix::CreateScale(0.34f, 0.8f, 0.34f) * Matrix::CreateTranslation(terminal.position));
            effect.setDiffuseColorProperty(
                terminal.activated ? Vector3(0.2f, 0.88f, 0.94f) : Vector3(0.92f, 0.44f, 0.08f));

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();
                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(enemyVertices_.size()),
                    0,
                    static_cast<int>(enemyIndices_.size() / 3));
            }
        }

        for (const EnemyProjectile& projectile : enemyProjectiles_)
        {
            effect.setWorldProperty(
                Matrix::CreateScale(0.09f) * Matrix::CreateTranslation(projectile.position));
            effect.setDiffuseColorProperty(Vector3(1.0f, 0.42f, 0.08f));

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();
                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(enemyVertices_.size()),
                    0,
                    static_cast<int>(enemyIndices_.size() / 3));
            }
        }

        for (const Vector3& exit : exits_)
        {
            effect.setWorldProperty(Matrix::CreateScale(0.62f, 1.0f, 0.62f) * Matrix::CreateTranslation(exit));
            effect.setDiffuseColorProperty(
                IsExitUnlocked() ? Vector3(0.18f, 0.72f, 0.94f) : Vector3(0.86f, 0.18f, 0.12f));

            for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();
                device.DrawIndexedPrimitives(
                    PrimitiveType::TriangleList,
                    0,
                    0,
                    static_cast<int>(enemyVertices_.size()),
                    0,
                    static_cast<int>(enemyIndices_.size() / 3));
            }
        }
    }
}
