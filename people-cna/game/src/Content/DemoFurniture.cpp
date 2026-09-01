#include "People/Content/DemoFurniture.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace People::Content
{
    namespace
    {
        constexpr int SpriteAnchorX = 64;
        constexpr int SpriteAnchorY = 96;

        [[nodiscard]] Objects::ObjectVisualDefinition MakeVisual(
            const std::string_view assetStem)
        {
            const std::array<std::string_view, 4> directions{{
                "north", "east", "south", "west"
            }};
            Objects::DirectionalSpriteSet spriteSet;
            for (std::size_t index = 0; index < directions.size(); ++index)
            {
                spriteSet.directions[index] = {
                    std::string(assetStem) + ".default." + std::string(directions[index]),
                    SpriteAnchorX,
                    SpriteAnchorY
                };
            }

            Objects::ObjectVisualDefinition visual;
            visual.states.emplace("default", std::move(spriteSet));
            return visual;
        }

        [[nodiscard]] Objects::ObjectDefinition MakeDefinition(
            const std::string_view id,
            std::string displayName,
            const Objects::ObjectCategory category,
            const std::int64_t price,
            std::vector<Objects::FootprintOffset> footprint,
            std::vector<Objects::FootprintOffset> clearance)
        {
            return {
                std::string(id),
                std::move(displayName),
                category,
                price,
                std::move(footprint),
                std::move(clearance),
                0x0F,
                MakeVisual(std::string("people.generated.") + std::string(id.substr(7))),
                {}
            };
        }

        void Register(Objects::ObjectWorld& world, Objects::ObjectDefinition definition)
        {
            if (!world.RegisterDefinition(std::move(definition)))
                throw std::logic_error("duplicate demo furniture definition ID");
        }

        void Place(Objects::ObjectWorld& world, Objects::ObjectInstance instance)
        {
            const Objects::PlacementResult result = world.Place(std::move(instance));
            if (!result.IsValid())
                throw std::logic_error("demo furniture placement failed validation");
        }
    }

    void DemoFurniture::Populate(Objects::ObjectWorld& world)
    {
        Register(world, MakeDefinition(
            BedId, "Cedar Nest Bed", Objects::ObjectCategory::Beds, 425,
            {{0, 0}, {0, -1}}, {{0, 1}}));
        Register(world, MakeDefinition(
            ChairId, "Sunny Dining Chair", Objects::ObjectCategory::Seating, 80,
            {{0, 0}}, {{0, 1}}));
        Register(world, MakeDefinition(
            TableId, "Roundleaf Table", Objects::ObjectCategory::Surfaces, 165,
            {{0, 0}}, {}));
        Register(world, MakeDefinition(
            RefrigeratorId, "Mintbox Refrigerator", Objects::ObjectCategory::Appliances, 650,
            {{0, 0}}, {{0, 1}}));
        Register(world, MakeDefinition(
            ToiletId, "Cloudline Toilet", Objects::ObjectCategory::Plumbing, 280,
            {{0, 0}}, {{0, 1}}));

        Place(world, {1001, std::string(BedId), {7, 7, 0}, Objects::ObjectRotation::North});
        Place(world, {1002, std::string(ChairId), {8, 10, 0}, Objects::ObjectRotation::East});
        Place(world, {1003, std::string(TableId), {9, 9, 0}, Objects::ObjectRotation::South});
        Place(world, {1004, std::string(RefrigeratorId), {10, 6, 0}, Objects::ObjectRotation::West});
        Place(world, {1005, std::string(ToiletId), {11, 9, 0}, Objects::ObjectRotation::South});
    }
}
