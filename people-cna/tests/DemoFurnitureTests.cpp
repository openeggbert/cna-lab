#include "People/Content/DemoFurniture.hpp"

#include <exception>
#include <iostream>
#include <set>
#include <string>

using namespace People::Content;
using namespace People::Objects;
using namespace People::World;

namespace
{
    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    void TestFiveDefinitionsAndPlacements()
    {
        LotGrid lot(20, 20);
        ObjectWorld world(lot);
        DemoFurniture::Populate(world);

        Check(world.Catalog().Size() == 5, "demo catalog contains exactly five definitions");
        Check(world.Instances().size() == 5, "demo lot contains one of each furniture type");

        const std::set<std::string_view> expectedIds{
            DemoFurniture::BedId,
            DemoFurniture::ChairId,
            DemoFurniture::TableId,
            DemoFurniture::RefrigeratorId,
            DemoFurniture::ToiletId
        };
        std::set<std::string> assetIds;
        std::set<ObjectRotation> placedRotations;
        for (const std::string_view id : expectedIds)
        {
            const ObjectDefinition* definition = world.Catalog().Find(id);
            Check(definition != nullptr, "every named demo definition is queryable");
            if (definition == nullptr)
                continue;
            Check(definition->price > 0, "every demo definition has a positive price");
            Check(definition->visual.states.size() == 1,
                  "every demo definition has one initial visual state");
            const auto state = definition->visual.states.find(definition->visual.defaultState);
            Check(state != definition->visual.states.end(),
                  "every demo definition resolves its default state");
            if (state == definition->visual.states.end())
                continue;
            for (const ObjectSpriteReference& sprite : state->second.directions)
            {
                Check(!sprite.assetId.empty(), "every demo direction has an asset ID");
                Check(sprite.anchorX == 64 && sprite.anchorY == 96,
                      "all generated furniture uses the v1 shared contact anchor");
                Check(assetIds.insert(sprite.assetId).second,
                      "all twenty directional asset IDs are unique");
            }
        }
        Check(assetIds.size() == 20, "five objects expose twenty directional sprite IDs");

        for (const auto& [id, instance] : world.Instances())
        {
            placedRotations.insert(instance.rotation);
            Check(id == instance.id, "instance map key matches stable persistent ID");
            const std::vector<TileCoordinate> footprint = world.FootprintCells(instance);
            Check(!footprint.empty(), "every demo instance has a physical footprint");
            for (const TileCoordinate tile : footprint)
            {
                Check(lot.Contains(tile), "every demo footprint cell stays inside the lot");
                Check(world.OccupiedBy(tile) == std::optional<ObjectInstanceId>(id),
                      "every demo footprint cell selects its owning instance");
            }
        }
        Check(placedRotations.size() == 4,
              "demo arrangement visibly exercises all four simulation orientations");

        const ObjectInstance* bed = world.Find(1001);
        Check(bed != nullptr && world.FootprintCells(*bed).size() == 2,
              "bed proves a validated rotated multi-tile footprint");
    }
}

int main()
{
    try
    {
        TestFiveDefinitionsAndPlacements();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: unexpected demo furniture exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0)
    {
        std::cerr << failures << " People demo-furniture test(s) failed\n";
        return 1;
    }
    std::cout << "All People demo-furniture tests passed\n";
    return 0;
}
