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
        bool hasLitPixelInEachFrame = true;
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
            bool hasLitPixel = false;
            for (const std::string_view row : frame.visibleRows()) {
                hasLitPixel = hasLitPixel || row.find('#') != std::string_view::npos;
            }
            hasLitPixelInEachFrame = hasLitPixelInEachFrame && hasLitPixel;
        }
        expect(hasLitPixelInEachFrame,
            "each declared P1 idle phase must contain a visible one-bit drawing");
        expect(sprite.idleFrame(0).rows != sprite.idleFrame(1).rows
                || sprite.idleFrame(0).originX != sprite.idleFrame(1).originX
                || sprite.idleFrame(0).originY != sprite.idleFrame(1).originY
                || sprite.idleFrame(0).rowCount != sprite.idleFrame(1).rowCount,
            "the first two P1 idle phases must remain independently drawn");
        expect(sprite.idleFrame(1).rows != sprite.idleFrame(2).rows
                || sprite.idleFrame(1).originX != sprite.idleFrame(2).originX
                || sprite.idleFrame(1).originY != sprite.idleFrame(2).originY
                || sprite.idleFrame(1).rowCount != sprite.idleFrame(2).rowCount,
            "the latter P1 idle phases must remain independently drawn");
    }
}

void testEggKeepsItsObservedP1Silhouette()
{
    const P1Sprite& egg = P1SpriteCatalog::spriteForCharacter("egg");

    // This is a hand transcription of a visual P1 LCD observation, rather
    // than data imported from a ROM or another emulator implementation.
    expect(egg.idleFrame(0).originX == 8 && egg.idleFrame(0).originY == 2
            && egg.idleFrame(0).rowCount == 11U,
        "the observed P1 egg phase must retain its larger true LCD bounds");
    expect(egg.idleFrameSeconds == 1.0F,
        "the observed P1 egg phase cadence must not use the faster provisional rate");
    expect(egg.idleFrame(0).rows[0] == ".....####.......",
        "the P1 egg must retain its observed four-pixel crown");
    expect(egg.idleFrame(0).rows[5] == "..####..####....",
        "the P1 egg must retain its asymmetric middle crack");
    expect(egg.idleFrame(0).rows[10] == "....#######.....",
        "the P1 egg must retain its observed lower shell");
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
