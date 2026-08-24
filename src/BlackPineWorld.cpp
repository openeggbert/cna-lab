#include "BlackPineWorld.hpp"

#include "explore2d/Renderer.hpp"

#include <optional>
#include <string>
#include <utility>

namespace black_pine {
namespace e2d = explore2d;
namespace {

constexpr e2d::Rgba sky{30, 48, 58, 255};
constexpr e2d::Rgba night{18, 30, 38, 255};
constexpr e2d::Rgba pine{38, 76, 60, 255};
constexpr e2d::Rgba pineDark{26, 54, 43, 255};
constexpr e2d::Rgba ground{69, 65, 55, 255};
constexpr e2d::Rgba wood{112, 83, 58, 255};
constexpr e2d::Rgba metal{115, 129, 132, 255};
constexpr e2d::Rgba rust{143, 76, 49, 255};
constexpr e2d::Rgba pale{220, 224, 210, 255};
constexpr e2d::Rgba amber{239, 186, 73, 255};
constexpr e2d::Rgba red{205, 70, 62, 255};
constexpr e2d::Rgba green{81, 173, 111, 255};
constexpr e2d::Rgba blue{79, 147, 179, 255};

e2d::Visual box(float x, float y, float w, float h, e2d::Rgba color, bool filled = true) {
    return e2d::RectVisual{{x, y, w, h}, color, filled};
}

e2d::Visual line(float x1, float y1, float x2, float y2, e2d::Rgba color) {
    return e2d::LineVisual{{x1, y1}, {x2, y2}, color};
}

e2d::Visual label(float x, float y, std::string text, e2d::Rgba color = pale, int scale = 1) {
    return e2d::TextVisual{{x, y}, std::move(text), color, scale};
}

void addGround(e2d::RoomDefinition& room, float x = 0.0F, float width = 512.0F) {
    room.solids.push_back({x, 260.0F, width, 28.0F});
    room.decorations.push_back(box(x, 260.0F, width, 28.0F, ground));
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
    theme.frame = {176, 191, 190, 255};
    theme.panel = {13, 21, 26, 255};
    theme.panelAlt = {20, 31, 37, 255};
    theme.text = pale;
    theme.dimText = {113, 132, 136, 255};
    theme.accent = amber;
    theme.danger = red;
    theme.player = {226, 218, 195, 255};
    theme.playerAccent = blue;
    return theme;
}

e2d::WorldDefinition buildWorld() {
    e2d::WorldDefinition world;
    world.title = "Black Pine";
    world.startRoom = "trailhead";

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
        box(0, 180, 512, 80, pineDark),
        box(18, 122, 18, 138, wood), box(82, 104, 16, 156, wood), box(438, 116, 18, 144, wood),
        line(27, 122, 7, 178, pine), line(27, 122, 52, 178, pine),
        line(90, 104, 58, 176, pine), line(90, 104, 126, 176, pine),
        line(447, 116, 415, 176, pine), line(447, 116, 480, 176, pine),
        box(210, 198, 92, 46, wood, false), label(225, 213, "TRAIL MAP", amber),
        box(362, 222, 58, 30, metal), box(367, 218, 48, 5, metal)
    });
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
        box(48, 82, 415, 178, wood), box(64, 101, 383, 159, {51, 49, 43, 255}),
        box(350, 155, 90, 58, wood), box(354, 159, 82, 50, {65, 60, 50, 255}), label(365, 178, "RADIO", amber),
        box(188, 204, 92, 40, wood), box(214, 194, 41, 10, pale),
        box(94, 178, 15, 66, metal), box(82, 188, 39, 8, metal),
        box(300, 204, 22, 40, pale), box(295, 190, 32, 20, {174, 135, 96, 255})
    });
    cabin.hotspots.push_back({"caretaker", "MARA", {274, 170, 74, 90}, e2d::HotspotKind::character, {}, {}});
    cabin.hotspots.push_back({"cabin_desk", "DESK", {166, 184, 132, 76}, e2d::HotspotKind::scenery, {}, {}});
    cabin.hotspots.push_back({"brass_key_pickup", "BRASS KEY", {185, 188, 118, 72}, e2d::HotspotKind::item,
        {e2d::Condition::flag("key_revealed"), e2d::Condition::notFlag("key_taken")}, {box(238, 216, 18, 5, amber)}});
    cabin.hotspots.push_back({"wrench_pickup", "WRENCH", {64, 164, 76, 96}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("wrench_taken")}, {box(91, 202, 42, 5, metal)}});
    cabin.hotspots.push_back({"dead_radio", "RADIO", {338, 142, 116, 94}, e2d::HotspotKind::mechanism, {}, {}});
    cabin.exits.push_back(exit(e2d::Direction::left, "trailhead", {482, 232}));
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
        box(60, 120, 7, 140, metal), box(130, 95, 7, 165, metal), box(205, 125, 7, 135, metal),
        line(63, 120, 133, 95, metal), line(133, 95, 208, 125, metal),
        box(364, 116, 11, 144, rust), box(472, 116, 11, 144, rust),
        line(369, 116, 477, 116, rust), line(369, 145, 477, 145, rust), line(369, 174, 477, 174, rust),
        line(369, 203, 477, 203, rust), line(369, 232, 477, 232, rust), label(383, 94, "GATE", amber)
    });
    yard.hotspots.push_back({"yard_gate", "YARD GATE", {340, 94, 160, 166}, e2d::HotspotKind::mechanism, {}, {}});
    yard.hotspots.push_back({"yard_gate_open", "OPEN GATE", {340, 94, 160, 166}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag("gate_open")}, {box(383, 134, 80, 5, green), label(397, 151, "OPEN", green)}});
    yard.hotspots.push_back({"relay_frame", "RELAY FRAME", {38, 82, 198, 178}, e2d::HotspotKind::scenery, {}, {}});
    yard.exits.push_back(exit(e2d::Direction::left, "cabin", {482, 232}));
    auto yardRight = exit(e2d::Direction::right, "generator", {8, 232});
    yardRight.availableWhen = {e2d::Condition::flag("gate_open")};
    yardRight.blockedMessage = "The locked yard gate blocks the way to the generator shed.";
    yard.exits.push_back(std::move(yardRight));
    world.addRoom(std::move(yard));

    // Generator ----------------------------------------------------------------
    e2d::RoomDefinition generator;
    generator.id = "generator";
    generator.label = "GENERATOR SHED";
    generator.background = {32, 35, 34, 255};
    generator.defaultSpawn = {28, 232};
    addGround(generator);
    generator.decorations.insert(generator.decorations.end(), {
        box(78, 112, 242, 132, {67, 75, 72, 255}), box(96, 130, 92, 74, metal),
        box(210, 134, 78, 62, {49, 56, 54, 255}), label(218, 150, "MAIN", amber),
        box(358, 152, 72, 92, wood), box(369, 166, 49, 7, pale),
        line(115, 202, 115, 238, blue), line(143, 202, 143, 238, blue),
        box(248, 208, 14, 30, rust), line(255, 208, 275, 193, rust)
    });
    generator.hotspots.push_back({"fuse_pickup", "CERAMIC FUSE", {340, 132, 105, 112}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("fuse_taken")}, {box(382, 181, 18, 6, pale)}});
    generator.hotspots.push_back({"fuse_socket", "FUSE SOCKET", {190, 114, 120, 104}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"cable_terminals", "BLUE TERMINALS", {88, 178, 92, 80}, e2d::HotspotKind::mechanism, {}, {}});
    generator.hotspots.push_back({"generator_lever", "MAIN LEVER", {228, 184, 70, 74}, e2d::HotspotKind::mechanism, {}, {}});
    generator.exits.push_back(exit(e2d::Direction::left, "yard", {482, 232}));
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
        line(230, 52, 176, 260, metal), line(230, 52, 286, 260, metal),
        line(194, 196, 267, 196, metal), line(205, 154, 256, 154, metal), line(216, 112, 245, 112, metal),
        box(392, 112, 22, 148, wood), line(392, 128, 414, 128, pale), line(392, 154, 414, 154, pale),
        line(392, 180, 414, 180, pale), line(392, 206, 414, 206, pale), line(392, 232, 414, 232, pale),
        label(361, 92, "LADDER", amber)
    });
    towerBase.hotspots.push_back({"tower_ladder", "LADDER", {360, 88, 88, 172}, e2d::HotspotKind::mechanism, {}, {}});
    towerBase.hotspots.push_back({"tower_structure", "TOWER", {148, 44, 164, 216}, e2d::HotspotKind::scenery, {}, {}});
    towerBase.exits.push_back(exit(e2d::Direction::left, "generator", {482, 232}));
    towerBase.exits.push_back(exit(e2d::Direction::right, "ravine", {8, 232}));
    world.addRoom(std::move(towerBase));

    // Tower top ----------------------------------------------------------------
    e2d::RoomDefinition towerTop;
    towerTop.id = "tower_top";
    towerTop.label = "TOWER PLATFORM";
    towerTop.background = {23, 44, 57, 255};
    towerTop.defaultSpawn = {50, 232};
    addGround(towerTop);
    towerTop.decorations.insert(towerTop.decorations.end(), {
        box(33, 220, 446, 40, metal),
        line(256, 32, 256, 220, pale), line(256, 54, 205, 96, pale), line(256, 54, 307, 96, pale),
        box(330, 158, 105, 62, {51, 63, 65, 255}), label(342, 178, "CONTROL", amber),
        box(80, 188, 54, 32, rust), line(92, 189, 118, 164, rust), label(70, 145, "ANTENNA", amber),
        box(454, 182, 18, 38, wood), line(454, 189, 472, 189, pale), line(454, 205, 472, 205, pale)
    });
    towerTop.hotspots.push_back({"antenna_mount", "ANTENNA MOUNT", {52, 136, 112, 102}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"relay_console", "RELAY CONSOLE", {306, 136, 148, 104}, e2d::HotspotKind::mechanism, {}, {}});
    towerTop.hotspots.push_back({"tower_down_ladder", "LADDER DOWN", {438, 166, 55, 82}, e2d::HotspotKind::mechanism, {}, {}});
    world.addRoom(std::move(towerTop));

    // Ravine -------------------------------------------------------------------
    e2d::RoomDefinition ravine;
    ravine.id = "ravine";
    ravine.label = "SERVICE RAVINE";
    ravine.background = {23, 41, 45, 255};
    ravine.defaultSpawn = {24, 232};
    addGround(ravine, 0, 166);
    addGround(ravine, 350, 162);
    ravine.decorations.insert(ravine.decorations.end(), {
        box(166, 260, 184, 28, {9, 14, 17, 255}),
        line(170, 253, 343, 230, wood), line(170, 246, 343, 223, wood),
        label(206, 205, "ROTTEN BRIDGE", red), box(416, 244, 15, 10, amber)
    });
    ravine.hotspots.push_back({"ravine_bridge", "ROTTEN BRIDGE", {142, 190, 230, 76}, e2d::HotspotKind::hazard, {}, {}});
    ravine.hotspots.push_back({"badge_pickup", "OLD BADGE", {382, 210, 86, 56}, e2d::HotspotKind::item,
        {e2d::Condition::notFlag("badge_taken")}, {box(416, 244, 15, 10, amber)}});
    ravine.hazards.push_back({"ravine_drop", {164, 232, 188, 56}, "The rotten bridge tears loose and the ravine wins.", {}});
    ravine.exits.push_back(exit(e2d::Direction::left, "tower_base", {482, 232}));
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
