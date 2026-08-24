#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace WolfCna
{
    class CampaignProgress final
    {
    public:
        [[nodiscard]] static int LoadHighestUnlocked(
            const std::filesystem::path& path,
            int levelCount);
        static void SaveHighestUnlocked(
            const std::filesystem::path& path,
            int highestUnlocked,
            int levelCount);

        [[nodiscard]] static int ParseHighestUnlocked(
            std::string_view text,
            int levelCount);
        [[nodiscard]] static std::string SerializeHighestUnlocked(
            int highestUnlocked,
            int levelCount);
    };
}
