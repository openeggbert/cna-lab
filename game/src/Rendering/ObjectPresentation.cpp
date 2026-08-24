#include "People/Rendering/ObjectPresentation.hpp"

#include <stdexcept>

namespace People::Rendering
{
    namespace
    {
        [[nodiscard]] int ObjectQuarterTurns(const Objects::ObjectRotation rotation)
        {
            switch (rotation)
            {
                case Objects::ObjectRotation::North: return 0;
                case Objects::ObjectRotation::East: return 1;
                case Objects::ObjectRotation::South: return 2;
                case Objects::ObjectRotation::West: return 3;
            }
            throw std::invalid_argument("object rotation must be one of four directions");
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

    SpriteDirection ObjectPresentation::PresentedDirection(
        const Objects::ObjectRotation objectRotation,
        const World::ViewRotation viewRotation)
    {
        const int direction = (ObjectQuarterTurns(objectRotation)
                               + ViewQuarterTurns(viewRotation)) % 4;
        return static_cast<SpriteDirection>(direction);
    }

    ObjectSpriteSelection ObjectPresentation::SelectSprite(
        const Objects::ObjectDefinition& definition,
        const std::string_view state,
        const Objects::ObjectRotation objectRotation,
        const World::ViewRotation viewRotation)
    {
        const auto found = definition.visual.states.find(state);
        if (found == definition.visual.states.end())
            throw std::invalid_argument("object visual state is not authored");

        const SpriteDirection direction = PresentedDirection(objectRotation, viewRotation);
        const auto index = static_cast<std::size_t>(direction);
        return {direction, &found->second.directions[index]};
    }

    ObjectSpriteSelection ObjectPresentation::SelectDefaultSprite(
        const Objects::ObjectDefinition& definition,
        const Objects::ObjectRotation objectRotation,
        const World::ViewRotation viewRotation)
    {
        return SelectSprite(
            definition, definition.visual.defaultState, objectRotation, viewRotation);
    }
}
