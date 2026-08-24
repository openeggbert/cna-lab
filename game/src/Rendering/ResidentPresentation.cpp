#include "People/Rendering/ResidentPresentation.hpp"

#include <stdexcept>

namespace People::Rendering
{
    namespace
    {
        [[nodiscard]] int FacingQuarterTurns(const Simulation::ResidentFacing facing)
        {
            switch (facing)
            {
                case Simulation::ResidentFacing::North: return 0;
                case Simulation::ResidentFacing::East: return 1;
                case Simulation::ResidentFacing::South: return 2;
                case Simulation::ResidentFacing::West: return 3;
            }
            throw std::invalid_argument("resident facing must be one of four directions");
        }

        [[nodiscard]] int ViewQuarterTurns(const World::ViewRotation rotation)
        {
            switch (rotation)
            {
                case World::ViewRotation::North: return 0;
                case World::ViewRotation::East: return 1;
                case World::ViewRotation::South: return 2;
                case World::ViewRotation::West: return 3;
            }
            throw std::invalid_argument("view rotation must be one of four directions");
        }
    }

    SpriteDirection ResidentPresentation::PresentedDirection(
        const Simulation::ResidentFacing facing,
        const World::ViewRotation viewRotation)
    {
        return static_cast<SpriteDirection>(
            (FacingQuarterTurns(facing) + ViewQuarterTurns(viewRotation)) % 4);
    }

    const ResidentSpriteReference& ResidentPresentation::SelectIdleSprite(
        const ResidentIdleSpriteSet& sprites,
        const Simulation::ResidentFacing facing,
        const World::ViewRotation viewRotation)
    {
        return sprites.directions[static_cast<std::size_t>(
            PresentedDirection(facing, viewRotation))];
    }
}
