#include "CnaTamagotchi/Domain/P1SpriteCatalog.hpp"

#include <array>
#include <iostream>
#include <string_view>

using namespace CnaTamagotchi::Domain;

namespace {

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

bool framesDiffer(const P1SpriteFrame& first, const P1SpriteFrame& second)
{
    return first.rows != second.rows || first.originX != second.originX ||
           first.originY != second.originY || first.rowCount != second.rowCount;
}

void testP1FramesAreFixedCellAnimations()
{
    const P1Sprite& mametchi = P1SpriteCatalog::spriteForCharacter("mametchi");
    expect(mametchi.idleFrames.size() == P1Sprite::MaximumIdleFrameCount,
           "every P1 home sprite must provide the maximum phase storage");
    expect(mametchi.idleFrameCount == 3U,
           "Mametchi must retain its three active reference phases");

    for (const P1SpriteFrame& frame : mametchi.visibleIdleFrames()) {
        expect(frame.rowCount == 10U, "current Mametchi cells must be ten pixels high");
        expect(frame.originX == 8 && frame.originY == 3,
               "current Mametchi cells must retain their observed origin");
        for (const std::string_view row : frame.visibleRows()) {
            expect(row.size() == 16U, "P1 character cells must be sixteen pixels wide");
        }
    }

    // Reference-derived P1 phases have a fixed cell origin but change their
    // silhouette. This guards against returning to the old translate-a-sprite
    // pseudo-animation.
    expect(mametchi.idleFrame(0).rows[3] == ".......##.......",
           "Mametchi's first home phase must retain the P1 reference silhouette");
    expect(mametchi.idleFrame(1).rows[2] == "......####......",
           "Mametchi's second home phase must be a distinct P1 drawing");
    expect(mametchi.idleFrame(0).rows != mametchi.idleFrame(1).rows,
           "adjacent P1 idle phases must not be the same static drawing");
    expect(mametchi.idleFrame(1).rows != mametchi.idleFrame(2).rows,
           "the third P1 idle phase must remain distinct");
}

void testEveryKnownP1CharacterHasUsableFrames()
{
    constexpr std::array<std::string_view, 12> characterIds{{
        "egg",
        "babytchi",
        "marutchi",
        "tamatchi",
        "kuchitamatchi",
        "mametchi",
        "ginjirotchi",
        "maskutchi",
        "kuchipatchi",
        "nyorotchi",
        "tarakotchi",
        "bill",
    }};

    for (const std::string_view id : characterIds) {
        const P1Sprite& sprite = P1SpriteCatalog::spriteForCharacter(id);
        expect(sprite.idleFrameCount >= 2U &&
                   sprite.idleFrameCount <= P1Sprite::MaximumIdleFrameCount,
               "each P1 home sequence must declare a supported active phase count");
        bool hasLitPixelInEachFrame = true;
        for (const P1SpriteFrame& frame : sprite.visibleIdleFrames()) {
            expect(frame.rowCount > 0U && frame.rowCount <= P1SpriteFrame::MaximumRows,
                   "each P1 frame must declare a usable row count");
            expect(frame.originX >= 0 && frame.originY >= 0,
                   "each P1 frame must start inside the LCD");
            std::size_t frameWidth = 0U;
            for (const std::string_view row : frame.visibleRows()) {
                expect(!row.empty(), "active P1 animation rows must not be empty");
                if (frameWidth == 0U) {
                    frameWidth = row.size();
                } else {
                    expect(row.size() == frameWidth,
                           "all rows in one P1 phase must have the same true width");
                }
                expect(frame.originX + static_cast<int>(row.size()) <= 32,
                       "all P1 animation rows must stay inside the LCD width");
            }
            expect(frame.originY + static_cast<int>(frame.rowCount) <= 16,
                   "all P1 animation rows must stay inside the LCD height");
            bool hasLitPixel = false;
            for (const std::string_view row : frame.visibleRows()) {
                hasLitPixel = hasLitPixel || row.find('#') != std::string_view::npos;
            }
            hasLitPixelInEachFrame = hasLitPixelInEachFrame && hasLitPixel;
        }
        expect(hasLitPixelInEachFrame,
               "each declared P1 idle phase must contain a visible one-bit drawing");
        for (std::size_t frameIndex = 1U; frameIndex < sprite.idleFrameCount; ++frameIndex) {
            expect(framesDiffer(sprite.idleFrame(frameIndex - 1U), sprite.idleFrame(frameIndex)),
                   "adjacent active P1 idle phases must remain independently drawn");
        }
    }
}

void testEggKeepsItsObservedP1Silhouette()
{
    const P1Sprite& egg = P1SpriteCatalog::spriteForCharacter("egg");

    // These are hand transcriptions of stable cells in a fresh visual P1 LCD
    // trace, rather than data imported from a ROM or emulator implementation.
    constexpr std::array<std::string_view, 11> expectedWide{{
        ".......###......", ".....#######....", "....#.#####.#...", "...#..#####..#..",
        "..##.##########.", "..##.##########.", "..#####...#####.", "..#..##...#####.",
        "...#..#####..#..", "....#######.#...", "...###########..",
    }};
    constexpr std::array<std::string_view, 12> expectedTall{{
        "......#####.....", ".....#.#####....", "....#..##..##...", "....#.###...#...",
        "...###########..", "...###########..", "...####...####..", "...####...#..#..",
        "...#..#####.##..", "....#..######...", ".....###...#....", "....#########...",
    }};

    expect(egg.idleFrameCount == 2U, "the P1 egg must alternate exactly two stable silhouettes");
    expect(egg.idleFrame(0).originX == 8 && egg.idleFrame(0).originY == 4 &&
               egg.idleFrame(0).rowCount == 11U,
           "the wide P1 egg phase must retain its true LCD bounds");
    expect(egg.idleFrame(1).originX == 8 && egg.idleFrame(1).originY == 3 &&
               egg.idleFrame(1).rowCount == 12U,
           "the tall P1 egg phase must retain its true LCD bounds");
    expect(egg.idleFrameSeconds == 0.625F,
           "the P1 egg must retain the cadence inferred from its 30 fps trace");
    for (std::size_t row = 0; row < expectedWide.size(); ++row) {
        expect(egg.idleFrame(0).rows[row] == expectedWide[row],
               "every hand-read row of the wide P1 egg must remain exact");
    }
    for (std::size_t row = 0; row < expectedTall.size(); ++row) {
        expect(egg.idleFrame(1).rows[row] == expectedTall[row],
               "every hand-read row of the tall P1 egg must remain exact");
    }
    expect(!framesDiffer(egg.idleFrame(2), egg.idleFrame(0)),
           "the two-phase P1 egg sequence must wrap directly back to its first "
           "phase");
}

void testBabytchiKeepsItsObservedMovingTrace()
{
    const P1Sprite& babytchi = P1SpriteCatalog::spriteForCharacter("babytchi");
    constexpr std::array<int, 20> expectedOrigins{{
        11, 9, 13, 16,
        18, 15, 14, 11,
        6, 11, 9, 13,
        15, 18, 16, 14,
        10, 6, 12, 9,
    }};
    constexpr std::array<std::string_view, 6> expectedFull{{
        ".####.", "#.##.#", "######", "##..##", "##..##", "######",
    }};

    expect(babytchi.idleFrameCount == expectedOrigins.size(),
           "Babytchi must retain all twenty consecutive observed stable phases");
    expect(babytchi.idleFrameSeconds == 0.46F,
           "Babytchi must retain the cadence inferred from its 30 fps trace");

    for (std::size_t phase = 0U; phase < expectedOrigins.size(); ++phase) {
        const P1SpriteFrame& frame = babytchi.idleFrame(phase);
        const bool isFullPose = phase % 4U < 2U;
        expect(frame.originX == expectedOrigins[phase],
               "every observed Babytchi phase must retain its horizontal origin");

        if (isFullPose) {
            expect(frame.originY == 10 && frame.rowCount == expectedFull.size(),
                   "each full Babytchi pose must retain its 6x6 lower-LCD bounds");
            for (std::size_t row = 0U; row < expectedFull.size(); ++row) {
                expect(frame.rows[row] == expectedFull[row],
                       "every hand-read full Babytchi row must remain exact");
            }
        } else {
            expect(frame.originY == 15 && frame.rowCount == 1U,
                   "each squashed Babytchi pose must stay on the last LCD row");
            expect(frame.rows[0] == "####",
                   "the hand-read squashed Babytchi row must remain exact");
        }
    }

    expect(!framesDiffer(babytchi.idleFrame(expectedOrigins.size()), babytchi.idleFrame(0)),
           "the observed Babytchi trace must wrap at its declared active count");
}

} // namespace

int main()
{
    testP1FramesAreFixedCellAnimations();
    testEveryKnownP1CharacterHasUsableFrames();
    testEggKeepsItsObservedP1Silhouette();
    testBabytchiKeepsItsObservedMovingTrace();

    if (failures == 0) {
        std::cout << "P1SpriteCatalogTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
