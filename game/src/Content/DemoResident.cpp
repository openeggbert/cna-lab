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

    Rendering::ResidentIdleSpriteSet DemoResident::MaraIdleSprites()
    {
        const std::array<std::string_view, 4> directions{{
            "north", "east", "south", "west"
        }};
        Rendering::ResidentIdleSpriteSet result;
        for (std::size_t index = 0; index < directions.size(); ++index)
        {
            result.directions[index] = {
                "people.generated.resident.mara_vale.idle."
                    + std::string(directions[index]),
                32,
                88
            };
        }
        return result;
    }
}
