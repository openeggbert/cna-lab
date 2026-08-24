#include "BlackPineWorld.hpp"

#include "explore2d/Renderer.hpp"

#include <optional>
#include <string>
#include <utility>

namespace black_pine {
namespace e2d = explore2d;
namespace {

using P = e2d::PaletteColor;
constexpr P sky = P::brightBlue;
constexpr P night = P::blue;
constexpr P pine = P::brightGreen;
constexpr P pineDark = P::green;
constexpr P ground = P::brown;
constexpr P wood = P::brown;
constexpr P metal = P::lightGray;
constexpr P rust = P::red;
constexpr P pale = P::white;
constexpr P amber = P::brightYellow;
constexpr P red = P::brightRed;
constexpr P green = P::brightGreen;
constexpr P blue = P::brightCyan;

e2d::Visual box(float x, float y, float w, float h, P color, bool filled = true) {
    return e2d::RectVisual{{x, y, w, h}, color, filled};
}

e2d::Visual line(float x1, float y1, float x2, float y2, P color) {
    return e2d::LineVisual{{x1, y1}, {x2, y2}, color};
}

e2d::Visual circle(float x, float y, float radius, P color, bool filled = true) {
    return e2d::CircleVisual{{x, y}, radius, color, filled};
}

e2d::Visual ellipse(float x, float y, float rx, float ry, P color, bool filled = true) {
    return e2d::EllipseVisual{{x, y}, {rx, ry}, color, filled};
}

e2d::LocalizedText tr(std::string english, std::string czech) {
    e2d::LocalizedText result{std::move(english)};
    result.addTranslation("cs", std::move(czech));
    return result;
}

e2d::Visual label(float x, float y, e2d::LocalizedText text, P color = pale, int scale = 1) {
    return e2d::TextVisual{{x, y}, std::move(text), color, scale};
}

void addPine(e2d::RoomDefinition& room, const float x, const float base, const float height) {
    room.decorations.push_back(box(x - 3.0F, base - height * 0.48F, 6.0F, height * 0.48F, wood));
    room.decorations.push_back(ellipse(x, base - height * 0.72F, height * 0.19F, height * 0.28F, pineDark));
    room.decorations.push_back(ellipse(x, base - height * 0.87F, height * 0.14F, height * 0.22F, pine));
}

void addGround(e2d::RoomDefinition& room, float x = 0.0F, float width = 492.0F) {
    room.solids.push_back({x, 260.0F, width, 28.0F});
    room.decorations.push_back(box(x, 260.0F, width, 28.0F, ground));
    room.decorations.push_back(line(x, 260.0F, x + width, 260.0F, amber));
}

e2d::ExitDefinition exit(e2d::Direction direction, std::string destination, e2d::Vec2 spawn) {
    return {direction, std::move(destination), spawn, {}, {}};
}

e2d::Message inspect(e2d::LocalizedText text) { return {std::move(text), e2d::MessageStyle::inspect}; }
e2d::Message speech(e2d::LocalizedText text) {
    return {std::move(text), e2d::MessageStyle::speech, e2d::MessageSpeaker::target};
}
e2d::Message playerSpeech(e2d::LocalizedText text) {
    return {std::move(text), e2d::MessageStyle::speech, e2d::MessageSpeaker::player};
}
e2d::Message warning(e2d::LocalizedText text) { return {std::move(text), e2d::MessageStyle::warning}; }

void configureCzechInterface(e2d::InterfaceTextDefinition& ui) {
    ui.inventoryEmpty = tr("(NOTHING)", "(NIC)");
    ui.verbUse = tr("USE", "POUŽIJ");
    ui.verbExamine = tr("EXAMINE", "PROZKOUMEJ");
    ui.verbTake = tr("TAKE", "SEBER");
    ui.useWhat = tr("USE WHAT?", "CO POUŽÍT?");
    ui.confirmCancel = tr("ENTER / ESC", "ENTER / ESC");
    ui.travelMap = tr("TRAVEL MAP", "CESTOVNÍ MAPA");
    ui.travelHelp = tr("ARROWS + ENTER   ESC BACK", "ŠIPKY + ENTER   ESC ZPĚT");
    ui.messageAdvance = tr("ENTER", "ENTER");
    ui.missionComplete = tr("MISSION COMPLETE", "MISE SPLNĚNA");
    ui.missionFailed = tr("MISSION FAILED", "MISE SELHALA");
    ui.restartPrompt = tr("ENTER TO RESTART", "ENTER PRO NOVÝ START");
    ui.paused = tr("GAME PAUSED", "HRA POZASTAVENA");
    ui.resume = tr("RESUME GAME", "POKRAČOVAT");
    ui.settings = tr("SETTINGS", "NASTAVENÍ");
    ui.returnToTitle = tr("RETURN TO TITLE", "ZPĚT NA TITULNÍ MENU");
    ui.language = tr("LANGUAGE", "JAZYK");
    ui.back = tr("BACK", "ZPĚT");
    ui.settingsHelp = tr("LEFT / RIGHT CHANGE   ESC BACK", "VLEVO / VPRAVO ZMĚNIT   ESC ZPĚT");
    ui.help = tr("HELP", "NÁPOVĚDA");
    ui.nextStep = tr("NEXT STEP", "DALŠÍ KROK");
    ui.closeHelp = tr("F1 / ENTER / ESC BACK", "F1 / ENTER / ESC ZPĚT");
    ui.noHint = tr("Explore nearby objects and characters for another clue.",
        "Prozkoumej okolní předměty a postavy a hledej další stopu.");
    ui.nothingToUseOn = tr("There is nothing close enough to use an item on.",
        "Nablízku není nic, na co by šel předmět použít.");
    ui.nothingUsable = tr("You are not carrying anything usable.",
        "Neneseš nic použitelného.");
    ui.nothingToExamine = tr("There is nothing here that catches your eye.",
        "Není tu nic, co by stálo za prozkoumání.");
    ui.nothingToTake = tr("There is nothing within reach to take.",
        "Na dosah není nic k sebrání.");
    ui.cannotTake = tr("You cannot take that.", "To nemůžeš sebrat.");
    ui.doesNotWork = tr("That does not seem to work here.", "Tady to zřejmě nefunguje.");
    ui.noticeNothing = tr("You notice nothing unusual.", "Nic neobvyklého.");
    ui.noTravelDestinations = tr("No travel destinations have been discovered yet.",
        "Zatím nebyl objeven žádný cíl cesty.");
    ui.gameSaved = tr("Game saved.", "Hra byla uložena.");
    ui.saveFailed = tr("Save failed.", "Uložení selhalo.");
    ui.loadFailed = tr("Load failed.", "Načtení selhalo.");
    ui.loadWorldMismatch = tr("Load failed: save does not match this world.",
        "Načtení selhalo: uložená hra nepatří do tohoto světa.");
    ui.gameLoaded = tr("Game loaded.", "Hra byla načtena.");
    ui.fellBeyondEdge = tr("You fell beyond the edge of the screen.",
        "Pád za okraj obrazovky byl smrtelný.");
}

} // namespace

e2d::RendererTheme buildTheme() {
    e2d::RendererTheme theme;
    theme.frame = amber;
    theme.panel = P::black;
    theme.panelPattern = P::red;
    theme.text = pale;
    theme.dimText = metal;
    theme.accent = blue;
    theme.selected = P::brightMagenta;
    theme.danger = red;
    theme.playerSkin = amber;
    theme.playerShirt = P::brightRed;
    theme.playerPants = P::brightCyan;
    return theme;
}

e2d::WorldDefinition buildWorld() {
    e2d::WorldDefinition world;
    world.localization.defaultLanguage = "en";
    world.localization.languages = {
        {"en", tr("English", "Angličtina")},
        {"cs", tr("Czech", "Čeština")},
    };
    world.title = tr("Black Pine", "Black Pine");
    world.startRoom = "trailhead";
    configureCzechInterface(world.presentation.interfaceText);
    world.presentation.inventoryHeading = tr("YOU CARRY", "NESEŠ");
    world.presentation.creditLine = tr("A BLACK PINE STORY", "PŘÍBĚH Z BLACK PINE");
    world.presentation.title.subtitle = tr("A MOUNTAIN RELAY MYSTERY", "ZÁHADA HORSKÉHO PŘEVADĚČE");
    world.presentation.title.byline = tr("AN EXPLORE2D ADVENTURE", "ADVENTURA V EXPLORE2D");
    world.presentation.title.startLabel = tr("NEW GAME", "NOVÁ HRA");
    world.presentation.title.loadLabel = tr("LOAD GAME", "NAČÍST HRU");
    world.presentation.title.settingsLabel = tr("SETTINGS", "NASTAVENÍ");
    world.presentation.title.quitLabel = tr("QUIT", "KONEC");
    world.presentation.title.titleColors = {P::brightGreen, P::brightCyan, P::brightYellow, P::brightMagenta};
    world.presentation.title.artwork = {
        box(18, 80, 604, 151, sky),
        circle(548, 108, 19, amber),
        line(18, 214, 118, 126, metal), line(118, 126, 214, 214, metal),
        line(160, 214, 292, 104, P::white), line(292, 104, 424, 214, P::white),
        line(360, 214, 472, 136, P::darkGray), line(472, 136, 622, 214, P::darkGray),
        box(18, 214, 604, 17, pineDark),
        line(462, 104, 432, 214, P::lightGray), line(462, 104, 494, 214, P::lightGray),
        line(442, 170, 483, 170, P::lightGray), line(449, 146, 476, 146, P::lightGray),
        line(456, 122, 469, 122, P::lightGray),
        line(462, 104, 462, 82, P::white), line(449, 88, 475, 88, P::white),
        circle(462, 82, 4, red, true),
        ellipse(74, 190, 26, 35, pineDark), ellipse(112, 198, 19, 29, pine),
        ellipse(550, 194, 30, 40, pineDark), ellipse(592, 201, 18, 28, pine),
        label(32, 218, tr("STORM OVER BLACK PINE RIDGE", "BOUŘE NAD HŘEBENEM BLACK PINE"), P::brightYellow),
    };

    // Deliberately QBasic-like: monophonic square-wave phrases expressed as
    // SOUND frequency/timer-tick pairs, with no sampled music or modern DSP.
    world.addSoundEffect({"title", {{392, 2}, {523, 2}, {659, 2}, {784, 4}, {0, 1}, {659, 2}}, 0.17F});
    world.addSoundEffect({"menu", {{880, 1}}, 0.12F});
    world.addSoundEffect({"confirm", {{523, 1}, {784, 2}}, 0.15F});
    world.addSoundEffect({"talk", {{330, 1}}, 0.10F});
    world.addSoundEffect({"pickup", {{440, 1}, {660, 1}, {880, 2}}, 0.15F});
    world.addSoundEffect({"jump", {{220, 1}, {330, 1}}, 0.11F});
    world.addSoundEffect({"warning", {{147, 2}, {110, 3}}, 0.16F});
    world.addSoundEffect({"death", {{330, 2}, {262, 2}, {196, 2}, {131, 5}}, 0.18F});
    world.addSoundEffect({"victory", {{392, 2}, {523, 2}, {659, 2}, {784, 2}, {1047, 6}}, 0.18F});
    world.addSoundEffect({"save", {{659, 1}, {880, 2}}, 0.12F});
    world.addSoundEffect({"load", {{880, 1}, {659, 2}}, 0.12F});
    world.addSoundEffect({"unlock", {{196, 1}, {247, 1}, {330, 2}}, 0.15F});
    world.addSoundEffect({"repair", {{880, 1}, {0, 1}, {880, 1}}, 0.13F});
    world.addSoundEffect({"power", {{110, 2}, {165, 2}, {220, 2}, {330, 3}}, 0.17F});
    world.addSoundEffect({"climb", {{262, 1}, {294, 1}, {330, 1}, {349, 1}}, 0.11F});
    world.presentation.sounds = {
        "title", "menu", "confirm", "talk", "pickup", "jump",
        "warning", "death", "victory", "save", "load"};

    world.addItem({"patch_cable", tr("PATCH CABLE", "PROPOJOVACÍ KABEL"),
        tr("A weatherproof copper patch cable, short but intact.",
            "Krátký, ale neporušený měděný propojovací kabel odolný proti počasí."), true});
    world.addItem({"field_note", tr("FIELD NOTE", "SERVISNÍ POZNÁMKA"),
        tr("A faded maintenance note: MAIN FUSE FIRST. PATCH THE BLUE TERMINALS. THEN THROW THE LEVER.",
            "Vybledlá servisní poznámka: NEJPRVE HLAVNÍ POJISTKA. PROPOJIT MODRÉ SVORKY. POTOM PÁKA."), false});
    world.addItem({"brass_key", tr("BRASS KEY", "MOSAZNÝ KLÍČ"),
        tr("A heavy brass key stamped YARD GATE.",
            "Těžký mosazný klíč s vyraženým nápisem BRÁNA AREÁLU."), true});
    world.addItem({"ceramic_fuse", tr("CERAMIC FUSE", "KERAMICKÁ POJISTKA"),
        tr("A 30 amp ceramic fuse. Miraculously uncracked.",
            "Třicetiampérová keramická pojistka. Jako zázrakem není prasklá."), true});
    world.addItem({"wrench", tr("WRENCH", "MONTÁŽNÍ KLÍČ"),
        tr("A long handled 17 mm wrench, scarred by years of field repairs.",
            "Sedmnáctimilimetrový klíč s dlouhou rukojetí a stopami mnoha oprav v terénu."), true});
    world.addItem({"old_badge", tr("OLD BADGE", "STARÝ ODZNAK"),
        tr("An enamel badge from the BLACK PINE radio relay. Purely sentimental.",
            "Smaltovaný odznak radiového převaděče BLACK PINE. Má pouze citovou hodnotu."), false});

    // Trailhead ----------------------------------------------------------------
    e2d::RoomDefinition trail;
    trail.id = "trailhead";
    trail.label = tr("BLACK PINE TRAILHEAD", "VÝCHOZIŠTĚ BLACK PINE");
    trail.background = sky;
    trail.defaultSpawn = {42, 232};
    trail.travelAnchor = true;
    trail.travelLabel = tr("TRAILHEAD", "VÝCHOZIŠTĚ");
    addGround(trail);
    trail.decorations.insert(trail.decorations.end(), {
        circle(400, 46, 18, amber),
        line(0, 173, 74, 89, metal), line(74, 89, 151, 173, metal),
        line(113, 173, 221, 69, pale), line(221, 69, 330, 173, pale),
        line(286, 173, 376, 104, P::darkGray), line(376, 104, 492, 173, P::darkGray),
        box(0, 172, 492, 88, pineDark),
        ellipse(155, 187, 68, 25, pine), ellipse(330, 181, 79, 31, pine),
        box(198, 191, 116, 61, wood), box(204, 197, 104, 49, P::black),
        label(222, 207, tr("BLACK PINE", "BLACK PINE"), amber), label(225, 221, tr("TRAIL MAP", "MAPA STEZEK"), pale),
        line(213, 238, 286, 238, green), circle(243, 238, 3, red), circle(276, 238, 3, blue),
        box(356, 220, 66, 32, metal), box(361, 215, 56, 7, pale),
        line(362, 234, 416, 234, P::darkGray), circle(389, 224, 2, P::black)
    });
    addPine(trail, 34, 260, 126);
    addPine(trail, 92, 260, 154);
    addPine(trail, 451, 260, 142);
    trail.hotspots.push_back({"trail_sign", tr("TRAIL MAP", "MAPA STEZEK"), {188, 178, 136, 82}, e2d::HotspotKind::scenery, {}, {}});
    trail.hotspots.push_back({"trail_toolbox", tr("TOOLBOX", "SKŘÍŇKA S NÁŘADÍM"), {338, 198, 106, 62}, e2d::HotspotKind::scenery, {}, {}});
    trail.hotspots.push_back({"patch_cable_pickup", tr("PATCH CABLE", "PROPOJOVACÍ KABEL"), {345, 205, 100, 55}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("cable_taken")}, {box(382, 228, 24, 8, blue)}});
    trail.hotspots.push_back({"field_note_pickup", tr("FIELD NOTE", "SERVISNÍ POZNÁMKA"), {192, 194, 130, 66}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("note_taken")}, {box(262, 239, 19, 11, pale)}});
    trail.exits.push_back(exit(e2d::Direction::right, "cabin", {8, 232}));
    world.addRoom(std::move(trail));

    // Cabin --------------------------------------------------------------------
    e2d::RoomDefinition cabin;
    cabin.id = "cabin";
    cabin.label = tr("CARETAKER CABIN", "SPRÁVCOVSKÁ CHATA");
    cabin.background = night;
    cabin.defaultSpawn = {28, 232};
    cabin.travelAnchor = true;
    cabin.travelLabel = tr("CABIN", "CHATA");
    addGround(cabin);
    cabin.decorations.insert(cabin.decorations.end(), {
        box(39, 69, 421, 191, wood), box(51, 82, 397, 178, P::red),
        line(51, 105, 448, 105, P::brown), line(51, 141, 448, 141, P::brown),
        line(51, 177, 448, 177, P::brown), line(51, 213, 448, 213, P::brown),
        box(75, 102, 76, 66, P::blue), box(81, 108, 64, 54, P::brightBlue),
        line(113, 108, 113, 162, pale), line(81, 135, 145, 135, pale),
        circle(174, 104, 12, amber), line(174, 116, 174, 139, metal),
        box(344, 148, 99, 68, wood), box(351, 155, 85, 54, P::black),
        label(364, 163, tr("RADIO", "RÁDIO"), amber), circle(367, 190, 8, metal, false),
        line(387, 190, 424, 190, green), line(387, 197, 418, 197, blue),
        box(177, 202, 115, 44, wood), box(184, 208, 101, 9, P::lightGray),
        box(207, 193, 52, 10, pale), line(180, 246, 180, 260, P::darkGray), line(288, 246, 288, 260, P::darkGray),
        box(84, 176, 17, 70, metal), circle(92, 183, 9, pale, false),
        line(92, 195, 128, 221, metal), circle(128, 221, 7, metal, false),
        circle(311, 190, 11, amber), box(302, 201, 19, 44, pale),
        line(302, 211, 289, 230, amber), line(320, 211, 332, 230, amber)
    });
    cabin.hotspots.push_back({"caretaker", tr("MARA", "MARA"), {274, 170, 74, 90}, e2d::HotspotKind::character, {}, {}});
    cabin.hotspots.push_back({"cabin_desk", tr("DESK", "STŮL"), {166, 184, 132, 76}, e2d::HotspotKind::scenery, {}, {}});
    cabin.hotspots.push_back({"brass_key_pickup", tr("BRASS KEY", "MOSAZNÝ KLÍČ"), {185, 188, 118, 72}, e2d::HotspotKind::item,
        {e2d::Condition::flag("key_revealed"), e2d::Condition::notFlag("key_taken")}, {box(238, 216, 18, 5, amber)}});
    cabin.hotspots.push_back({"wrench_pickup", tr("WRENCH", "MONTÁŽNÍ KLÍČ"), {64, 164, 76, 96}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("wrench_taken")}, {box(91, 202, 42, 5, metal)}});
    cabin.hotspots.push_back({"dead_radio", tr("RADIO", "RÁDIO"), {338, 142, 116, 94}, e2d::HotspotKind::mechanism, {}, {}});
    cabin.exits.push_back(exit(e2d::Direction::left, "trailhead", {462, 232}));
    cabin.exits.push_back(exit(e2d::Direction::right, "yard", {8, 232}));
    world.addRoom(std::move(cabin));

    // Yard ---------------------------------------------------------------------
    e2d::RoomDefinition yard;
    yard.id = "yard";
    yard.label = tr("RELAY YARD", "AREÁL PŘEVADĚČE");
    yard.background = sky;
    yard.defaultSpawn = {24, 232};
    yard.travelAnchor = true;
    yard.travelLabel = tr("RELAY YARD", "AREÁL PŘEVADĚČE");
    addGround(yard);
    yard.decorations.insert(yard.decorations.end(), {
        circle(421, 48, 17, amber), line(0, 187, 102, 96, P::darkGray), line(102, 96, 215, 187, P::darkGray),
        line(154, 187, 274, 76, metal), line(274, 76, 390, 187, metal),
        box(55, 116, 8, 144, metal), box(126, 90, 8, 170, metal), box(202, 121, 8, 139, metal),
        line(59, 116, 130, 90, metal), line(130, 90, 206, 121, metal),
        line(59, 116, 206, 121, blue), circle(130, 90, 4, red),
        box(364, 116, 11, 144, rust), box(472, 116, 11, 144, rust),
        line(369, 116, 477, 116, rust), line(369, 145, 477, 145, rust), line(369, 174, 477, 174, rust),
        line(369, 203, 477, 203, rust), line(369, 232, 477, 232, rust),
        line(369, 116, 477, 232, rust), line(477, 116, 369, 232, rust),
        box(414, 169, 20, 28, amber), circle(424, 178, 3, P::black), label(389, 96, tr("GATE", "BRÁNA"), amber)
    });
    addPine(yard, 27, 260, 104);
    addPine(yard, 323, 260, 86);
    yard.hotspots.push_back({"yard_gate", tr("YARD GATE", "BRÁNA AREÁLU"), {340, 94, 160, 166}, e2d::HotspotKind::mechanism, {}, {}});
    yard.hotspots.push_back({"yard_gate_open", tr("OPEN GATE", "OTEVŘENÁ BRÁNA"), {340, 94, 160, 166}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("gate_open")}, {box(383, 134, 80, 5, green), label(397, 151, tr("OPEN", "OTEVŘENO"), green)}});
    yard.hotspots.push_back({"relay_frame", tr("RELAY FRAME", "RÁM PŘEVADĚČE"), {38, 82, 198, 178}, e2d::HotspotKind::scenery, {}, {}});
    yard.exits.push_back(exit(e2d::Direction::left, "cabin", {462, 232}));
    auto yardRight = exit(e2d::Direction::right, "generator", {8, 232});
    yardRight.availableWhen = {e2d::Condition::flag("gate_open")};
    yardRight.blockedMessage = tr("The locked yard gate blocks the way to the generator shed.",
        "Cestu ke generátorovně blokuje zamčená brána areálu.");
    yard.exits.push_back(std::move(yardRight));
    world.addRoom(std::move(yard));

    // Generator ----------------------------------------------------------------
    e2d::RoomDefinition generator;
    generator.id = "generator";
    generator.label = tr("GENERATOR SHED", "GENERÁTOROVNA");
    generator.background = P::darkGray;
    generator.defaultSpawn = {28, 232};
    addGround(generator);
    generator.decorations.insert(generator.decorations.end(), {
        box(0, 28, 492, 232, P::darkGray),
        line(0, 64, 492, 64, P::lightGray), line(0, 112, 492, 112, P::lightGray),
        line(0, 160, 492, 160, P::lightGray), line(0, 208, 492, 208, P::lightGray),
        box(66, 93, 267, 151, P::lightGray), box(73, 100, 253, 137, P::black),
        label(123, 108, tr("BLACK PINE GENERATOR", "GENERÁTOR BLACK PINE"), amber),
        box(90, 127, 102, 80, metal), box(98, 135, 86, 64, P::blue),
        circle(120, 162, 18, P::white, false), line(120, 162, 132, 151, red),
        circle(161, 162, 12, P::white, false), line(161, 162, 161, 151, green),
        label(104, 188, tr("TERMINALS", "SVORKY"), pale),
        box(210, 127, 92, 72, P::darkGray), label(222, 139, tr("MAIN", "HLAVNÍ"), amber),
        box(232, 158, 46, 22, P::black), circle(243, 169, 6, red), circle(266, 169, 6, green),
        box(351, 140, 84, 104, wood), box(359, 149, 68, 17, P::red),
        label(366, 154, tr("SPARES", "NÁHRADNÍ"), pale), line(361, 178, 425, 178, metal),
        line(361, 204, 425, 204, metal), circle(393, 191, 10, pale, false),
        line(113, 207, 113, 238, blue), line(160, 207, 160, 238, blue),
        circle(113, 220, 5, blue), circle(160, 220, 5, blue),
        box(244, 205, 14, 34, rust), line(251, 205, 279, 185, rust), circle(280, 184, 4, amber)
    });
    generator.hotspots.push_back({"fuse_pickup", tr("CERAMIC FUSE", "KERAMICKÁ POJISTKA"), {340, 132, 105, 112}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("fuse_taken")}, {box(382, 181, 18, 6, pale)}});
    generator.hotspots.push_back({"fuse_socket", tr("FUSE SOCKET", "DRŽÁK POJISTKY"), {190, 114, 120, 104}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"cable_terminals", tr("BLUE TERMINALS", "MODRÉ SVORKY"), {88, 178, 92, 80}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"generator_lever", tr("MAIN LEVER", "HLAVNÍ PÁKA"), {228, 184, 70, 74}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"installed_fuse_visual", tr("INSTALLED FUSE", "VLOŽENÁ POJISTKA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("fuse_installed")}, {box(246, 164, 18, 7, pale), line(249, 164, 249, 171, red)}});
    generator.hotspots.push_back({"installed_cable_visual", tr("PATCHED TERMINALS", "PROPOJENÉ SVORKY"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("cable_installed")}, {line(113, 220, 160, 220, blue), line(113, 221, 160, 221, blue)}});
    generator.hotspots.push_back({"power_lamp_visual", tr("POWER LAMP", "KONTROLKA NAPÁJENÍ"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(266, 169, 6, green), circle(280, 184, 4, green)}});
    generator.animations.push_back({"generator_start", false, false, {}, {
        {2, {line(231, 195, 218, 181, amber), line(231, 195, 218, 201, pale)}},
        {2, {line(231, 195, 211, 193, red), circle(231, 195, 8, amber, false)}},
        {3, {line(231, 195, 220, 207, pale), line(231, 195, 245, 204, amber)}},
    }});
    generator.animations.push_back({"generator_lamps", true, true,
        {e2d::Condition::flag("power_on")}, {
            {7, {circle(266, 169, 6, P::brightGreen), circle(280, 184, 4, P::brightGreen)}},
            {7, {circle(266, 169, 6, green), circle(280, 184, 4, green)}},
        }});
    generator.exits.push_back(exit(e2d::Direction::left, "yard", {462, 232}));
    generator.exits.push_back(exit(e2d::Direction::right, "tower_base", {8, 232}));
    world.addRoom(std::move(generator));

    // Tower base ---------------------------------------------------------------
    e2d::RoomDefinition towerBase;
    towerBase.id = "tower_base";
    towerBase.label = tr("BLACK PINE TOWER", "VĚŽ BLACK PINE");
    towerBase.background = sky;
    towerBase.defaultSpawn = {28, 232};
    towerBase.travelAnchor = true;
    towerBase.travelLabel = tr("TOWER", "VĚŽ");
    addGround(towerBase);
    towerBase.decorations.insert(towerBase.decorations.end(), {
        circle(68, 48, 18, amber),
        line(0, 186, 126, 83, P::darkGray), line(126, 83, 248, 186, P::darkGray),
        line(230, 40, 172, 260, metal), line(230, 40, 291, 260, metal),
        line(230, 40, 230, 260, P::lightGray),
        line(184, 214, 278, 214, metal), line(194, 176, 268, 176, metal),
        line(204, 138, 258, 138, metal), line(215, 100, 247, 100, metal),
        line(184, 214, 268, 176, P::lightGray), line(278, 214, 194, 176, P::lightGray),
        line(194, 176, 258, 138, P::lightGray), line(268, 176, 204, 138, P::lightGray),
        line(204, 138, 247, 100, P::lightGray), line(258, 138, 215, 100, P::lightGray),
        line(230, 40, 207, 70, pale), line(230, 40, 253, 70, pale),
        line(205, 58, 255, 58, pale), circle(230, 40, 4, red),
        box(383, 104, 30, 156, wood), line(389, 111, 407, 111, pale),
        line(389, 137, 407, 137, pale), line(389, 163, 407, 163, pale),
        line(389, 189, 407, 189, pale), line(389, 215, 407, 215, pale), line(389, 241, 407, 241, pale),
        label(371, 85, tr("LADDER", "ŽEBŘÍK"), amber), box(310, 226, 46, 34, P::red), label(317, 236, tr("HV", "VN"), amber)
    });
    towerBase.hotspots.push_back({"tower_ladder", tr("LADDER", "ŽEBŘÍK"), {360, 88, 88, 172}, e2d::HotspotKind::mechanism, {}, {}});
    towerBase.hotspots.push_back({"tower_structure", tr("TOWER", "VĚŽ"), {148, 44, 164, 216}, e2d::HotspotKind::scenery, {}, {}});
    towerBase.hotspots.push_back({"tower_beacon_visual", tr("TOWER BEACON", "MAJÁK VĚŽE"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(230, 40, 7, red, false), circle(230, 40, 3, P::brightRed)}});
    towerBase.animations.push_back({"tower_beacon", true, true,
        {e2d::Condition::flag("power_on")}, {
            {8, {circle(230, 40, 9, P::brightRed, false), circle(230, 40, 4, P::brightRed)}},
            {8, {circle(230, 40, 7, red, false), circle(230, 40, 3, red)}},
        }});
    towerBase.exits.push_back(exit(e2d::Direction::left, "generator", {462, 232}));
    towerBase.exits.push_back(exit(e2d::Direction::right, "ravine", {8, 232}));
    world.addRoom(std::move(towerBase));

    // Tower top ----------------------------------------------------------------
    e2d::RoomDefinition towerTop;
    towerTop.id = "tower_top";
    towerTop.label = tr("TOWER PLATFORM", "PLOŠINA VĚŽE");
    towerTop.background = P::blue;
    towerTop.defaultSpawn = {50, 232};
    addGround(towerTop);
    towerTop.decorations.insert(towerTop.decorations.end(), {
        circle(54, 45, 16, amber),
        line(0, 193, 93, 116, P::darkGray), line(93, 116, 192, 193, P::darkGray),
        line(139, 193, 264, 86, metal), line(264, 86, 386, 193, metal),
        box(26, 218, 453, 42, metal), line(26, 218, 479, 218, pale),
        line(26, 238, 479, 238, P::darkGray),
        line(256, 27, 256, 218, pale), line(256, 54, 202, 94, pale), line(256, 54, 310, 94, pale),
        ellipse(203, 94, 24, 8, metal, false), ellipse(309, 94, 24, 8, metal, false),
        line(256, 40, 222, 40, red), line(256, 40, 290, 40, red), circle(256, 27, 4, red),
        box(321, 151, 119, 67, P::darkGray), box(329, 159, 103, 51, P::black),
        label(338, 166, tr("CONTROL", "OVLÁDÁNÍ"), amber), circle(350, 192, 7, green),
        circle(375, 192, 7, red), circle(400, 192, 7, blue), line(337, 205, 423, 205, metal),
        box(67, 181, 71, 37, rust), line(81, 181, 119, 151, rust),
        line(75, 190, 130, 190, amber), label(72, 137, tr("ANTENNA", "ANTÉNA"), amber),
        box(452, 178, 22, 40, wood), line(455, 187, 471, 187, pale), line(455, 203, 471, 203, pale)
    });
    towerTop.hotspots.push_back({"antenna_mount", tr("ANTENNA MOUNT", "DRŽÁK ANTÉNY"), {52, 136, 112, 102}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"relay_console", tr("RELAY CONSOLE", "PANEL PŘEVADĚČE"), {306, 136, 148, 104}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"tower_down_ladder", tr("LADDER DOWN", "ŽEBŘÍK DOLŮ"), {438, 166, 55, 82}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"aligned_antenna_visual", tr("ALIGNED ANTENNA", "VYROVNANÁ ANTÉNA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("antenna_aligned")}, {line(81, 181, 132, 155, green), circle(132, 155, 4, green)}});
    towerTop.hotspots.push_back({"console_power_visual", tr("LIVE CONSOLE", "AKTIVNÍ PANEL"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(350, 192, 7, P::brightGreen), line(337, 205, 423, 205, blue)}});
    towerTop.animations.push_back({"console_scan", true, true,
        {e2d::Condition::flag("power_on")}, {
            {6, {circle(350, 192, 7, P::brightGreen), line(337, 205, 371, 205, blue)}},
            {6, {circle(350, 192, 7, green), line(389, 205, 423, 205, P::brightCyan)}},
        }});
    towerTop.animations.push_back({"antenna_align", false, false, {}, {
        {2, {line(78, 178, 138, 149, amber), circle(138, 149, 5, pale, false)}},
        {2, {line(81, 181, 135, 153, pale), circle(135, 153, 6, amber, false)}},
        {3, {line(81, 181, 132, 155, green), circle(132, 155, 7, P::brightGreen, false)}},
    }});
    world.addRoom(std::move(towerTop));

    // Ravine -------------------------------------------------------------------
    e2d::RoomDefinition ravine;
    ravine.id = "ravine";
    ravine.label = tr("SERVICE RAVINE", "SERVISNÍ ROKLE");
    ravine.background = P::blue;
    ravine.defaultSpawn = {24, 232};
    addGround(ravine, 0, 166);
    addGround(ravine, 350, 162);
    ravine.decorations.insert(ravine.decorations.end(), {
        circle(430, 51, 17, amber),
        line(0, 184, 109, 88, P::darkGray), line(109, 88, 218, 184, P::darkGray),
        line(274, 184, 375, 96, metal), line(375, 96, 492, 184, metal),
        box(0, 184, 166, 76, pineDark), box(350, 184, 142, 76, pineDark),
        box(166, 232, 184, 28, P::black),
        line(166, 252, 348, 225, wood), line(166, 244, 348, 217, wood),
        line(176, 242, 186, 251, rust), line(224, 234, 239, 241, rust), line(291, 226, 308, 231, rust),
        label(207, 198, tr("ROTTEN BRIDGE", "SHNILÝ MOST"), red), circle(423, 244, 8, amber), circle(423, 244, 4, blue)
    });
    ravine.hotspots.push_back({"ravine_bridge", tr("ROTTEN BRIDGE", "SHNILÝ MOST"), {142, 190, 230, 76}, e2d::HotspotKind::hazard, {}, {}});
    ravine.hotspots.push_back({"badge_pickup", tr("OLD BADGE", "STARÝ ODZNAK"), {382, 210, 86, 56}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("badge_taken")}, {circle(423, 244, 8, amber), circle(423, 244, 4, blue)}});
    ravine.hazards.push_back({"ravine_drop", {164, 232, 188, 56},
        tr("The rotten bridge tears loose and the ravine wins.",
            "Shnilý most se utrhl a rokle zvítězila."), {}});
    ravine.exits.push_back(exit(e2d::Direction::left, "tower_base", {462, 232}));
    world.addRoom(std::move(ravine));

    // Context-sensitive F1 guidance. Higher priorities keep required earlier
    // steps ahead of later objectives even when the player explores out of
    // sequence; completed flags naturally remove obsolete hints.
    world.hints.push_back({tr(
        "At the trailhead, TAKE the patch cable from the toolbox. You will need it for the generator.",
        "Na výchozišti SEBER propojovací kabel ze skříňky. Bude potřeba u generátoru."),
        {e2d::Condition::notFlag("cable_taken")}, 120});
    world.hints.push_back({tr(
        "Go right to the caretaker cabin and speak to Mara with ENTER.",
        "Jdi doprava do správcovské chaty a promluv s Marou klávesou ENTER."),
        {e2d::Condition::flag("cable_taken"), e2d::Condition::notFlag("met_mara")}, 110});
    world.hints.push_back({tr(
        "EXAMINE Mara's desk in the cabin. Her logbook hides the yard key.",
        "PROZKOUMEJ Mařin stůl v chatě. Pod knihou záznamů je ukrytý klíč od areálu."),
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("key_revealed")}, 100});
    world.hints.push_back({tr(
        "TAKE the brass key revealed on the cabin desk.",
        "SEBER mosazný klíč odkrytý na stole v chatě."),
        {e2d::Condition::flag("key_revealed"), e2d::Condition::notFlag("key_taken")}, 90});
    world.hints.push_back({tr(
        "TAKE the wrench leaning inside the caretaker cabin.",
        "SEBER montážní klíč opřený uvnitř správcovské chaty."),
        {e2d::Condition::flag("key_taken"), e2d::Condition::notFlag("wrench_taken")}, 80});
    world.hints.push_back({tr(
        "Go right to the relay yard and USE the brass key on the locked gate.",
        "Jdi doprava do areálu převaděče a POUŽIJ mosazný klíč na zamčenou bránu."),
        {e2d::Condition::flag("wrench_taken"), e2d::Condition::notFlag("gate_open")}, 70});
    world.hints.push_back({tr(
        "Enter the generator shed beyond the gate and TAKE the spare ceramic fuse.",
        "Vstup za bránou do generátorovny a SEBER náhradní keramickou pojistku."),
        {e2d::Condition::flag("gate_open"), e2d::Condition::notFlag("fuse_taken")}, 60});
    world.hints.push_back({tr(
        "USE the ceramic fuse on the empty MAIN FUSE holder in the generator.",
        "POUŽIJ keramickou pojistku na prázdný držák HLAVNÍ POJISTKY v generátoru."),
        {e2d::Condition::flag("fuse_taken"), e2d::Condition::notFlag("fuse_installed")}, 50});
    world.hints.push_back({tr(
        "USE the patch cable on the two blue terminals in the generator shed.",
        "POUŽIJ propojovací kabel na dvě modré svorky v generátorovně."),
        {e2d::Condition::flag("fuse_installed"), e2d::Condition::notFlag("cable_installed")}, 40});
    world.hints.push_back({tr(
        "With the fuse and cable installed, operate the generator's main lever with ENTER.",
        "Po instalaci pojistky a kabelu spusť hlavní páku generátoru klávesou ENTER."),
        {e2d::Condition::flag("fuse_installed"), e2d::Condition::flag("cable_installed"),
            e2d::Condition::notFlag("power_on")}, 30});
    world.hints.push_back({tr(
        "Go right to the tower base and climb the ladder with ENTER.",
        "Jdi doprava k patě věže a vyšplhej po žebříku klávesou ENTER."),
        {e2d::Condition::flag("power_on")}, 20});
    world.hints.push_back({tr(
        "On the tower platform, USE the wrench on the twisted antenna mount.",
        "Na plošině věže POUŽIJ montážní klíč na pootočený držák antény."),
        {e2d::Condition::flag("power_on"), e2d::Condition::visited("tower_top"),
            e2d::Condition::notFlag("antenna_aligned")}, 30});
    world.hints.push_back({tr(
        "Use ENTER at the powered relay console to put Black Pine back on the air.",
        "U aktivního panelu převaděče stiskni ENTER a vrať Black Pine do éteru."),
        {e2d::Condition::flag("power_on"), e2d::Condition::flag("antenna_aligned")}, 40});

    // Interactions: trail -------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "trail_sign", std::nullopt, {},
        {inspect(tr("The map marks a caretaker cabin, the fenced relay yard, a generator shed and the tower. Handwritten below: RESTORE POWER, THEN ALIGN ANTENNA.",
            "Mapa označuje správcovskou chatu, oplocený areál převaděče, generátorovnu a věž. Rukou je dopsáno: OBNOVIT NAPÁJENÍ, POTOM VYROVNAT ANTÉNU."))}, {}, 0, {}});
    world.addInteraction({e2d::Verb::examine, "trail_toolbox", std::nullopt, {},
        {inspect(tr("The toolbox has been picked nearly clean. One weatherproof patch cable remains.",
            "Skříňka s nářadím je téměř prázdná. Zůstal v ní jeden propojovací kabel odolný proti počasí."))}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "patch_cable_pickup", std::nullopt, {},
        {inspect(tr("You coil the patch cable and add it to your pack.",
            "Smotáš propojovací kabel a uložíš ho do batohu."))},
        {e2d::Mutation::addItem("patch_cable"), e2d::Mutation::setFlag("cable_taken")}, 0, "cable_take_once"});
    world.addInteraction({e2d::Verb::take, "field_note_pickup", std::nullopt, {},
        {inspect(tr("A maintenance note is wedged under the trail map frame.",
            "Pod rámem mapy stezek je zastrčená servisní poznámka."))},
        {e2d::Mutation::addItem("field_note"), e2d::Mutation::setFlag("note_taken")}, 0, "note_take_once"});

    // Cabin --------------------------------------------------------------------
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt, {e2d::Condition::notFlag("met_mara")},
        {speech(tr("Mara: The storm killed the relay. The yard key should be under my desk logbook, if the mice did not move it.",
            "Mara: Bouře vyřadila převaděč. Klíč od areálu by měl být pod knihou záznamů na stole, pokud ho nepřestěhovaly myši.")),
         playerSpeech(tr("I will check the desk, repair the generator, and get the transmitter back on the air.",
            "Prohlédnu stůl, opravím generátor a znovu zprovozním vysílač.")),
         speech(tr("Mara: The generator needs its fuse and the blue terminals patched before you touch the main lever.",
            "Mara: Než sáhneš na hlavní páku, generátor potřebuje pojistku a propojit modré svorky."))},
        {e2d::Mutation::setFlag("met_mara")}, 10, {}});
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt,
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("gate_open")},
        {speech(tr("Mara: Check the desk closely. Then get that yard gate open.",
            "Mara: Pořádně prohlédni stůl. Pak otevři bránu areálu."))}, {}, 1, {}});
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt, {e2d::Condition::flag("gate_open")},
        {speech(tr("Mara: Good. If the generator starts, climb the tower and realign the antenna mount.",
            "Mara: Dobře. Jestli generátor nastartuje, vylez na věž a vyrovnej držák antény."))}, {}, 1, {}});
    world.addInteraction({e2d::Verb::examine, "cabin_desk", std::nullopt,
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("key_revealed")},
        {inspect(tr("You lift the swollen logbook. A brass key is taped beneath the cover.",
            "Zvedneš nabobtnalou knihu záznamů. Pod deskami je přilepený mosazný klíč."))},
        {e2d::Mutation::setFlag("key_revealed")}, 10, {}});
    world.addInteraction({e2d::Verb::examine, "cabin_desk", std::nullopt, {},
        {inspect(tr("Old maintenance logs, coffee rings and mouse tracks. Nothing else useful.",
            "Staré servisní záznamy, kolečka od kávy a myší stopy. Nic dalšího užitečného."))}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "brass_key_pickup", std::nullopt, {},
        {inspect(tr("The key is stamped YARD GATE.",
            "Na klíči je vyraženo BRÁNA AREÁLU."))},
        {e2d::Mutation::addItem("brass_key"), e2d::Mutation::setFlag("key_taken")}, 0, "key_take_once"});
    world.addInteraction({e2d::Verb::take, "wrench_pickup", std::nullopt, {},
        {inspect(tr("A solid field wrench. Heavy enough to trust.",
            "Pořádný montážní klíč do terénu. Je dost těžký, aby mu šlo věřit."))},
        {e2d::Mutation::addItem("wrench"), e2d::Mutation::setFlag("wrench_taken")}, 0, "wrench_take_once"});
    world.addInteraction({e2d::Verb::examine, "dead_radio", std::nullopt, {},
        {inspect(tr("The cabin radio has power, but the mountain relay answers with pure static.",
            "Rádio v chatě má proud, ale horský převaděč odpovídá jen čistým šumem."))}, {}, 0, {}});

    // Yard ---------------------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "yard_gate", std::nullopt, {e2d::Condition::notFlag("gate_open")},
        {inspect(tr("A mechanical lock. The storm did not break this part, unfortunately.",
            "Mechanický zámek. Tenhle kus bouře bohužel nepoškodila."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "yard_gate", std::nullopt, {e2d::Condition::flag("gate_open")},
        {inspect(tr("The gate hangs open toward the generator shed.",
            "Brána zůstává otevřená směrem ke generátorovně."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "yard_gate", std::optional<std::string>{"brass_key"}, {e2d::Condition::notFlag("gate_open")},
        {inspect(tr("The brass key turns with a hard metallic snap. The yard gate is open.",
            "Mosazný klíč se otočí s tvrdým kovovým cvaknutím. Brána areálu je otevřená."))},
        {e2d::Mutation::setFlag("gate_open")}, 10, {}, "unlock"});
    world.addInteraction({e2d::Verb::examine, "relay_frame", std::nullopt, {},
        {inspect(tr("The old relay frame is only a skeleton now. All useful equipment was moved into the shed and tower console.",
            "Ze starého rámu převaděče zůstala jen kostra. Veškeré užitečné zařízení bylo přestěhováno do generátorovny a panelu na věži."))}, {}, 0, {}});

    // Generator ----------------------------------------------------------------
    world.addInteraction({e2d::Verb::take, "fuse_pickup", std::nullopt, {},
        {inspect(tr("A spare ceramic fuse survives in the parts rack.",
            "V regálu s náhradními díly přežila keramická pojistka."))},
        {e2d::Mutation::addItem("ceramic_fuse"), e2d::Mutation::setFlag("fuse_taken")}, 0, "fuse_take_once"});
    world.addInteraction({e2d::Verb::examine, "fuse_socket", std::nullopt, {e2d::Condition::notFlag("fuse_installed")},
        {inspect(tr("The MAIN FUSE socket is empty. Burn marks stop at the holder.",
            "Držák HLAVNÍ POJISTKY je prázdný. Stopy po spálení končí u držáku."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "fuse_socket", std::nullopt, {e2d::Condition::flag("fuse_installed")},
        {inspect(tr("The replacement fuse sits firmly in the MAIN holder.",
            "Náhradní pojistka pevně sedí v HLAVNÍM držáku."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "fuse_socket", std::optional<std::string>{"ceramic_fuse"}, {e2d::Condition::notFlag("fuse_installed")},
        {inspect(tr("The ceramic fuse locks into the holder.",
            "Keramická pojistka zapadne do držáku."))},
        {e2d::Mutation::setFlag("fuse_installed"), e2d::Mutation::removeItem("ceramic_fuse")}, 10, {}, "repair"});
    world.addInteraction({e2d::Verb::examine, "cable_terminals", std::nullopt, {e2d::Condition::notFlag("cable_installed")},
        {inspect(tr("Two blue terminals should be linked. The original cable has burned away.",
            "Dvě modré svorky mají být propojené. Původní kabel shořel."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "cable_terminals", std::optional<std::string>{"patch_cable"}, {e2d::Condition::notFlag("cable_installed")},
        {inspect(tr("The patch cable bridges the blue terminals exactly.",
            "Propojovací kabel přesně spojí modré svorky."))},
        {e2d::Mutation::setFlag("cable_installed"), e2d::Mutation::removeItem("patch_cable")}, 10, {}, "repair"});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt,
        {e2d::Condition::flag("fuse_installed"), e2d::Condition::flag("cable_installed"), e2d::Condition::notFlag("power_on")},
        {inspect(tr("You throw the main lever. The generator coughs twice, then settles into a steady roar. Lights wake across the relay site.",
            "Přehodíš hlavní páku. Generátor dvakrát zakašle a pak se ustálí v pravidelném řevu. V celém areálu převaděče se rozsvítí světla."))},
        {e2d::Mutation::setFlag("power_on"), e2d::Mutation::playAnimation("generator_start")}, 20, {}, "power"});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt, {e2d::Condition::flag("power_on")},
        {inspect(tr("The generator is running steadily. Best leave the lever alone.",
            "Generátor běží pravidelně. Nejlepší bude nechat páku na pokoji."))}, {}, 15, {}});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt, {},
        {warning(tr("The lever refuses to latch. Something in the generator circuit is still incomplete.",
            "Páka odmítá zapadnout. V obvodu generátoru stále něco chybí."))}, {}, 0, {}});

    // Tower --------------------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "tower_structure", std::nullopt, {},
        {inspect(tr("A steel lattice tower, old but sound. The maintenance platform is reached by the ladder on the right.",
            "Stará, ale pevná ocelová příhradová věž. Na servisní plošinu vede žebřík vpravo."))}, {}, 0, {}});
    world.addInteraction({e2d::Verb::context, "tower_ladder", std::nullopt, {},
        {inspect(tr("You climb to the maintenance platform.",
            "Vylezeš na servisní plošinu."))}, {e2d::Mutation::moveTo("tower_top")}, 0, {}, "climb"});
    world.addInteraction({e2d::Verb::context, "tower_down_ladder", std::nullopt, {},
        {inspect(tr("You climb back down to the base of the tower.",
            "Slezeš zpět k patě věže."))}, {e2d::Mutation::moveTo("tower_base")}, 0, {}, "climb"});
    world.addInteraction({e2d::Verb::examine, "antenna_mount", std::nullopt, {e2d::Condition::notFlag("antenna_aligned")},
        {inspect(tr("The storm twisted the azimuth mount several degrees off its painted alignment marks. The locking nut will not move by hand.",
            "Bouře pootočila azimutový držák o několik stupňů mimo vyznačené rysky. Pojistnou maticí rukou nepohneš."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "antenna_mount", std::optional<std::string>{"wrench"}, {e2d::Condition::notFlag("antenna_aligned")},
        {inspect(tr("The wrench breaks the nut free. You swing the antenna onto the old alignment marks and lock it down.",
            "Klíčem uvolníš matici. Natočíš anténu podle starých rysek a znovu ji zajistíš."))},
        {e2d::Mutation::setFlag("antenna_aligned"), e2d::Mutation::playAnimation("antenna_align")}, 10, {}, "repair"});
    world.addInteraction({e2d::Verb::examine, "relay_console", std::nullopt, {e2d::Condition::notFlag("power_on")},
        {inspect(tr("The console is dark. The generator below is still offline.",
            "Panel je zhasnutý. Generátor dole stále neběží."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "relay_console", std::nullopt, {e2d::Condition::flag("power_on")},
        {inspect(tr("The console is alive. Carrier power is available, but the directional antenna must also be aligned before transmission.",
            "Panel je aktivní. Napájení nosné vlny je k dispozici, ale před vysíláním je nutné vyrovnat směrovou anténu."))}, {}, 5, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt,
        {e2d::Condition::flag("power_on"), e2d::Condition::flag("antenna_aligned")}, {},
        {e2d::Mutation::win(tr("THE BLACK PINE TRANSMITTER IS BACK ON THE AIR. A clean carrier rolls across the valley, and the emergency net answers within seconds.",
            "VYSÍLAČ BLACK PINE JE ZNOVU V ÉTERU. Čistá nosná vlna se rozléhá údolím a nouzová síť během několika sekund odpovídá."))}, 20, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt, {e2d::Condition::notFlag("power_on")},
        {warning(tr("Nothing happens. The console has no power.",
            "Nic se nestane. Panel není napájený."))}, {}, 10, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt,
        {e2d::Condition::flag("power_on"), e2d::Condition::notFlag("antenna_aligned")},
        {warning(tr("The transmitter keys, but reflected power spikes instantly. The antenna is still misaligned.",
            "Vysílač se zaklíčuje, ale odražený výkon okamžitě vyskočí. Anténa stále není správně vyrovnaná."))}, {}, 10, {}});

    // Optional ravine -----------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "ravine_bridge", std::nullopt, {},
        {warning(tr("The service bridge is rotten through. Crossing it would be a very bad experiment.",
            "Servisní most je skrz naskrz shnilý. Přechod by byl velmi špatný pokus."))}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "badge_pickup", std::nullopt, {},
        {inspect(tr("You pocket an old enamel relay badge. It will not fix anything, but it feels worth saving.",
            "Schováš si starý smaltovaný odznak převaděče. Nic neopraví, ale stojí za záchranu."))},
        {e2d::Mutation::addItem("old_badge"), e2d::Mutation::setFlag("badge_taken")}, 0, "badge_take_once"});

    return world;
}

} // namespace black_pine
