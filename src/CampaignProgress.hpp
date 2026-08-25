#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Controls.hpp"
#include "HighScores.hpp"

namespace WolfCna
{
    // The classic view-size slider. The largest step fills the area above the HUD; each
    // smaller step shrinks the 3D window and leaves a border, exactly as in 1992.
    inline constexpr int MaximumViewSizeStep = 4;

    [[nodiscard]] constexpr float ViewSizeScale(int step)
    {
        return 0.5f + 0.125f * static_cast<float>(
            step < 0 ? 0 : step > MaximumViewSizeStep ? MaximumViewSizeStep : step);
    }

    struct CampaignProfile final
    {
        int highestUnlocked = 0;
        int soundVolume = 4;
        int difficulty = 1;
        int fieldOfView = 72;
        int viewSizeStep = MaximumViewSizeStep;
        ControlSettings controls;
        std::vector<HighScoreEntry> highScores;
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
