#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "World.hpp"

namespace WolfCna
{
    struct RunSaveState
    {
        int levelIndex = 0;
        int difficulty = 1;
        float playerX = 0.0f;
        float playerY = 0.62f;
        float playerZ = 0.0f;
        float yaw = 0.0f;
        int health = 100;
        int ammunition = 12;
        int score = 0;
        int lives = 3;
        int nextExtraLifeScore = 40000;
        int sectorEntryScore = 0;
        int sectorEntryNextExtraLifeScore = 40000;
        float levelElapsedSeconds = 0.0f;
        bool hasSecurityCard = false;
        int weapon = 1;
        int lastFirearm = 1;
        bool hasRepeater = false;
        bool hasHeavyWeapon = false;
        std::vector<bool> exploredCells;
        World::SaveState world;
    };

    class RunSave final
    {
    public:
        [[nodiscard]] static std::string Serialize(const RunSaveState& state);
        [[nodiscard]] static std::optional<RunSaveState> Parse(
            std::string_view text,
            std::string& error);
        [[nodiscard]] static bool SaveFile(
            const std::filesystem::path& path,
            const RunSaveState& state,
            std::string& error);
        [[nodiscard]] static std::optional<RunSaveState> LoadFile(
            const std::filesystem::path& path,
            std::string& error);
    };
}
