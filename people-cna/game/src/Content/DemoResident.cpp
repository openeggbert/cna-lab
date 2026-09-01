#include "People/Content/DemoResident.hpp"

#include <array>
#include <string>
#include <string_view>

namespace People::Content
{
    Simulation::ResidentState DemoResident::MaraState()
    {
        return {
            MaraId,
            Household,
            "Mara Vale",
            {9, 10, 0},
            Simulation::ResidentFacing::South,
            std::nullopt,
            std::nullopt
        };
    }

    namespace
    {
        constexpr std::array<std::string_view, 4> DirectionNames{{
            "north", "east", "south", "west"
        }};

        /** @brief Foot anchor shared by every Mara Vale frame in every clip. */
        constexpr int FootAnchorX = 32;
        constexpr int FootAnchorY = 88;
    }

    Rendering::ResidentIdleSpriteSet DemoResident::MaraIdleSprites()
    {
        Rendering::ResidentIdleSpriteSet result;
        for (std::size_t index = 0; index < DirectionNames.size(); ++index)
        {
            result.directions[index] = {
                "people.generated.resident.mara_vale.idle."
                    + std::string(DirectionNames[index]),
                FootAnchorX,
                FootAnchorY
            };
        }
        return result;
    }

    Rendering::ResidentWalkSpriteSet DemoResident::MaraWalkSprites()
    {
        Rendering::ResidentWalkSpriteSet result;
        for (std::size_t frame = 0; frame < Rendering::ResidentWalkSpriteSet::FrameCount;
             ++frame)
        {
            for (std::size_t index = 0; index < DirectionNames.size(); ++index)
            {
                result.frames[frame][index] = {
                    "people.generated.resident.mara_vale.walk."
                        + std::string(DirectionNames[index])
                        + "." + std::to_string(frame),
                    FootAnchorX,
                    FootAnchorY
                };
            }
        }
        return result;
    }
}
