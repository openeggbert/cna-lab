#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Missions/PrototypeMission.hpp"

#include <optional>
#include <string>

namespace IronGang
{
    struct SaveSnapshot
    {
        PrototypeMissionState missionState{PrototypeMissionState::Introduction};
        Vector3 playerPosition{};
        float playerYaw{0.0F};
        Vector3 vehiclePosition{};
        float vehicleYaw{0.0F};
        float vehicleSpeed{0.0F};
        bool playerDriving{false};
        // Added for gate M5 (district loading, plan_13). Older save files without this field
        // (format=iron-gang-save-v1, same as now -- no version bump for one additive field)
        // load with a WarehouseBlock default rather than failing.
        DistrictId districtId{DistrictId::WarehouseBlock};
    };

    class SaveGame final
    {
    public:
        [[nodiscard]] static bool Write(const std::string& path,
                                        const SaveSnapshot& snapshot,
                                        std::string& errorMessage);
        [[nodiscard]] static std::optional<SaveSnapshot> Read(const std::string& path,
                                                              std::string& errorMessage);
    };
}
