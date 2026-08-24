#include "CnaTamagotchi/Domain/P1SpriteCatalog.hpp"

namespace CnaTamagotchi::Domain {
namespace {

constexpr P1SpriteFrame frame(const std::string_view row0, const std::string_view row1,
                              const std::string_view row2, const std::string_view row3,
                              const std::string_view row4, const std::string_view row5,
                              const std::string_view row6, const std::string_view row7,
                              const std::string_view row8, const std::string_view row9) noexcept
{
    return {8, 3, 10U, {{row0, row1, row2, row3, row4, row5, row6, row7, row8, row9,
                         "", ""}}};
}

constexpr P1SpriteFrame wideEggFrame(const std::string_view row0, const std::string_view row1,
                                     const std::string_view row2, const std::string_view row3,
                                     const std::string_view row4, const std::string_view row5,
                                     const std::string_view row6, const std::string_view row7,
                                     const std::string_view row8,
                                     const std::string_view row9) noexcept
{
    return {10, 3, 10U,
            {{row0, row1, row2, row3, row4, row5, row6, row7, row8, row9, "", ""}}};
}

constexpr P1SpriteFrame tallEggFrame(const std::string_view row0, const std::string_view row1,
                                     const std::string_view row2, const std::string_view row3,
                                     const std::string_view row4, const std::string_view row5,
                                     const std::string_view row6, const std::string_view row7,
                                     const std::string_view row8, const std::string_view row9,
                                     const std::string_view row10) noexcept
{
    return {11, 2, 11U,
            {{row0, row1, row2, row3, row4, row5, row6, row7, row8, row9, row10, ""}}};
}

constexpr P1Sprite sprite(const P1SpriteFrame first, const P1SpriteFrame second,
                          const P1SpriteFrame third,
                          const float idleFrameSeconds = P1Sprite::DefaultIdleFrameSeconds) noexcept
{
    return {idleFrameSeconds, 3U, {{first, second, third}}};
}

constexpr P1Sprite twoPhaseSprite(const P1SpriteFrame first, const P1SpriteFrame second,
                                  const float idleFrameSeconds) noexcept
{
    return {idleFrameSeconds, 2U, {{first, second, first}}};
}

template <typename... Frames>
constexpr P1Sprite sequence(const float idleFrameSeconds, const Frames... frames) noexcept
{
    static_assert(sizeof...(Frames) > 0U);
    static_assert(sizeof...(Frames) <= P1Sprite::MaximumIdleFrameCount);
    return {idleFrameSeconds, sizeof...(Frames), {{frames...}}};
}

constexpr P1SpriteFrame babytchiFullFrame(const int originX) noexcept
{
    return {originX, 9, 6U,
            {{".####.", "#.##.#", "######", "##..##", "######", ".####.",
              "", "", "", "", "", ""}}};
}

constexpr P1SpriteFrame babytchiSquashFrame(const int originX) noexcept
{

    return {originX, 13, 3U,
            {{"..####..", ".#.##.#.", "########", "", "", "", "", "", "", "", "", ""}}};
}
constexpr P1SpriteFrame marutchiLongFrame() noexcept
{
    return {11, 3, 9U,
            {{"..######..", ".#......#.", "#.##..##.#", "#........#", "#...##...#",
              "#........#", "#........#", ".#......#.", "..######..", "", "", ""}}};
}

constexpr P1SpriteFrame marutchiShortFrame() noexcept
{
    return {11, 3, 8U,
            {{"..######..", ".#......#.", "#.##..##.#", "#........#", "#...##...#",
              "#........#", ".#......#.", "..######..", "", "", "", ""}}};
}

constexpr P1SpriteFrame wasteFrame(const std::string_view row0, const std::string_view row1,
                                   const std::string_view row2, const std::string_view row3,
                                   const std::string_view row4, const std::string_view row5,
                                   const std::string_view row6,
                                   const std::string_view row7) noexcept
{
    return {24, 8, 8U,
            {{row0, row1, row2, row3, row4, row5, row6, row7, "", "", "", ""}}};
}
constexpr P1SpriteFrame babytchiSickFrame(const std::string_view row0,
                                          const std::string_view row1,
                                          const std::string_view row2) noexcept
{
    return {12, 13, 3U,
            {{row0, row1, row2, "", "", "", "", "", "", "", "", ""}}};
}


constexpr P1SpriteFrame smallSleepFrame() noexcept
{
    return {24, 0, 6U,
            {{"....###", "......#", ".....#.", "....#..", "..#.###", "#......",
              "", "", "", "", "", ""}}};
}

constexpr P1SpriteFrame largeSleepFrame() noexcept
{
    return {25, 2, 6U,
            {{"####", "...#", "..#.", ".#..", "#...", "####",
              "", "", "", "", "", ""}}};
}

// Every drawing and origin is hand-transcribed P1 LCD data. Repeated character
// poses at new origins are retained when the reference really moves them; the
// renderer never invents a translation that is absent from the sequence.
constexpr P1Sprite Egg = twoPhaseSprite(
    // Manually read from the stable cells of a fresh 32x16 reference trace.
    // The external programme redraws the LCD over several host frames; those
    // partial writes were excluded from both silhouettes.
    wideEggFrame(".....##.....", "...######...", "..#.####.#..", ".#..####..#.",
                 "##.#########", "#####..#####", "#..##..#####", ".#..####..#.",
                 "..######.#..", ".##########."),
    tallEggFrame("...##.#...", "..#.####..", ".#..##.##.", ".#.###..#.",
                 "##########", "####..####", "####..#..#", "#..####.##",
                 ".#..#####.", "..###..#..", ".########."),
    0.70F);

// One complete 36-phase cycle from a fresh post-hatch trace. The cycle repeated
// in a 30-second capture; one-host-frame incremental LCD writes are omitted.
constexpr P1Sprite Babytchi = sequence(
    0.46F,
    babytchiFullFrame(7), babytchiFullFrame(11),
    babytchiSquashFrame(8), babytchiSquashFrame(11),
    babytchiFullFrame(15), babytchiFullFrame(18),
    babytchiSquashFrame(14), babytchiSquashFrame(12),
    babytchiFullFrame(10), babytchiFullFrame(7),
    babytchiSquashFrame(10), babytchiSquashFrame(8),
    babytchiFullFrame(12), babytchiFullFrame(15),
    babytchiSquashFrame(17), babytchiSquashFrame(14),
    babytchiFullFrame(13), babytchiFullFrame(10),
    babytchiSquashFrame(6), babytchiSquashFrame(10),
    babytchiFullFrame(9), babytchiFullFrame(12),
    babytchiSquashFrame(14), babytchiSquashFrame(17),
    babytchiFullFrame(15), babytchiFullFrame(13),
    babytchiSquashFrame(9), babytchiSquashFrame(6),
    babytchiFullFrame(11), babytchiFullFrame(9),
    babytchiSquashFrame(11), babytchiSquashFrame(14),
    babytchiFullFrame(18), babytchiFullFrame(15),
    babytchiSquashFrame(12), babytchiSquashFrame(9));
// A complete ten-second sick-state trace repeats these two bottom poses every
// 28 host frames at 30 fps. The skull and waste remain independent overlays.
constexpr P1Sprite SickBabytchi = twoPhaseSprite(
    babytchiSickFrame(".######.", "#..##..#", "########"),
    babytchiSickFrame("..####..", ".#.##.#.", "########"),
    0.93F);

constexpr P1SpriteFrame SicknessIndicator{
    25, 1, 7U,
    {{".#####.", "#######", "#..#..#", "#######", "###.###", ".#####.", ".#.#.#.",
      "", "", "", "", ""}}};

// The two stable silhouettes are separated spatially from two waste piles in
// the source trace. A clean-state horizontal path, if any, remains unverified.
constexpr P1Sprite Marutchi =
    twoPhaseSprite(marutchiLongFrame(), marutchiShortFrame(), 0.92F);

constexpr P1Sprite Waste = twoPhaseSprite(
    wasteFrame(".......#", ".#....#.", "#......#", ".#.#....",
               "...##...", "..##.#..", ".####.#.", ".######."),
    wasteFrame("#.......", ".#....#.", "#......#", "...#..#.",
               "...##...", "..##.#..", ".#.####.", ".######."),
    1.0F);

// The two Z arrangements remain spatially independent from the sleeping
// character and waste. Stable transitions begin every 24-25 frames at 30 fps.
constexpr P1Sprite SleepIndicator =
    twoPhaseSprite(smallSleepFrame(), largeSleepFrame(), 0.82F);

constexpr P1Sprite Tamatchi = sprite(
    frame("....##....##....", "...####..####...", "...##.####.##...", "..##........##..",
          "..##..####..##..", "...##.####.##...", "....##....##....", ".....##..##.....",
          "....##....##....", "................"),
    frame("....##....##....", "...####..####...", "...##.####.##...", "..##........##..",
          "..##..####..##..", "...##.####.##...", "....##....##....", "....##....##....",
          ".....##..##.....", "................"),
    frame("....##....##....", "...####..####...", "...##.####.##...", "..##........##..",
          "..##..####..##..", "...##.####.##...", "....##....##....", "...##..##..##...",
          "................", "................"));

constexpr P1Sprite Kuchitamatchi = sprite(
    frame("...##......##...", "..####....####..", ".##..######..##.", ".##..##..##..##.",
          ".##..........##.", "..##.######.##..", "...##..##..##...", "....##....##....",
          "...##......##...", "................"),
    frame("...##......##...", "..####....####..", ".##..######..##.", ".##..##..##..##.",
          ".##..........##.", "..##.######.##..", "...##..##..##...", "...##......##...",
          "....##....##....", "................"),
    frame("...##......##...", "..####....####..", ".##..######..##.", ".##..##..##..##.",
          ".##..........##.", "..##.######.##..", "...##..##..##...", "..##..##..##..##",
          "................", "................"));

// These three Mametchi phases were transcribed from the independent P1 visual
// reference capture as 32×16 LCD coordinates, then placed in this centred
// 16×10 character cell.  They are intentionally kept as data, not generated
// from a position offset.
constexpr P1Sprite Mametchi = sprite(
    frame("................", "................", "................", ".......##.......",
          ".....######.....", "....#.####.#....", "...#..####..#...", "..##.#########..",
          "..#####..#####..", "..#..##..#####.."),
    frame("................", "................", "......####......", ".....#.####.....",
          "....#..##.##....", "....#.###..#....", "...##########...", "...####..####...",
          "...####..#..#...", "...#..####.##..."),
    frame("................", "................", "......####......", ".....#.####.....",
          "....#..##.#.....", "....#.###..#....", "...########.#...", "...####..#####..",
          "...####..#####..", "...#..##.#####.."));

constexpr P1Sprite Ginjirotchi = sprite(
    frame("......####......", "....########....", "...##..##..##...", "..##...##...##..",
          ".##..........##.", ".##.##########..", "..##..##..##....", "...##......##...",
          "....##....##....", "................"),
    frame("......####......", "....########....", "...##..##..##...", "..##...##...##..",
          ".##..........##.", ".##.##########..", "..##..##..##....", "....##....##....",
          "...##......##...", "................"),
    frame("......####......", "....########....", "...##..##..##...", "..##...##...##..",
          ".##..........##.", ".##.##########..", "..##..##..##....", "..##..##..##....",
          "................", "................"));

constexpr P1Sprite Maskutchi = sprite(
    frame("....########....", "..##........##..", ".##..##########.", "##..##########..",
          "##..##..##..##..", "##............##", ".##..##########.", "..##........##..",
          "...##......##...", "................"),
    frame("....########....", "..##........##..", ".##..##########.", "##..##########..",
          "##..##..##..##..", "##............##", ".##..##########.", "...##......##...",
          "..##........##..", "................"),
    frame("....########....", "..##........##..", ".##..##########.", "##..##########..",
          "##..##..##..##..", "##............##", ".##..##########.", "##..##......##..",
          "................", "................"));

constexpr P1Sprite Kuchipatchi = sprite(
    frame("....########....", "..##........##..", ".##..##..##..##.", "##............##",
          "##...########.##", ".##..##....##.##", "..###........###", "...##......##...",
          "....##....##....", "................"),
    frame("....########....", "..##........##..", ".##..##..##..##.", "##............##",
          "##...########.##", ".##..##....##.##", "..###........###", "....##....##....",
          "...##......##...", "................"),
    frame("....########....", "..##........##..", ".##..##..##..##.", "##............##",
          "##...########.##", ".##..##....##.##", "..###........###", "..##..##..##..##",
          "................", "................"));

constexpr P1Sprite Nyorotchi = sprite(
    frame("......####......", "....##....##....", "...##..##..##...", "..##........##..",
          ".##..##..##..##.", "##............##", ".##..##....##.##", "..##..........##",
          "...##........##.", "................"),
    frame("......####......", "....##....##....", "...##..##..##...", "..##........##..",
          ".##..##..##..##.", "##............##", ".##..##....##.##", "...##........##.",
          "..##..........##", "................"),
    frame("......####......", "....##....##....", "...##..##..##...", "..##........##..",
          ".##..##..##..##.", "##............##", ".##..##....##.##", "##..##........##",
          "................", "................"));

constexpr P1Sprite Tarakotchi = sprite(
    frame(".....######.....", "...##......##...", "..##..##..##.##.", ".##..........##.",
          "##....####....##", "##...##..##...##", ".##..########.##", "..##..........##",
          "...##........##.", "................"),
    frame(".....######.....", "...##......##...", "..##..##..##.##.", ".##..........##.",
          "##....####....##", "##...##..##...##", ".##..########.##", "...##........##.",
          "..##..........##", "................"),
    frame(".....######.....", "...##......##...", "..##..##..##.##.", ".##..........##.",
          "##....####....##", "##...##..##...##", ".##..########.##", "##..##........##",
          "................", "................"));

constexpr P1Sprite Bill = sprite(
    frame("...##......##...", "....##....##....", ".....########...", "...##..##..##.##",
          ".##..........##.", "##...########.##", ".##..##....##.##", "..##..........##",
          "...##........##.", "................"),
    frame("...##......##...", "....##....##....", ".....########...", "...##..##..##.##",
          ".##..........##.", "##...########.##", ".##..##....##.##", "...##........##.",
          "..##..........##", "................"),
    frame("...##......##...", "....##....##....", ".....########...", "...##..##..##.##",
          ".##..........##.", "##...########.##", ".##..##....##.##", "##..##........##",
          "................", "................"));

} // namespace

const P1Sprite& P1SpriteCatalog::spriteForCharacter(const std::string_view characterId) noexcept
{
    if (characterId == "egg") return Egg;
    if (characterId == "babytchi") return Babytchi;
    if (characterId == "marutchi") return Marutchi;
    if (characterId == "tamatchi") return Tamatchi;
    if (characterId == "kuchitamatchi") return Kuchitamatchi;
    if (characterId == "mametchi") return Mametchi;
    if (characterId == "ginjirotchi") return Ginjirotchi;
    if (characterId == "maskutchi") return Maskutchi;
    if (characterId == "kuchipatchi") return Kuchipatchi;
    if (characterId == "nyorotchi") return Nyorotchi;
    if (characterId == "tarakotchi") return Tarakotchi;
    if (characterId == "bill") return Bill;
    return Egg;
}

const P1Sprite& P1SpriteCatalog::sickSpriteForCharacter(
    const std::string_view characterId) noexcept
{
    if (characterId == "babytchi") {
        return SickBabytchi;
    }
    return spriteForCharacter(characterId);
}

const P1SpriteFrame& P1SpriteCatalog::sicknessIndicator() noexcept
{
    return SicknessIndicator;
}

const P1Sprite& P1SpriteCatalog::sleepIndicator() noexcept
{
    return SleepIndicator;
}

const P1Sprite& P1SpriteCatalog::waste() noexcept
{
    return Waste;
}

} // namespace CnaTamagotchi::Domain
