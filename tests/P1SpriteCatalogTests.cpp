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

void testP1FramesAreFixedCellAnimations()
{
    const P1Sprite& mametchi = P1SpriteCatalog::spriteForCharacter("mametchi");
    expect(mametchi.idleFrames.size() == P1Sprite::IdleFrameCount,
        "every P1 home sprite must provide all idle animation phases");

    for (const P1SpriteFrame& frame : mametchi.idleFrames) {
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
        "egg", "babytchi", "marutchi", "tamatchi", "kuchitamatchi", "mametchi",
        "ginjirotchi", "maskutchi", "kuchipatchi", "nyorotchi", "tarakotchi", "bill",
    }};

    for (const std::string_view id : characterIds) {
        const P1Sprite& sprite = P1SpriteCatalog::spriteForCharacter(id);
        for (const P1SpriteFrame& frame : sprite.idleFrames) {
            expect(frame.rowCount > 0U && frame.rowCount <= P1SpriteFrame::MaximumRows,
                "each P1 frame must declare a usable row count");
            expect(frame.originX >= 0 && frame.originY >= 0,
                "each P1 frame must start inside the LCD");
            for (const std::string_view row : frame.visibleRows()) {
                expect(row.size() == 16U, "all P1 animation rows must fit their cell");
                expect(frame.originX + static_cast<int>(row.size()) <= 32,
                    "all P1 animation rows must stay inside the LCD width");
            }
            expect(frame.originY + static_cast<int>(frame.rowCount) <= 16,
                "all P1 animation rows must stay inside the LCD height");
        }
    }
}

void testEggKeepsItsObservedP1Silhouette()
{
    const P1Sprite& egg = P1SpriteCatalog::spriteForCharacter("egg");

    // This is a hand transcription of a visual P1 LCD observation, rather
    // than data imported from a ROM or another emulator implementation.
    expect(egg.idleFrame(0).rows[0] == "......##........",
        "the P1 egg must retain its observed two-pixel crown");
    expect(egg.idleFrame(0).rows[4] == ".##.##.#####....",
        "the P1 egg must retain its asymmetric middle crack");
    expect(egg.idleFrame(0).rows[9] == "..##########....",
        "the P1 egg must retain its wide lower shell");
}

} // namespace

int main()
{
    testP1FramesAreFixedCellAnimations();
    testEveryKnownP1CharacterHasUsableFrames();
    testEggKeepsItsObservedP1Silhouette();

    if (failures == 0) {
        std::cout << "P1SpriteCatalogTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
