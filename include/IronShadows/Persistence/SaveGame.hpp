#pragma once

#include "IronShadows/Core/WorldTypes.hpp"
#include "IronShadows/Missions/PrototypeMission.hpp"

#include <optional>
#include <string>

namespace IronShadows
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
