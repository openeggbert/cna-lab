#include "RunSave.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace WolfCna
{
    namespace
    {
        constexpr std::string_view Magic = "WOLF-CNA-RUN-SAVE-1";
        constexpr std::size_t MaximumRecordCount = 65536;

        bool ReadTag(std::istream& input, std::string_view expected, std::string& error)
        {
            std::string actual;
            if (!(input >> actual) || actual != expected)
            {
                error = "expected save section " + std::string(expected);
                return false;
            }
            return true;
        }

        bool ReadBool(std::istream& input, bool& value, std::string& error)
        {
            int encoded = 0;
            if (!(input >> encoded) || (encoded != 0 && encoded != 1))
            {
                error = "save contains an invalid boolean";
                return false;
            }
            value = encoded != 0;
            return true;
        }

        bool ReadCount(std::istream& input, std::size_t& count, std::string& error)
        {
            std::uint64_t encoded = 0;
            if (!(input >> encoded) || encoded > MaximumRecordCount)
            {
                error = "save section has an invalid record count";
                return false;
            }
            count = static_cast<std::size_t>(encoded);
            return true;
        }

        bool IsFinite(float value)
        {
            return std::isfinite(value);
        }

        bool ValidateGameState(const RunSaveState& state, std::string& error)
        {
            if (state.levelIndex < 0 || state.levelIndex > 63 ||
                state.difficulty < 0 || state.difficulty > 2 ||
                !IsFinite(state.playerX) || !IsFinite(state.playerY) ||
                !IsFinite(state.playerZ) || !IsFinite(state.yaw) ||
                state.health < 1 || state.health > 100 ||
                state.ammunition < 0 || state.ammunition > 99 ||
                state.score < 0 || state.score > 2000000000 ||
                state.lives < 1 || state.lives > 99 ||
                state.nextExtraLifeScore < 40000 ||
                state.nextExtraLifeScore > 2000000000 ||
                !IsFinite(state.levelElapsedSeconds) ||
                state.levelElapsedSeconds < 0.0f || state.levelElapsedSeconds > 10000000.0f ||
                state.weapon < 0 || state.weapon > 3 ||
                state.lastFirearm < 1 || state.lastFirearm > 3 ||
                state.exploredCells.size() > MaximumRecordCount)
            {
                error = "save contains invalid player or campaign values";
                return false;
            }
            if ((state.weapon == 2 || state.lastFirearm == 2) && !state.hasRepeater)
            {
                error = "save selects a repeater that is not owned";
                return false;
            }
            if ((state.weapon == 3 || state.lastFirearm == 3) && !state.hasHeavyWeapon)
            {
                error = "save selects a heavy weapon that is not owned";
                return false;
            }
            return true;
        }
    }

    std::string RunSave::Serialize(const RunSaveState& state)
    {
        std::ostringstream output;
        output << std::setprecision(std::numeric_limits<float>::max_digits10);
        output << Magic << '\n';
        output << "GAME " << state.levelIndex << ' ' << state.difficulty << ' '
            << state.playerX << ' ' << state.playerY << ' ' << state.playerZ << ' '
            << state.yaw << ' ' << state.health << ' ' << state.ammunition << ' '
            << state.score << ' ' << state.lives << ' ' << state.nextExtraLifeScore << ' '
            << state.levelElapsedSeconds << ' ' << state.hasSecurityCard << ' '
            << state.weapon << ' ' << state.lastFirearm << ' ' << state.hasRepeater << ' '
            << state.hasHeavyWeapon << '\n';
        output << "EXPLORED " << state.exploredCells.size() << ' ';
        for (bool visited : state.exploredCells)
            output << (visited ? '1' : '0');
        output << '\n';
        output << "WORLD " << state.world.defeatedEnemies << ' '
            << state.world.collectedGold << ' ' << state.world.foundSecrets << '\n';
        output << "DOORS " << state.world.doors.size() << '\n';
        for (const World::DoorSaveState& door : state.world.doors)
            output << door.opening << ' ' << door.openAmount << ' ' << door.closeDelay << '\n';
        output << "ENEMIES " << state.world.enemies.size() << '\n';
        for (const World::EnemySaveState& enemy : state.world.enemies)
        {
            output << enemy.state << ' ' << enemy.health << ' '
                << enemy.positionX << ' ' << enemy.positionZ << ' '
                << enemy.facingX << ' ' << enemy.facingZ << ' '
                << enemy.lastKnownX << ' ' << enemy.lastKnownZ << ' '
                << enemy.attackCooldown << ' ' << enemy.attackVisualSeconds << ' '
                << enemy.painVisualSeconds << ' ' << enemy.visualAnimationSeconds << ' '
                << enemy.reactionRemaining << ' ' << enemy.searchRemaining << ' '
                << enemy.distanceTravelled << '\n';
        }
        output << "PICKUPS " << state.world.pickups.size() << '\n';
        for (const World::PickupSaveState& pickup : state.world.pickups)
        {
            output << pickup.positionX << ' ' << pickup.positionZ << ' ' << pickup.type << ' '
                << pickup.collected << ' ' << pickup.amount << '\n';
        }
        output << "TERMINALS " << state.world.terminalsActivated.size();
        for (bool active : state.world.terminalsActivated)
            output << ' ' << active;
        output << '\n';
        output << "RELAYS " << state.world.relaysActivated.size();
        for (bool active : state.world.relaysActivated)
            output << ' ' << active;
        output << '\n';
        output << "EXITS " << state.world.exitOpenAmounts.size();
        for (float amount : state.world.exitOpenAmounts)
            output << ' ' << amount;
        output << '\n';
        output << "PROJECTILES " << state.world.projectiles.size() << '\n';
        for (const World::ProjectileSaveState& projectile : state.world.projectiles)
        {
            output << projectile.positionX << ' ' << projectile.positionY << ' '
                << projectile.positionZ << ' ' << projectile.velocityX << ' '
                << projectile.velocityY << ' ' << projectile.velocityZ << ' '
                << projectile.remainingLifetime << ' ' << projectile.damage << '\n';
        }
        output << "END\n";
        return output.str();
    }

    std::optional<RunSaveState> RunSave::Parse(std::string_view text, std::string& error)
    {
        error.clear();
        std::istringstream input{std::string(text)};
        RunSaveState state;
        std::string magic;
        if (!(input >> magic) || magic != Magic)
        {
            error = "save has an unsupported header";
            return std::nullopt;
        }
        if (!ReadTag(input, "GAME", error) ||
            !(input >> state.levelIndex >> state.difficulty >> state.playerX >> state.playerY >>
                state.playerZ >> state.yaw >> state.health >> state.ammunition >> state.score >>
                state.lives >> state.nextExtraLifeScore >> state.levelElapsedSeconds) ||
            !ReadBool(input, state.hasSecurityCard, error) ||
            !(input >> state.weapon >> state.lastFirearm) ||
            !ReadBool(input, state.hasRepeater, error) ||
            !ReadBool(input, state.hasHeavyWeapon, error))
        {
            if (error.empty())
                error = "save has an invalid GAME section";
            return std::nullopt;
        }

        std::size_t count = 0;
        if (!ReadTag(input, "EXPLORED", error) || !ReadCount(input, count, error))
            return std::nullopt;
        std::string explored;
        if (!(input >> explored) || explored.size() != count ||
            explored.find_first_not_of("01") != std::string::npos)
        {
            error = "save has invalid explored-map data";
            return std::nullopt;
        }
        state.exploredCells.reserve(count);
        for (char value : explored)
            state.exploredCells.push_back(value == '1');

        if (!ReadTag(input, "WORLD", error) ||
            !(input >> state.world.defeatedEnemies >> state.world.collectedGold >>
                state.world.foundSecrets) ||
            !ReadTag(input, "DOORS", error) || !ReadCount(input, count, error))
        {
            if (error.empty())
                error = "save has an invalid WORLD section";
            return std::nullopt;
        }
        state.world.doors.resize(count);
        for (World::DoorSaveState& door : state.world.doors)
        {
            if (!ReadBool(input, door.opening, error) ||
                !(input >> door.openAmount >> door.closeDelay))
                return std::nullopt;
        }

        if (!ReadTag(input, "ENEMIES", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.enemies.resize(count);
        for (World::EnemySaveState& enemy : state.world.enemies)
        {
            if (!(input >> enemy.state >> enemy.health >> enemy.positionX >> enemy.positionZ >>
                enemy.facingX >> enemy.facingZ >> enemy.lastKnownX >> enemy.lastKnownZ >>
                enemy.attackCooldown >> enemy.attackVisualSeconds >> enemy.painVisualSeconds >>
                enemy.visualAnimationSeconds >> enemy.reactionRemaining >> enemy.searchRemaining >>
                enemy.distanceTravelled))
            {
                error = "save has an invalid enemy record";
                return std::nullopt;
            }
        }

        if (!ReadTag(input, "PICKUPS", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.pickups.resize(count);
        for (World::PickupSaveState& pickup : state.world.pickups)
        {
            if (!(input >> pickup.positionX >> pickup.positionZ >> pickup.type) ||
                !ReadBool(input, pickup.collected, error) || !(input >> pickup.amount))
            {
                if (error.empty())
                    error = "save has an invalid pickup record";
                return std::nullopt;
            }
        }

        if (!ReadTag(input, "TERMINALS", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.terminalsActivated.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            bool active = false;
            if (!ReadBool(input, active, error))
                return std::nullopt;
            state.world.terminalsActivated.push_back(active);
        }
        if (!ReadTag(input, "RELAYS", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.relaysActivated.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            bool active = false;
            if (!ReadBool(input, active, error))
                return std::nullopt;
            state.world.relaysActivated.push_back(active);
        }
        if (!ReadTag(input, "EXITS", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.exitOpenAmounts.resize(count);
        for (float& amount : state.world.exitOpenAmounts)
        {
            if (!(input >> amount))
            {
                error = "save has an invalid exit record";
                return std::nullopt;
            }
        }
        if (!ReadTag(input, "PROJECTILES", error) || !ReadCount(input, count, error))
            return std::nullopt;
        state.world.projectiles.resize(count);
        for (World::ProjectileSaveState& projectile : state.world.projectiles)
        {
            if (!(input >> projectile.positionX >> projectile.positionY >> projectile.positionZ >>
                projectile.velocityX >> projectile.velocityY >> projectile.velocityZ >>
                projectile.remainingLifetime >> projectile.damage))
            {
                error = "save has an invalid projectile record";
                return std::nullopt;
            }
        }
        if (!ReadTag(input, "END", error))
            return std::nullopt;
        input >> std::ws;
        if (!input.eof())
        {
            error = "save contains trailing data";
            return std::nullopt;
        }
        if (!ValidateGameState(state, error))
            return std::nullopt;
        return state;
    }

    bool RunSave::SaveFile(
        const std::filesystem::path& path,
        const RunSaveState& state,
        std::string& error)
    {
        error.clear();
        if (!ValidateGameState(state, error))
            return false;

        const std::filesystem::path temporary(path.string() + ".tmp");
        const std::filesystem::path backup(path.string() + ".bak");
        std::error_code filesystemError;
        std::filesystem::remove(temporary, filesystemError);
        filesystemError.clear();
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "could not create temporary save file";
                return false;
            }
            output << Serialize(state);
            output.flush();
            if (!output)
            {
                error = "could not finish writing temporary save file";
                output.close();
                std::filesystem::remove(temporary, filesystemError);
                return false;
            }
        }

        const bool hadPrevious = std::filesystem::exists(path, filesystemError);
        if (filesystemError)
        {
            error = "could not inspect the previous save file";
            std::filesystem::remove(temporary, filesystemError);
            return false;
        }
        if (hadPrevious)
        {
            std::filesystem::remove(backup, filesystemError);
            filesystemError.clear();
            std::filesystem::rename(path, backup, filesystemError);
            if (filesystemError)
            {
                error = "could not preserve the previous save file";
                std::filesystem::remove(temporary, filesystemError);
                return false;
            }
        }
        std::filesystem::rename(temporary, path, filesystemError);
        if (filesystemError)
        {
            error = "could not install the new save file";
            if (hadPrevious)
            {
                std::error_code restoreError;
                std::filesystem::rename(backup, path, restoreError);
            }
            std::filesystem::remove(temporary, filesystemError);
            return false;
        }
        if (hadPrevious)
            std::filesystem::remove(backup, filesystemError);
        return true;
    }

    std::optional<RunSaveState> RunSave::LoadFile(
        const std::filesystem::path& path,
        std::string& error)
    {
        error.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "save slot is empty";
            return std::nullopt;
        }
        std::ostringstream text;
        text << input.rdbuf();
        if (input.bad())
        {
            error = "could not read save file";
            return std::nullopt;
        }
        return Parse(text.str(), error);
    }
}
