#pragma once

#include "People/Rendering/ResidentPresentation.hpp"
#include "People/Simulation/ResidentModel.hpp"

namespace People::Content
{
    /** @brief Original predefined adult used until household creation exists. */
    class DemoResident final
    {
    public:
        static constexpr Simulation::ResidentId MaraId = 2001;
        static constexpr Simulation::HouseholdId Household = 1;

        [[nodiscard]] static Simulation::ResidentState MaraState();
        [[nodiscard]] static Rendering::ResidentIdleSpriteSet MaraIdleSprites();
    };
}
