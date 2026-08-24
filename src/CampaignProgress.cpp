#include "CampaignProgress.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace WolfCna
{
    namespace
    {
        constexpr std::string_view Header = "WOLF-CNA-PROGRESS-1";
    }

    int CampaignProgress::LoadHighestUnlocked(
        const std::filesystem::path& path,
        int levelCount)
    {
        std::ifstream input(path);
        if (!input)
            return 0;

        std::ostringstream text;
        text << input.rdbuf();
        if (input.bad())
            return 0;
        return ParseHighestUnlocked(text.str(), levelCount);
    }

    void CampaignProgress::SaveHighestUnlocked(
        const std::filesystem::path& path,
        int highestUnlocked,
        int levelCount)
    {
        std::ofstream output(path, std::ios::trunc);
        if (!output)
            return;
        output << SerializeHighestUnlocked(highestUnlocked, levelCount);
    }

    int CampaignProgress::ParseHighestUnlocked(std::string_view text, int levelCount)
    {
        if (levelCount <= 0)
            return 0;

        std::istringstream input{std::string(text)};
        std::string header;
        int highestUnlocked = 0;
        std::string trailing;
        if (!(input >> header >> highestUnlocked) || header != Header || (input >> trailing))
            return 0;
        return std::clamp(highestUnlocked, 0, levelCount - 1);
    }

    std::string CampaignProgress::SerializeHighestUnlocked(
        int highestUnlocked,
        int levelCount)
    {
        const int maximum = std::max(0, levelCount - 1);
        return std::string(Header) + "\n" + std::to_string(std::clamp(highestUnlocked, 0, maximum)) + "\n";
    }
}
