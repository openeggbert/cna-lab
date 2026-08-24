#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace WolfCna
{
    struct CampaignProfile final
    {
        int highestUnlocked = 0;
        int soundVolume = 4;
        int difficulty = 1;
    };

    class CampaignProgress final
    {
    public:
        [[nodiscard]] static CampaignProfile Load(
            const std::filesystem::path& path,
            int levelCount);
        static void Save(
            const std::filesystem::path& path,
            const CampaignProfile& profile,
            int levelCount);

        [[nodiscard]] static CampaignProfile Parse(
            std::string_view text,
            int levelCount);
        [[nodiscard]] static std::string Serialize(
            const CampaignProfile& profile,
            int levelCount);
    };
}
