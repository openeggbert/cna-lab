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
        expect(frame.rows.size() == 10U, "P1 character cells must be ten pixels high");
        for (const std::string_view row : frame.rows) {
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
            for (const std::string_view row : frame.rows) {
                expect(row.size() == 16U, "all P1 animation rows must fit their cell");
            }
        }
    }
}

} // namespace

int main()
{
    testP1FramesAreFixedCellAnimations();
    testEveryKnownP1CharacterHasUsableFrames();

    if (failures == 0) {
        std::cout << "P1SpriteCatalogTests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
