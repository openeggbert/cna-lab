#include "People/Rendering/ObjectPresentation.hpp"

#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <string>

using namespace People::Objects;
using namespace People::Rendering;
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

    void CheckThrows(const std::function<void()>& operation, const std::string& message)
    {
        try
        {
            operation();
            Check(false, message);
        }
        catch (const std::exception&)
        {
        }
    }

    DirectionalSpriteSet MakeSpriteSet(const std::string& state)
    {
        DirectionalSpriteSet result;
        result.directions = {{
            ObjectSpriteReference{"people.chair." + state + ".north", 24, 47},
            ObjectSpriteReference{"people.chair." + state + ".east", 24, 47},
            ObjectSpriteReference{"people.chair." + state + ".south", 24, 47},
            ObjectSpriteReference{"people.chair." + state + ".west", 24, 47}
        }};
        return result;
    }

    ObjectDefinition MakeVisualChair()
    {
        ObjectDefinition definition{
            "people.chair.visual", "Visual Chair", ObjectCategory::Seating,
            85, {{0, 0}}, {{0, 1}}, 0x0F, {}, {}
        };
        definition.visual.defaultState = "default";
        definition.visual.states.emplace("default", MakeSpriteSet("default"));
        definition.visual.states.emplace("broken", MakeSpriteSet("broken"));
        return definition;
    }

    void TestAllDirectionCombinations()
    {
        const ObjectDefinition definition = MakeVisualChair();
        const std::array<std::string, 4> suffixes{{"north", "east", "south", "west"}};
        for (int objectIndex = 0; objectIndex < 4; ++objectIndex)
        {
            for (int viewIndex = 0; viewIndex < 4; ++viewIndex)
            {
                const auto objectRotation = static_cast<ObjectRotation>(objectIndex);
                const auto viewRotation = static_cast<ViewRotation>(viewIndex);
                const int expected = (objectIndex + viewIndex) % 4;
                const ObjectSpriteSelection selection = ObjectPresentation::SelectDefaultSprite(
                    definition, objectRotation, viewRotation);
                Check(static_cast<int>(selection.direction) == expected,
                      "all 16 orientation/view combinations use the projection convention");
                Check(selection.reference != nullptr
                          && selection.reference->assetId
                              == "people.chair.default." + suffixes[static_cast<std::size_t>(expected)],
                      "direction selection resolves exact authored metadata");
                Check(selection.reference != nullptr
                          && selection.reference->anchorX == 24
                          && selection.reference->anchorY == 47,
                      "selection preserves authored floor-contact anchor");
            }
        }
    }

    void TestStateSelectionAndValidation()
    {
        const ObjectDefinition definition = MakeVisualChair();
        const ObjectSpriteSelection broken = ObjectPresentation::SelectSprite(
            definition, "broken", ObjectRotation::West, ViewRotation::East);
        Check(broken.direction == SpriteDirection::North
                  && broken.reference != nullptr
                  && broken.reference->assetId == "people.chair.broken.north",
              "state and relative direction select one exact authored frame");
        CheckThrows([&] {
            (void)ObjectPresentation::SelectSprite(
                definition, "dirty", ObjectRotation::North, ViewRotation::North);
        }, "unknown visual state is rejected without silent fallback");

        LotGrid lot(3, 3);
        ObjectWorld world(lot);
        ObjectDefinition invalid = MakeVisualChair();
        invalid.visual.defaultState = "missing";
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "catalog rejects a missing default visual state");
        invalid = MakeVisualChair();
        invalid.visual.states.at("default").directions[2].assetId.clear();
        CheckThrows([&] { (void)world.RegisterDefinition(invalid); },
                    "catalog rejects an incomplete four-direction set");
    }
}

int main()
{
    TestAllDirectionCombinations();
    TestStateSelectionAndValidation();

    if (failures != 0)
    {
        std::cerr << failures << " People object-presentation test(s) failed\n";
        return 1;
    }
    std::cout << "All People object-presentation tests passed\n";
    return 0;
}
