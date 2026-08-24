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

e2d::Visual label(float x, float y, std::string text, P color = pale, int scale = 1) {
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

e2d::Message inspect(std::string text) { return {std::move(text), e2d::MessageStyle::inspect}; }
e2d::Message speech(std::string text) { return {std::move(text), e2d::MessageStyle::speech}; }
e2d::Message warning(std::string text) { return {std::move(text), e2d::MessageStyle::warning}; }

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
    world.title = "Black Pine";
    world.startRoom = "trailhead";
    world.presentation.inventoryHeading = "YOU CARRY";
    world.presentation.creditLine = "A BLACK PINE STORY";
    world.presentation.title.subtitle = "A MOUNTAIN RELAY MYSTERY";
    world.presentation.title.byline = "AN EXPLORE2D ADVENTURE";
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
        label(32, 218, "STORM OVER BLACK PINE RIDGE", P::brightYellow),
    };

    world.addItem({"patch_cable", "PATCH CABLE", "A weatherproof copper patch cable, short but intact.", true});
    world.addItem({"field_note", "FIELD NOTE", "A faded maintenance note: MAIN FUSE FIRST. PATCH THE BLUE TERMINALS. THEN THROW THE LEVER.", false});
    world.addItem({"brass_key", "BRASS KEY", "A heavy brass key stamped YARD GATE.", true});
    world.addItem({"ceramic_fuse", "CERAMIC FUSE", "A 30 amp ceramic fuse. Miraculously uncracked.", true});
    world.addItem({"wrench", "WRENCH", "A long handled 17 mm wrench, scarred by years of field repairs.", true});
    world.addItem({"old_badge", "OLD BADGE", "An enamel badge from the BLACK PINE radio relay. Purely sentimental.", false});

    // Trailhead ----------------------------------------------------------------
    e2d::RoomDefinition trail;
    trail.id = "trailhead";
    trail.label = "BLACK PINE TRAILHEAD";
    trail.background = sky;
    trail.defaultSpawn = {42, 232};
    trail.travelAnchor = true;
    trail.travelLabel = "TRAILHEAD";
    addGround(trail);
    trail.decorations.insert(trail.decorations.end(), {
        circle(400, 46, 18, amber),
        line(0, 173, 74, 89, metal), line(74, 89, 151, 173, metal),
        line(113, 173, 221, 69, pale), line(221, 69, 330, 173, pale),
        line(286, 173, 376, 104, P::darkGray), line(376, 104, 492, 173, P::darkGray),
        box(0, 172, 492, 88, pineDark),
        ellipse(155, 187, 68, 25, pine), ellipse(330, 181, 79, 31, pine),
        box(198, 191, 116, 61, wood), box(204, 197, 104, 49, P::black),
        label(222, 207, "BLACK PINE", amber), label(225, 221, "TRAIL MAP", pale),
        line(213, 238, 286, 238, green), circle(243, 238, 3, red), circle(276, 238, 3, blue),
        box(356, 220, 66, 32, metal), box(361, 215, 56, 7, pale),
        line(362, 234, 416, 234, P::darkGray), circle(389, 224, 2, P::black)
    });
    addPine(trail, 34, 260, 126);
    addPine(trail, 92, 260, 154);
    addPine(trail, 451, 260, 142);
    trail.hotspots.push_back({"trail_sign", "TRAIL MAP", {188, 178, 136, 82}, e2d::HotspotKind::scenery, {}, {}});
    trail.hotspots.push_back({"trail_toolbox", "TOOLBOX", {338, 198, 106, 62}, e2d::HotspotKind::scenery, {}, {}});
    trail.hotspots.push_back({"patch_cable_pickup", "PATCH CABLE", {345, 205, 100, 55}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("cable_taken")}, {box(382, 228, 24, 8, blue)}});
    trail.hotspots.push_back({"field_note_pickup", "FIELD NOTE", {192, 194, 130, 66}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("note_taken")}, {box(262, 239, 19, 11, pale)}});
    trail.exits.push_back(exit(e2d::Direction::right, "cabin", {8, 232}));
    world.addRoom(std::move(trail));

    // Cabin --------------------------------------------------------------------
    e2d::RoomDefinition cabin;
    cabin.id = "cabin";
    cabin.label = "CARETAKER CABIN";
    cabin.background = night;
    cabin.defaultSpawn = {28, 232};
    cabin.travelAnchor = true;
    cabin.travelLabel = "CABIN";
    addGround(cabin);
    cabin.decorations.insert(cabin.decorations.end(), {
        box(39, 69, 421, 191, wood), box(51, 82, 397, 178, P::red),
        line(51, 105, 448, 105, P::brown), line(51, 141, 448, 141, P::brown),
        line(51, 177, 448, 177, P::brown), line(51, 213, 448, 213, P::brown),
        box(75, 102, 76, 66, P::blue), box(81, 108, 64, 54, P::brightBlue),
        line(113, 108, 113, 162, pale), line(81, 135, 145, 135, pale),
        circle(174, 104, 12, amber), line(174, 116, 174, 139, metal),
        box(344, 148, 99, 68, wood), box(351, 155, 85, 54, P::black),
        label(364, 163, "RADIO", amber), circle(367, 190, 8, metal, false),
        line(387, 190, 424, 190, green), line(387, 197, 418, 197, blue),
        box(177, 202, 115, 44, wood), box(184, 208, 101, 9, P::lightGray),
        box(207, 193, 52, 10, pale), line(180, 246, 180, 260, P::darkGray), line(288, 246, 288, 260, P::darkGray),
        box(84, 176, 17, 70, metal), circle(92, 183, 9, pale, false),
        line(92, 195, 128, 221, metal), circle(128, 221, 7, metal, false),
        circle(311, 190, 11, amber), box(302, 201, 19, 44, pale),
        line(302, 211, 289, 230, amber), line(320, 211, 332, 230, amber)
    });
    cabin.hotspots.push_back({"caretaker", "MARA", {274, 170, 74, 90}, e2d::HotspotKind::character, {}, {}});
    cabin.hotspots.push_back({"cabin_desk", "DESK", {166, 184, 132, 76}, e2d::HotspotKind::scenery, {}, {}});
    cabin.hotspots.push_back({"brass_key_pickup", "BRASS KEY", {185, 188, 118, 72}, e2d::HotspotKind::item,
        {e2d::Condition::flag("key_revealed"), e2d::Condition::notFlag("key_taken")}, {box(238, 216, 18, 5, amber)}});
    cabin.hotspots.push_back({"wrench_pickup", "WRENCH", {64, 164, 76, 96}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("wrench_taken")}, {box(91, 202, 42, 5, metal)}});
    cabin.hotspots.push_back({"dead_radio", "RADIO", {338, 142, 116, 94}, e2d::HotspotKind::mechanism, {}, {}});
    cabin.exits.push_back(exit(e2d::Direction::left, "trailhead", {462, 232}));
    cabin.exits.push_back(exit(e2d::Direction::right, "yard", {8, 232}));
    world.addRoom(std::move(cabin));

    // Yard ---------------------------------------------------------------------
    e2d::RoomDefinition yard;
    yard.id = "yard";
    yard.label = "RELAY YARD";
    yard.background = sky;
    yard.defaultSpawn = {24, 232};
    yard.travelAnchor = true;
    yard.travelLabel = "RELAY YARD";
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
        box(414, 169, 20, 28, amber), circle(424, 178, 3, P::black), label(389, 96, "GATE", amber)
    });
    addPine(yard, 27, 260, 104);
    addPine(yard, 323, 260, 86);
    yard.hotspots.push_back({"yard_gate", "YARD GATE", {340, 94, 160, 166}, e2d::HotspotKind::mechanism, {}, {}});
    yard.hotspots.push_back({"yard_gate_open", "OPEN GATE", {340, 94, 160, 166}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("gate_open")}, {box(383, 134, 80, 5, green), label(397, 151, "OPEN", green)}});
    yard.hotspots.push_back({"relay_frame", "RELAY FRAME", {38, 82, 198, 178}, e2d::HotspotKind::scenery, {}, {}});
    yard.exits.push_back(exit(e2d::Direction::left, "cabin", {462, 232}));
    auto yardRight = exit(e2d::Direction::right, "generator", {8, 232});
    yardRight.availableWhen = {e2d::Condition::flag("gate_open")};
    yardRight.blockedMessage = "The locked yard gate blocks the way to the generator shed.";
    yard.exits.push_back(std::move(yardRight));
    world.addRoom(std::move(yard));

    // Generator ----------------------------------------------------------------
    e2d::RoomDefinition generator;
    generator.id = "generator";
    generator.label = "GENERATOR SHED";
    generator.background = P::darkGray;
    generator.defaultSpawn = {28, 232};
    addGround(generator);
    generator.decorations.insert(generator.decorations.end(), {
        box(0, 28, 492, 232, P::darkGray),
        line(0, 64, 492, 64, P::lightGray), line(0, 112, 492, 112, P::lightGray),
        line(0, 160, 492, 160, P::lightGray), line(0, 208, 492, 208, P::lightGray),
        box(66, 93, 267, 151, P::lightGray), box(73, 100, 253, 137, P::black),
        label(123, 108, "BLACK PINE GENERATOR", amber),
        box(90, 127, 102, 80, metal), box(98, 135, 86, 64, P::blue),
        circle(120, 162, 18, P::white, false), line(120, 162, 132, 151, red),
        circle(161, 162, 12, P::white, false), line(161, 162, 161, 151, green),
        label(104, 188, "TERMINALS", pale),
        box(210, 127, 92, 72, P::darkGray), label(222, 139, "MAIN", amber),
        box(232, 158, 46, 22, P::black), circle(243, 169, 6, red), circle(266, 169, 6, green),
        box(351, 140, 84, 104, wood), box(359, 149, 68, 17, P::red),
        label(366, 154, "SPARES", pale), line(361, 178, 425, 178, metal),
        line(361, 204, 425, 204, metal), circle(393, 191, 10, pale, false),
        line(113, 207, 113, 238, blue), line(160, 207, 160, 238, blue),
        circle(113, 220, 5, blue), circle(160, 220, 5, blue),
        box(244, 205, 14, 34, rust), line(251, 205, 279, 185, rust), circle(280, 184, 4, amber)
    });
    generator.hotspots.push_back({"fuse_pickup", "CERAMIC FUSE", {340, 132, 105, 112}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("fuse_taken")}, {box(382, 181, 18, 6, pale)}});
    generator.hotspots.push_back({"fuse_socket", "FUSE SOCKET", {190, 114, 120, 104}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"cable_terminals", "BLUE TERMINALS", {88, 178, 92, 80}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"generator_lever", "MAIN LEVER", {228, 184, 70, 74}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"installed_fuse_visual", "INSTALLED FUSE", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("fuse_installed")}, {box(246, 164, 18, 7, pale), line(249, 164, 249, 171, red)}});
    generator.hotspots.push_back({"installed_cable_visual", "PATCHED TERMINALS", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("cable_installed")}, {line(113, 220, 160, 220, blue), line(113, 221, 160, 221, blue)}});
    generator.hotspots.push_back({"power_lamp_visual", "POWER LAMP", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(266, 169, 6, green), circle(280, 184, 4, green)}});
    generator.exits.push_back(exit(e2d::Direction::left, "yard", {462, 232}));
    generator.exits.push_back(exit(e2d::Direction::right, "tower_base", {8, 232}));
    world.addRoom(std::move(generator));

    // Tower base ---------------------------------------------------------------
    e2d::RoomDefinition towerBase;
    towerBase.id = "tower_base";
    towerBase.label = "BLACK PINE TOWER";
    towerBase.background = sky;
    towerBase.defaultSpawn = {28, 232};
    towerBase.travelAnchor = true;
    towerBase.travelLabel = "TOWER";
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
        label(371, 85, "LADDER", amber), box(310, 226, 46, 34, P::red), label(317, 236, "HV", amber)
    });
    towerBase.hotspots.push_back({"tower_ladder", "LADDER", {360, 88, 88, 172}, e2d::HotspotKind::mechanism, {}, {}});
    towerBase.hotspots.push_back({"tower_structure", "TOWER", {148, 44, 164, 216}, e2d::HotspotKind::scenery, {}, {}});
    towerBase.hotspots.push_back({"tower_beacon_visual", "TOWER BEACON", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(230, 40, 7, red, false), circle(230, 40, 3, P::brightRed)}});
    towerBase.exits.push_back(exit(e2d::Direction::left, "generator", {462, 232}));
    towerBase.exits.push_back(exit(e2d::Direction::right, "ravine", {8, 232}));
    world.addRoom(std::move(towerBase));

    // Tower top ----------------------------------------------------------------
    e2d::RoomDefinition towerTop;
    towerTop.id = "tower_top";
    towerTop.label = "TOWER PLATFORM";
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
        label(338, 166, "CONTROL", amber), circle(350, 192, 7, green),
        circle(375, 192, 7, red), circle(400, 192, 7, blue), line(337, 205, 423, 205, metal),
        box(67, 181, 71, 37, rust), line(81, 181, 119, 151, rust),
        line(75, 190, 130, 190, amber), label(72, 137, "ANTENNA", amber),
        box(452, 178, 22, 40, wood), line(455, 187, 471, 187, pale), line(455, 203, 471, 203, pale)
    });
    towerTop.hotspots.push_back({"antenna_mount", "ANTENNA MOUNT", {52, 136, 112, 102}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"relay_console", "RELAY CONSOLE", {306, 136, 148, 104}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"tower_down_ladder", "LADDER DOWN", {438, 166, 55, 82}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"aligned_antenna_visual", "ALIGNED ANTENNA", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("antenna_aligned")}, {line(81, 181, 132, 155, green), circle(132, 155, 4, green)}});
    towerTop.hotspots.push_back({"console_power_visual", "LIVE CONSOLE", {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("power_on")}, {circle(350, 192, 7, P::brightGreen), line(337, 205, 423, 205, blue)}});
    world.addRoom(std::move(towerTop));

    // Ravine -------------------------------------------------------------------
    e2d::RoomDefinition ravine;
    ravine.id = "ravine";
    ravine.label = "SERVICE RAVINE";
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
        label(207, 198, "ROTTEN BRIDGE", red), circle(423, 244, 8, amber), circle(423, 244, 4, blue)
    });
    ravine.hotspots.push_back({"ravine_bridge", "ROTTEN BRIDGE", {142, 190, 230, 76}, e2d::HotspotKind::hazard, {}, {}});
    ravine.hotspots.push_back({"badge_pickup", "OLD BADGE", {382, 210, 86, 56}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("badge_taken")}, {circle(423, 244, 8, amber), circle(423, 244, 4, blue)}});
    ravine.hazards.push_back({"ravine_drop", {164, 232, 188, 56}, "The rotten bridge tears loose and the ravine wins.", {}});
    ravine.exits.push_back(exit(e2d::Direction::left, "tower_base", {462, 232}));
    world.addRoom(std::move(ravine));

    // Interactions: trail -------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "trail_sign", std::nullopt, {},
        {inspect("The map marks a caretaker cabin, the fenced relay yard, a generator shed and the tower. Handwritten below: RESTORE POWER, THEN ALIGN ANTENNA.")}, {}, 0, {}});
    world.addInteraction({e2d::Verb::examine, "trail_toolbox", std::nullopt, {},
        {inspect("The toolbox has been picked nearly clean. One weatherproof patch cable remains.")}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "patch_cable_pickup", std::nullopt, {},
        {inspect("You coil the patch cable and add it to your pack.")},
        {e2d::Mutation::addItem("patch_cable"), e2d::Mutation::setFlag("cable_taken")}, 0, "cable_take_once"});
    world.addInteraction({e2d::Verb::take, "field_note_pickup", std::nullopt, {},
        {inspect("A maintenance note is wedged under the trail map frame.")},
        {e2d::Mutation::addItem("field_note"), e2d::Mutation::setFlag("note_taken")}, 0, "note_take_once"});

    // Cabin --------------------------------------------------------------------
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt, {e2d::Condition::notFlag("met_mara")},
        {speech("Mara: The storm killed the relay. The yard key should be under my desk logbook, if the mice did not move it."),
         speech("Mara: The generator needs its fuse and the blue terminals patched before you touch the main lever.")},
        {e2d::Mutation::setFlag("met_mara")}, 10, {}});
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt,
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("gate_open")},
        {speech("Mara: Check the desk closely. Then get that yard gate open.")}, {}, 1, {}});
    world.addInteraction({e2d::Verb::context, "caretaker", std::nullopt, {e2d::Condition::flag("gate_open")},
        {speech("Mara: Good. If the generator starts, climb the tower and realign the antenna mount.")}, {}, 1, {}});
    world.addInteraction({e2d::Verb::examine, "cabin_desk", std::nullopt,
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("key_revealed")},
        {inspect("You lift the swollen logbook. A brass key is taped beneath the cover.")},
        {e2d::Mutation::setFlag("key_revealed")}, 10, {}});
    world.addInteraction({e2d::Verb::examine, "cabin_desk", std::nullopt, {},
        {inspect("Old maintenance logs, coffee rings and mouse tracks. Nothing else useful.")}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "brass_key_pickup", std::nullopt, {},
        {inspect("The key is stamped YARD GATE.")},
        {e2d::Mutation::addItem("brass_key"), e2d::Mutation::setFlag("key_taken")}, 0, "key_take_once"});
    world.addInteraction({e2d::Verb::take, "wrench_pickup", std::nullopt, {},
        {inspect("A solid field wrench. Heavy enough to trust.")},
        {e2d::Mutation::addItem("wrench"), e2d::Mutation::setFlag("wrench_taken")}, 0, "wrench_take_once"});
    world.addInteraction({e2d::Verb::examine, "dead_radio", std::nullopt, {},
        {inspect("The cabin radio has power, but the mountain relay answers with pure static.")}, {}, 0, {}});

    // Yard ---------------------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "yard_gate", std::nullopt, {e2d::Condition::notFlag("gate_open")},
        {inspect("A mechanical lock. The storm did not break this part, unfortunately.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "yard_gate", std::nullopt, {e2d::Condition::flag("gate_open")},
        {inspect("The gate hangs open toward the generator shed.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "yard_gate", std::optional<std::string>{"brass_key"}, {e2d::Condition::notFlag("gate_open")},
        {inspect("The brass key turns with a hard metallic snap. The yard gate is open.")},
        {e2d::Mutation::setFlag("gate_open")}, 10, {}});
    world.addInteraction({e2d::Verb::examine, "relay_frame", std::nullopt, {},
        {inspect("The old relay frame is only a skeleton now. All useful equipment was moved into the shed and tower console.")}, {}, 0, {}});

    // Generator ----------------------------------------------------------------
    world.addInteraction({e2d::Verb::take, "fuse_pickup", std::nullopt, {},
        {inspect("A spare ceramic fuse survives in the parts rack.")},
        {e2d::Mutation::addItem("ceramic_fuse"), e2d::Mutation::setFlag("fuse_taken")}, 0, "fuse_take_once"});
    world.addInteraction({e2d::Verb::examine, "fuse_socket", std::nullopt, {e2d::Condition::notFlag("fuse_installed")},
        {inspect("The MAIN FUSE socket is empty. Burn marks stop at the holder.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "fuse_socket", std::nullopt, {e2d::Condition::flag("fuse_installed")},
        {inspect("The replacement fuse sits firmly in the MAIN holder.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "fuse_socket", std::optional<std::string>{"ceramic_fuse"}, {e2d::Condition::notFlag("fuse_installed")},
        {inspect("The ceramic fuse locks into the holder.")},
        {e2d::Mutation::setFlag("fuse_installed"), e2d::Mutation::removeItem("ceramic_fuse")}, 10, {}});
    world.addInteraction({e2d::Verb::examine, "cable_terminals", std::nullopt, {e2d::Condition::notFlag("cable_installed")},
        {inspect("Two blue terminals should be linked. The original cable has burned away.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "cable_terminals", std::optional<std::string>{"patch_cable"}, {e2d::Condition::notFlag("cable_installed")},
        {inspect("The patch cable bridges the blue terminals exactly.")},
        {e2d::Mutation::setFlag("cable_installed"), e2d::Mutation::removeItem("patch_cable")}, 10, {}});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt,
        {e2d::Condition::flag("fuse_installed"), e2d::Condition::flag("cable_installed"), e2d::Condition::notFlag("power_on")},
        {inspect("You throw the main lever. The generator coughs twice, then settles into a steady roar. Lights wake across the relay site.")},
        {e2d::Mutation::setFlag("power_on")}, 20, {}});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt, {e2d::Condition::flag("power_on")},
        {inspect("The generator is running steadily. Best leave the lever alone.")}, {}, 15, {}});
    world.addInteraction({e2d::Verb::context, "generator_lever", std::nullopt, {},
        {warning("The lever refuses to latch. Something in the generator circuit is still incomplete.")}, {}, 0, {}});

    // Tower --------------------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "tower_structure", std::nullopt, {},
        {inspect("A steel lattice tower, old but sound. The maintenance platform is reached by the ladder on the right.")}, {}, 0, {}});
    world.addInteraction({e2d::Verb::context, "tower_ladder", std::nullopt, {},
        {inspect("You climb to the maintenance platform.")}, {e2d::Mutation::moveTo("tower_top")}, 0, {}});
    world.addInteraction({e2d::Verb::context, "tower_down_ladder", std::nullopt, {},
        {inspect("You climb back down to the base of the tower.")}, {e2d::Mutation::moveTo("tower_base")}, 0, {}});
    world.addInteraction({e2d::Verb::examine, "antenna_mount", std::nullopt, {e2d::Condition::notFlag("antenna_aligned")},
        {inspect("The storm twisted the azimuth mount several degrees off its painted alignment marks. The locking nut will not move by hand.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::use, "antenna_mount", std::optional<std::string>{"wrench"}, {e2d::Condition::notFlag("antenna_aligned")},
        {inspect("The wrench breaks the nut free. You swing the antenna onto the old alignment marks and lock it down.")},
        {e2d::Mutation::setFlag("antenna_aligned")}, 10, {}});
    world.addInteraction({e2d::Verb::examine, "relay_console", std::nullopt, {e2d::Condition::notFlag("power_on")},
        {inspect("The console is dark. The generator below is still offline.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::examine, "relay_console", std::nullopt, {e2d::Condition::flag("power_on")},
        {inspect("The console is alive. Carrier power is available, but the directional antenna must also be aligned before transmission.")}, {}, 5, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt,
        {e2d::Condition::flag("power_on"), e2d::Condition::flag("antenna_aligned")}, {},
        {e2d::Mutation::win("THE BLACK PINE TRANSMITTER IS BACK ON THE AIR. A clean carrier rolls across the valley, and the emergency net answers within seconds.")}, 20, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt, {e2d::Condition::notFlag("power_on")},
        {warning("Nothing happens. The console has no power.")}, {}, 10, {}});
    world.addInteraction({e2d::Verb::context, "relay_console", std::nullopt,
        {e2d::Condition::flag("power_on"), e2d::Condition::notFlag("antenna_aligned")},
        {warning("The transmitter keys, but reflected power spikes instantly. The antenna is still misaligned.")}, {}, 10, {}});

    // Optional ravine -----------------------------------------------------------
    world.addInteraction({e2d::Verb::examine, "ravine_bridge", std::nullopt, {},
        {warning("The service bridge is rotten through. Crossing it would be a very bad experiment.")}, {}, 0, {}});
    world.addInteraction({e2d::Verb::take, "badge_pickup", std::nullopt, {},
        {inspect("You pocket an old enamel relay badge. It will not fix anything, but it feels worth saving.")},
        {e2d::Mutation::addItem("old_badge"), e2d::Mutation::setFlag("badge_taken")}, 0, "badge_take_once"});

    return world;
}

} // namespace black_pine
