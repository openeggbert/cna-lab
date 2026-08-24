#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace WolfCna
{
    enum class CampaignExitRoute
    {
        Standard,
        Secret
    };

    enum class CampaignSectorKind
    {
        Standard,
        Secret,
        Boss
    };

    struct CampaignSector final
    {
        std::string_view file;
        std::string_view menuName;
        std::string_view displayCode;
        int chapter = 1;
        std::string_view chapterName;
        int targetSeconds = 240;
        CampaignSectorKind kind = CampaignSectorKind::Standard;
        int standardDestination = -1;
        int secretDestination = -1;
        int selectableIndex = -1;
    };

    inline constexpr std::array<CampaignSector, 6> CampaignSectors = {{
        {
            "assets/levels/starter.level",
            "SECTOR 1 STORAGE",
            "1",
            1,
            "LOWER BUNKER",
            210,
            CampaignSectorKind::Standard,
            1,
            -1,
            0},
        {
            "assets/levels/sector-02.level",
            "SECTOR 2 FOUNDRY",
            "2",
            1,
            "LOWER BUNKER",
            240,
            CampaignSectorKind::Standard,
            2,
            4,
            1},
        {
            "assets/levels/sector-03.level",
            "SECTOR 3 LABS",
            "3",
            2,
            "WARDEN NETWORK",
            250,
            CampaignSectorKind::Standard,
            3,
            -1,
            2},
        {
            "assets/levels/sector-04.level",
            "SECTOR 4 ARCHIVE",
            "4",
            2,
            "WARDEN NETWORK",
            270,
            CampaignSectorKind::Standard,
            5,
            -1,
            3},
        {
            "assets/levels/hidden-reservoir.level",
            "HIDDEN RESERVOIR",
            "S",
            1,
            "LOWER BUNKER",
            210,
            CampaignSectorKind::Secret,
            2,
            -1,
            -1},
        {
            "assets/levels/warden-core.level",
            "SECTOR 5 CORE",
            "5",
            2,
            "WARDEN NETWORK",
            260,
            CampaignSectorKind::Boss,
            -1,
            -1,
            4}
    }};

    inline constexpr std::array<int, 5> SelectableCampaignSectors = {0, 1, 2, 3, 5};

    [[nodiscard]] constexpr const CampaignSector& GetCampaignSector(int sectorIndex)
    {
        return CampaignSectors[static_cast<std::size_t>(sectorIndex)];
    }

    [[nodiscard]] constexpr const CampaignSector& GetSelectableCampaignSector(int selectableIndex)
    {
        return GetCampaignSector(
            SelectableCampaignSectors[static_cast<std::size_t>(selectableIndex)]);
    }

    [[nodiscard]] constexpr std::optional<int> CampaignDestination(
        int sectorIndex,
        CampaignExitRoute route)
    {
        const CampaignSector& sector = GetCampaignSector(sectorIndex);
        const int destination = route == CampaignExitRoute::Secret
            ? sector.secretDestination
            : sector.standardDestination;
        return destination >= 0 ? std::optional<int>(destination) : std::nullopt;
    }

    [[nodiscard]] constexpr int HighestUnlockAfterCompletion(
        int sectorIndex,
        int currentHighestUnlock)
    {
        const CampaignSector& sector = GetCampaignSector(sectorIndex);
        if (sector.selectableIndex < 0)
            return currentHighestUnlock;
        return sector.standardDestination >= 0
            ? std::max(currentHighestUnlock, sector.selectableIndex + 1)
            : currentHighestUnlock;
    }
}
