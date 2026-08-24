#include "People/Content/DemoResident.hpp"
#include "People/Rendering/ResidentPresentation.hpp"

#include <exception>
#include <iostream>
#include <set>
#include <string>

using namespace People::Content;
using namespace People::Rendering;
using namespace People::Simulation;
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

    void TestMaraContent()
    {
        const ResidentState state = DemoResident::MaraState();
        Check(state.id == DemoResident::MaraId
                  && state.householdId == DemoResident::Household
                  && state.displayName == "Mara Vale",
              "predefined resident has original stable identity");

        const ResidentIdleSpriteSet sprites = DemoResident::MaraIdleSprites();
        std::set<std::string> assetIds;
        for (const ResidentSpriteReference& sprite : sprites.directions)
        {
            Check(sprite.footAnchorX == 32 && sprite.footAnchorY == 88,
                  "all four idle directions share one foot anchor");
            Check(assetIds.insert(sprite.assetId).second,
                  "all four idle directions have unique asset IDs");
        }
        Check(assetIds.size() == 4, "predefined resident authors exactly four idle views");
    }

    void TestAllFacingAndViewCombinations()
    {
        const ResidentIdleSpriteSet sprites = DemoResident::MaraIdleSprites();
        for (int facingIndex = 0; facingIndex < 4; ++facingIndex)
        {
            for (int viewIndex = 0; viewIndex < 4; ++viewIndex)
            {
                const auto facing = static_cast<ResidentFacing>(facingIndex);
                const auto view = static_cast<ViewRotation>(viewIndex);
                const int expected = (facingIndex + viewIndex) % 4;
                Check(static_cast<int>(ResidentPresentation::PresentedDirection(facing, view))
                          == expected,
                      "all sixteen resident-facing/view combinations follow lot projection");
                Check(&ResidentPresentation::SelectIdleSprite(sprites, facing, view)
                          == &sprites.directions[static_cast<std::size_t>(expected)],
                      "idle selection returns exact authored direction metadata");
            }
        }

        try
        {
            (void)ResidentPresentation::PresentedDirection(
                static_cast<ResidentFacing>(99), ViewRotation::North);
            Check(false, "invalid resident facing is rejected by presentation");
        }
        catch (const std::exception&)
        {
        }
    }
}

int main()
{
    TestMaraContent();
    TestAllFacingAndViewCombinations();

    if (failures != 0)
    {
        std::cerr << failures << " People resident-presentation test(s) failed\n";
        return 1;
    }
    std::cout << "All People resident-presentation tests passed\n";
    return 0;
}
