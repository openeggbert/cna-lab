#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace CopperBoots
{
    struct CampaignStage
    {
        std::string_view Id;
        std::string_view DisplayName;
        std::string_view LevelPath;
    };

    inline constexpr std::array<CampaignStage, 2> CampaignStageList{{
        {"green-ruins", "Green Ruins Relay",
         "Content/Levels/green_ruins.cbl"},
        {"factory", "Brassworks Shift",
         "Content/Levels/factory.cbl"},
    }};

    [[nodiscard]] constexpr std::span<const CampaignStage>
    CampaignStages() noexcept
    {
        return CampaignStageList;
    }

    [[nodiscard]] constexpr std::optional<std::size_t>
    NextCampaignStage(const std::size_t currentStage) noexcept
    {
        if (currentStage >= CampaignStageList.size() ||
            currentStage + 1 >= CampaignStageList.size()) {
            return std::nullopt;
        }
        return currentStage + 1;
    }
}
