#include "People/Content/DemoResident.hpp"
#include "People/Rendering/ResidentPresentation.hpp"

#include <cstddef>
#include <cstdint>
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

    void TestMaraWalkContent()
    {
        const ResidentIdleSpriteSet idle = DemoResident::MaraIdleSprites();
        const ResidentWalkSpriteSet walk = DemoResident::MaraWalkSprites();
        Check(ResidentWalkSpriteSet::FrameCount >= 2,
              "walk clip authors at least two frames per direction");

        std::set<std::string> assetIds;
        for (const ResidentSpriteReference& sprite : idle.directions)
            assetIds.insert(sprite.assetId);
        for (const auto& frame : walk.frames)
        {
            for (const ResidentSpriteReference& sprite : frame)
            {
                Check(sprite.footAnchorX == 32 && sprite.footAnchorY == 88,
                      "every walk frame reuses the shared idle foot anchor");
                Check(assetIds.insert(sprite.assetId).second,
                      "every walk frame has an asset ID distinct from idle and siblings");
            }
        }
        Check(assetIds.size() == 4 + 4 * ResidentWalkSpriteSet::FrameCount,
              "idle and walk clips together author one asset per direction and frame");
    }

    void TestWalkFrameBoundaries()
    {
        constexpr std::uint32_t perFrame = ResidentPresentation::WalkUnitsPerFrame;
        constexpr std::uint32_t cycle = perFrame * ResidentWalkSpriteSet::FrameCount;

        Check(ResidentPresentation::WalkFrameIndex(0) == 0,
              "a route starts on the first walk frame");
        Check(ResidentPresentation::WalkFrameIndex(perFrame - 1) == 0,
              "the first frame holds until the exact phase boundary");
        Check(ResidentPresentation::WalkFrameIndex(perFrame) == 1,
              "the second frame begins exactly at the phase boundary");
        Check(ResidentPresentation::WalkFrameIndex(cycle - 1) == 1,
              "the last frame holds until the cycle wraps");
        Check(ResidentPresentation::WalkFrameIndex(cycle) == 0,
              "the walk cycle wraps to the first frame");
        Check(ResidentPresentation::WalkFrameIndex(cycle + perFrame) == 1,
              "the walk cycle repeats deterministically across tiles");

        // 1000 units per tile and 125 per tick means the flip lands on a tick.
        Check(cycle == 1000, "one walk cycle covers exactly one traversed tile");
        Check(perFrame % 125 == 0,
              "walk frames change on a fixed simulation tick, never between ticks");
    }

    void TestWalkSelectionAllCombinations()
    {
        const ResidentWalkSpriteSet walk = DemoResident::MaraWalkSprites();
        constexpr std::uint32_t perFrame = ResidentPresentation::WalkUnitsPerFrame;
        for (int facingIndex = 0; facingIndex < 4; ++facingIndex)
        {
            for (int viewIndex = 0; viewIndex < 4; ++viewIndex)
            {
                const auto facing = static_cast<ResidentFacing>(facingIndex);
                const auto view = static_cast<ViewRotation>(viewIndex);
                const auto expected = static_cast<std::size_t>((facingIndex + viewIndex) % 4);
                for (std::size_t frame = 0;
                     frame < ResidentWalkSpriteSet::FrameCount; ++frame)
                {
                    const std::uint32_t units =
                        static_cast<std::uint32_t>(frame) * perFrame;
                    Check(&ResidentPresentation::SelectWalkSprite(walk, facing, view, units)
                              == &walk.frames[frame][expected],
                          "walk selection returns the authored frame for every "
                          "direction and view");
                }
            }
        }
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
    TestMaraWalkContent();
    TestWalkFrameBoundaries();
    TestWalkSelectionAllCombinations();

    if (failures != 0)
    {
        std::cerr << failures << " People resident-presentation test(s) failed\n";
        return 1;
    }
    std::cout << "All People resident-presentation tests passed\n";
    return 0;
}
