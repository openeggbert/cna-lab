#include "TamagotchiCna/Domain/P1SpriteCatalog.hpp"

#include <array>
#include <iostream>
#include <string_view>

using namespace TamagotchiCna::Domain;

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
    constexpr std::array<std::string_view, 10> expectedWide{{
        ".....##.....", "...######...", "..#.####.#..", ".#..####..#.",
        "##.#########", "#####..#####", "#..##..#####", ".#..####..#.",
        "..######.#..", ".##########.",
    }};
    constexpr std::array<std::string_view, 11> expectedTall{{
        "...##.#...", "..#.####..", ".#..##.##.", ".#.###..#.", "##########",
        "####..####", "####..#..#", "#..####.##", ".#..#####.", "..###..#..",
        ".########.",
    }};

    expect(egg.idleFrameCount == 2U, "the P1 egg must alternate exactly two stable silhouettes");
    expect(egg.idleFrame(0).originX == 10 && egg.idleFrame(0).originY == 3 &&
               egg.idleFrame(0).rowCount == expectedWide.size(),
           "the wide P1 egg phase must retain its true LCD bounds");
    expect(egg.idleFrame(1).originX == 11 && egg.idleFrame(1).originY == 2 &&
               egg.idleFrame(1).rowCount == expectedTall.size(),
           "the tall P1 egg phase must retain its true LCD bounds");
    expect(egg.idleFrameSeconds == 0.70F,
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
    constexpr std::array<int, 36> expectedOrigins{{
        7, 11, 8, 11,
        15, 18, 14, 12,
        10, 7, 10, 8,
        12, 15, 17, 14,
        13, 10, 6, 10,
        9, 12, 14, 17,
        15, 13, 9, 6,
        11, 9, 11, 14,
        18, 15, 12, 9,
    }};
    constexpr std::array<std::string_view, 6> expectedFull{{
        ".####.", "#.##.#", "######", "##..##", "######", ".####.",
    }};
    constexpr std::array<std::string_view, 3> expectedSquash{{
        "..####..", ".#.##.#.", "########"}};

    expect(babytchi.idleFrameCount == expectedOrigins.size(),
           "Babytchi must retain its complete thirty-six-phase observed cycle");
    expect(babytchi.idleFrameSeconds == 0.46F,
           "Babytchi must retain the cadence inferred from its 30 fps trace");

    for (std::size_t phase = 0U; phase < expectedOrigins.size(); ++phase) {
        const P1SpriteFrame& frame = babytchi.idleFrame(phase);
        const bool isFullPose = phase % 4U < 2U;
        expect(frame.originX == expectedOrigins[phase],
               "every observed Babytchi phase must retain its horizontal origin");

        if (isFullPose) {
            expect(frame.originY == 9 && frame.rowCount == expectedFull.size(),
                   "each full Babytchi pose must retain its exact 6x6 bounds");
            for (std::size_t row = 0U; row < expectedFull.size(); ++row) {
                expect(frame.rows[row] == expectedFull[row],
                       "every hand-read full Babytchi row must remain exact");
            }
        } else {
            expect(frame.originY == 13 && frame.rowCount == expectedSquash.size(),
                   "each squashed Babytchi pose must retain its exact 8x3 bounds");
            for (std::size_t row = 0U; row < expectedSquash.size(); ++row) {
                expect(frame.rows[row] == expectedSquash[row],
                       "every hand-read squashed Babytchi row must remain exact");
            }
        }
    }

    expect(!framesDiffer(babytchi.idleFrame(expectedOrigins.size()), babytchi.idleFrame(0)),
           "the observed Babytchi cycle must wrap at its declared active count");
}

void testMarutchiKeepsItsObservedP1Silhouettes()
{
    const P1Sprite& marutchi = P1SpriteCatalog::spriteForCharacter("marutchi");
    constexpr std::array<std::string_view, 9> expectedLong{{
        "..######..", ".#......#.", "#..#..#..#", "#........#", "#...##...#",
        "#........#", "#........#", ".#......#.", "..######..",
    }};
    constexpr std::array<std::string_view, 8> expectedShort{{
        "..######..", ".#......#.", "#.#....#.#", "#...##...#", "#...##...#",
        "#........#", ".#......#.", "..######..",
    }};
    constexpr std::array<int, 28> expectedOrigins{{
        9, 7, 5, 7, 9, 11, 13, 11, 9, 10, 11, 12, 13, 11,
        9, 7, 5, 7, 9, 11, 13, 11, 9, 10, 11, 12, 13, 11,
    }};

    expect(marutchi.idleFrameCount == expectedOrigins.size(),
           "awake Marutchi must retain its complete observed 28-phase path");
    expect(marutchi.idleFrameSeconds == 16.0F / 30.0F,
           "awake Marutchi must retain its observed nominal 16-frame cadence");
    for (std::size_t phase = 0U; phase < expectedOrigins.size(); ++phase) {
        const bool longPose = phase % 4U == 0U || phase % 4U == 3U;
        const P1SpriteFrame& frame = marutchi.idleFrame(phase);
        expect(frame.originX == expectedOrigins[phase] && frame.originY == 3,
               "each awake Marutchi phase must retain its observed LCD origin");
        expect(frame.rowCount == (longPose ? expectedLong.size() : expectedShort.size()),
               "each awake Marutchi phase must retain its observed pose height");
        const std::span<const std::string_view> expected = longPose
            ? std::span<const std::string_view>(expectedLong)
            : std::span<const std::string_view>(expectedShort);
        for (std::size_t row = 0U; row < expected.size(); ++row) {
            expect(frame.rows[row] == expected[row],
                   "every hand-read awake Marutchi row must remain exact");
        }
    }
    expect(!framesDiffer(marutchi.idleFrame(expectedOrigins.size()), marutchi.idleFrame(0)),
           "the 28-phase awake Marutchi path must wrap directly");
}

void testWasteKeepsItsObservedStackedP1Animation()
{
    const P1Sprite& waste = P1SpriteCatalog::waste();
    constexpr std::array<std::string_view, 8> expectedFirst{{
        ".......#", ".#....#.", "#......#", ".#.#....",
        "...##...", "..##.#..", ".####.#.", ".######.",
    }};
    constexpr std::array<std::string_view, 8> expectedSecond{{
        "#.......", ".#....#.", "#......#", "...#..#.",
        "...##...", "..##.#..", ".#.####.", ".######.",
    }};

    expect(waste.idleFrameCount == 2U,
           "P1 waste must alternate exactly two observed stable phases");
    expect(waste.idleFrameSeconds == 1.0F,
           "P1 waste must retain its approximately one-second cadence");
    for (std::size_t phase = 0U; phase < waste.idleFrameCount; ++phase) {
        const P1SpriteFrame& frame = waste.idleFrame(phase);
        expect(frame.originX == 24 && frame.originY == 8 && frame.rowCount == 8U,
               "one P1 waste glyph must occupy the bottom-right 8x8 cell");
        const auto& expected = phase == 0U ? expectedFirst : expectedSecond;
        for (std::size_t row = 0U; row < expected.size(); ++row) {
            expect(frame.rows[row] == expected[row],
                   "every hand-read P1 waste row must remain exact");
        }
    }
    expect(!framesDiffer(waste.idleFrame(2), waste.idleFrame(0)),
           "the two-phase waste animation must wrap directly");

    expect(waste.idleFrame(0).originY == 8,
           "one waste must occupy the lower-right 8x8 cell");
    expect(waste.idleFrame(0).originY - 8 == 0,
           "a second waste must stack directly above the first at the top edge");
}

void testBabytchiSicknessKeepsItsObservedP1Cycle()
{
    const P1Sprite& sick = P1SpriteCatalog::sickSpriteForCharacter("babytchi");
    constexpr std::array<std::string_view, 3> expectedWide{{
        ".######.", "#..##..#", "########",
    }};
    constexpr std::array<std::string_view, 3> expectedNarrow{{
        "..####..", ".#.##.#.", "########",
    }};

    expect(sick.idleFrameCount == 2U && sick.idleFrameSeconds == 0.93F,
           "sick Babytchi must retain its two observed bottom poses and cadence");
    for (std::size_t phase = 0U; phase < sick.idleFrameCount; ++phase) {
        const P1SpriteFrame& frame = sick.idleFrame(phase);
        expect(frame.originX == 12 && frame.originY == 13 && frame.rowCount == 3U,
               "each sick Babytchi phase must retain its exact 8x3 bottom bounds");
        const auto& expected = phase == 0U ? expectedWide : expectedNarrow;
        for (std::size_t row = 0U; row < expected.size(); ++row) {
            expect(frame.rows[row] == expected[row],
                   "every hand-read sick Babytchi row must remain exact");
        }
    }
    expect(!framesDiffer(sick.idleFrame(2), sick.idleFrame(0)),
           "the sick Babytchi cycle must wrap directly");

    const P1SpriteFrame& indicator = P1SpriteCatalog::sicknessIndicator();
    constexpr std::array<std::string_view, 7> expectedIndicator{{
        ".#####.", "#######", "#..#..#", "#######", "###.###", ".#####.", ".#.#.#.",
    }};
    expect(indicator.originX == 25 && indicator.originY == 1
               && indicator.rowCount == expectedIndicator.size(),
           "the P1 sickness indicator must retain its exact top-right 7x7 bounds");
    for (std::size_t row = 0U; row < expectedIndicator.size(); ++row) {
        expect(indicator.rows[row] == expectedIndicator[row],
               "every hand-read sickness-indicator row must remain exact");
    }

    expect(&P1SpriteCatalog::sickSpriteForCharacter("marutchi")
               == &P1SpriteCatalog::spriteForCharacter("marutchi"),
           "an unobserved sick form must keep its normal pose rather than inventing one");
}

void testSleepIndicatorKeepsItsObservedP1Cycle()
{
    const P1Sprite& sleep = P1SpriteCatalog::sleepIndicator();
    constexpr std::array<std::string_view, 6> expectedSmall{{
        "....###", "......#", ".....#.", "....#..", "..#.###", "#......",
    }};
    constexpr std::array<std::string_view, 6> expectedLarge{{
        "####", "...#", "..#.", ".#..", "#...", "####",
    }};

    expect(sleep.idleFrameCount == 2U && sleep.idleFrameSeconds == 0.82F,
           "the P1 sleep overlay must retain its two observed Z phases and cadence");
    expect(sleep.idleFrame(0).originX == 24 && sleep.idleFrame(0).originY == 0
               && sleep.idleFrame(0).rowCount == expectedSmall.size(),
           "the small-Z arrangement must retain its exact 7x6 bounds");
    expect(sleep.idleFrame(1).originX == 25 && sleep.idleFrame(1).originY == 2
               && sleep.idleFrame(1).rowCount == expectedLarge.size(),
           "the large Z must retain its exact 4x6 bounds");
    for (std::size_t row = 0U; row < expectedSmall.size(); ++row) {
        expect(sleep.idleFrame(0).rows[row] == expectedSmall[row],
               "every hand-read small-Z row must remain exact");
        expect(sleep.idleFrame(1).rows[row] == expectedLarge[row],
               "every hand-read large-Z row must remain exact");
    }
    expect(!framesDiffer(sleep.idleFrame(2), sleep.idleFrame(0)),
           "the two-phase sleep overlay must wrap directly");

    const P1Sprite& marutchi = P1SpriteCatalog::sleepingSpriteForCharacter("marutchi");
    constexpr std::array<std::string_view, 9> expectedLongBody{{
        "..######..", ".#......#.", "#.##..##.#", "#........#", "#...##...#",
        "#........#", "#........#", ".#......#.", "..######..",
    }};
    expect(marutchi.idleFrameCount == 2U && marutchi.idleFrameSeconds == 0.92F
               && framesDiffer(marutchi.idleFrame(0), marutchi.idleFrame(1)),
           "sleeping Marutchi must retain its separate fixed-origin two-phase body cycle");
    expect(marutchi.idleFrame(0).originX == 11 && marutchi.idleFrame(0).originY == 3,
           "sleeping Marutchi must retain its measured body origin below the Z overlay");
    for (std::size_t row = 0U; row < expectedLongBody.size(); ++row) {
        expect(marutchi.idleFrame(0).rows[row] == expectedLongBody[row],
               "every closed-eye sleeping Marutchi row must remain exact");
    }
}

} // namespace

int main()
{
    testP1FramesAreFixedCellAnimations();
    testEveryKnownP1CharacterHasUsableFrames();
    testEggKeepsItsObservedP1Silhouette();
    testBabytchiKeepsItsObservedMovingTrace();
    testMarutchiKeepsItsObservedP1Silhouettes();
    testWasteKeepsItsObservedStackedP1Animation();
    testBabytchiSicknessKeepsItsObservedP1Cycle();
    testSleepIndicatorKeepsItsObservedP1Cycle();
    if (failures == 0) {
        std::cout << "P1SpriteCatalogTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
