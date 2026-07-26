#include "CnaTamagotchi/Domain/P1SpriteCatalog.hpp"

namespace CnaTamagotchi::Domain {
namespace {

constexpr P1Sprite Egg{{
    "      ####      ", "    ##    ##    ", "   ##  ##  ##   ", "  ##        ##  ",
    "  ##  ##    ##  ", "   ##    ####   ", "    ########    ", "                ",
}};

constexpr P1Sprite Babytchi{{
    "     ##  ##     ", "    ########    ", "   ##  ##  ##   ", "  ##        ##  ",
    "  ##  ## ## ##  ", "  ##        ##  ", "   ##  ######   ", "     ##    ##   ",
}};

constexpr P1Sprite Marutchi{{
    "      ####      ", "    ##    ##    ", "   ##  ##  ##   ", "  ##        ##  ",
    " ##    ####   ##", " ##     ##    ##", "  ##  ####  ##  ", "    ##      ##  ",
}};

constexpr P1Sprite Tamatchi{{
    "    ##    ##    ", "   ####  ####   ", "  ##  ####  ##  ", " ##  ##  ##  ## ",
    " ##          ## ", "  ##  ####  ##  ", "   ##  ##  ##   ", "    ##    ##    ",
}};

constexpr P1Sprite Kuchitamatchi{{
    "  ##        ##  ", " ####      #### ", "##  ########  ##", "##   ##  ##   ##",
    "##            ##", " ##  ######## ##", "  ##  ##  ##  ##", "    ##      ##  ",
}};

constexpr P1Sprite Mametchi{{
    "  ##        ##  ", " ####      #### ", "##  ########  ##", "##  ## ## ##  ##",
    "##     ##     ##", " ##  ######## ##", "  ##  ##  ##  ##", "    ##      ##  ",
}};

constexpr P1Sprite Ginjirotchi{{
    "      ####      ", "    ########    ", "   ##  ##  ##   ", "  ##   ##   ##  ",
    " ##          ## ", " ##  ########## ", "  ##  ##  ##    ", "    ##      ##  ",
}};

constexpr P1Sprite Maskutchi{{
    "    ########    ", "  ##        ##  ", " ##  ########## ", "##  ##########  ",
    "##  ##  ##  ##  ", "##            ##", " ##  ######## ##", "  ##          ##",
}};

constexpr P1Sprite Kuchipatchi{{
    "    ########    ", "  ##        ##  ", " ##  ##  ##  ## ", "##            ##",
    "##   ######## ##", " ##  ##    ## ##", "  ###        ###", "    ##      ##  ",
}};

constexpr P1Sprite Nyorotchi{{
    "      ####      ", "    ##    ##    ", "   ##  ##  ##   ", "  ##        ##  ",
    " ##  ##  ##  ## ", "##            ##", " ##  ##    ## ##", "  ##          ##",
}};

constexpr P1Sprite Tarakotchi{{
    "     ######     ", "   ##      ##   ", "  ##  ##  ## ## ", " ##          ## ",
    "##    ####    ##", "##   ##  ##   ##", " ##  ######## ##", "  ##          ##",
}};

constexpr P1Sprite Bill{{
    "  ##        ##  ", "   ##      ##   ", "    ########    ", "  ##  ##  ## ## ",
    " ##          ## ", "##   ######## ##", " ##  ##    ## ##", "  ##          ##",
}};

} // namespace

const P1Sprite& P1SpriteCatalog::spriteForCharacter(const std::string_view characterId) noexcept
{
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

} // namespace CnaTamagotchi::Domain
