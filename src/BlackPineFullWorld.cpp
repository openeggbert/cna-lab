#include "BlackPineWorld.hpp"

#include "BlackPineContent.hpp"

#include "explore2d/Renderer.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace black_pine {
namespace e2d = explore2d;
namespace {

using P = e2d::PaletteColor;
using content::Region;

constexpr P pale = P::white;
constexpr P amber = P::brightYellow;
constexpr P signalBlue = P::brightCyan;
constexpr P danger = P::brightRed;

e2d::LocalizedText tr(std::string english, std::string czech) {
    e2d::LocalizedText result{std::move(english)};
    result.addTranslation("cs", std::move(czech));
    return result;
}

e2d::Visual box(float x, float y, float width, float height, P color, bool filled = true) {
    return e2d::RectVisual{{x, y, width, height}, color, filled};
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

e2d::Visual label(float x, float y, e2d::LocalizedText text, P color = pale, int scale = 1) {
    return e2d::TextVisual{{x, y}, std::move(text), color, scale};
}

bool oneOf(const std::string_view value, const std::initializer_list<std::string_view> choices) {
    return std::ranges::find(choices, value) != choices.end();
}

std::vector<e2d::Visual> pickupVisuals(const std::string_view itemId, const float centerX) {
    constexpr float ground = 258.0F;
    std::vector<e2d::Visual> result{
        ellipse(centerX, ground, 15, 3, P::black),
    };

    if (oneOf(itemId, {"patch_cable", "climbing_rope", "siphon_hose", "drive_belt",
            "magnet_cord", "coolant_hose"})) {
        result.push_back(ellipse(centerX, 247, 13, 8, amber, false));
        result.push_back(ellipse(centerX, 247, 8, 5, P::brown, false));
        result.push_back(line(centerX + 11, 250, centerX + 18, 255, amber));
    } else if (oneOf(itemId, {"field_note", "site_map", "survey_notebook", "mine_map",
                   "punched_card", "calder_photo"})) {
        result.push_back(box(centerX - 12, 237, 24, 18, pale));
        result.push_back(line(centerX + 5, 237, centerX + 12, 244, P::lightGray));
        result.push_back(line(centerX - 8, 246, centerX + 7, 246, P::blue));
        result.push_back(line(centerX - 8, 250, centerX + 4, 250, P::blue));
    } else if (oneOf(itemId, {"brass_key", "quarry_office_key", "rail_switch_key", "dome_key",
                   "override_key", "transmitter_key"})) {
        result.push_back(circle(centerX - 8, 246, 6, amber, false));
        result.push_back(line(centerX - 2, 246, centerX + 14, 246, amber));
        result.push_back(line(centerX + 8, 246, centerX + 8, 252, amber));
        result.push_back(line(centerX + 13, 246, centerX + 13, 250, amber));
    } else if (oneOf(itemId, {"wrench", "pruning_saw", "iron_hook", "pulley_pin",
                   "spillway_crank", "valve_wheel", "calibration_fork", "grounding_clamp"})) {
        result.push_back(circle(centerX - 9, 250, 5, P::lightGray, false));
        result.push_back(line(centerX - 5, 247, centerX + 12, 236, pale));
        result.push_back(line(centerX + 10, 235, centerX + 15, 239, pale));
        result.push_back(line(centerX + 10, 235, centerX + 10, 242, P::darkGray));
    } else if (oneOf(itemId, {"hand_crank_torch", "multimeter", "mine_lamp", "compass", "hand_mirror"})) {
        result.push_back(box(centerX - 11, 235, 22, 21, P::darkGray));
        result.push_back(box(centerX - 8, 238, 16, 11, signalBlue));
        result.push_back(circle(centerX, 243, 4, pale, false));
        result.push_back(line(centerX - 6, 253, centerX + 6, 253, amber));
    } else if (oneOf(itemId, {"ceramic_fuse", "red_phase_coil", "dry_cell", "copper_bus_bar",
                   "lift_fuse", "phase_prism", "beacon_crystal"})) {
        result.push_back(box(centerX - 14, 240, 28, 12, P::red));
        result.push_back(box(centerX - 10, 237, 20, 18, danger, false));
        result.push_back(circle(centerX - 7, 246, 3, amber));
        result.push_back(circle(centerX + 7, 246, 3, signalBlue));
    } else if (oneOf(itemId, {"bandage_roll", "signal_flare", "charcoal", "filled_fuel_can",
                   "oil_can", "sealed_ration", "insulated_boots", "respirator", "filter_housing",
                   "first_aid_kit"})) {
        result.push_back(box(centerX - 13, 235, 26, 21, P::brown));
        result.push_back(box(centerX - 10, 238, 20, 15, P::red));
        result.push_back(line(centerX - 5, 245, centerX + 5, 245, pale));
        result.push_back(line(centerX, 240, centerX, 250, pale));
    } else if (oneOf(itemId, {"pine_bird", "relay_badge", "ranger_patch", "old_relay_badge",
                   "quartz_sample", "logger_token", "turbine_badge", "nightjar_patch"})) {
        result.push_back(e2d::PolygonVisual{{
            {centerX, 234}, {centerX + 12, 246}, {centerX, 256}, {centerX - 12, 246}},
            signalBlue, true});
        result.push_back(circle(centerX, 246, 5, amber));
    } else {
        result.push_back(box(centerX - 13, 238, 26, 17, P::brightMagenta));
        result.push_back(box(centerX - 10, 241, 20, 11, P::magenta));
        result.push_back(line(centerX - 8, 246, centerX + 8, 246, pale));
    }

    // A tiny SCREEN 9-style glint separates a collectible from scenery without
    // adding a modern marker or floating label.
    result.push_back(line(centerX + 17, 232, centerX + 17, 238, pale));
    result.push_back(line(centerX + 14, 235, centerX + 20, 235, pale));
    return result;
}

std::vector<e2d::Visual> emergencyPhoneVisuals(const bool ringing) {
    const P caseColor = ringing ? danger : P::red;
    return {
        ellipse(415, 258, 27, 4, P::black),
        box(408, 218, 14, 40, P::darkGray),
        box(389, 184, 52, 42, P::brown),
        box(393, 188, 44, 34, caseColor),
        box(397, 192, 36, 26, P::black),
        circle(415, 205, 8, P::lightGray, false),
        circle(415, 205, 2, amber),
        box(397, 185, 36, 7, amber),
        circle(399, 189, 5, amber),
        circle(431, 189, 5, amber),
        line(431, 192, 438, 202, P::black),
        line(438, 202, 438, 214, P::black),
    };
}

e2d::Message inspect(e2d::LocalizedText text) {
    return {std::move(text), e2d::MessageStyle::inspect};
}

e2d::Message speech(e2d::LocalizedText text, e2d::MessageSpeaker speaker = e2d::MessageSpeaker::target) {
    return {std::move(text), e2d::MessageStyle::speech, speaker};
}

e2d::Message warning(e2d::LocalizedText text) {
    return {std::move(text), e2d::MessageStyle::warning};
}

std::string screenPrefix(const int number) {
    std::string digits = std::to_string(number);
    digits.insert(digits.begin(), 3U - std::min<std::size_t>(3U, digits.size()), '0');
    return "s" + digits + "_";
}

const content::Screen& screen(const int number) {
    return content::screens.at(static_cast<std::size_t>(number - 1));
}

e2d::RoomDefinition& room(e2d::WorldDefinition& world, const int number) {
    return world.rooms.at(std::string{screen(number).id});
}

std::string targetId(const int number, const std::string_view name) {
    return screenPrefix(number) + std::string{name};
}

void addGround(e2d::RoomDefinition& result, const P color) {
    result.solids.push_back({0, 260, 492, 28});
    result.decorations.push_back(box(0, 260, 492, 28, color));
    result.decorations.push_back(line(0, 259, 492, 259, amber));
}

void addPine(e2d::RoomDefinition& result, const float x, const float base, const float height) {
    result.decorations.push_back(box(x - 3, base - height * 0.48F, 6, height * 0.48F, P::brown));
    result.decorations.push_back(e2d::PolygonVisual{{
        {x, base - height}, {x - height * 0.22F, base - height * 0.47F},
        {x + height * 0.22F, base - height * 0.47F}}, P::green, true});
    result.decorations.push_back(e2d::PolygonVisual{{
        {x, base - height * 0.82F}, {x - height * 0.27F, base - height * 0.29F},
        {x + height * 0.27F, base - height * 0.29F}}, P::brightGreen, true});
}

void addForestArt(e2d::RoomDefinition& result, const int seed, const bool severe) {
    result.background = severe ? P::blue : P::brightBlue;
    result.decorations.push_back(circle(410 - seed % 73, 38 + seed % 29, 15, amber));
    result.decorations.push_back(line(0, 173, 92, 88 + seed % 24, P::darkGray));
    result.decorations.push_back(line(92, 88 + seed % 24, 191, 173, P::darkGray));
    result.decorations.push_back(line(129, 173, 276, 64 + seed % 31, P::lightGray));
    result.decorations.push_back(line(276, 64 + seed % 31, 421, 173, P::lightGray));
    result.decorations.push_back(box(0, 172, 492, 88, P::green));
    addGround(result, P::brown);
    addPine(result, 35 + seed % 44, 260, 94 + seed % 52);
    addPine(result, 154 + seed % 57, 260, 78 + seed % 61);
    addPine(result, 391 + seed % 55, 260, 103 + seed % 47);
    result.decorations.push_back(line(14, 249, 470, 249 - seed % 9, P::brightYellow));
}

void addIndustrialArt(e2d::RoomDefinition& result, const int seed, const P wall, const P machine) {
    result.background = wall;
    result.decorations.push_back(box(0, 28, 492, 232, wall));
    for (int y = 64; y <= 208; y += 48) result.decorations.push_back(line(0, y, 492, y, P::darkGray));
    const float first = 42.0F + static_cast<float>(seed % 61);
    result.decorations.push_back(box(first, 100, 118, 135, machine));
    result.decorations.push_back(box(first + 9, 112, 100, 112, P::black));
    result.decorations.push_back(circle(first + 34, 153, 16, P::lightGray, false));
    result.decorations.push_back(line(first + 34, 153, first + 45, 143, danger));
    result.decorations.push_back(box(262, 85 + seed % 43, 151, 112, P::darkGray));
    result.decorations.push_back(box(272, 96 + seed % 43, 131, 90, P::black));
    result.decorations.push_back(circle(296, 121 + seed % 43, 5, danger));
    result.decorations.push_back(circle(319, 121 + seed % 43, 5, P::brightGreen));
    result.decorations.push_back(line(284, 153 + seed % 43, 385, 153 + seed % 43, signalBlue));
    addGround(result, P::brown);
}

void addWaterArt(e2d::RoomDefinition& result, const int seed) {
    result.background = P::brightBlue;
    result.decorations.push_back(line(0, 142, 103, 71 + seed % 22, P::darkGray));
    result.decorations.push_back(line(103, 71 + seed % 22, 218, 142, P::darkGray));
    result.decorations.push_back(box(0, 144, 492, 116, P::blue));
    for (int y = 157; y < 250; y += 18) {
        result.decorations.push_back(line(seed % 31, y, 182 + seed % 73, y, P::brightCyan));
        result.decorations.push_back(line(263 - seed % 29, y + 7, 476, y + 7, P::cyan));
    }
    result.decorations.push_back(box(38 + seed % 43, 93, 306, 32, P::lightGray));
    result.decorations.push_back(line(50, 125, 50, 260, P::darkGray));
    result.decorations.push_back(line(329, 125, 329, 260, P::darkGray));
    addGround(result, P::brown);
}

void addMineArt(e2d::RoomDefinition& result, const int seed) {
    result.background = P::black;
    result.decorations.push_back(e2d::PolygonVisual{{
        {0, 44}, {static_cast<float>(84 + seed % 39), 24}, {169, 59},
        {static_cast<float>(276 + seed % 27), 31},
        {492, 55}, {492, 260}, {0, 260}}, P::darkGray, true});
    result.decorations.push_back(box(59, 77, 13, 183, P::brown));
    result.decorations.push_back(box(411, 77, 13, 183, P::brown));
    result.decorations.push_back(line(65, 77, 417, 77, P::brown));
    result.decorations.push_back(line(65, 77, 116, 128, P::brown));
    result.decorations.push_back(line(417, 77, 366, 128, P::brown));
    result.decorations.push_back(line(108, 207, 376, 207, P::lightGray));
    result.decorations.push_back(line(122, 207, 122, 260, P::lightGray));
    result.decorations.push_back(line(355, 207, 355, 260, P::lightGray));
    result.decorations.push_back(circle(242 + seed % 18, 111, 9, amber));
    addGround(result, P::brown);
}

void addTowerArt(e2d::RoomDefinition& result, const int seed) {
    result.background = P::blue;
    result.decorations.push_back(circle(424, 43, 12, amber));
    result.decorations.push_back(line(40, 221, 111, 92 + seed % 23, P::darkGray));
    result.decorations.push_back(line(111, 92 + seed % 23, 183, 221, P::darkGray));
    result.decorations.push_back(line(286, 260, 360, 42, P::lightGray));
    result.decorations.push_back(line(434, 260, 360, 42, P::lightGray));
    for (int y = 82; y < 250; y += 32) {
        result.decorations.push_back(line(300 + (y - 82) / 6, y, 420 - (y - 82) / 6, y, P::lightGray));
    }
    result.decorations.push_back(line(286, 260, 434, 260, P::white));
    addGround(result, P::brown);
}

void replaceWithCabinInterior(e2d::RoomDefinition& result) {
    result.background = P::black;
    result.decorations.clear();
    result.decorations.insert(result.decorations.end(), {
        box(0, 0, 492, 260, P::brown),
        box(8, 10, 476, 210, P::red),
        box(18, 20, 456, 190, P::brown),
        line(18, 69, 474, 69, P::darkGray),
        line(18, 118, 474, 118, P::darkGray),
        line(18, 167, 474, 167, P::darkGray),
        box(0, 220, 492, 40, P::darkGray),
        line(0, 220, 492, 220, amber),

        // Front door back to the porch.
        box(8, 126, 52, 134, P::darkGray),
        box(13, 132, 42, 128, P::red),
        box(18, 138, 32, 116, P::brown),
        circle(44, 199, 3, amber),
        label(15, 145, tr("OUT", "VEN"), pale),

        // Mara's searchable writing desk.
        box(69, 174, 111, 16, P::darkGray),
        box(73, 165, 103, 17, P::brown),
        box(78, 190, 9, 50, P::brown),
        box(163, 190, 9, 50, P::brown),
        box(92, 153, 53, 11, P::white),
        line(97, 157, 137, 157, P::blue),
        label(101, 197, tr("DESK", "STŮL"), amber),

        // Rainy window, stove and radio shelf establish a real interior.
        box(204, 82, 92, 62, P::darkGray),
        box(210, 88, 80, 50, P::blue),
        line(250, 88, 250, 138, P::brightCyan),
        line(210, 113, 290, 113, P::brightCyan),
        line(220, 91, 214, 106, pale),
        line(270, 93, 263, 109, pale),
        box(359, 145, 71, 75, P::darkGray),
        box(366, 153, 57, 60, P::black),
        circle(394, 183, 18, P::red),
        circle(394, 183, 10, danger),
        line(393, 145, 393, 100, P::darkGray),
        box(334, 72, 119, 13, P::brown),
        box(345, 51, 28, 21, P::lightGray),
        line(379, 61, 437, 61, signalBlue),
    });
}

void replaceWithRadioNookInterior(e2d::RoomDefinition& result) {
    result.background = P::black;
    result.decorations.clear();
    result.decorations.insert(result.decorations.end(), {
        box(0, 0, 492, 260, P::brown),
        box(10, 12, 472, 208, P::darkGray),
        box(20, 22, 452, 188, P::brown),
        box(0, 220, 492, 40, P::darkGray),
        line(0, 220, 492, 220, amber),

        box(8, 126, 54, 134, P::darkGray),
        box(14, 133, 42, 121, P::brown),
        circle(48, 199, 3, amber),
        label(16, 145, tr("BACK", "ZPĚT"), pale),

        box(105, 80, 330, 135, P::lightGray),
        box(114, 89, 312, 116, P::black),
        circle(155, 147, 31, P::lightGray, false),
        circle(155, 147, 21, P::darkGray, false),
        box(206, 108, 173, 58, P::blue),
        line(214, 145, 236, 126, signalBlue),
        line(236, 126, 259, 149, P::brightGreen),
        line(259, 149, 283, 122, signalBlue),
        line(283, 122, 311, 145, danger),
        line(311, 145, 370, 145, signalBlue),
        circle(226, 186, 6, danger),
        circle(254, 186, 6, amber),
        circle(282, 186, 6, P::brightGreen),
        box(343, 176, 65, 19, P::brown),
        label(127, 96, tr("BLACK PINE RADIO", "RÁDIO BLACK PINE"), amber),
    });
}

void replaceWithToolShedInterior(e2d::RoomDefinition& result) {
    result.background = P::black;
    result.decorations.clear();
    result.decorations.insert(result.decorations.end(), {
        box(0, 0, 492, 260, P::brown),
        box(10, 10, 472, 210, P::darkGray),
        box(18, 18, 456, 194, P::brown),
        box(0, 220, 492, 40, P::darkGray),
        line(0, 220, 492, 220, amber),

        box(18, 91, 48, 129, P::red),
        box(24, 98, 36, 116, P::brown),
        label(25, 108, tr("CABIN", "CHATA"), pale),
        circle(53, 166, 3, amber),

        box(86, 52, 290, 112, P::black),
        box(94, 60, 274, 96, P::brown),
        line(111, 71, 111, 139, P::lightGray),
        circle(111, 71, 10, P::lightGray, false),
        line(158, 69, 208, 142, amber),
        line(158, 142, 208, 69, amber),
        line(250, 72, 250, 139, P::lightGray),
        line(250, 89, 287, 72, P::lightGray),
        box(313, 85, 39, 31, P::red, false),
        line(313, 135, 352, 135, signalBlue),
        box(79, 164, 306, 16, P::darkGray),
        box(91, 180, 12, 40, P::brown),
        box(361, 180, 12, 40, P::brown),

        e2d::PolygonVisual{{{397, 104}, {451, 104}, {466, 118}, {451, 132}, {397, 132}}, amber, true},
        label(404, 113, tr("MAST", "STOŽÁR"), P::black),
    });
}

void replaceWithRootCellarInterior(e2d::RoomDefinition& result) {
    result.background = P::black;
    result.decorations.clear();
    result.decorations.insert(result.decorations.end(), {
        box(0, 0, 492, 260, P::darkGray),
        box(8, 8, 476, 212, P::black),
        line(8, 54, 484, 54, P::lightGray),
        line(8, 102, 484, 102, P::lightGray),
        line(8, 150, 484, 150, P::lightGray),
        line(8, 198, 484, 198, P::lightGray),
        line(82, 8, 82, 220, P::lightGray),
        line(172, 8, 172, 220, P::lightGray),
        line(268, 8, 268, 220, P::lightGray),
        line(380, 8, 380, 220, P::lightGray),
        box(0, 220, 492, 40, P::brown),
        line(0, 220, 492, 220, amber),

        line(14, 210, 79, 115, P::brown),
        line(35, 210, 100, 115, P::brown),
        line(29, 188, 49, 188, P::lightGray),
        line(41, 170, 61, 170, P::lightGray),
        line(53, 152, 73, 152, P::lightGray),
        line(65, 134, 85, 134, P::lightGray),
        label(18, 108, tr("UP", "NAHORU"), pale),

        box(194, 139, 112, 81, P::brown),
        box(202, 147, 96, 65, P::red, false),
        line(202, 179, 298, 179, P::red),
        label(215, 158, tr("NIGHTJAR", "NIGHTJAR"), danger),

        box(375, 188, 98, 32, P::darkGray),
        box(381, 193, 86, 21, P::brown),
        line(392, 202, 456, 202, signalBlue),
        label(390, 174, tr("SERVICE HATCH", "SERVISNÍ POKLOP"), amber),
    });
}

enum class Motif {
    trailSign,
    gate,
    bridge,
    cabin,
    radio,
    tools,
    mast,
    cable,
    generator,
    control,
    person,
    forestClue,
    bear,
    rope,
    tunnel,
    crusher,
    hoist,
    rail,
    waterworks,
    pump,
    mine,
    lift,
    camera,
    archive,
    dome,
    laboratory,
    capacitor,
    tower,
    finalConsole,
};

Motif motifFor(const int number) {
    switch (number) {
    case 1: case 2: case 3: case 12: case 25: case 36: case 51: return Motif::trailSign;
    case 4: case 13: case 17: case 21: case 23: case 34: case 49: case 88: case 116: case 118: return Motif::cable;
    case 5: case 28: case 40: case 62: case 66: return Motif::bridge;
    case 6: case 7: case 31: case 46: case 57: case 58: case 93: case 94: case 95: return Motif::cabin;
    case 8: case 29: case 45: case 63: return Motif::radio;
    case 9: case 10: case 22: case 26: case 32: case 48: case 54: case 65: case 76: case 83: case 109: return Motif::tools;
    case 11: case 15: case 16: case 37: case 38: return Motif::mast;
    case 14: case 44: case 89: case 103: case 104: case 105: case 114: case 115: return Motif::gate;
    case 18: case 19: case 20: return Motif::generator;
    case 24: case 59: case 67: case 68: case 69: case 86: case 87: case 101: case 102: case 112: return Motif::control;
    case 27: case 30: case 33: case 35: return number == 35 ? Motif::bear : Motif::forestClue;
    case 39: case 41: case 43: case 117: return Motif::rope;
    case 42: case 72: case 75: case 77: case 78: case 79: case 80: case 81: case 82: return Motif::mine;
    case 47: return Motif::crusher;
    case 50: return Motif::hoist;
    case 52: case 53: case 55: case 56: case 60: case 61: return Motif::rail;
    case 64: case 71: case 73: case 74: return Motif::waterworks;
    case 70: return Motif::pump;
    case 84: case 85: case 90: return Motif::lift;
    case 91: case 92: return Motif::camera;
    case 96: case 97: return Motif::archive;
    case 98: case 106: case 107: case 108: case 111: case 113: return Motif::laboratory;
    case 99: case 100: return Motif::dome;
    case 110: return Motif::capacitor;
    case 119: case 120: case 121: case 122: case 123: return Motif::tower;
    case 124: return Motif::finalConsole;
    default: return Motif::person;
    }
}

void addStoryLandmark(e2d::RoomDefinition& result, const int number) {
    const Motif motif = motifFor(number);
    const float x = 182.0F + static_cast<float>((number * 7) % 37);
    switch (motif) {
    case Motif::trailSign:
        result.decorations.insert(result.decorations.end(), {
            box(x, 128, 123, 72, P::brown), box(x + 7, 135, 109, 56, P::black),
            line(x + 61, 200, x + 61, 260, P::brown),
            label(x + 17, 145, tr("BLACK PINE", "BLACK PINE"), amber),
            line(x + 18, 174, x + 94, 174, signalBlue), circle(x + 35, 174, 3, danger),
        });
        break;
    case Motif::gate:
        result.decorations.insert(result.decorations.end(), {
            box(x - 28, 92, 10, 168, P::red), box(x + 136, 92, 10, 168, P::red),
            line(x - 23, 104, x + 141, 242, P::lightGray), line(x + 141, 104, x - 23, 242, P::lightGray),
            line(x - 23, 104, x + 141, 104, P::lightGray), line(x - 23, 242, x + 141, 242, P::lightGray),
            box(x + 48, 166, 24, 31, amber), circle(x + 60, 176, 3, P::black),
        });
        break;
    case Motif::bridge:
        result.decorations.insert(result.decorations.end(), {
            line(78, 210, 414, 210, P::brown), line(78, 224, 414, 224, P::brown),
            line(91, 210, 91, 260, P::lightGray), line(401, 210, 401, 260, P::lightGray),
            line(91, 210, 160, 224, P::lightGray), line(160, 224, 230, 210, P::lightGray),
            line(262, 224, 332, 210, P::lightGray), line(332, 210, 401, 224, P::lightGray),
            line(230, 210, 262, 230, danger),
        });
        break;
    case Motif::cabin:
        result.decorations.insert(result.decorations.end(), {
            box(x - 63, 126, 204, 134, P::red),
            e2d::PolygonVisual{{{x - 81, 126}, {x + 38, 65}, {x + 159, 126}}, P::brown, true},
            box(x - 41, 157, 58, 45, P::brightBlue), line(x - 12, 157, x - 12, 202, pale),
            box(x + 58, 165, 49, 95, P::brown), circle(x + 96, 210, 3, amber),
            line(x - 63, 151, x + 141, 151, amber),
        });
        break;
    case Motif::radio:
        result.decorations.insert(result.decorations.end(), {
            box(x - 42, 112, 198, 126, P::lightGray), box(x - 31, 123, 176, 103, P::black),
            circle(x + 1, 173, 26, P::lightGray, false), circle(x + 1, 173, 18, P::darkGray, false),
            line(x + 45, 157, x + 122, 157, P::brightGreen),
            line(x + 45, 171, x + 103, 171, signalBlue),
            line(x + 45, 185, x + 132, 185, danger), circle(x + 123, 208, 6, amber),
        });
        break;
    case Motif::tools:
        result.decorations.insert(result.decorations.end(), {
            box(x - 72, 101, 234, 139, P::brown), box(x - 63, 111, 216, 119, P::black),
            line(x - 36, 126, x - 36, 209, P::lightGray), circle(x - 36, 126, 12, P::lightGray, false),
            line(x + 14, 129, x + 66, 205, amber), line(x + 14, 205, x + 66, 129, amber),
            box(x + 83, 146, 47, 31, P::red, false), line(x + 83, 193, x + 130, 193, signalBlue),
        });
        break;
    case Motif::mast:
        result.decorations.insert(result.decorations.end(), {
            line(x + 32, 45, x - 41, 260, P::lightGray), line(x + 32, 45, x + 107, 260, P::lightGray),
            line(x - 25, 211, x + 90, 211, P::lightGray), line(x - 9, 164, x + 74, 164, P::lightGray),
            line(x + 7, 117, x + 57, 117, P::lightGray), line(x + 32, 45, x + 32, 25, pale),
            line(x + 4, 34, x + 60, 34, pale), circle(x + 32, 25, 5, danger),
        });
        break;
    case Motif::cable:
        result.decorations.insert(result.decorations.end(), {
            box(x - 71, 105, 230, 130, P::darkGray), box(x - 61, 116, 210, 108, P::black),
            circle(x - 25, 169, 15, signalBlue, false), circle(x + 111, 169, 15, signalBlue, false),
            line(x - 10, 169, x + 41, 145, signalBlue), line(x + 41, 145, x + 96, 169, signalBlue),
            line(x + 18, 200, x + 71, 200, danger), circle(x + 44, 200, 5, amber),
        });
        break;
    case Motif::generator:
        result.decorations.insert(result.decorations.end(), {
            box(x - 85, 104, 254, 139, P::lightGray), box(x - 74, 115, 232, 117, P::black),
            circle(x - 28, 173, 32, P::lightGray, false), line(x - 28, 173, x - 8, 151, danger),
            box(x + 27, 137, 104, 66, P::darkGray), circle(x + 53, 168, 7, danger),
            circle(x + 104, 168, 7, P::brightGreen), line(x + 54, 213, x + 128, 213, amber),
        });
        break;
    case Motif::control:
        result.decorations.insert(result.decorations.end(), {
            box(x - 88, 93, 266, 151, P::lightGray), box(x - 77, 104, 244, 129, P::black),
            box(x - 60, 121, 94, 65, P::blue),
            line(x - 50, 170, x - 31, 143, signalBlue), line(x - 31, 143, x - 7, 161, P::brightGreen),
            line(x - 7, 161, x + 23, 134, signalBlue),
            circle(x + 72, 140, 6, danger), circle(x + 98, 140, 6, amber),
            circle(x + 124, 140, 6, P::brightGreen),
            box(x + 62, 165, 77, 44, P::darkGray, false),
        });
        break;
    case Motif::person:
        result.decorations.insert(result.decorations.end(), {
            circle(x + 40, 123, 15, amber), box(x + 26, 140, 29, 64, P::brightMagenta),
            line(x + 26, 157, x - 1, 183, amber), line(x + 55, 157, x + 81, 183, amber),
            line(x + 33, 204, x + 19, 250, P::lightGray), line(x + 48, 204, x + 63, 250, P::lightGray),
        });
        break;
    case Motif::forestClue:
        result.decorations.insert(result.decorations.end(), {
            line(61, 226, 421, 151, P::brown), line(61, 239, 421, 164, P::brown),
            circle(x - 24, 215, 10, P::darkGray, false), circle(x + 8, 207, 10, P::darkGray, false),
            line(x + 63, 198, x + 96, 174, danger), line(x + 96, 174, x + 118, 199, danger),
        });
        break;
    case Motif::bear:
        result.decorations.insert(result.decorations.end(), {
            ellipse(x + 26, 190, 67, 39, P::black), circle(x + 81, 163, 31, P::black),
            circle(x + 67, 135, 11, P::black), circle(x + 93, 135, 11, P::black),
            circle(x + 91, 160, 3, amber), line(x - 13, 217, x - 13, 251, P::black),
            line(x + 51, 217, x + 51, 251, P::black),
        });
        break;
    case Motif::rope:
        result.decorations.insert(result.decorations.end(), {
            circle(x - 52, 108, 12, P::lightGray, false),
            e2d::PolylineVisual{{{x - 42, 110}, {x + 4, 139}, {x + 29, 186}, {x + 91, 235}}, amber, false},
            line(x - 87, 239, x - 8, 161, P::darkGray), line(x + 108, 239, x + 34, 161, P::darkGray),
            line(x - 87, 239, x + 108, 239, P::black),
        });
        break;
    case Motif::tunnel:
    case Motif::mine:
        result.decorations.insert(result.decorations.end(), {
            e2d::ArcVisual{{x + 26, 221}, {105, 128}, 3.14159F, 6.28318F, P::brown},
            box(x - 79, 150, 14, 110, P::brown), box(x + 117, 150, 14, 110, P::brown),
            line(x - 72, 151, x + 124, 151, P::brown),
            line(x - 36, 247, x + 85, 247, P::lightGray), circle(x + 24, 185, 12, amber),
        });
        break;
    case Motif::crusher:
        result.decorations.insert(result.decorations.end(), {
            box(x - 86, 91, 256, 156, P::red), box(x - 73, 104, 230, 130, P::black),
            e2d::PolygonVisual{{{x - 41, 118}, {x + 114, 118}, {x + 87, 166}, {x - 16, 166}}, P::lightGray, true},
            e2d::PolygonVisual{{{x - 22, 224}, {x + 95, 224}, {x + 75, 180}, {x - 1, 180}}, P::lightGray, true},
            line(x - 62, 212, x + 142, 212, danger),
        });
        break;
    case Motif::hoist:
        result.decorations.insert(result.decorations.end(), {
            circle(x + 23, 115, 43, P::lightGray, false), circle(x + 23, 115, 11, amber),
            line(x + 23, 158, x + 23, 238, P::lightGray),
            box(x - 20, 187, 86, 61, P::darkGray, false),
            line(x - 89, 229, x + 147, 163, amber), line(x - 89, 237, x + 147, 171, amber),
        });
        break;
    case Motif::rail:
        result.decorations.insert(result.decorations.end(), {
            box(x - 91, 147, 247, 72, P::red), box(x - 58, 109, 106, 61, P::darkGray),
            box(x - 47, 120, 84, 39, P::black), circle(x - 48, 226, 27, P::black),
            circle(x + 111, 226, 27, P::black), circle(x - 48, 226, 14, P::lightGray),
            circle(x + 111, 226, 14, P::lightGray), line(x + 156, 161, x + 183, 161, amber),
        });
        break;
    case Motif::waterworks:
        result.decorations.insert(result.decorations.end(), {
            box(x - 91, 98, 250, 135, P::lightGray),
            line(x - 73, 107, x - 73, 233, P::darkGray), line(x - 23, 107, x - 23, 233, P::darkGray),
            line(x + 27, 107, x + 27, 233, P::darkGray), line(x + 77, 107, x + 77, 233, P::darkGray),
            line(x + 127, 107, x + 127, 233, P::darkGray),
            line(x - 91, 241, x + 159, 241, signalBlue), circle(x + 17, 150, 23, danger, false),
        });
        break;
    case Motif::pump:
        result.decorations.insert(result.decorations.end(), {
            circle(x + 17, 174, 64, P::lightGray, false), circle(x + 17, 174, 21, amber, false),
            line(x - 47, 174, x - 91, 174, signalBlue), line(x + 81, 174, x + 142, 174, signalBlue),
            box(x - 9, 99, 52, 29, P::darkGray), circle(x + 17, 113, 6, P::brightGreen),
        });
        break;
    case Motif::lift:
        result.decorations.insert(result.decorations.end(), {
            box(x - 77, 67, 226, 193, P::lightGray, false), box(x - 60, 84, 192, 176, P::darkGray),
            line(x + 36, 84, x + 36, 260, P::lightGray),
            line(x - 60, 84, x + 132, 260, P::lightGray), line(x + 132, 84, x - 60, 260, P::lightGray),
            box(x - 27, 43, 127, 23, P::black), circle(x + 72, 54, 6, P::brightGreen),
        });
        break;
    case Motif::camera:
        result.decorations.insert(result.decorations.end(), {
            box(x - 69, 103, 179, 109, P::darkGray), box(x - 54, 118, 149, 79, P::black),
            circle(x + 21, 157, 32, P::lightGray, false), circle(x + 21, 157, 13, danger),
            line(x + 76, 103, x + 136, 70, P::lightGray), line(x + 21, 190, x + 21, 241, signalBlue),
        });
        break;
    case Motif::archive:
        result.decorations.insert(result.decorations.end(), {
            box(x - 97, 91, 275, 153, P::brown),
            box(x - 85, 103, 61, 57, P::darkGray), box(x - 12, 103, 61, 57, P::darkGray),
            box(x + 61, 103, 61, 57, P::darkGray), box(x + 134, 103, 31, 129, P::darkGray),
            circle(x - 55, 196, 23, P::lightGray, false), circle(x + 17, 196, 23, P::lightGray, false),
            line(x - 32, 196, x - 6, 196, signalBlue),
        });
        break;
    case Motif::dome:
        result.decorations.insert(result.decorations.end(), {
            e2d::ArcVisual{{x + 20, 213}, {118, 118}, 3.14159F, 6.28318F, P::lightGray},
            line(x + 20, 95, x + 20, 213, P::black), line(x - 98, 213, x + 138, 213, P::lightGray),
            line(x + 20, 151, x + 116, 112, P::lightGray), circle(x + 116, 112, 17, signalBlue, false),
            box(x - 61, 214, 162, 32, P::darkGray),
        });
        break;
    case Motif::laboratory:
        result.decorations.insert(result.decorations.end(), {
            box(x - 91, 102, 245, 135, P::lightGray), box(x - 80, 113, 223, 113, P::black),
            circle(x - 32, 168, 36, danger, false), circle(x + 25, 168, 36, signalBlue, false),
            circle(x + 82, 168, 36, P::brightMagenta, false),
            line(x - 68, 214, x + 129, 123, P::brightGreen),
        });
        break;
    case Motif::capacitor:
        result.decorations.insert(result.decorations.end(), {
            box(x - 91, 92, 67, 146, P::darkGray), box(x - 3, 92, 67, 146, P::darkGray),
            box(x + 85, 92, 67, 146, P::darkGray),
            circle(x - 58, 117, 9, danger), circle(x + 30, 117, 9, amber), circle(x + 118, 117, 9, P::brightGreen),
            line(x - 58, 141, x - 2, 177, signalBlue), line(x + 30, 141, x + 84, 177, signalBlue),
            line(x + 118, 141, x + 54, 204, signalBlue),
        });
        break;
    case Motif::tower:
        result.decorations.insert(result.decorations.end(), {
            line(x + 20, 49, x - 71, 260, pale), line(x + 20, 49, x + 111, 260, pale),
            line(x - 57, 226, x + 97, 226, P::lightGray), line(x - 42, 190, x + 82, 190, P::lightGray),
            line(x - 27, 154, x + 67, 154, P::lightGray), line(x - 12, 118, x + 52, 118, P::lightGray),
            line(x + 20, 49, x + 20, 27, amber), circle(x + 20, 25, 7, P::brightGreen),
        });
        break;
    case Motif::finalConsole:
        result.decorations.insert(result.decorations.end(), {
            box(77, 69, 338, 180, P::darkGray), box(90, 82, 312, 154, P::black),
            box(108, 101, 171, 73, P::blue),
            line(118, 151, 144, 130, signalBlue), line(144, 130, 169, 158, P::brightGreen),
            line(169, 158, 195, 116, signalBlue), line(195, 116, 221, 149, amber),
            line(221, 149, 267, 121, signalBlue),
            circle(310, 113, 8, danger), circle(338, 113, 8, amber), circle(366, 113, 8, P::brightGreen),
            box(301, 146, 78, 51, P::darkGray, false), label(316, 163, tr("4-1-3", "4-1-3"), amber),
        });
        break;
    }
}

void addStoryAnimation(e2d::RoomDefinition& result, const content::Screen& spec) {
    const Motif motif = motifFor(spec.number);
    const float x = 182.0F + static_cast<float>((spec.number * 7) % 37);
    std::vector<e2d::AnimationFrame> frames;
    switch (motif) {
    case Motif::trailSign:
        frames = {
            {8, {line(x + 4, 114, x + 1, 125, signalBlue)}},
            {8, {line(x + 4, 118, x + 1, 129, signalBlue)}},
        };
        break;
    case Motif::gate:
        frames = {
            {10, {line(x + 31, 133, x + 73, 151, P::lightGray)}},
            {10, {line(x + 31, 136, x + 73, 148, P::lightGray)}},
        };
        break;
    case Motif::bridge:
        frames = {
            {7, {line(103, 244, 215, 244, signalBlue), line(281, 251, 397, 251, P::brightCyan)}},
            {7, {line(86, 251, 198, 251, P::brightCyan), line(298, 244, 414, 244, signalBlue)}},
        };
        break;
    case Motif::cabin:
        frames = {
            {12, {circle(x + 116, 87, 5, P::lightGray, false), circle(x + 121, 76, 3, P::darkGray, false)}},
            {12, {circle(x + 120, 82, 6, P::lightGray, false), circle(x + 127, 69, 3, P::darkGray, false)}},
        };
        break;
    case Motif::radio:
    case Motif::control:
    case Motif::laboratory:
    case Motif::finalConsole:
        frames = {
            {6, {line(x - 35, 74, x - 8, 66, signalBlue), line(x - 8, 66, x + 18, 80, P::brightGreen),
                line(x + 18, 80, x + 47, 61, signalBlue)}},
            {6, {line(x - 35, 74, x - 8, 82, P::brightGreen), line(x - 8, 82, x + 18, 62, signalBlue),
                line(x + 18, 62, x + 47, 76, amber)}},
        };
        break;
    case Motif::mast:
    case Motif::tower:
        frames = {
            {9, {circle(x + 20, 25, 7, danger), line(x - 3, 25, x - 18, 25, danger)}},
            {9, {circle(x + 20, 25, 7, amber), line(x + 43, 25, x + 58, 25, amber)}},
        };
        break;
    case Motif::cable:
    case Motif::capacitor:
        frames = {
            {5, {e2d::PolylineVisual{{{x - 12, 82}, {x + 3, 69}, {x + 13, 85}, {x + 29, 66}}, signalBlue, false}}},
            {8, {e2d::PolylineVisual{{{x - 7, 72}, {x + 6, 86}, {x + 18, 68}, {x + 33, 81}}, pale, false}}},
        };
        break;
    case Motif::generator:
    case Motif::hoist:
    case Motif::pump:
    case Motif::lift:
        frames = {
            {7, {circle(x + 22, 72, 13, P::lightGray, false), line(x + 22, 59, x + 22, 85, amber)}},
            {7, {circle(x + 22, 72, 13, P::lightGray, false), line(x + 9, 72, x + 35, 72, amber)}},
        };
        break;
    case Motif::person:
        frames = {
            {13, {circle(x + 40, 123, 15, amber)}},
            {13, {circle(x + 40, 121, 15, amber)}},
        };
        break;
    case Motif::bear:
        frames = {
            {12, {circle(x + 91, 160, 3, amber), line(x + 102, 170, x + 119, 174, P::brightRed)}},
            {12, {circle(x + 91, 160, 3, amber), line(x + 102, 174, x + 119, 170, P::brightRed)}},
        };
        break;
    case Motif::rope:
        frames = {
            {10, {e2d::PolylineVisual{{{x - 42, 110}, {x + 1, 141}, {x + 31, 184}, {x + 91, 235}}, amber, false}}},
            {10, {e2d::PolylineVisual{{{x - 42, 110}, {x + 8, 136}, {x + 25, 190}, {x + 91, 235}}, amber, false}}},
        };
        break;
    case Motif::tunnel:
    case Motif::mine:
        frames = {
            {11, {circle(x + 24, 185, 10, amber), circle(x + 24, 185, 15, P::brown, false)}},
            {11, {circle(x + 24, 185, 13, P::brightYellow), circle(x + 24, 185, 18, P::brown, false)}},
        };
        break;
    case Motif::crusher:
        frames = {
            {6, {line(x - 55, 212, x + 125, 212, danger), line(x - 35, 207, x - 18, 217, amber)}},
            {6, {line(x - 55, 212, x + 125, 212, danger), line(x + 62, 207, x + 79, 217, amber)}},
        };
        break;
    case Motif::rail:
        frames = {
            {8, {circle(x - 48, 226, 14, P::lightGray), line(x - 62, 226, x - 34, 226, P::black)}},
            {8, {circle(x - 48, 226, 14, P::lightGray), line(x - 48, 212, x - 48, 240, P::black)}},
        };
        break;
    case Motif::waterworks:
        frames = {
            {7, {line(25, 248, 172, 248, P::brightCyan), line(251, 242, 445, 242, signalBlue)}},
            {7, {line(51, 242, 206, 242, signalBlue), line(278, 248, 471, 248, P::brightCyan)}},
        };
        break;
    case Motif::camera:
        frames = {
            {8, {line(x + 21, 190, x - 31, 245, signalBlue)}},
            {8, {line(x + 21, 190, x + 79, 245, signalBlue)}},
        };
        break;
    case Motif::archive:
        frames = {
            {9, {circle(x - 55, 196, 22, P::lightGray, false), line(x - 55, 174, x - 55, 218, amber)}},
            {9, {circle(x - 55, 196, 22, P::lightGray, false), line(x - 77, 196, x - 33, 196, amber)}},
        };
        break;
    case Motif::dome:
        frames = {
            {12, {line(x + 20, 95, x + 20, 213, signalBlue)}},
            {12, {line(x - 5, 98, x + 20, 213, signalBlue)}},
        };
        break;
    case Motif::tools:
    case Motif::forestClue:
        return;
    }
    result.animations.push_back({targetId(spec.number, "story_motion"), true, true, {}, std::move(frames)});
}

void addRegionArtwork(e2d::RoomDefinition& result, const content::Screen& spec) {
    switch (spec.region) {
    case Region::caretaker:
    case Region::forest: addForestArt(result, spec.number, spec.number == 4); break;
    case Region::relay: addIndustrialArt(result, spec.number, P::darkGray, P::lightGray); break;
    case Region::quarry: addIndustrialArt(result, spec.number, P::brown, P::red); break;
    case Region::railway: addIndustrialArt(result, spec.number, P::brown, P::lightGray); break;
    case Region::reservoir: addWaterArt(result, spec.number); break;
    case Region::mine: addMineArt(result, spec.number); break;
    case Region::observatory: addIndustrialArt(result, spec.number, P::lightGray, P::blue); break;
    case Region::bunker: addIndustrialArt(result, spec.number, P::darkGray, P::red); break;
    case Region::summit:
        if (spec.number == 124) addIndustrialArt(result, spec.number, P::blue, P::darkGray);
        else addTowerArt(result, spec.number);
        break;
    }

    addStoryLandmark(result, spec.number);
    addStoryAnimation(result, spec);
}

void configureInterface(e2d::InterfaceTextDefinition& ui) {
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
    ui.resumePrompt = tr("ENTER TO RETURN TO THE LAST SAFE PLACE",
        "ENTER PRO NÁVRAT NA POSLEDNÍ BEZPEČNÉ MÍSTO");
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
    ui.nothingUsable = tr("You are not carrying anything usable.", "Neneseš nic použitelného.");
    ui.nothingToExamine = tr("There is nothing here that catches your eye.", "Není tu nic k prozkoumání.");
    ui.nothingToTake = tr("There is nothing within reach to take.", "Na dosah není nic k sebrání.");
    ui.cannotTake = tr("You cannot take that.", "To nemůžeš sebrat.");
    ui.doesNotWork = tr("That does not seem to work here.", "Tady to zřejmě nefunguje.");
    ui.noticeNothing = tr("You notice nothing unusual.", "Nic neobvyklého.");
    ui.noTravelDestinations = tr("No travel destinations have been discovered yet.", "Zatím nebyl objeven žádný cíl cesty.");
    ui.gameSaved = tr("Game saved.", "Hra byla uložena.");
    ui.saveFailed = tr("Save failed.", "Uložení selhalo.");
    ui.loadFailed = tr("Load failed.", "Načtení selhalo.");
    ui.loadWorldMismatch = tr("Load failed: save does not match this world.", "Načtení selhalo: uložená hra patří jinému světu.");
    ui.gameLoaded = tr("Game loaded.", "Hra byla načtena.");
    ui.fellBeyondEdge = tr("You fell beyond the edge of the screen.", "Pád za okraj obrazovky byl smrtelný.");
}

struct ItemSpec final {
    std::string_view id;
    std::string_view english;
    std::string_view czech;
    std::string_view englishDescription;
    std::string_view czechDescription;
    bool usable{true};
};

constexpr std::array items{
    ItemSpec{"patch_cable", "PATCH CABLE", "PROPOJOVACÍ KABEL", "Weatherproof copper cable.", "Měděný kabel odolný proti počasí."},
    ItemSpec{"field_note", "FIELD NOTE", "SERVISNÍ POZNÁMKA", "Fuse, terminals, lever—in that order.", "Pojistka, svorky, páka—v tomto pořadí.", false},
    ItemSpec{"pine_bird", "CARVED PINE BIRD", "VYŘEZÁVANÝ PTÁČEK", "A small trailhead keepsake.", "Drobná památka z výchoziště.", false},
    ItemSpec{"brass_key", "BRASS YARD KEY", "MOSAZNÝ KLÍČ OD AREÁLU", "A reusable old master key.", "Starý opakovaně použitelný hlavní klíč."},
    ItemSpec{"site_map", "MARA'S SITE MAP", "MAŘINA MAPA AREÁLU", "A map annotated with relay routes.", "Mapa s poznámkami o trasách převaděče.", false},
    ItemSpec{"wrench", "17 MM WRENCH", "KLÍČ 17 MM", "A trusted field wrench.", "Spolehlivý montážní klíč."},
    ItemSpec{"lineman_gloves", "LINEMAN GLOVES", "ELEKTRIKÁŘSKÉ RUKAVICE", "Insulated gloves for live equipment.", "Izolované rukavice pro živá zařízení."},
    ItemSpec{"pruning_saw", "PRUNING SAW", "PROŘEZÁVACÍ PILA", "A compact folding saw.", "Kompaktní skládací pila."},
    ItemSpec{"ceramic_fuse", "CERAMIC FUSE", "KERAMICKÁ POJISTKA", "A sound 30 amp main fuse.", "Nepoškozená hlavní pojistka 30 A."},
    ItemSpec{"hand_crank_torch", "HAND-CRANK TORCH", "RUČNÍ SVÍTILNA", "A lamp and small hand generator.", "Svítilna s malým ručním generátorem."},
    ItemSpec{"multimeter", "MULTIMETER", "MULTIMETR", "Calder's analogue test meter.", "Calderové analogový měřicí přístroj."},
    ItemSpec{"relay_badge", "RELAY BADGE", "ODZNAK PŘEVADĚČE", "An enamel Black Pine badge.", "Smaltovaný odznak Black Pine.", false},
    ItemSpec{"bandage_roll", "BANDAGE ROLL", "OBVAZ", "A sealed ranger bandage.", "Uzavřený obvaz strážců."},
    ItemSpec{"signal_flare", "SIGNAL FLARE", "SIGNÁLNÍ SVĚTLICE", "A bright non-lethal deterrent.", "Jasný neškodný odstrašující prostředek."},
    ItemSpec{"climbing_rope", "CLIMBING ROPE", "HOROLEZECKÉ LANO", "Theo's dry climbing rope.", "Theovo suché horolezecké lano."},
    ItemSpec{"iron_hook", "IRON HOOK", "ŽELEZNÝ HÁK", "A hook sized for service anchors.", "Hák vhodný pro servisní kotvy."},
    ItemSpec{"mine_lamp", "MINE LAMP", "DŮLNÍ LAMPA", "A rugged safety lamp.", "Odolná bezpečnostní lampa."},
    ItemSpec{"compass", "RANGER COMPASS", "KOMPAS STRÁŽCŮ", "A liquid-damped compass.", "Kapalinou tlumený kompas."},
    ItemSpec{"charcoal", "HARDWOOD CHARCOAL", "DŘEVĚNÉ UHLÍ", "Clean filter-grade charcoal.", "Čisté uhlí vhodné do filtru."},
    ItemSpec{"quarry_office_key", "QUARRY OFFICE KEY", "KLÍČ OD KANCELÁŘE LOMU", "A rusted quarry key.", "Rezavý klíč od lomu."},
    ItemSpec{"pulley_pin", "HOIST PULLEY PIN", "ČEP NAVIJÁKU", "A machined hoist pin.", "Obrobený čep navijáku."},
    ItemSpec{"red_phase_coil", "RED PHASE COIL", "ČERVENÁ FÁZOVÁ CÍVKA", "A stolen Nightjar phase component.", "Ukradená fázová součást Nightjaru."},
    ItemSpec{"survey_notebook", "SURVEY NOTEBOOK", "PRŮZKUMNICKÝ ZÁPISNÍK", "Voss's false survey records.", "Vossovy falešné průzkumnické záznamy.", false},
    ItemSpec{"siphon_hose", "SIPHON HOSE", "PŘEČERPÁVACÍ HADICE", "A fuel-safe service hose.", "Servisní hadice odolná palivu."},
    ItemSpec{"filled_fuel_can", "FILLED FUEL CAN", "PLNÝ KANYSTR", "Protected reserve fuel for the logging engine.", "Palivo z chráněné zásoby pro lesní lokomotivu."},
    ItemSpec{"quartz_sample", "BLUE QUARTZ", "MODRÝ KŘEMEN", "An optional mountain keepsake.", "Volitelná horská památka.", false},
    ItemSpec{"drive_belt", "DRIVE BELT", "HNACÍ ŘEMEN", "A serviceable planer belt.", "Použitelný řemen z hoblovky."},
    ItemSpec{"oil_can", "OIL CAN", "OLEJNIČKA", "Heavy machine oil.", "Hustý strojní olej."},
    ItemSpec{"hand_mirror", "HAND MIRROR", "RUČNÍ ZRCÁTKO", "Useful for reversed writing and cameras.", "Užitečné pro obrácené písmo a kamery."},
    ItemSpec{"spark_plug", "SPARK PLUG", "ZAPALOVACÍ SVÍČKA", "A dry engine plug.", "Suchá motorová svíčka."},
    ItemSpec{"rail_switch_key", "RAIL SWITCH KEY", "KLÍČ OD VÝHYBKY", "The sawmill switch key.", "Klíč k výhybce u pily."},
    ItemSpec{"logger_token", "LOGGER TOKEN", "DŘEVAŘSKÝ ŽETON", "An optional camp token.", "Volitelný žeton z tábora.", false},
    ItemSpec{"sealed_ration", "SEALED RATION", "UZAVŘENÁ DÁVKA", "June's emergency ration.", "Junina nouzová dávka."},
    ItemSpec{"insulated_boots", "INSULATED BOOTS", "IZOLAČNÍ BOTY", "Rubber boots for wet electrical work.", "Gumové boty pro práci ve vodě."},
    ItemSpec{"turbine_badge", "TURBINE BADGE", "ODZNAK TURBÍNY", "Jonah's dam access badge.", "Jonahův přístupový odznak přehrady."},
    ItemSpec{"spillway_crank", "SPILLWAY HAND CRANK", "RUČNÍ KLIKA PŘELIVU", "The removable emergency crank for the gatehouse.", "Odnimatelná nouzová klika domku stavidel."},
    ItemSpec{"pump_gasket", "PUMP GASKET", "TĚSNĚNÍ ČERPADLA", "A fresh emergency-pump gasket.", "Nové těsnění nouzového čerpadla."},
    ItemSpec{"dry_cell", "DRY-CELL BATTERY", "SUCHÝ ČLÁNEK", "A charged pump starter cell.", "Nabitý článek startéru čerpadla."},
    ItemSpec{"magnet_cord", "MAGNET ON CORD", "MAGNET NA ŠŇŮŘE", "A retrieval magnet on strong cord.", "Vytahovací magnet na pevné šňůře."},
    ItemSpec{"valve_wheel", "VALVE WHEEL", "VENTILOVÉ KOLO", "A detachable bypass wheel.", "Odnímatelné kolo obtoku."},
    ItemSpec{"respirator", "RESPIRATOR", "RESPIRÁTOR", "A mask body awaiting filter charcoal.", "Tělo masky čekající na filtrační uhlí."},
    ItemSpec{"filter_housing", "EMPTY FILTER HOUSING", "PRÁZDNÉ POUZDRO FILTRU", "A respirator cartridge ready for clean charcoal.", "Respirátorová vložka připravená na čisté uhlí."},
    ItemSpec{"copper_bus_bar", "COPPER BUS BAR", "MĚDĚNÁ PŘÍPOJNICE", "A heavy cut copper link.", "Těžká měděná spojnice."},
    ItemSpec{"lift_fuse", "LIFT FUSE", "POJISTKA VÝTAHU", "A sealed freight-lift fuse.", "Utěsněná pojistka nákladního výtahu."},
    ItemSpec{"mine_map", "MINE MAP", "DŮLNÍ MAPA", "A marked underground route map.", "Označená mapa podzemních tras.", false},
    ItemSpec{"research_badge", "KLINE'S RESEARCH BADGE", "KLINEOVÉ VÝZKUMNÝ ODZNAK", "Emergency Nightjar access.", "Nouzový přístup do Nightjaru."},
    ItemSpec{"punched_card", "PUNCHED CODE CARD", "DĚRNÝ KÓDOVÝ ŠTÍTEK", "A reversible emergency code card.", "Oboustranný nouzový kódový štítek."},
    ItemSpec{"nightjar_patch", "NIGHTJAR PATCH", "NÁŠIVKA NIGHTJAR", "An optional cloth insignia.", "Volitelná látková nášivka.", false},
    ItemSpec{"ranger_patch", "RANGER SERVICE PATCH", "SLUŽEBNÍ NÁŠIVKA STRÁŽCŮ", "Theo's optional service patch.", "Theova volitelná služební nášivka.", false},
    ItemSpec{"old_relay_badge", "SILTED RELAY BADGE", "STARÝ ODZNAK Z NÁNOSU", "A 1964 predecessor to Mara's enamel badge.", "Předchůdce Mařina smaltovaného odznaku z roku 1964.", false},
    ItemSpec{"first_aid_kit", "FIRST-AID KIT", "LÉKÁRNIČKA", "A complete observatory kit.", "Úplná lékárnička observatoře."},
    ItemSpec{"cipher_lens", "CIPHER LENS", "ŠIFROVACÍ ČOČKA", "A coloured Nightjar decoding lens.", "Barevná dekódovací čočka Nightjaru."},
    ItemSpec{"archive_reel", "ARCHIVE REEL", "ARCHIVNÍ KOTOUČ", "Calder's magnetic project archive.", "Calderové magnetický projektový archiv."},
    ItemSpec{"phase_prism", "PHASE PRISM", "FÁZOVÝ HRANOL", "The second stolen field component.", "Druhá ukradená součást pole."},
    ItemSpec{"calibration_fork", "CALIBRATION FORK", "KALIBRAČNÍ LADIČKA", "A precisely tuned Nightjar fork.", "Přesně naladěná ladička Nightjaru."},
    ItemSpec{"alignment_chart", "ANTENNA ALIGNMENT CHART", "PLÁN SEŘÍZENÍ ANTÉNY", "Nell's three-landmark azimuth record.", "Nellin azimutový záznam tří orientačních bodů.", false},
    ItemSpec{"dome_key", "INSTRUMENT DOME KEY", "KLÍČ OD PŘÍSTROJOVÉ KOPULE", "The security office dome key.", "Klíč od kopule z bezpečnostní kanceláře."},
    ItemSpec{"coolant_hose", "COOLANT HOSE", "CHLADICÍ HADICE", "A pressure-rated replacement hose.", "Náhradní tlaková hadice."},
    ItemSpec{"grounding_clamp", "GROUNDING CLAMP", "ZEMNICÍ SVORKA", "A heavy high-current clamp.", "Těžká silnoproudá svorka."},
    ItemSpec{"calder_photo", "RUTH CALDER PHOTOGRAPH", "FOTOGRAFIE RUTH CALDEROVÉ", "Calder beside the first protected-carrier rig.", "Calderová u první soupravy chráněné nosné vlny.", false},
    ItemSpec{"evidence_spool", "EVIDENCE SPOOL", "DŮKAZNÍ KOTOUČ", "A copy of Nightjar records and Voss's admission.", "Kopie záznamů Nightjaru a Vossova přiznání."},
    ItemSpec{"override_key", "EMERGENCY OVERRIDE KEY", "NOUZOVÝ OVLÁDACÍ KLÍČ", "Kline's local-control key.", "Klineové klíč místního ovládání."},
    ItemSpec{"beacon_crystal", "BEACON CRYSTAL", "KRYSTAL MAJÁKU", "The removable reference crystal from the summit beacon.", "Odnimatelný referenční krystal z vrcholového majáku."},
    ItemSpec{"transmitter_key", "TRANSMITTER KEY", "KLÍČ VYSÍLAČE", "Voss's summit transmitter key.", "Vossův klíč vrcholového vysílače."},
};

void addItems(e2d::WorldDefinition& world) {
    for (const ItemSpec& item : items) {
        world.addItem({std::string{item.id}, tr(std::string{item.english}, std::string{item.czech}),
            tr(std::string{item.englishDescription}, std::string{item.czechDescription}), item.usable});
    }
}

void addPresentation(e2d::WorldDefinition& world) {
    world.localization.defaultLanguage = "en";
    world.localization.languages = {{"en", tr("English", "Angličtina")}, {"cs", tr("Czech", "Čeština")}};
    world.title = tr("Black Pine: The Long Silence", "Black Pine: Dlouhé ticho");
    world.startRoom = std::string{content::screens.front().id};
    configureInterface(world.presentation.interfaceText);
    world.presentation.inventoryHeading = tr("YOU CARRY", "NESEŠ");
    world.presentation.creditLine = tr("A BLACK PINE STORY", "PŘÍBĚH Z BLACK PINE");
    world.presentation.title.subtitle = tr("THE LONG SILENCE", "DLOUHÉ TICHO");
    world.presentation.title.byline = tr("AN EXPLORE2D ADVENTURE", "ADVENTURA V EXPLORE2D");
    world.presentation.title.startLabel = tr("NEW GAME", "NOVÁ HRA");
    world.presentation.title.loadLabel = tr("LOAD GAME", "NAČÍST HRU");
    world.presentation.title.settingsLabel = tr("SETTINGS", "NASTAVENÍ");
    world.presentation.title.quitLabel = tr("QUIT", "KONEC");
    world.presentation.title.titleColors = {P::brightGreen, signalBlue, amber, P::brightMagenta};
    world.presentation.title.artwork = {
        box(18, 80, 604, 151, P::blue), circle(548, 108, 19, amber),
        line(18, 214, 138, 116, P::darkGray), line(138, 116, 254, 214, P::darkGray),
        line(173, 214, 312, 91, pale), line(312, 91, 452, 214, pale),
        box(18, 214, 604, 17, P::green), line(473, 93, 443, 214, P::lightGray),
        line(473, 93, 505, 214, P::lightGray), line(452, 159, 493, 159, P::lightGray),
        line(460, 132, 486, 132, P::lightGray), circle(473, 93, 4, danger),
        label(32, 218, tr("ONE CLEAR VOICE AGAINST THE STORM", "JEDEN ČISTÝ HLAS PROTI BOUŘI"), amber),
    };

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
    world.presentation.sounds = {"title", "menu", "confirm", "talk", "pickup", "jump", "warning", "death", "victory", "save", "load"};
}

void addScreens(e2d::WorldDefinition& world) {
    for (std::size_t index = 0; index < content::screens.size(); ++index) {
        const content::Screen& spec = content::screens[index];
        e2d::RoomDefinition result;
        result.id = std::string{spec.id};
        result.label = tr(std::string{spec.englishTitle}, std::string{spec.czechTitle});
        result.travelAnchor = spec.travelAnchor;
        result.travelLabel = result.label;
        result.defaultSpawn = {24, 232};
        addRegionArtwork(result, spec);

        const std::string sceneTarget = targetId(spec.number, "story");
        result.hotspots.push_back({sceneTarget, result.label,
            {327, 84, 157, 166}, e2d::HotspotKind::scenery, {}, {
                box(436 - static_cast<float>(spec.number % 51), 205, 18, 10, signalBlue, false)}});

        if (index > 0) {
            result.exits.push_back({e2d::Direction::left, std::string{content::screens[index - 1].id}, {462, 232}, {}, {}});
        }
        if (index + 1 < content::screens.size()) {
            result.exits.push_back({e2d::Direction::right, std::string{content::screens[index + 1].id}, {8, 232}, {}, {}});
        }
        world.addRoom(std::move(result));

        const std::string observedFlag = "observed_" + std::string{spec.id};
        world.addInteraction({e2d::Verb::examine, sceneTarget, std::nullopt,
            {e2d::Condition::notFlag(observedFlag)},
            {inspect(tr(std::string{spec.englishStory}, std::string{spec.czechStory}))},
            {e2d::Mutation::setFlag(observedFlag)}, 10, "once_" + observedFlag});
        world.addInteraction({e2d::Verb::examine, sceneTarget, std::nullopt,
            {e2d::Condition::flag(observedFlag)},
            {inspect(tr("Iris has already recorded this location and its useful details in her field notes.",
                "Iris už toto místo i jeho užitečné podrobnosti zaznamenala do terénních poznámek."))},
            {}, 0, {}});
    }
}

e2d::HotspotDefinition& ensureHotspot(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view name,
    e2d::LocalizedText hotspotLabel,
    const e2d::Rect area,
    const e2d::HotspotKind kind,
    const int slot = 0)
{
    e2d::RoomDefinition& current = room(world, screenNumber);
    const std::string id = targetId(screenNumber, name);
    const auto existing = std::ranges::find_if(current.hotspots,
        [&id](const e2d::HotspotDefinition& hotspot) { return hotspot.id == id; });
    if (existing != current.hotspots.end()) return *existing;
    const P accent = slot % 3 == 0 ? amber : (slot % 3 == 1 ? signalBlue : danger);
    current.hotspots.push_back({id, std::move(hotspotLabel), area, kind, {}, {
        box(area.x + area.width * 0.35F, area.y + area.height * 0.42F,
            std::max(8.0F, area.width * 0.28F), std::max(6.0F, area.height * 0.12F), accent, false),
    }});
    return current.hotspots.back();
}

void addPickup(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view itemId,
    std::string englishMessage,
    std::string czechMessage,
    const int slot,
    std::vector<e2d::Condition> extraConditions = {})
{
    const std::string flag = "taken_" + std::string{itemId};
    extraConditions.push_back(e2d::Condition::notFlag(flag));
    const float x = 34.0F + static_cast<float>(slot % 5) * 86.0F;
    auto& hotspot = ensureHotspot(world, screenNumber, std::string{"take_"} + std::string{itemId},
        world.item(itemId)->label, {x, 183, 76, 77}, e2d::HotspotKind::item, slot);
    hotspot.visibleWhen = extraConditions;
    hotspot.visuals = pickupVisuals(itemId, x + 38.0F);
    world.addInteraction({e2d::Verb::take, hotspot.id, std::nullopt, extraConditions,
        {inspect(tr(std::move(englishMessage), std::move(czechMessage)))},
        {e2d::Mutation::addItem(std::string{itemId}), e2d::Mutation::setFlag(flag)},
        20, "once_" + flag});
}

void addUse(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view targetName,
    std::string englishLabel,
    std::string czechLabel,
    const std::string_view itemId,
    const std::string_view resultFlag,
    std::string englishMessage,
    std::string czechMessage,
    std::vector<e2d::Condition> conditions = {},
    const bool consume = false,
    const int slot = 0)
{
    conditions.push_back(e2d::Condition::notFlag(std::string{resultFlag}));
    const float x = 63.0F + static_cast<float>(slot % 4) * 101.0F;
    auto& hotspot = ensureHotspot(world, screenNumber, targetName,
        tr(std::move(englishLabel), std::move(czechLabel)), {x, 135, 96, 125},
        e2d::HotspotKind::mechanism, slot);
    const std::string hotspotId = hotspot.id;
    std::vector<e2d::Mutation> mutations{e2d::Mutation::setFlag(std::string{resultFlag})};
    if (consume) mutations.push_back(e2d::Mutation::removeItem(std::string{itemId}));
    room(world, screenNumber).hotspots.push_back({hotspotId + "_complete",
        tr("COMPLETED REPAIR", "DOKONČENÁ OPRAVA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag(std::string{resultFlag})}, {
            box(x + 30, 174, 34, 12, P::brightGreen), circle(x + 47, 180, 4, pale),
        }});
    world.addInteraction({e2d::Verb::use, hotspotId, std::string{itemId}, std::move(conditions),
        {inspect(tr(std::move(englishMessage), std::move(czechMessage)))},
        std::move(mutations), 30, {}, "repair"});
}

void addContext(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view targetName,
    std::string englishLabel,
    std::string czechLabel,
    const std::string_view resultFlag,
    std::vector<e2d::Message> messages,
    std::vector<e2d::Condition> conditions = {},
    std::vector<e2d::Mutation> extraMutations = {},
    const int slot = 0,
    std::string sound = "talk")
{
    conditions.push_back(e2d::Condition::notFlag(std::string{resultFlag}));
    const float x = 72.0F + static_cast<float>(slot % 4) * 99.0F;
    auto& hotspot = ensureHotspot(world, screenNumber, targetName,
        tr(std::move(englishLabel), std::move(czechLabel)), {x, 137, 92, 123},
        e2d::HotspotKind::mechanism, slot);
    const std::string hotspotId = hotspot.id;
    std::vector<e2d::Mutation> mutations{e2d::Mutation::setFlag(std::string{resultFlag})};
    mutations.insert(mutations.end(), std::make_move_iterator(extraMutations.begin()),
        std::make_move_iterator(extraMutations.end()));
    room(world, screenNumber).hotspots.push_back({hotspotId + "_complete",
        tr("ACTIVE STATE", "AKTIVNÍ STAV"), {0, 0, 0, 0}, e2d::HotspotKind::scenery,
        {e2d::Condition::flag(std::string{resultFlag})}, {
            circle(x + 46, 178, 7, P::brightGreen), circle(x + 46, 178, 3, pale),
        }});
    world.addInteraction({e2d::Verb::context, hotspotId, std::nullopt, std::move(conditions),
        std::move(messages), std::move(mutations), 30, {}, std::move(sound)});
}

void gateRight(
    e2d::WorldDefinition& world,
    const int screenNumber,
    std::vector<e2d::Condition> conditions,
    std::string englishMessage,
    std::string czechMessage)
{
    auto& exits = room(world, screenNumber).exits;
    const auto found = std::ranges::find_if(exits,
        [](const e2d::ExitDefinition& candidate) { return candidate.direction == e2d::Direction::right; });
    if (found == exits.end()) return;
    found->availableWhen = std::move(conditions);
    found->blockedMessage = tr(std::move(englishMessage), std::move(czechMessage));
}

void gateLeft(
    e2d::WorldDefinition& world,
    const int screenNumber,
    std::vector<e2d::Condition> conditions,
    std::string englishMessage,
    std::string czechMessage)
{
    auto& exits = room(world, screenNumber).exits;
    const auto found = std::ranges::find_if(exits,
        [](const e2d::ExitDefinition& candidate) { return candidate.direction == e2d::Direction::left; });
    if (found == exits.end()) return;
    found->availableWhen = std::move(conditions);
    found->blockedMessage = tr(std::move(englishMessage), std::move(czechMessage));
}

void setHorizontalRoute(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::optional<int> leftScreen,
    const std::optional<int> rightScreen)
{
    auto& exits = room(world, screenNumber).exits;
    exits.clear();
    if (leftScreen.has_value()) {
        exits.push_back({e2d::Direction::left, std::string{screen(*leftScreen).id}, {462, 232}, {}, {}});
    }
    if (rightScreen.has_value()) {
        exits.push_back({e2d::Direction::right, std::string{screen(*rightScreen).id}, {8, 232}, {}, {}});
    }
}

e2d::HotspotDefinition& addPortal(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view name,
    std::string englishLabel,
    std::string czechLabel,
    const e2d::Rect area,
    const int destinationScreen,
    std::vector<e2d::Visual> visuals,
    std::vector<e2d::Condition> conditions = {})
{
    auto& hotspot = ensureHotspot(world, screenNumber, name,
        tr(std::move(englishLabel), std::move(czechLabel)), area,
        e2d::HotspotKind::mechanism);
    hotspot.visuals = std::move(visuals);
    world.addInteraction({e2d::Verb::context, hotspot.id, std::nullopt,
        std::move(conditions), {},
        {e2d::Mutation::moveTo(std::string{screen(destinationScreen).id})},
        20, {}, "climb"});
    return hotspot;
}

void addCharacter(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view name,
    std::string englishLabel,
    std::string czechLabel,
    const std::string_view resultFlag,
    std::vector<e2d::Message> messages,
    std::vector<e2d::Condition> conditions = {},
    std::vector<e2d::Mutation> extraMutations = {})
{
    conditions.push_back(e2d::Condition::notFlag(std::string{resultFlag}));
    auto& hotspot = ensureHotspot(world, screenNumber, name,
        tr(std::move(englishLabel), std::move(czechLabel)), {205, 145, 85, 115},
        e2d::HotspotKind::character, 2);
    hotspot.visuals.push_back(circle(247, 171, 10, amber));
    hotspot.visuals.push_back(box(237, 182, 20, 48, P::brightMagenta));
    std::vector<e2d::Mutation> mutations{e2d::Mutation::setFlag(std::string{resultFlag})};
    mutations.insert(mutations.end(), std::make_move_iterator(extraMutations.begin()),
        std::make_move_iterator(extraMutations.end()));
    world.addInteraction({e2d::Verb::context, hotspot.id, std::nullopt, std::move(conditions),
        std::move(messages), std::move(mutations), 40, {}, "talk"});
}

void addFollowUpDialogue(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view characterName,
    const std::string_view onceFlag,
    std::vector<e2d::Message> messages,
    std::vector<e2d::Condition> conditions)
{
    conditions.push_back(e2d::Condition::notFlag(std::string{onceFlag}));
    world.addInteraction({e2d::Verb::context, targetId(screenNumber, characterName), std::nullopt,
        std::move(conditions), std::move(messages), {e2d::Mutation::setFlag(std::string{onceFlag})},
        60, {}, "talk"});
}

void addHazard(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view name,
    const std::string_view safeFlag,
    std::string englishDeath,
    std::string czechDeath)
{
    room(world, screenNumber).hazards.push_back({targetId(screenNumber, name), {360, 200, 92, 60},
        tr(std::move(englishDeath), std::move(czechDeath)),
        {e2d::Condition::notFlag(std::string{safeFlag})}});
}

void addHint(
    e2d::WorldDefinition& world,
    const std::string_view unfinishedFlag,
    std::string english,
    std::string czech,
    const int priority,
    std::vector<e2d::Condition> prerequisites = {})
{
    prerequisites.push_back(e2d::Condition::notFlag(std::string{unfinishedFlag}));
    world.hints.push_back({tr(std::move(english), std::move(czech)), std::move(prerequisites), priority});
}

void addActOne(e2d::WorldDefinition& world) {
    addContext(world, 1, "emergency_phone", "STORM EMERGENCY PHONE", "BOUŘKOVÝ NOUZOVÝ TELEFON",
        "mission_started", {
            speech(tr("Mara: Iris, the relay failed at 02:17. Kestrel Six is down beyond the ridge with an injured child aboard.",
                "Mara: Iris, převaděč selhal ve 02:17. Kestrel Six je za hřebenem na zemi a na palubě má zraněné dítě.")),
            speech(tr("Iris: I am at the storm gate. Ordinary radio is only carrying a timed pulse.",
                "Iris: Jsem u bouřkové brány. Běžné rádio přenáší jen pravidelný pulz."), e2d::MessageSpeaker::player),
            speech(tr("Mara: Reach the cabin. Bring any dry repair supplies you find. We need one clear channel before the beacon dies.",
                "Mara: Dojdi k chatě. Vezmi všechno suché vybavení k opravě, které najdeš. Potřebujeme jeden čistý kanál, než maják zhasne.")),
        }, {}, {}, 3, "warning");
    auto& emergencyPhone = ensureHotspot(world, 1, "emergency_phone",
        tr("STORM EMERGENCY PHONE", "BOUŘKOVÝ NOUZOVÝ TELEFON"),
        {369, 137, 92, 123}, e2d::HotspotKind::mechanism, 3);
    emergencyPhone.visuals = emergencyPhoneVisuals(true);
    auto& answeredPhone = room(world, 1).hotspots.back();
    answeredPhone.visuals = emergencyPhoneVisuals(false);
    room(world, 1).animations.push_back({"emergency_phone_ring", true, true,
        {e2d::Condition::notFlag("mission_started")}, {
            {4, {circle(415, 205, 12, amber, false)}},
            {4, {circle(415, 205, 15, pale, false)}},
        }});
    gateRight(world, 1, {e2d::Condition::flag("mission_started")},
        "The emergency phone is pulsing. Iris must answer before leaving the trailhead.",
        "Nouzový telefon pulzuje. Iris ho musí před odchodem z výchoziště zvednout.");
    addPickup(world, 1, "patch_cable", "You coil the weatherproof cable from the damaged toolbox.",
        "Z poškozené skříňky smotáš kabel odolný proti počasí.", 0);
    addPickup(world, 1, "field_note", "The folded note gives the generator repair order.",
        "Složená poznámka uvádí pořadí opravy generátoru.", 1);
    addPickup(world, 2, "pine_bird", "You save a tiny carved bird from the wet noticeboard.",
        "Z mokré nástěnky zachráníš malého vyřezávaného ptáčka.", 0);
    addContext(world, 3, "deer_path", "DEER-PATH DETOUR", "OBJÍŽĎKA PO JELENÍ STEZCE",
        "deer_path_taken", {inspect(tr("Iris follows the narrow deer path through Pine Hollow and leaves the arcing feeder above her.",
            "Iris sleduje úzkou jelení stezku přes Borový úvoz a nechá jiskřící vedení nad sebou."))},
        {e2d::Condition::flag("mission_started")},
        {e2d::Mutation::moveTo(std::string{screen(5).id})}, 2, "climb");
    auto& deerPath = ensureHotspot(world, 3, "deer_path",
        tr("MARKED DEER PATH TO CABIN", "OZNAČENÁ JELENÍ STEZKA K CHATĚ"),
        {270, 137, 92, 123}, e2d::HotspotKind::mechanism, 2);
    const std::vector<e2d::Visual> deerPathVisuals{
        ellipse(316, 258, 36, 4, P::black),
        box(311, 211, 8, 47, P::brown),
        e2d::PolygonVisual{{{278, 211}, {334, 211}, {350, 224}, {334, 237}, {278, 237}}, amber, true},
        box(282, 215, 49, 18, P::brightYellow),
        label(291, 221, tr("CABIN", "CHATA"), P::black),
        line(273, 251, 281, 247, pale),
        line(287, 256, 295, 252, pale),
    };
    deerPath.visuals = deerPathVisuals;
    room(world, 3).hotspots.back().visuals = deerPathVisuals;
    world.addInteraction({e2d::Verb::context, deerPath.id, std::nullopt,
        {e2d::Condition::flag("deer_path_taken")},
        {inspect(tr("Iris takes the marked deer path around the live feeder.",
            "Iris obejde živý přívod po označené jelení stezce."))},
        {e2d::Mutation::moveTo(std::string{screen(5).id})}, 10, {}, "climb"});
    gateRight(world, 3, {e2d::Condition::flag("feeder_isolated")},
        "Blue arcs block the direct switchback. Use the yellow CABIN sign with ENTER to take the safe deer path.",
        "Modré výboje blokují přímou serpentinu. U žluté šipky CHATA stiskni ENTER a použij bezpečnou jelení stezku.");
    gateLeft(world, 5, {e2d::Condition::flag("feeder_isolated")},
        "The direct switchback is still live. Use the yellow TRAIL sign with ENTER to return safely.",
        "Přímá serpentina je stále pod proudem. Pro bezpečný návrat použij klávesou ENTER žlutou šipku STEZKA.");
    auto& deerPathReturn = ensureHotspot(world, 5, "deer_path_return",
        tr("MARKED DEER PATH BACK", "OZNAČENÁ JELENÍ STEZKA ZPĚT"),
        {0, 137, 92, 123}, e2d::HotspotKind::mechanism, 0);
    deerPathReturn.visuals = {
        ellipse(46, 258, 36, 4, P::black),
        box(42, 211, 8, 47, P::brown),
        e2d::PolygonVisual{{{8, 224}, {24, 211}, {80, 211}, {80, 237}, {24, 237}}, amber, true},
        box(28, 215, 47, 18, P::brightYellow),
        label(34, 221, tr("TRAIL", "STEZKA"), P::black),
    };
    world.addInteraction({e2d::Verb::context, deerPathReturn.id, std::nullopt,
        {e2d::Condition::notFlag("feeder_isolated")},
        {inspect(tr("Iris follows the deer path back below the live feeder.",
            "Iris se vrátí po jelení stezce pod živým přívodem."))},
        {e2d::Mutation::moveTo(std::string{screen(3).id})}, 10, {}, "climb"});

    // The caretaker area is a small hub in the design, not a run of six
    // unrelated left/right screens. Keep its outdoor paths directional and
    // use visible doors and hatches for the indoor branches.
    setHorizontalRoute(world, 6, 5, 9);
    setHorizontalRoute(world, 7, std::nullopt, std::nullopt);
    setHorizontalRoute(world, 8, 7, std::nullopt);
    setHorizontalRoute(world, 9, 6, 11);
    setHorizontalRoute(world, 10, std::nullopt, std::nullopt);
    setHorizontalRoute(world, 11, 9, 12);
    replaceWithRadioNookInterior(room(world, 8));
    replaceWithToolShedInterior(room(world, 9));
    replaceWithRootCellarInterior(room(world, 10));

    auto& cabinDoor = ensureHotspot(world, 6, "cabin_door",
        tr("CARETAKER CABIN DOOR", "DVEŘE SPRÁVCOVSKÉ CHATY"),
        {235, 145, 80, 115}, e2d::HotspotKind::mechanism, 1);
    cabinDoor.visuals = {
        box(245, 163, 49, 97, amber, false),
        box(249, 167, 41, 89, P::brown, false),
        circle(284, 210, 4, amber),
        label(251, 150, tr("ENTER", "VSTUP"), pale),
    };
    world.addInteraction({e2d::Verb::context, cabinDoor.id, std::nullopt,
        {e2d::Condition::notFlag("cabin_entered")},
        {inspect(tr("Iris opens the caretaker's door and steps into the warm cabin. Mara waits beside the desk.",
            "Iris otevře dveře a vstoupí do teplé správcovské chaty. Mara čeká vedle stolu."))},
        {e2d::Mutation::setFlag("cabin_entered"),
            e2d::Mutation::moveTo(std::string{screen(7).id})}, 30, {}, "unlock"});
    world.addInteraction({e2d::Verb::context, cabinDoor.id, std::nullopt,
        {e2d::Condition::flag("cabin_entered")}, {},
        {e2d::Mutation::moveTo(std::string{screen(7).id})}, 20, {}, "climb"});
    gateRight(world, 6, {e2d::Condition::flag("cabin_entered")},
        "The path continues through the cabin. Stand at the highlighted door and press ENTER.",
        "Cesta pokračuje skrz chatu. Postav se ke zvýrazněným dveřím a stiskni ENTER.");

    replaceWithCabinInterior(room(world, 7));
    auto& insideDoor = ensureHotspot(world, 7, "cabin_exit",
        tr("FRONT DOOR TO PORCH", "VCHODOVÉ DVEŘE NA VERANDU"),
        {0, 132, 67, 128}, e2d::HotspotKind::mechanism, 0);
    insideDoor.visuals = {
        box(12, 131, 44, 129, amber, false),
        circle(44, 199, 3, amber),
    };
    world.addInteraction({e2d::Verb::context, insideDoor.id, std::nullopt, {}, {},
        {e2d::Mutation::moveTo(std::string{screen(6).id})}, 20, {}, "climb"});

    auto& radioDoor = ensureHotspot(world, 7, "radio_door",
        tr("DOOR TO RADIO NOOK", "DVEŘE DO RÁDIOVÉHO KOUTU"),
        {424, 116, 68, 144}, e2d::HotspotKind::mechanism, 1);
    radioDoor.visuals = {
        box(438, 121, 46, 139, P::darkGray),
        box(444, 128, 34, 126, P::brown),
        circle(450, 198, 3, amber),
        label(441, 143, tr("RADIO", "RÁDIO"), pale),
    };
    world.addInteraction({e2d::Verb::context, radioDoor.id, std::nullopt,
        {e2d::Condition::flag("met_mara")}, {},
        {e2d::Mutation::moveTo(std::string{screen(8).id})}, 20, {}, "climb"});

    auto& cellarHatch = ensureHotspot(world, 7, "cellar_hatch",
        tr("ROOT CELLAR HATCH", "POKLOP DO SKLEPA"),
        {280, 208, 89, 52}, e2d::HotspotKind::mechanism, 2);
    cellarHatch.visuals = {
        box(286, 230, 73, 27, P::black),
        box(292, 234, 61, 19, P::brown),
        line(300, 243, 345, 243, amber),
        label(299, 218, tr("CELLAR", "SKLEP"), amber),
    };
    world.addInteraction({e2d::Verb::context, cellarHatch.id, std::nullopt,
        {e2d::Condition::flag("met_mara")}, {},
        {e2d::Mutation::moveTo(std::string{screen(10).id})}, 20, {}, "climb"});

    auto& radioReturn = ensureHotspot(world, 8, "cabin_return",
        tr("DOOR BACK TO CABIN", "DVEŘE ZPĚT DO CHATY"),
        {0, 126, 70, 134}, e2d::HotspotKind::mechanism, 0);
    radioReturn.visuals = {
        box(12, 131, 46, 129, amber, false),
        circle(48, 199, 3, amber),
    };
    world.addInteraction({e2d::Verb::context, radioReturn.id, std::nullopt, {}, {},
        {e2d::Mutation::moveTo(std::string{screen(7).id})}, 20, {}, "climb"});

    auto& shedPath = ensureHotspot(world, 6, "shed_path",
        tr("PATH TO TOOL SHED", "CESTA KE KŮLNĚ"),
        {372, 170, 120, 90}, e2d::HotspotKind::mechanism, 3);
    shedPath.visuals = {
        e2d::PolygonVisual{{{385, 213}, {438, 213}, {456, 226}, {438, 239}, {385, 239}}, amber, true},
        label(397, 220, tr("SHED", "KŮLNA"), P::black),
    };
    world.addInteraction({e2d::Verb::context, shedPath.id, std::nullopt,
        {e2d::Condition::flag("cabin_entered")}, {},
        {e2d::Mutation::moveTo(std::string{screen(9).id})}, 20, {}, "climb"});

    auto& shedReturn = ensureHotspot(world, 9, "cabin_path",
        tr("PATH BACK TO CABIN", "CESTA ZPĚT K CHATĚ"),
        {0, 91, 72, 169}, e2d::HotspotKind::mechanism, 0);
    shedReturn.visuals = {box(20, 96, 44, 124, amber, false)};
    world.addInteraction({e2d::Verb::context, shedReturn.id, std::nullopt, {}, {},
        {e2d::Mutation::moveTo(std::string{screen(6).id})}, 20, {}, "climb"});

    auto& mastPath = ensureHotspot(world, 9, "mast_path",
        tr("PATH TO WEATHER MAST", "CESTA K METEOSTOŽÁRU"),
        {386, 89, 106, 171}, e2d::HotspotKind::mechanism, 3);
    mastPath.visuals = {box(396, 100, 73, 36, amber, false)};
    world.addInteraction({e2d::Verb::context, mastPath.id, std::nullopt, {}, {},
        {e2d::Mutation::moveTo(std::string{screen(11).id})}, 20, {}, "climb"});

    auto& cellarStairs = ensureHotspot(world, 10, "cellar_stairs",
        tr("STAIRS TO CABIN", "SCHODY DO CHATY"),
        {0, 104, 112, 156}, e2d::HotspotKind::mechanism, 0);
    cellarStairs.visuals = {box(10, 108, 94, 108, amber, false)};
    world.addInteraction({e2d::Verb::context, cellarStairs.id, std::nullopt, {}, {},
        {e2d::Mutation::moveTo(std::string{screen(7).id})}, 20, {}, "climb"});

    auto& serviceHatch = ensureHotspot(world, 10, "service_hatch",
        tr("LOW SERVICE HATCH", "NÍZKÝ SERVISNÍ POKLOP"),
        {365, 166, 119, 94}, e2d::HotspotKind::mechanism, 3);
    serviceHatch.visuals = {box(375, 184, 98, 39, amber, false)};
    world.addInteraction({e2d::Verb::context, serviceHatch.id, std::nullopt,
        {e2d::Condition::has("hand_crank_torch")}, {},
        {e2d::Mutation::moveTo(std::string{screen(11).id})}, 30, {}, "climb"});
    world.addInteraction({e2d::Verb::context, serviceHatch.id, std::nullopt,
        {e2d::Condition::lacks("hand_crank_torch")},
        {inspect(tr("The service crawl is completely dark. A portable light is needed.",
            "Servisní průlez je úplně temný. Je potřeba přenosné světlo."))}, {}, 10, {}});

    auto& mastCellarHatch = ensureHotspot(world, 11, "cellar_hatch",
        tr("CELLAR SERVICE HATCH", "SERVISNÍ POKLOP DO SKLEPA"),
        {8, 201, 100, 59}, e2d::HotspotKind::mechanism, 0);
    mastCellarHatch.visuals = {
        box(16, 229, 84, 28, P::brown),
        box(20, 233, 76, 20, amber, false),
        label(29, 217, tr("CELLAR", "SKLEP"), amber),
    };
    world.addInteraction({e2d::Verb::context, mastCellarHatch.id, std::nullopt,
        {e2d::Condition::has("hand_crank_torch")}, {},
        {e2d::Mutation::moveTo(std::string{screen(10).id})}, 20, {}, "climb"});

    addCharacter(world, 7, "mara", "MARA VENN", "MARA VENN", "met_mara", {
        speech(tr("Mara: This should have been a fuse and cable job. The storm is hiding something deliberate.",
            "Mara: Měla to být výměna pojistky a kabelu. Bouře skrývá něco úmyslného.")),
        speech(tr("Iris: I will restore the local chain and trace whatever is riding the receiver.",
            "Iris: Obnovím místní řetězec a vystopuji vše, co leze do přijímače."), e2d::MessageSpeaker::player),
        speech(tr("Mara: My yard key is under the swollen desk log. Take the site map from the radio nook.",
            "Mara: Klíč od areálu je pod nabobtnalým deníkem. Vezmi mapu z rádiového kouta.")),
    });
    auto& desk = ensureHotspot(world, 7, "mara_desk", tr("MARA'S DESK", "MAŘIN STŮL"),
        {61, 157, 110, 103}, e2d::HotspotKind::scenery, 0);
    world.addInteraction({e2d::Verb::examine, desk.id, std::nullopt,
        {e2d::Condition::flag("met_mara"), e2d::Condition::notFlag("key_revealed")},
        {inspect(tr("Beneath the swollen logbook, a brass yard key is taped to the cover.",
            "Pod nabobtnalým deníkem je k deskám přilepený mosazný klíč."))},
        {e2d::Mutation::setFlag("key_revealed")}, 30, {}});
    addPickup(world, 7, "brass_key", "You peel the reusable brass master key from the logbook.",
        "Odlepíš z deníku opakovaně použitelný mosazný klíč.", 0,
        {e2d::Condition::flag("key_revealed")});
    auto& brassKeyPickup = ensureHotspot(world, 7, "take_brass_key",
        world.item("brass_key")->label, {61, 145, 120, 115}, e2d::HotspotKind::item, 0);
    brassKeyPickup.interactionArea = {61, 145, 120, 115};
    brassKeyPickup.visuals = {
        circle(108, 169, 5, amber, false),
        line(113, 169, 137, 169, amber),
        line(128, 169, 128, 175, amber),
        line(136, 169, 136, 173, amber),
        line(103, 157, 103, 163, pale),
        line(100, 160, 106, 160, pale),
    };
    addPickup(world, 8, "site_map", "Mara's annotations name the relay, quarry, dam and lookout.",
        "Mařiny poznámky označují převaděč, lom, přehradu a hlásku.", 0,
        {e2d::Condition::flag("met_mara")});
    auto& siteMapPickup = ensureHotspot(world, 8, "take_site_map",
        world.item("site_map")->label, {326, 148, 116, 112}, e2d::HotspotKind::item, 0);
    siteMapPickup.interactionArea = {326, 148, 116, 112};
    siteMapPickup.visuals = {
        box(352, 174, 50, 22, pale),
        line(377, 174, 377, 196, P::lightGray),
        line(357, 181, 372, 188, P::green),
        line(382, 188, 397, 180, P::blue),
        line(407, 170, 407, 176, pale),
        line(404, 173, 410, 173, pale),
    };
    addPickup(world, 9, "wrench", "You take Mara's scarred 17 mm field wrench.",
        "Vezmeš Mařin odřený montážní klíč 17 mm.", 0);
    addPickup(world, 9, "lineman_gloves", "The lineman gloves are dry and their insulation is sound.",
        "Elektrikářské rukavice jsou suché a jejich izolace neporušená.", 1);
    addPickup(world, 9, "pruning_saw", "A folding pruning saw joins the repair kit.",
        "Skládací prořezávací pila doplní opravárenskou výbavu.", 2);
    addPickup(world, 10, "ceramic_fuse", "A 30 amp ceramic fuse survives beside the Nightjar crate.",
        "Vedle bedny Nightjar přežila keramická pojistka 30 A.", 0);
    addPickup(world, 10, "hand_crank_torch", "The crank torch produces a narrow but dependable beam.",
        "Ruční svítilna vydává úzký, ale spolehlivý paprsek.", 1);

    // The service-road fork and relay compound are a navigable hub. The
    // catalogue remains ordered for documentation, but indoor branches are
    // entered through visible, labelled portals instead of fake edge exits.
    setHorizontalRoute(world, 12, 11, 13);
    setHorizontalRoute(world, 13, 12, 14);
    setHorizontalRoute(world, 14, 13, 15);
    setHorizontalRoute(world, 15, 14, 16);
    setHorizontalRoute(world, 16, 15, std::nullopt);
    for (int branch = 17; branch <= 24; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }
    setHorizontalRoute(world, 25, 12, 26);

    auto& forestRoute = ensureHotspot(world, 12, "forest_route",
        tr("FORESTRY BARRIER TO NORTH ROAD", "LESNICKÁ ZÁVORA K SEVERNÍ CESTĚ"),
        {181, 126, 142, 134}, e2d::HotspotKind::mechanism, 1);
    forestRoute.visuals = {
        box(190, 154, 9, 106, P::brown), box(300, 154, 9, 106, P::brown),
        box(193, 178, 110, 12, danger), box(193, 181, 110, 4, pale),
        circle(249, 184, 5, amber),
        e2d::PolygonVisual{{{198, 135}, {286, 135}, {304, 146}, {286, 157}, {198, 157}}, amber, true},
        label(215, 141, tr("FOREST", "LES"), P::black),
    };
    world.addInteraction({e2d::Verb::context, forestRoute.id, std::nullopt,
        {e2d::Condition::flag("act1_complete"), e2d::Condition::notFlag("forest_route_entered")},
        {inspect(tr("The site-map code lifts the forestry barrier. Iris follows bearing 017 north.",
            "Kód z mapy zvedne lesnickou závoru. Iris pokračuje na sever podle náměru 017."))},
        {e2d::Mutation::setFlag("forest_route_entered"),
            e2d::Mutation::moveTo(std::string{screen(25).id})}, 40, {}, "unlock"});
    world.addInteraction({e2d::Verb::context, forestRoute.id, std::nullopt,
        {e2d::Condition::flag("act1_complete"), e2d::Condition::flag("forest_route_entered")}, {},
        {e2d::Mutation::moveTo(std::string{screen(25).id})}, 30, {}, "climb"});
    world.addInteraction({e2d::Verb::context, forestRoute.id, std::nullopt,
        {e2d::Condition::notFlag("act1_complete")},
        {inspect(tr("The forestry barrier needs Mara's map code and a confirmed Nightjar bearing. Restore the relay first.",
            "Lesnická závora vyžaduje kód z Mařiny mapy a potvrzený náměr Nightjaru. Nejprve obnov převaděč."))},
        {}, 10, {}});
    room(world, 12).hotspots.push_back({targetId(12, "forest_route_open"),
        tr("OPEN NORTH ROAD", "OTEVŘENÁ SEVERNÍ CESTA"), {0, 0, 0, 0},
        e2d::HotspotKind::scenery, {e2d::Condition::flag("act1_complete")}, {
            box(299, 83, 9, 103, P::brown),
            box(301, 89, 12, 100, danger),
            circle(304, 190, 5, P::brightGreen),
        }});

    addPortal(world, 15, "trench_ladder", "LADDER TO CABLE TRENCH", "ŽEBŘÍK DO KABELOVÉHO VÝKOPU",
        {24, 165, 126, 95}, 17, {
            box(31, 233, 110, 24, P::black), line(43, 184, 43, 256, amber),
            line(66, 184, 66, 256, amber), line(43, 198, 66, 198, pale),
            line(43, 216, 66, 216, pale), line(43, 234, 66, 234, pale),
            label(79, 210, tr("TRENCH", "VÝKOP"), amber),
        });
    addPortal(world, 15, "hall_door", "DOOR TO LOWER RELAY HALL", "DVEŘE DO DOLNÍHO SÁLU",
        {356, 127, 128, 133}, 23, {
            box(382, 136, 78, 124, P::lightGray), box(389, 144, 64, 110, P::blue),
            circle(443, 199, 4, amber), label(396, 158, tr("HALL", "SÁL"), pale),
        });
    addPortal(world, 17, "yard_ladder", "LADDER TO RELAY YARD", "ŽEBŘÍK DO AREÁLU",
        {365, 139, 119, 121}, 15, {
            line(395, 146, 395, 258, amber), line(426, 146, 426, 258, amber),
            line(395, 164, 426, 164, pale), line(395, 186, 426, 186, pale),
            line(395, 208, 426, 208, pale), line(395, 230, 426, 230, pale),
            label(386, 130, tr("YARD", "AREÁL"), amber),
        });

    addPortal(world, 16, "generator_path", "PATH TO GENERATOR SHED", "CESTA KE GENERÁTOROVNĚ",
        {18, 139, 130, 121}, 18, {
            e2d::PolygonVisual{{{26, 180}, {105, 180}, {126, 194}, {105, 208}, {26, 208}}, amber, true},
            label(38, 189, tr("GENERATOR", "GENERÁTOR"), P::black),
        });
    addPortal(world, 16, "fuel_path", "PATH TO FUEL PUMP", "CESTA K PALIVOVÉMU ČERPADLU",
        {180, 139, 132, 121}, 20, {
            box(192, 174, 107, 38, P::brown), box(198, 180, 95, 26, amber),
            label(224, 189, tr("PUMP", "ČERPADLO"), P::black),
        });
    addPortal(world, 16, "transformer_path", "PATH TO TRANSFORMER PAD", "CESTA K TRANSFORMÁTORU",
        {346, 139, 138, 121}, 21, {
            e2d::PolygonVisual{{{357, 180}, {440, 180}, {466, 194}, {440, 208}, {357, 208}}, signalBlue, true},
            label(371, 189, tr("TRANSFORMER", "TRANSFORMÁTOR"), P::black),
        });

    addPortal(world, 18, "yard_door", "DOOR TO RELAY YARD", "DVEŘE DO AREÁLU",
        {0, 126, 60, 134}, 16, {
            box(8, 132, 44, 128, P::brown), circle(43, 198, 3, amber),
            label(13, 145, tr("YARD", "AREÁL"), pale),
        });
    addPortal(world, 18, "workshop_door", "DOOR TO RELAY WORKSHOP", "DVEŘE DO DÍLNY",
        {172, 126, 84, 134}, 22, {
            box(181, 136, 66, 124, P::brown), circle(238, 199, 3, amber),
            label(188, 150, tr("WORKSHOP", "DÍLNA"), pale),
        });
    addPortal(world, 18, "battery_door", "DOOR TO BATTERY ROOM", "DVEŘE DO AKUMULÁTOROVNY",
        {386, 126, 98, 134}, 19, {
            box(397, 136, 77, 124, P::brown), circle(464, 199, 3, amber),
            label(405, 150, tr("BATTERY", "BATERIE"), pale),
        });
    addPortal(world, 19, "generator_door", "DOOR TO GENERATOR SHED", "DVEŘE DO GENERÁTOROVNY",
        {387, 126, 97, 134}, 18, {
            box(399, 136, 74, 124, P::brown), circle(463, 199, 3, amber),
            label(405, 150, tr("GENERATOR", "GENERÁTOR"), pale),
        });
    addPortal(world, 20, "yard_path", "PATH TO RELAY YARD", "CESTA DO AREÁLU",
        {382, 151, 102, 109}, 16, {
            e2d::PolygonVisual{{{391, 181}, {447, 181}, {470, 195}, {447, 209}, {391, 209}}, amber, true},
            label(404, 190, tr("YARD", "AREÁL"), P::black),
        });
    addPortal(world, 21, "yard_path", "PATH TO RELAY YARD", "CESTA DO AREÁLU",
        {382, 151, 102, 109}, 16, {
            e2d::PolygonVisual{{{391, 181}, {447, 181}, {470, 195}, {447, 209}, {391, 209}}, amber, true},
            label(404, 190, tr("YARD", "AREÁL"), P::black),
        });
    auto& switchbackPath = addPortal(world, 21, "switchback_path",
        "MAINTENANCE PATH TO SWITCHBACK", "ÚDRŽBOVÁ CESTA K SERPENTINĚ",
        {220, 151, 140, 109}, 4, {
            e2d::PolygonVisual{{{230, 181}, {318, 181}, {344, 195}, {318, 209}, {230, 209}}, signalBlue, true},
            label(241, 190, tr("SWITCHBACK", "SERPENTINA"), P::black),
        }, {e2d::Condition::flag("feeder_isolated")});
    world.addInteraction({e2d::Verb::context, switchbackPath.id, std::nullopt,
        {e2d::Condition::notFlag("feeder_isolated")},
        {warning(tr("Blue arcs cover the maintenance path. Isolate the fallen feeder first.",
            "Údržbovou cestu křižují modré výboje. Nejprve odpoj spadlý přívod."))},
        {}, 10, {}, "warning"});
    addPortal(world, 22, "generator_door", "DOOR TO GENERATOR SHED", "DVEŘE DO GENERÁTOROVNY",
        {386, 126, 98, 134}, 18, {
            box(397, 136, 77, 124, P::brown), circle(464, 199, 3, amber),
            label(403, 150, tr("GENERATOR", "GENERÁTOR"), pale),
        });
    addPortal(world, 23, "yard_door", "DOOR TO RELAY YARD", "DVEŘE DO AREÁLU",
        {0, 126, 62, 134}, 15, {
            box(8, 136, 46, 124, P::brown), circle(45, 199, 3, amber),
            label(13, 150, tr("YARD", "AREÁL"), pale),
        });
    addPortal(world, 23, "control_stairs", "STAIRS TO LOCAL CONTROL", "SCHODY DO MÍSTNÍHO VELÍNU",
        {376, 126, 108, 134}, 24, {
            line(389, 250, 464, 151, amber), line(411, 250, 484, 151, amber),
            line(397, 235, 420, 235, pale), line(409, 217, 433, 217, pale),
            line(422, 199, 446, 199, pale), line(435, 181, 459, 181, pale),
            label(382, 137, tr("CONTROL", "VELÍN"), amber),
        });
    addPortal(world, 24, "hall_stairs", "STAIRS TO LOWER RELAY HALL", "SCHODY DO DOLNÍHO SÁLU",
        {0, 126, 92, 134}, 23, {
            line(10, 151, 74, 250, amber), line(32, 151, 94, 250, amber),
            line(23, 173, 46, 173, pale), line(35, 191, 59, 191, pale),
            line(47, 209, 71, 209, pale), line(59, 227, 83, 227, pale),
            label(11, 137, tr("DOWN", "DOLŮ"), amber),
        });

    addUse(world, 14, "vehicle_gate", "VEHICLE GATE", "VJEZDOVÁ BRÁNA", "brass_key", "vehicle_gate_open",
        "The old master key drops the gate chain. You keep it for the workshop.",
        "Starý hlavní klíč uvolní řetěz brány. Necháš si ho pro dílnu.");
    auto& vehicleGate = ensureHotspot(world, 14, "vehicle_gate",
        tr("VEHICLE GATE", "VJEZDOVÁ BRÁNA"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    vehicleGate.visibleWhen = {e2d::Condition::notFlag("vehicle_gate_open")};
    vehicleGate.visuals = {
        line(75, 194, 145, 194, P::lightGray), line(80, 188, 140, 201, P::lightGray),
        box(101, 181, 27, 31, amber), circle(114, 191, 4, P::black),
        label(86, 218, tr("LOCKED", "ZAMČENO"), danger),
    };
    auto& openGate = ensureHotspot(world, 14, "vehicle_gate_complete",
        tr("OPEN VEHICLE GATE", "OTEVŘENÁ VJEZDOVÁ BRÁNA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    openGate.visuals = {
        box(101, 181, 27, 31, P::brightGreen, false),
        label(82, 218, tr("GATE OPEN", "BRÁNA OTEVŘENA"), P::brightGreen),
    };
    gateRight(world, 14, {e2d::Condition::flag("vehicle_gate_open")},
        "The locked vehicle gate blocks the relay yard.", "Zamčená vjezdová brána blokuje areál převaděče.");
    addUse(world, 17, "blue_terminals", "BLUE TERMINALS", "MODRÉ SVORKY", "patch_cable", "cable_patched",
        "Iris kneels and bridges the deliberately empty blue terminals.",
        "Iris se skloní a propojí úmyslně prázdné modré svorky.", {}, true);
    auto& blueTerminals = ensureHotspot(world, 17, "blue_terminals",
        tr("BLUE TERMINALS", "MODRÉ SVORKY"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    blueTerminals.visibleWhen = {e2d::Condition::notFlag("cable_patched")};
    blueTerminals.visuals = {
        box(73, 157, 76, 80, P::darkGray), box(79, 163, 64, 68, P::black),
        circle(94, 190, 8, signalBlue, false), circle(128, 190, 8, signalBlue, false),
        line(102, 190, 120, 190, danger), label(82, 169, tr("BLUE", "MODRÉ"), signalBlue),
    };
    auto& patchedTerminals = ensureHotspot(world, 17, "blue_terminals_complete",
        tr("PATCHED BLUE TERMINALS", "PROPOJENÉ MODRÉ SVORKY"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    patchedTerminals.visuals = {
        box(73, 157, 76, 80, P::darkGray), box(79, 163, 64, 68, P::black),
        circle(94, 190, 8, signalBlue, false), circle(128, 190, 8, signalBlue, false),
        e2d::PolylineVisual{{{94, 190}, {109, 174}, {128, 190}}, P::brightGreen, false},
        circle(111, 210, 4, P::brightGreen),
    };
    addUse(world, 18, "main_fuse_holder", "MAIN FUSE HOLDER", "DRŽÁK HLAVNÍ POJISTKY", "ceramic_fuse", "fuse_installed",
        "The ceramic fuse locks into the MAIN holder with a clean click.",
        "Keramická pojistka čistě zaklapne do HLAVNÍHO držáku.", {}, true);
    auto& fuseHolder = ensureHotspot(world, 18, "main_fuse_holder",
        tr("MAIN FUSE HOLDER", "DRŽÁK HLAVNÍ POJISTKY"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    fuseHolder.visibleWhen = {e2d::Condition::notFlag("fuse_installed")};
    fuseHolder.visuals = {
        box(74, 155, 75, 80, P::darkGray), box(82, 163, 59, 64, P::black),
        circle(95, 195, 7, P::red), circle(128, 195, 7, P::red),
        line(95, 195, 128, 195, P::lightGray),
        label(88, 169, tr("MAIN", "HLAVNÍ"), amber),
    };
    auto& installedFuse = ensureHotspot(world, 18, "main_fuse_holder_complete",
        tr("INSTALLED MAIN FUSE", "NASAZENÁ HLAVNÍ POJISTKA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    installedFuse.visuals = {
        box(74, 155, 75, 80, P::darkGray), box(82, 163, 59, 64, P::black),
        circle(95, 195, 7, amber), circle(128, 195, 7, signalBlue),
        box(95, 188, 33, 14, danger), box(100, 191, 23, 8, P::red),
        circle(111, 216, 4, P::brightGreen),
    };
    addUse(world, 19, "battery_bus", "BATTERY BUS LINK", "SPOJNICE AKUMULÁTORŮ", "wrench", "battery_linked",
        "The wrench secures the loose battery bus without crossing the acid stain.",
        "Klíčem upevníš spojení akumulátorů, aniž vstoupíš do kyseliny.");
    auto& batteryBus = ensureHotspot(world, 19, "battery_bus",
        tr("BATTERY BUS LINK", "SPOJNICE AKUMULÁTORŮ"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    batteryBus.visibleWhen = {e2d::Condition::notFlag("battery_linked")};
    batteryBus.visuals = {
        box(70, 167, 38, 59, P::blue), box(113, 167, 38, 59, P::blue),
        box(77, 158, 9, 9, danger), box(92, 158, 9, 9, P::brightGreen),
        box(120, 158, 9, 9, danger), box(135, 158, 9, 9, P::brightGreen),
        line(97, 162, 124, 181, P::lightGray), label(78, 234, tr("BUS", "SPOJNICE"), amber),
    };
    auto& linkedBus = ensureHotspot(world, 19, "battery_bus_complete",
        tr("CONNECTED BATTERY BUS", "PŘIPOJENÁ SPOJNICE AKUMULÁTORŮ"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    linkedBus.visuals = {
        box(70, 167, 38, 59, P::blue), box(113, 167, 38, 59, P::blue),
        box(77, 158, 9, 9, danger), box(92, 158, 9, 9, P::brightGreen),
        box(120, 158, 9, 9, danger), box(135, 158, 9, 9, P::brightGreen),
        box(97, 158, 27, 8, amber), circle(111, 181, 4, P::brightGreen),
    };
    addUse(world, 20, "fuel_valve", "FUEL SUPPLY VALVE", "PŘÍVODNÍ VENTIL PALIVA", "wrench", "fuel_valve_open",
        "The seized valve turns and fuel rises in the sight glass.",
        "Zadřený ventil se otočí a palivo stoupne v průhledítku.");
    auto& fuelValve = ensureHotspot(world, 20, "fuel_valve",
        tr("FUEL SUPPLY VALVE", "PŘÍVODNÍ VENTIL PALIVA"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    fuelValve.visibleWhen = {e2d::Condition::notFlag("fuel_valve_open")};
    fuelValve.visuals = {
        line(68, 210, 154, 210, amber), box(87, 202, 47, 16, P::brown),
        circle(111, 179, 23, danger, false), circle(111, 179, 6, amber),
        line(88, 179, 134, 179, danger), line(111, 156, 111, 202, danger),
        label(85, 232, tr("FUEL", "PALIVO"), amber),
    };
    auto& openFuelValve = ensureHotspot(world, 20, "fuel_valve_complete",
        tr("OPEN FUEL SUPPLY", "OTEVŘENÝ PŘÍVOD PALIVA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    openFuelValve.visuals = {
        line(68, 210, 154, 210, amber), box(87, 202, 47, 16, P::brown),
        circle(111, 179, 23, P::brightGreen, false), circle(111, 179, 6, amber),
        line(95, 163, 127, 195, P::brightGreen), line(127, 163, 95, 195, P::brightGreen),
        box(141, 169, 8, 38, P::black), box(143, 178, 4, 27, amber),
    };
    addPickup(world, 20, "siphon_hose", "You take the empty fuel-safe siphon hose.",
        "Vezmeš prázdnou hadici vhodnou pro přečerpávání paliva.", 2);
    addUse(world, 21, "fallen_feeder", "FALLEN FEEDER ISOLATOR", "ODPOJOVAČ SPADLÉHO PŘÍVODU",
        "lineman_gloves", "feeder_isolated", "Insulated hands open the feeder switch. The blue arcs die.",
        "Izolované ruce otevřou odpojovač. Modré výboje zhasnou.");
    auto& feeder = ensureHotspot(world, 21, "fallen_feeder",
        tr("FALLEN FEEDER ISOLATOR", "ODPOJOVAČ SPADLÉHO PŘÍVODU"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    feeder.visibleWhen = {e2d::Condition::notFlag("feeder_isolated")};
    feeder.visuals = {
        box(76, 154, 70, 83, P::lightGray), box(84, 163, 54, 65, P::black),
        circle(94, 181, 7, signalBlue), circle(128, 181, 7, signalBlue),
        line(94, 188, 124, 214, danger), line(128, 188, 98, 214, danger),
        label(88, 219, tr("LIVE", "ŽIVÉ"), danger),
    };
    auto& isolatedFeeder = ensureHotspot(world, 21, "fallen_feeder_complete",
        tr("ISOLATED FEEDER", "ODPOJENÝ PŘÍVOD"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    isolatedFeeder.visuals = {
        box(76, 154, 70, 83, P::lightGray), box(84, 163, 54, 65, P::black),
        circle(94, 181, 7, P::darkGray), circle(128, 181, 7, P::darkGray),
        line(94, 188, 128, 216, P::lightGray), circle(111, 218, 4, P::brightGreen),
        label(87, 169, tr("SAFE", "BEZPEČNÉ"), P::brightGreen),
    };
    addHazard(world, 4, "live_feeder", "feeder_isolated",
        "The fallen feeder arcs through Iris before she can pull away.",
        "Spadlý přívod zasáhne Iris dřív, než stačí ucuknout.");

    auto& cabinet = ensureHotspot(world, 22, "locked_cabinet", tr("CALDER'S CABINET", "CALDEROVÉ SKŘÍŇ"),
        {91, 132, 116, 128}, e2d::HotspotKind::mechanism, 0);
    cabinet.visibleWhen = {e2d::Condition::notFlag("workshop_open")};
    cabinet.visuals = {
        box(101, 143, 96, 112, P::brown), box(109, 151, 80, 96, P::red, false),
        line(149, 151, 149, 247, P::red), circle(158, 199, 5, amber),
        label(113, 161, tr("CALDER", "CALDEROVÁ"), pale),
    };
    world.addInteraction({e2d::Verb::use, cabinet.id, std::string{"brass_key"},
        {e2d::Condition::notFlag("workshop_open")},
        {inspect(tr("The key opens Calder's cabinet. Her multimeter and enamel badge are still inside.",
            "Klíč otevře Calderové skříň. Uvnitř zůstal multimetr a smaltovaný odznak."))},
        {e2d::Mutation::setFlag("workshop_open"), e2d::Mutation::addItem("multimeter"),
            e2d::Mutation::addItem("relay_badge")}, 30, {}, "unlock"});
    room(world, 22).hotspots.push_back({targetId(22, "locked_cabinet_open"),
        tr("OPEN CALDER CABINET", "OTEVŘENÁ CALDEROVÉ SKŘÍŇ"), {0, 0, 0, 0},
        e2d::HotspotKind::scenery, {e2d::Condition::flag("workshop_open")}, {
            box(101, 143, 96, 112, P::brown), box(109, 151, 34, 96, P::black),
            box(151, 151, 38, 96, P::red, false),
            line(116, 191, 136, 181, signalBlue), circle(126, 197, 8, amber),
            circle(170, 199, 4, P::brightGreen),
        }});
    addUse(world, 23, "nightjar_trunk", "NIGHTJAR TRUNK", "TRASA NIGHTJAR", "multimeter", "nightjar_signal_found",
        "The meter proves that a timed signal lives on a physically disconnected trunk.",
        "Měřidlo dokáže, že na fyzicky odpojené trase žije časovaný signál.");
    auto& nightjarTrunk = ensureHotspot(world, 23, "nightjar_trunk",
        tr("NIGHTJAR TRUNK", "TRASA NIGHTJAR"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    nightjarTrunk.visibleWhen = {e2d::Condition::notFlag("nightjar_signal_found")};
    nightjarTrunk.visuals = {
        box(72, 148, 78, 96, P::darkGray), box(80, 156, 62, 80, P::black),
        line(91, 166, 91, 226, signalBlue), line(111, 166, 111, 226, P::lightGray),
        line(131, 166, 131, 226, danger), circle(111, 197, 5, amber),
        label(83, 232, tr("NIGHTJAR", "NIGHTJAR"), danger),
    };
    auto& tracedTrunk = ensureHotspot(world, 23, "nightjar_trunk_complete",
        tr("TRACED NIGHTJAR TRUNK", "PROMĚŘENÁ TRASA NIGHTJAR"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    tracedTrunk.visuals = {
        box(72, 148, 78, 96, P::darkGray), box(80, 156, 62, 80, P::black),
        line(91, 166, 91, 226, signalBlue), line(111, 166, 111, 226, P::lightGray),
        line(131, 166, 131, 226, danger),
        e2d::PolylineVisual{{{84, 208}, {96, 190}, {107, 211}, {119, 184}, {138, 208}}, P::brightGreen, false},
    };
    addContext(world, 18, "main_lever", "MAIN LEVER", "HLAVNÍ PÁKA", "power_on", {
        inspect(tr("The generator coughs twice, catches, and drives power through every repaired link.",
            "Generátor dvakrát zakašle, chytne se a žene proud všemi opravenými spoji.")),
    }, {e2d::Condition::flag("fuse_installed"), e2d::Condition::flag("cable_patched"),
        e2d::Condition::flag("battery_linked"), e2d::Condition::flag("fuel_valve_open"),
        e2d::Condition::flag("feeder_isolated")}, {}, 2, "power");
    auto& mainLever = ensureHotspot(world, 18, "main_lever",
        tr("MAIN LEVER", "HLAVNÍ PÁKA"), {270, 137, 92, 123}, e2d::HotspotKind::mechanism, 2);
    mainLever.visibleWhen = {e2d::Condition::notFlag("power_on")};
    mainLever.visuals = {
        box(286, 171, 59, 67, P::darkGray), circle(315, 204, 20, P::black),
        line(315, 204, 300, 177, danger), circle(300, 177, 7, amber),
        label(293, 223, tr("MAIN", "HLAVNÍ"), pale),
    };
    auto& poweredLever = ensureHotspot(world, 18, "main_lever_complete",
        tr("POWERED MAIN LEVER", "ZAPNUTÁ HLAVNÍ PÁKA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    poweredLever.visuals = {
        box(286, 171, 59, 67, P::darkGray), circle(315, 204, 20, P::black),
        line(315, 204, 331, 177, P::brightGreen), circle(331, 177, 7, amber),
        circle(315, 229, 4, P::brightGreen),
    };
    addUse(world, 11, "weather_mast", "WEATHER MAST TEST LEADS", "TESTOVACÍ VÝVODY METEOSTOŽÁRU",
        "multimeter", "mast_calibrated", "The live mast settles on bearing 017 toward the north forest.",
        "Živý stožár se ustálí na náměru 017 k severnímu lesu.", {e2d::Condition::flag("power_on")});
    addContext(world, 24, "direction_console", "DIRECTION TRACE CONSOLE", "PANEL SMĚROVÉHO TRASOVAČE",
        "act1_complete", {
            speech(tr("Iris: NIGHTJAR QUIET FIELD. Bearing zero-one-seven. This failure was prepared.",
                "Iris: NIGHTJAR QUIET FIELD. Náměr nula-jedna-sedm. Tohle selhání bylo připravené."), e2d::MessageSpeaker::player),
            speech(tr("Mara: Nightjar was buried years ago. Open the north barrier and find who woke it.",
                "Mara: Nightjar byl pohřben před lety. Otevři severní závoru a najdi, kdo ho probudil.")),
        }, {e2d::Condition::flag("power_on"), e2d::Condition::flag("nightjar_signal_found"),
            e2d::Condition::flag("mast_calibrated")}, {}, 2, "power");
    auto& directionConsole = ensureHotspot(world, 24, "direction_console",
        tr("DIRECTION TRACE CONSOLE", "PANEL SMĚROVÉHO TRASOVAČE"),
        {270, 137, 92, 123}, e2d::HotspotKind::mechanism, 2);
    directionConsole.visibleWhen = {e2d::Condition::notFlag("act1_complete")};
    directionConsole.visuals = {
        box(275, 151, 82, 93, P::lightGray), box(282, 159, 68, 48, P::blue),
        circle(316, 183, 19, P::black, false), line(316, 183, 302, 170, signalBlue),
        circle(292, 221, 5, danger), circle(316, 221, 5, amber), circle(340, 221, 5, P::brightGreen),
        label(282, 235, tr("TRACE", "TRASA"), pale),
    };
    auto& tracedConsole = ensureHotspot(world, 24, "direction_console_complete",
        tr("NIGHTJAR BEARING 017", "NÁMĚR NIGHTJAR 017"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    tracedConsole.visuals = {
        box(275, 151, 82, 93, P::lightGray), box(282, 159, 68, 48, P::blue),
        circle(316, 183, 19, P::black, false), line(316, 183, 326, 167, P::brightGreen),
        circle(340, 221, 5, P::brightGreen), label(287, 212, tr("017", "017"), amber),
    };
    addFollowUpDialogue(world, 7, "mara", "mara_nightjar_briefing", {
        speech(tr("Mara: Bearing zero-one-seven ends at Nightjar. Ruth Calder shut that place down because the field could pull aircraft off their instruments.",
            "Mara: Náměr nula-jedna-sedm končí u Nightjaru. Ruth Calderová to místo zavřela, protože pole mohlo odvést letadla z přístrojů.")),
        speech(tr("Iris: Then Kestrel Six was not only caught by weather. I will follow the pulse north.",
            "Iris: Pak Kestrel Six nezasáhlo jen počasí. Budu sledovat pulz na sever."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("met_mara"), e2d::Condition::flag("act1_complete")});
}

void addActTwo(e2d::WorldDefinition& world) {
    // The north forest is a looped exploration region: the road reaches the
    // creek, Theo and his cache; the kiln and Echo Grove form a second loop;
    // the firebreak then branches to weather evidence, Nell and the ravine.
    setHorizontalRoute(world, 25, 12, 26);
    setHorizontalRoute(world, 26, 25, 27);
    setHorizontalRoute(world, 27, 26, 28);
    setHorizontalRoute(world, 28, 27, std::nullopt);
    for (int branch = 29; branch <= 33; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }
    setHorizontalRoute(world, 34, 33, 35);
    setHorizontalRoute(world, 35, 34, 36);
    setHorizontalRoute(world, 36, 35, std::nullopt);
    setHorizontalRoute(world, 37, std::nullopt, std::nullopt);
    setHorizontalRoute(world, 38, std::nullopt, std::nullopt);
    setHorizontalRoute(world, 39, 36, 40);

    addPortal(world, 26, "kiln_path", "WEST LOOP TO CHARCOAL KILN", "ZÁPADNÍ OKRUH K MILÍŘI",
        {174, 154, 144, 106}, 32, {
            e2d::PolygonVisual{{{184, 181}, {276, 181}, {302, 195}, {276, 209}, {184, 209}}, amber, true},
            label(196, 190, tr("KILN LOOP", "OKRUH K MILÍŘI"), P::black),
        });
    addPortal(world, 32, "pine_path", "PATH TO BURNED PINES", "CESTA KE SPÁLENÝM BOROVICÍM",
        {0, 151, 116, 109}, 26, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {102, 181}, {102, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("PINES", "BOROVICE"), P::black),
        });
    addPortal(world, 32, "grove_path", "PATH TO ECHO GROVE", "CESTA DO HÁJE OZVĚN",
        {370, 151, 114, 109}, 33, {
            e2d::PolygonVisual{{{380, 181}, {445, 181}, {474, 195}, {445, 209}, {380, 209}}, signalBlue, true},
            label(391, 190, tr("GROVE", "HÁJ"), P::black),
        });

    addPortal(world, 28, "blind_path", "PATH TO HUNTER'S BLIND", "CESTA K LOVECKÉMU POSEDU",
        {153, 151, 148, 109}, 29, {
            e2d::PolygonVisual{{{163, 181}, {258, 181}, {284, 195}, {258, 209}, {163, 209}}, amber, true},
            label(179, 190, tr("BLIND", "POSED"), P::black),
        });
    addPortal(world, 28, "grove_path", "PATH TO ECHO GROVE", "CESTA DO HÁJE OZVĚN",
        {326, 151, 158, 109}, 33, {
            e2d::PolygonVisual{{{337, 181}, {438, 181}, {469, 195}, {438, 209}, {337, 209}}, signalBlue, true},
            label(354, 190, tr("ECHO GROVE", "HÁJ OZVĚN"), P::black),
        });
    addPortal(world, 29, "creek_path", "PATH TO COLD CREEK", "CESTA KE STUDENÉMU POTOKU",
        {0, 151, 112, 109}, 28, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {101, 181}, {101, 209}, {31, 209}}, signalBlue, true},
            label(34, 190, tr("CREEK", "POTOK"), P::black),
        });
    addPortal(world, 29, "hollow_path", "PATH TO MOSSY HOLLOW", "CESTA DO MECHOVÉ PROHLUBNĚ",
        {368, 151, 116, 109}, 30, {
            e2d::PolygonVisual{{{378, 181}, {444, 181}, {473, 195}, {444, 209}, {378, 209}}, amber, true},
            label(389, 190, tr("THEO", "THEO"), P::black),
        });
    addPortal(world, 30, "blind_path", "PATH TO HUNTER'S BLIND", "CESTA K LOVECKÉMU POSEDU",
        {0, 151, 110, 109}, 29, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {100, 181}, {100, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("BLIND", "POSED"), P::black),
        });
    auto& cachePath = addPortal(world, 30, "cache_path", "PATH TO RANGER CACHE", "CESTA KE SKRÝŠI STRÁŽCŮ",
        {368, 151, 116, 109}, 31, {
            e2d::PolygonVisual{{{378, 181}, {444, 181}, {473, 195}, {444, 209}, {378, 209}}, signalBlue, true},
            label(389, 190, tr("CACHE", "SKRÝŠ"), P::black),
        }, {e2d::Condition::flag("theo_briefed")});
    world.addInteraction({e2d::Verb::context, cachePath.id, std::nullopt,
        {e2d::Condition::notFlag("theo_briefed")},
        {inspect(tr("Theo knows the cache combination, but he cannot speak until he is freed and bandaged.",
            "Theo zná kombinaci ke skrýši, ale nepromluví, dokud ho neuvolníš a neošetříš."))},
        {}, 10, {}});
    addPortal(world, 31, "hollow_path", "PATH TO MOSSY HOLLOW", "CESTA DO MECHOVÉ PROHLUBNĚ",
        {0, 151, 112, 109}, 30, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {101, 181}, {101, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("THEO", "THEO"), P::black),
        });
    addPortal(world, 31, "firebreak_path", "PATH TO FIREBREAK JUNCTION", "CESTA K PROSEKU",
        {360, 151, 124, 109}, 36, {
            e2d::PolygonVisual{{{370, 181}, {442, 181}, {473, 195}, {442, 209}, {370, 209}}, signalBlue, true},
            label(381, 190, tr("FIREBREAK", "PROSEK"), P::black),
        });

    addPortal(world, 33, "kiln_path", "PATH TO CHARCOAL KILN", "CESTA K MILÍŘI",
        {0, 151, 108, 109}, 32, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {98, 181}, {98, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("KILN", "MILÍŘ"), P::black),
        });
    addPortal(world, 33, "creek_path", "PATH TO COLD CREEK", "CESTA KE STUDENÉMU POTOKU",
        {174, 151, 138, 109}, 28, {
            box(184, 181, 118, 28, signalBlue), label(205, 190, tr("CREEK", "POTOK"), P::black),
        });
    auto& ridgePath = addPortal(world, 33, "ridge_path", "BEARING PATH TO CABLE RIDGE", "CESTA PODLE NÁMĚRU KE KABELU",
        {366, 151, 118, 109}, 34, {
            e2d::PolygonVisual{{{376, 181}, {444, 181}, {474, 195}, {444, 209}, {376, 209}}, P::brightGreen, true},
            label(388, 190, tr("017", "017"), P::black),
        }, {e2d::Condition::flag("echo_route_solved")});
    world.addInteraction({e2d::Verb::context, ridgePath.id, std::nullopt,
        {e2d::Condition::notFlag("echo_route_solved")},
        {inspect(tr("Every unmeasured path echoes back to this grove. Use Theo's compass on the bearing marker.",
            "Každá nezměřená cesta se vrací do tohoto háje. Použij Theův kompas na značce náměru."))},
        {}, 10, {}});

    addPortal(world, 36, "cache_path", "PATH TO RANGER CACHE", "CESTA KE SKRÝŠI STRÁŽCŮ",
        {4, 151, 91, 109}, 31, {
            e2d::PolygonVisual{{{8, 195}, {30, 181}, {99, 181}, {99, 209}, {30, 209}}, amber, true},
            label(32, 190, tr("CACHE", "SKRÝŠ"), P::black),
        });
    addPortal(world, 36, "weather_path", "PATH TO WEATHER STATION", "CESTA K METEOSTANICI",
        {120, 151, 95, 109}, 37, {
            box(124, 181, 99, 28, pale), label(133, 190, tr("WEATHER", "METEO"), P::black),
        });
    addPortal(world, 36, "lookout_path", "PATH TO NORTH LOOKOUT", "CESTA K SEVERNÍ HLÁSCE",
        {240, 151, 95, 109}, 38, {
            box(244, 181, 99, 28, amber), label(257, 190, tr("LOOKOUT", "HLÁSKA"), P::black),
        });
    auto& ravinePath = addPortal(world, 36, "ravine_path", "MARKED PATH TO RAVINE", "OZNAČENÁ CESTA K ROKLI",
        {360, 151, 124, 109}, 39, {
            e2d::PolygonVisual{{{367, 181}, {443, 181}, {474, 195}, {443, 209}, {367, 209}}, signalBlue, true},
            label(379, 190, tr("RAVINE", "ROKLE"), P::black),
        }, {e2d::Condition::flag("lookout_briefed")});
    world.addInteraction({e2d::Verb::context, ravinePath.id, std::nullopt,
        {e2d::Condition::notFlag("lookout_briefed")},
        {inspect(tr("The firebreak sign has been turned. Nell can identify the safe ravine approach from the lookout.",
            "Ukazatel na proseku je otočený. Nell může z hlásky určit bezpečný přístup k rokli."))},
        {}, 10, {}});
    addPortal(world, 37, "junction_path", "PATH TO FIREBREAK JUNCTION", "CESTA K PROSEKU",
        {0, 151, 119, 109}, 36, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {109, 181}, {109, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("JUNCTION", "PROSEK"), P::black),
        });
    addPortal(world, 38, "junction_path", "PATH TO FIREBREAK JUNCTION", "CESTA K PROSEKU",
        {0, 151, 119, 109}, 36, {
            e2d::PolygonVisual{{{8, 195}, {31, 181}, {109, 181}, {109, 209}, {31, 209}}, amber, true},
            label(34, 190, tr("JUNCTION", "PROSEK"), P::black),
        });

    auto& bootCache = ensureHotspot(world, 26, "boot_cache",
        tr("ASH-COVERED RANGER BOOT", "POPELEM POKRYTÁ BOTA STRÁŽCE"),
        {58, 177, 102, 83}, e2d::HotspotKind::scenery, 0);
    bootCache.visuals = {
        ellipse(106, 255, 34, 5, P::black), box(82, 224, 31, 25, P::brown),
        box(108, 238, 36, 12, P::brown), line(83, 224, 112, 238, P::lightGray),
        line(75, 250, 145, 250, P::darkGray),
    };
    world.addInteraction({e2d::Verb::examine, bootCache.id, std::nullopt,
        {e2d::Condition::notFlag("bandage_cache_found")},
        {inspect(tr("A sealed bandage roll is tucked inside the ranger boot, protected from ash and rain.",
            "Uvnitř boty strážce je před popelem a deštěm chráněná uzavřená role obvazu."))},
        {e2d::Mutation::setFlag("bandage_cache_found")}, 30, {}});

    auto& surveyRibbon = ensureHotspot(world, 25, "survey_ribbon",
        tr("DISCARDED SURVEY RIBBON", "ODHOZENÁ PRŮZKUMNICKÁ STUHA"),
        {335, 176, 108, 84}, e2d::HotspotKind::scenery, 3);
    surveyRibbon.visuals = {
        line(363, 211, 397, 166, P::brown), line(397, 166, 414, 174, danger),
        line(397, 166, 405, 185, danger), line(405, 185, 419, 194, P::red),
        line(401, 176, 409, 176, pale),
    };
    world.addInteraction({e2d::Verb::examine, surveyRibbon.id, std::nullopt,
        {e2d::Condition::notFlag("survey_ribbon_recorded")},
        {inspect(tr("The red survey ribbon bears the same triangular mark as the cut tie below the cabin. Iris records it as evidence.",
            "Červená průzkumnická stuha nese stejný trojúhelník jako přeříznutá páska pod chatou. Iris ji zaznamená jako důkaz."))},
        {e2d::Mutation::setFlag("survey_ribbon_recorded")}, 30, {}});

    addPickup(world, 26, "bandage_roll", "A sealed ranger bandage waits inside the hidden boot cache.",
        "V ukryté schránce čeká uzavřený obvaz strážců.", 0,
        {e2d::Condition::flag("bandage_cache_found")});
    addUse(world, 27, "fallen_fir", "FALLEN FIR", "PADLÁ JEDLE", "pruning_saw", "fir_cut",
        "The saw frees a short section that rolls into a permanent step.",
        "Pila uvolní krátký díl, který se skutálí do podoby trvalého schodu.");
    auto& fallenFir = ensureHotspot(world, 27, "fallen_fir",
        tr("FALLEN FIR", "PADLÁ JEDLE"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    fallenFir.visibleWhen = {e2d::Condition::notFlag("fir_cut")};
    fallenFir.visuals = {
        line(58, 231, 169, 163, P::brown), line(63, 239, 174, 171, P::brown),
        line(90, 214, 76, 192, P::brightGreen), line(116, 198, 103, 174, P::brightGreen),
        line(141, 183, 129, 161, P::brightGreen), circle(113, 203, 7, amber, false),
    };
    auto& cutFir = ensureHotspot(world, 27, "fallen_fir_complete",
        tr("CUT FIR STEP", "SCHOD Z ROZŘEZANÉ JEDLE"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    cutFir.visuals = {
        line(58, 231, 101, 205, P::brown), line(63, 239, 106, 213, P::brown),
        line(132, 198, 174, 171, P::brown), line(137, 206, 179, 179, P::brown),
        circle(104, 209, 12, amber, false), circle(134, 202, 12, amber, false),
    };
    gateRight(world, 27, {e2d::Condition::flag("fir_cut")},
        "The fallen fir blocks the service road.", "Padlá jedle blokuje servisní cestu.");
    addPickup(world, 29, "signal_flare", "The emergency box still contains one dry signal flare.",
        "Nouzová skříňka stále obsahuje jednu suchou světlici.", 0);
    addUse(world, 30, "theo_branch", "BRANCH PINNING THEO", "VĚTEV NA THEOVI", "pruning_saw", "theo_freed",
        "Iris cuts the light branch into safe sections and frees Theo's leg.",
        "Iris rozřeže lehkou větev na bezpečné kusy a uvolní Theovu nohu.");
    addUse(world, 30, "theo_wound", "THEO'S WOUND", "THEOVO ZRANĚNÍ", "bandage_roll", "theo_rescued",
        "The bandage stops the bleeding. Theo can finally speak clearly.",
        "Obvaz zastaví krvácení. Theo konečně může jasně mluvit.",
        {e2d::Condition::flag("theo_freed")}, true, 2);
    addCharacter(world, 30, "theo", "THEO GRAY", "THEO GRAY", "theo_briefed", {
        speech(tr("Theo: Those surveyors cut the relay line before the rain. They carried a red coil toward the quarry.",
            "Theo: Ti průzkumníci přeřízli vedení před deštěm. Nesli červenou cívku k lomu.")),
        speech(tr("Iris: Rest here. I need your cache and the route they used.",
            "Iris: Odpočívej. Potřebuji tvůj sklad a cestu, kterou použili."), e2d::MessageSpeaker::player),
        speech(tr("Theo: Combination 2-7-1. Nell at the lookout can mark the ravine.",
            "Theo: Kombinace 2-7-1. Nell na hlásce ti označí rokli.")),
    }, {e2d::Condition::flag("theo_rescued")});
    addPickup(world, 31, "climbing_rope", "Theo's climbing rope is dry inside the cache.",
        "Theovo horolezecké lano je ve skladu suché.", 0, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "iron_hook", "You take the iron service-anchor hook.",
        "Vezmeš železný hák pro servisní kotvy.", 1, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "mine_lamp", "A rugged mine lamp joins the pack.",
        "Do batohu přibude odolná důlní lampa.", 2, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "compass", "Theo's liquid compass settles without a tremor.",
        "Theův kapalinový kompas se ustálí bez zachvění.", 3, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "ranger_patch", "Theo presses his service patch into Iris's hand as a promise to answer her final call.",
        "Theo vloží Iris do ruky svou služební nášivku jako slib, že odpoví na její závěrečné volání.", 4,
        {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 32, "charcoal", "You bag clean hardwood charcoal for a future filter.",
        "Nabereš čisté dřevěné uhlí pro budoucí filtr.", 0);
    addUse(world, 33, "bearing_route", "ECHO GROVE BEARING", "NÁMĚR V HÁJI OZVĚN", "compass", "echo_route_solved",
        "With the mast bearing, Iris chooses north-east, north, then east.",
        "Podle náměru ze stožáru Iris zvolí severovýchod, sever a východ.",
        {e2d::Condition::flag("mast_calibrated")});
    auto& bearingMarker = ensureHotspot(world, 33, "bearing_route",
        tr("ECHO GROVE BEARING", "NÁMĚR V HÁJI OZVĚN"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    bearingMarker.visibleWhen = {e2d::Condition::notFlag("echo_route_solved")};
    bearingMarker.visuals = {
        circle(111, 190, 31, P::lightGray, false), circle(111, 190, 5, amber),
        line(111, 190, 128, 166, danger), line(111, 190, 96, 216, signalBlue),
        label(94, 226, tr("017?", "017?"), amber),
    };
    auto& solvedBearing = ensureHotspot(world, 33, "bearing_route_complete",
        tr("SOLVED BEARING 017", "VYŘEŠENÝ NÁMĚR 017"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    solvedBearing.visuals = {
        circle(111, 190, 31, P::lightGray, false), circle(111, 190, 5, amber),
        line(111, 190, 128, 166, P::brightGreen),
        label(94, 226, tr("017", "017"), P::brightGreen),
    };
    addUse(world, 34, "cable_posts", "BURIED CABLE POSTS", "SLOUPKY ZAKOPANÉHO KABELU", "multimeter", "quarry_trace_found",
        "Three rising readings point away from the tower and into the quarry.",
        "Tři rostoucí hodnoty míří od věže do lomu.");
    auto& cablePosts = ensureHotspot(world, 34, "cable_posts",
        tr("BURIED CABLE POSTS", "SLOUPKY ZAKOPANÉHO KABELU"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    cablePosts.visibleWhen = {e2d::Condition::notFlag("quarry_trace_found")};
    cablePosts.visuals = {
        box(73, 171, 13, 69, P::lightGray), box(105, 157, 13, 83, P::lightGray),
        box(137, 143, 13, 97, P::lightGray), circle(79, 166, 5, signalBlue),
        circle(111, 152, 5, amber), circle(143, 138, 5, danger),
        e2d::PolylineVisual{{{79, 227}, {111, 216}, {143, 202}}, signalBlue, false},
    };
    auto& tracedCable = ensureHotspot(world, 34, "cable_posts_complete",
        tr("TRACED CABLE TO QUARRY", "KABEL VYSTOPOVANÝ K LOMU"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    tracedCable.visuals = {
        box(73, 171, 13, 69, P::lightGray), box(105, 157, 13, 83, P::lightGray),
        box(137, 143, 13, 97, P::lightGray), circle(79, 166, 5, P::brightGreen),
        circle(111, 152, 5, P::brightGreen), circle(143, 138, 5, P::brightGreen),
        e2d::PolylineVisual{{{79, 227}, {111, 216}, {143, 202}}, P::brightGreen, false},
    };
    addUse(world, 35, "bear_wind", "UPWIND EDGE", "NÁVĚTRNÁ HRANA", "signal_flare", "bear_gone",
        "The flare burns from upwind. The bear sniffs, turns and leaves unharmed.",
        "Světlice hoří proti větru. Medvěd zavětří, otočí se a bez úhony odejde.", {}, true);
    auto& bearMeadow = room(world, 35);
    bearMeadow.decorations.clear();
    bearMeadow.solids.clear();
    bearMeadow.animations.clear();
    addForestArt(bearMeadow, 35, false);
    auto& upwindEdge = ensureHotspot(world, 35, "bear_wind",
        tr("UPWIND EDGE", "NÁVĚTRNÁ HRANA"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    upwindEdge.visibleWhen = {e2d::Condition::notFlag("bear_gone")};
    upwindEdge.visuals = {
        e2d::PolygonVisual{{{55, 183}, {137, 183}, {158, 197}, {137, 211}, {55, 211}}, signalBlue, true},
        label(68, 192, tr("UPWIND", "NÁVĚTŘÍ"), P::black),
        line(74, 165, 102, 157, pale), line(102, 157, 124, 165, pale),
        ellipse(287, 198, 67, 39, P::black), circle(342, 171, 31, P::black),
        circle(328, 143, 11, P::black), circle(354, 143, 11, P::black),
        circle(353, 168, 3, amber), line(248, 225, 248, 253, P::black),
        line(312, 225, 312, 253, P::black),
    };
    auto& clearedMeadow = ensureHotspot(world, 35, "bear_wind_complete",
        tr("CLEAR MEADOW PATH", "VOLNÁ CESTA PŘES LOUKU"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    clearedMeadow.visuals = {
        e2d::PolygonVisual{{{331, 183}, {418, 183}, {449, 197}, {418, 211}, {331, 211}}, P::brightGreen, true},
        label(347, 192, tr("TRAIL CLEAR", "CESTA VOLNÁ"), P::black),
        ellipse(461, 211, 18, 10, P::black), circle(477, 204, 8, P::black),
    };
    bearMeadow.animations.push_back({targetId(35, "bear_warning"), true, true,
        {e2d::Condition::notFlag("bear_gone")}, {
            {12, {line(365, 178, 384, 182, danger), line(84, 165, 107, 157, pale)}},
            {12, {line(365, 182, 384, 178, danger), line(84, 157, 107, 165, pale)}},
        }});
    gateRight(world, 35, {e2d::Condition::flag("bear_gone")},
        "Iris backs away from the bear. Observe the wind ribbon and use the flare from the UPWIND marker.",
        "Iris před medvědem ustoupí. Sleduj stužku ve větru a použij světlici u značky NÁVĚTŘÍ.");
    addUse(world, 37, "weather_recorder", "STORM DATA RECORDER", "ZÁZNAMNÍK BOUŘKOVÝCH DAT",
        "hand_crank_torch", "weather_data_read",
        "The torch's charging lead wakes the chart drum: the sabotage began at 02:11, six minutes before the storm front.",
        "Nabíjecí kabel svítilny probudí buben: sabotáž začala ve 02:11, šest minut před bouřkovou frontou.");
    auto& weatherRecorder = ensureHotspot(world, 37, "weather_recorder",
        tr("STORM DATA RECORDER", "ZÁZNAMNÍK BOUŘKOVÝCH DAT"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    weatherRecorder.visibleWhen = {e2d::Condition::notFlag("weather_data_read")};
    weatherRecorder.visuals = {
        box(71, 151, 80, 91, P::lightGray), box(79, 159, 64, 75, P::black),
        circle(96, 183, 13, pale, false), line(96, 183, 104, 173, danger),
        box(116, 171, 19, 49, pale), line(119, 179, 132, 179, P::blue),
        line(119, 189, 132, 189, P::blue), line(119, 199, 132, 199, P::blue),
    };
    auto& recordedWeather = ensureHotspot(world, 37, "weather_recorder_complete",
        tr("RECORDED STORM DATA 02:11", "ZAZNAMENANÁ DATA BOUŘE 02:11"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    recordedWeather.visuals = {
        box(71, 151, 80, 91, P::lightGray), box(79, 159, 64, 75, P::black),
        circle(96, 183, 13, P::brightGreen, false), line(96, 183, 105, 175, P::brightGreen),
        box(112, 169, 31, 55, pale), line(116, 179, 138, 179, P::blue),
        line(116, 189, 138, 189, P::blue), label(115, 207, tr("02:11", "02:11"), danger),
        circle(84, 223, 4, P::brightGreen),
    };
    addCharacter(world, 38, "nell", "NELL HARKER", "NELL HARKEROVÁ", "lookout_briefed", {
        speech(tr("Nell: Three false surveyors, one quarry hoist, and a red light moving underground.",
            "Nell: Tři falešní průzkumníci, jeden lomový naviják a červené světlo v podzemí.")),
        speech(tr("Nell: Kestrel Six is flashing beyond the ridge. Weak, but alive. I marked the ravine route.",
            "Nell: Kestrel Six bliká za hřebenem. Slabě, ale žije. Označila jsem cestu roklí.")),
        speech(tr("Iris: Keep watching the beacon. I will open the old infrastructure.",
            "Iris: Sleduj maják. Já otevřu starou infrastrukturu."), e2d::MessageSpeaker::player),
    });

    // Ravine access descends below the broken bridge, then climbs through the
    // quarry. Doors, ladders and the hoist walkway express vertical links that
    // cannot be represented honestly by catalogue-order edge exits.
    setHorizontalRoute(world, 39, 36, 40);
    setHorizontalRoute(world, 40, 39, 44);
    setHorizontalRoute(world, 41, std::nullopt, 42);
    setHorizontalRoute(world, 42, 41, 43);
    setHorizontalRoute(world, 43, 42, 44);
    setHorizontalRoute(world, 44, 43, std::nullopt);
    for (int branch = 45; branch <= 48; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }
    setHorizontalRoute(world, 49, 47, 50);
    setHorizontalRoute(world, 50, 49, 51);

    addPortal(world, 41, "cliff_rope", "FIXED ROPE TO RAVINE LIP", "PEVNÉ LANO K HRANĚ ROKLE",
        {365, 132, 119, 128}, 39, {
            circle(421, 143, 11, P::lightGray, false),
            e2d::PolylineVisual{{{421, 154}, {407, 180}, {430, 204}, {410, 257}}, amber, false},
            label(383, 219, tr("CLIMB", "VYLÉZT"), amber),
        }, {e2d::Condition::flag("ravine_rope_fixed")});
    addPortal(world, 44, "quarry_path", "STAIRS TO QUARRY GATE", "SCHODY K BRÁNĚ LOMU",
        {350, 132, 134, 128}, 45, {
            line(365, 250, 452, 151, amber), line(389, 250, 476, 151, amber),
            line(378, 232, 402, 232, pale), line(393, 214, 417, 214, pale),
            line(409, 196, 433, 196, pale), line(425, 178, 449, 178, pale),
            label(370, 137, tr("QUARRY", "LOM"), amber),
        });
    auto& bridgePath = addPortal(world, 44, "bridge_path", "HOIST WALKWAY TO WEST BRIDGE",
        "LÁVKA NAVIJÁKU K ZÁPADNÍMU MOSTU", {42, 151, 166, 109}, 40, {
            e2d::PolygonVisual{{{50, 195}, {75, 181}, {190, 181}, {190, 209}, {75, 209}}, signalBlue, true},
            label(78, 190, tr("WEST BRIDGE", "ZÁPADNÍ MOST"), P::black),
        }, {e2d::Condition::flag("hoist_running")});
    world.addInteraction({e2d::Verb::context, bridgePath.id, std::nullopt,
        {e2d::Condition::notFlag("hoist_running")},
        {inspect(tr("Only empty hoist cables cross the flooded span. The east landing must deploy its walkway.",
            "Přes rozvodněný úsek vedou jen prázdná lana navijáku. Východní stanice musí vysunout lávku."))},
        {}, 10, {}});

    addPortal(world, 45, "floor_path", "STAIRS TO RAVINE FLOOR", "SCHODY NA DNO ROKLE",
        {0, 132, 84, 128}, 44, {
            line(9, 151, 66, 250, amber), line(30, 151, 87, 250, amber),
            line(21, 173, 43, 173, pale), line(33, 193, 55, 193, pale),
            line(45, 213, 67, 213, pale), label(9, 137, tr("DOWN", "DOLŮ"), amber),
        });
    auto& officeGate = addPortal(world, 45, "office_gate", "OPEN GATE TO QUARRY OFFICE",
        "OTEVŘENÁ BRÁNA KE KANCELÁŘI LOMU", {370, 132, 114, 128}, 46, {
            box(382, 151, 90, 104, P::lightGray, false),
            line(390, 159, 464, 247, P::lightGray), line(464, 159, 390, 247, P::lightGray),
            label(391, 137, tr("OFFICE", "KANCELÁŘ"), amber),
        }, {e2d::Condition::flag("quarry_gate_open")});
    world.addInteraction({e2d::Verb::context, officeGate.id, std::nullopt,
        {e2d::Condition::notFlag("quarry_gate_open")},
        {inspect(tr("A rusted chain seals the quarry gate. The key is caught behind the waterfall sluice.",
            "Rezavý řetěz uzavírá bránu lomu. Klíč vězí za stavidlem u vodopádu."))},
        {}, 10, {}});
    addPortal(world, 46, "gate_path", "DOOR TO QUARRY GATE", "DVEŘE K BRÁNĚ LOMU",
        {0, 126, 72, 134}, 45, {
            box(10, 136, 50, 124, P::brown), circle(50, 199, 3, amber),
            label(15, 150, tr("GATE", "BRÁNA"), pale),
        });
    addPortal(world, 46, "crusher_door", "DOOR TO CRUSHER DECK", "DVEŘE K DRTIČI",
        {390, 126, 94, 134}, 47, {
            box(402, 136, 71, 124, P::red), circle(463, 199, 3, amber),
            label(409, 150, tr("CRUSHER", "DRTIČ"), pale),
        });
    addPortal(world, 47, "office_door", "DOOR TO QUARRY OFFICE", "DVEŘE DO KANCELÁŘE LOMU",
        {0, 126, 55, 134}, 46, {
            box(8, 136, 39, 124, P::brown), circle(39, 199, 3, amber),
            label(9, 150, tr("OFFICE", "KANCELÁŘ"), pale),
        });
    auto& magazineDoor = addPortal(world, 47, "magazine_door", "DOOR TO EQUIPMENT MAGAZINE",
        "DVEŘE DO SKLADU VYBAVENÍ", {375, 126, 109, 134}, 48, {
            box(387, 136, 86, 124, P::brown), circle(463, 199, 3, amber),
            label(395, 150, tr("MAGAZINE", "SKLAD"), pale),
        }, {e2d::Condition::flag("brant_secured")});
    world.addInteraction({e2d::Verb::context, magazineDoor.id, std::nullopt,
        {e2d::Condition::notFlag("brant_secured")},
        {warning(tr("Brant and the moving crusher block the magazine. Use Owen's horn plan first.",
            "Brant a pohybující se drtič blokují sklad. Nejprve použij Owenův plán s houkačkou."))},
        {}, 10, {}, "warning"});
    addPortal(world, 47, "tunnel_path", "MARKED PATH TO QUARRY TUNNEL", "OZNAČENÁ CESTA DO LOMOVÉHO TUNELU",
        {174, 151, 82, 109}, 49, {
            e2d::PolygonVisual{{{181, 181}, {233, 181}, {251, 195}, {233, 209}, {181, 209}}, signalBlue, true},
            label(186, 190, tr("TUNNEL", "TUNEL"), P::black),
        }, {e2d::Condition::flag("brant_secured")});
    addPortal(world, 48, "crusher_door", "DOOR TO CRUSHER DECK", "DVEŘE K DRTIČI",
        {0, 126, 78, 134}, 47, {
            box(10, 136, 56, 124, P::brown), circle(56, 199, 3, amber),
            label(15, 150, tr("CRUSHER", "DRTIČ"), pale),
        });
    auto& westWalkway = addPortal(world, 50, "bridge_walkway", "CABLE WALKWAY TO WEST BRIDGE",
        "KABELOVÁ LÁVKA K ZÁPADNÍMU MOSTU", {320, 132, 164, 128}, 40, {
            box(401, 146, 48, 91, P::darkGray, false),
            line(407, 152, 443, 231, P::lightGray), line(443, 152, 407, 231, P::lightGray),
            label(333, 225, tr("WEST BRIDGE", "ZÁPADNÍ MOST"), amber),
        }, {e2d::Condition::flag("hoist_running")});
    world.addInteraction({e2d::Verb::context, westWalkway.id, std::nullopt,
        {e2d::Condition::notFlag("hoist_running")},
        {inspect(tr("The west cable walkway is still folded against the hoist frame.",
            "Západní kabelová lávka je stále složená u rámu navijáku."))}, {}, 10, {}});
    room(world, 50).hotspots.push_back({targetId(50, "bridge_walkway_deployed"),
        tr("DEPLOYED WEST WALKWAY", "VYSUNUTÁ ZÁPADNÍ LÁVKA"), {0, 0, 0, 0},
        e2d::HotspotKind::scenery, {e2d::Condition::flag("hoist_running")}, {
            line(333, 184, 470, 184, amber), line(333, 218, 470, 218, amber),
            line(333, 184, 333, 218, P::lightGray), line(470, 184, 470, 218, P::lightGray),
            line(350, 184, 369, 218, P::lightGray), line(369, 218, 388, 184, P::lightGray),
            line(407, 184, 426, 218, P::lightGray), line(426, 218, 445, 184, P::lightGray),
            label(351, 225, tr("WEST BRIDGE", "ZÁPADNÍ MOST"), P::brightGreen),
        }});
    auto& hoistWalkway = addPortal(world, 40, "hoist_walkway", "CABLE WALKWAY TO EAST HOIST",
        "KABELOVÁ LÁVKA K VÝCHODNÍMU NAVIJÁKU", {310, 132, 174, 128}, 50, {
            line(322, 184, 371, 218, P::lightGray), line(322, 218, 371, 184, P::lightGray),
            line(422, 184, 470, 218, P::lightGray), line(422, 218, 470, 184, P::lightGray),
            label(343, 225, tr("EAST HOIST", "VÝCHODNÍ NAVIJÁK"), danger),
        }, {e2d::Condition::flag("hoist_running")});
    world.addInteraction({e2d::Verb::context, hoistWalkway.id, std::nullopt,
        {e2d::Condition::notFlag("hoist_running")},
        {inspect(tr("The bridge ends above floodwater. The quarry hoist must deploy a cable walkway.",
            "Most končí nad rozvodněnou vodou. Lomový naviják musí vysunout kabelovou lávku."))}, {}, 10, {}});
    room(world, 40).hotspots.push_back({targetId(40, "hoist_walkway_deployed"),
        tr("DEPLOYED EAST WALKWAY", "VYSUNUTÁ VÝCHODNÍ LÁVKA"), {0, 0, 0, 0},
        e2d::HotspotKind::scenery, {e2d::Condition::flag("hoist_running")}, {
            line(322, 184, 470, 184, amber), line(322, 218, 470, 218, amber),
            line(322, 184, 322, 218, P::lightGray), line(470, 184, 470, 218, P::lightGray),
            line(341, 184, 360, 218, P::lightGray), line(360, 218, 379, 184, P::lightGray),
            line(398, 184, 417, 218, P::lightGray), line(417, 218, 436, 184, P::lightGray),
            label(343, 225, tr("EAST HOIST", "VÝCHODNÍ NAVIJÁK"), P::brightGreen),
        }});

    addUse(world, 39, "anchor_eye", "ANCHOR EYE", "KOTEVNÍ OKO", "iron_hook", "hook_fixed",
        "The iron hook seats behind the service anchor with a solid knock.",
        "Železný hák pevně zapadne za servisní kotvu.", {}, true);
    auto& anchorEye = ensureHotspot(world, 39, "anchor_eye",
        tr("ANCHOR EYE", "KOTEVNÍ OKO"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    anchorEye.visibleWhen = {e2d::Condition::notFlag("hook_fixed")};
    anchorEye.visuals = {
        circle(111, 181, 19, P::lightGray, false), circle(111, 181, 8, P::black, false),
        line(92, 181, 69, 226, P::darkGray), line(130, 181, 153, 226, P::darkGray),
        label(84, 232, tr("ANCHOR", "KOTVA"), amber),
    };
    addUse(world, 39, "fixed_hook", "FIXED IRON HOOK", "UPEVNĚNÝ ŽELEZNÝ HÁK", "climbing_rope", "ravine_rope_fixed",
        "Iris ties, tests and fixes the climbing rope down the ravine wall.",
        "Iris lano uváže, vyzkouší a upevní dolů po stěně rokle.",
        {e2d::Condition::flag("hook_fixed")}, true, 1);
    auto& fixedHook = ensureHotspot(world, 39, "fixed_hook",
        tr("FIXED IRON HOOK", "UPEVNĚNÝ ŽELEZNÝ HÁK"),
        {164, 135, 96, 125}, e2d::HotspotKind::mechanism, 1);
    fixedHook.visibleWhen = {e2d::Condition::flag("hook_fixed"), e2d::Condition::notFlag("ravine_rope_fixed")};
    fixedHook.visuals = {
        circle(212, 181, 19, P::lightGray, false),
        e2d::PolylineVisual{{{206, 173}, {218, 181}, {208, 193}, {197, 184}}, amber, false},
        label(189, 214, tr("HOOK", "HÁK"), amber),
    };
    addContext(world, 39, "rope_descent", "FIXED DESCENT ROPE", "PEVNÉ SESTUPOVÉ LANO",
        "ravine_descended", {inspect(tr("Iris descends below the broken bridge and reaches the west ravine floor.",
            "Iris sestoupí pod zřícený most a dorazí na západní dno rokle."))},
        {e2d::Condition::flag("ravine_rope_fixed")},
        {e2d::Mutation::moveTo(std::string{screen(41).id})}, 3, "climb");
    auto& descentRope = ensureHotspot(world, 39, "rope_descent",
        tr("FIXED DESCENT ROPE", "PEVNÉ SESTUPOVÉ LANO"),
        {369, 137, 92, 123}, e2d::HotspotKind::mechanism, 3);
    descentRope.visibleWhen = {e2d::Condition::flag("ravine_rope_fixed")};
    descentRope.visuals = {
        circle(414, 151, 12, P::lightGray, false),
        e2d::PolylineVisual{{{414, 163}, {393, 188}, {422, 214}, {398, 259}}, amber, false},
        label(378, 225, tr("DESCEND", "SESTOUPIT"), amber),
    };
    world.addInteraction({e2d::Verb::context, descentRope.id, std::nullopt,
        {e2d::Condition::flag("ravine_rope_fixed"), e2d::Condition::flag("ravine_descended")}, {},
        {e2d::Mutation::moveTo(std::string{screen(41).id})}, 20, {}, "climb"});
    gateRight(world, 40, {e2d::Condition::flag("hoist_running")},
        "The bridge ends safely at the missing span. The quarry hoist must deploy its walkway.",
        "Most bezpečně končí u chybějící části. Lomový naviják musí vysunout lávku.");
    addPickup(world, 41, "old_relay_badge", "You rinse a silted 1964 relay badge and keep the mountain's older memory.",
        "Opláchneš z nánosu odznak převaděče z roku 1964 a uchováš starší paměť hory.", 0,
        {e2d::Condition::flag("ravine_rope_fixed")});
    addUse(world, 42, "dark_culvert", "DARK CULVERT MARKERS", "TMAVÉ ZNAČKY V PROPUSTKU",
        "hand_crank_torch", "culvert_lit",
        "The crank torch reveals red survey paint through the culvert and rats scatter harmlessly into a drain.",
        "Ruční svítilna odhalí červenou průzkumnickou barvu v propustku a krysy neškodně utečou do odtoku.");
    auto& darkCulvert = ensureHotspot(world, 42, "dark_culvert",
        tr("DARK CULVERT MARKERS", "TMAVÉ ZNAČKY V PROPUSTKU"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    darkCulvert.visibleWhen = {e2d::Condition::notFlag("culvert_lit")};
    darkCulvert.visuals = {
        e2d::ArcVisual{{111, 234}, {42, 73}, 3.14159F, 6.28318F, P::darkGray},
        box(69, 191, 84, 49, P::black), circle(111, 210, 6, P::darkGray),
        label(85, 228, tr("DARK", "TMA"), P::lightGray),
    };
    auto& litCulvert = ensureHotspot(world, 42, "dark_culvert_complete",
        tr("LIT CULVERT ROUTE", "OSVĚTLENÁ CESTA PROPUSTKEM"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    litCulvert.visuals = {
        e2d::ArcVisual{{111, 234}, {42, 73}, 3.14159F, 6.28318F, P::lightGray},
        e2d::PolygonVisual{{{79, 234}, {142, 234}, {126, 183}, {96, 183}}, amber, true},
        line(94, 217, 135, 204, danger), circle(111, 210, 6, pale),
    };
    gateRight(world, 42, {e2d::Condition::flag("culvert_lit")},
        "The culvert is too dark to cross without a portable light.",
        "Propustek je příliš tmavý, než aby jím šlo projít bez přenosného světla.");
    addUse(world, 43, "sluice", "SMALL SLUICE", "MALÉ STAVIDLO", "wrench", "sluice_closed",
        "The wrench closes the sluice and weakens the waterfall current.",
        "Klíč zavře stavidlo a oslabí proud vodopádu.");
    auto& sluice = ensureHotspot(world, 43, "sluice",
        tr("SMALL SLUICE", "MALÉ STAVIDLO"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    sluice.visibleWhen = {e2d::Condition::notFlag("sluice_closed")};
    sluice.visuals = {
        box(72, 171, 78, 67, P::lightGray), box(80, 179, 62, 51, signalBlue),
        circle(111, 186, 22, danger, false), circle(111, 186, 5, amber),
        line(89, 186, 133, 186, danger), line(111, 164, 111, 208, danger),
    };
    auto& closedSluice = ensureHotspot(world, 43, "sluice_complete",
        tr("CLOSED SLUICE", "ZAVŘENÉ STAVIDLO"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    closedSluice.visuals = {
        box(72, 171, 78, 67, P::lightGray), box(80, 179, 62, 51, P::blue),
        circle(111, 186, 22, P::brightGreen, false), circle(111, 186, 5, amber),
        line(96, 171, 126, 201, P::brightGreen), line(126, 171, 96, 201, P::brightGreen),
        box(82, 214, 58, 12, P::darkGray),
    };
    gateRight(world, 43, {e2d::Condition::flag("sluice_closed")},
        "The waterfall current pushes Iris safely back to the shelf. Close the small sluice first.",
        "Proud vodopádu bezpečně zatlačí Iris zpět na římsu. Nejprve zavři malé stavidlo.");
    addPickup(world, 43, "quarry_office_key", "The quarry office key comes free from the quiet grate.",
        "Klíč od kanceláře lomu se uvolní z klidné mříže.", 2,
        {e2d::Condition::flag("sluice_closed")});
    addUse(world, 45, "quarry_gate", "QUARRY GATE", "BRÁNA LOMU", "quarry_office_key", "quarry_gate_open",
        "The rusted key opens the quarry. Voss's field radio lights at once.",
        "Rezavý klíč otevře lom. Vossovo polní rádio se okamžitě rozsvítí.");
    auto& lockedQuarryGate = ensureHotspot(world, 45, "quarry_gate",
        tr("QUARRY GATE", "BRÁNA LOMU"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    lockedQuarryGate.visibleWhen = {e2d::Condition::notFlag("quarry_gate_open")};
    lockedQuarryGate.visuals = {
        line(74, 190, 148, 190, P::lightGray), line(79, 181, 143, 201, P::lightGray),
        box(99, 177, 27, 32, amber), circle(112, 188, 4, P::black),
        label(85, 218, tr("LOCKED", "ZAMČENO"), danger),
    };
    auto& openQuarryGate = ensureHotspot(world, 45, "quarry_gate_complete",
        tr("OPEN QUARRY GATE", "OTEVŘENÁ BRÁNA LOMU"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    openQuarryGate.visuals = {
        box(99, 177, 27, 32, P::brightGreen, false),
        label(82, 218, tr("GATE OPEN", "BRÁNA OTEVŘENA"), P::brightGreen),
    };
    addCharacter(world, 46, "owen", "OWEN FINCH", "OWEN FINCH", "owen_freed", {
        speech(tr("Owen: Voss locked me in here when I refused to run the crusher for him.",
            "Owen: Voss mě tu zamkl, když jsem mu odmítl spustit drtič.")),
        speech(tr("Owen: Take this pulley pin. The crusher horn will draw Brant into the inspection cage.",
            "Owen: Vezmi tenhle čep. Houkačka drtiče naláká Branta do kontrolní klece.")),
        speech(tr("Iris: Then nobody gets hurt, and the hoist gets us across.",
            "Iris: Pak se nikomu nic nestane a naviják nás dostane přes rokli."), e2d::MessageSpeaker::player),
    }, {}, {e2d::Mutation::addItem("pulley_pin"), e2d::Mutation::setFlag("crusher_horn_known")});
    addContext(world, 47, "crusher_horn", "CRUSHER HORN", "HOUKAČKA DRTIČE", "horn_sounded",
        {inspect(tr("The horn blasts. Brant swears and enters the inspection cage.",
            "Houkačka zaduní. Brant zakleje a vleze do kontrolní klece."))},
        {e2d::Condition::flag("crusher_horn_known")}, {}, 0, "warning");
    addUse(world, 47, "inspection_cage", "INSPECTION CAGE", "KONTROLNÍ KLEC", "brass_key", "brant_secured",
        "The master key locks the empty outer gate. Brant is contained, angry and unharmed.",
        "Hlavní klíč zamkne vnější branku. Brant je zadržený, rozzlobený a nezraněný.",
        {e2d::Condition::flag("horn_sounded")}, false, 2);
    addHazard(world, 47, "crusher_belt", "brant_secured",
        "The active crusher belt carries Iris beneath the descending jaw.",
        "Aktivní pás odnese Iris pod klesající čelist drtiče.");
    addPickup(world, 48, "red_phase_coil", "You lift Nightjar's pulsing red phase coil into its padded case.",
        "Uložíš pulzující červenou fázovou cívku Nightjaru do pouzdra.", 0);
    addPickup(world, 48, "survey_notebook", "Voss's notebook links the false survey crew to Nightjar.",
        "Vossův zápisník spojuje falešné průzkumníky s Nightjarem.", 1);
    addPickup(world, 48, "quartz_sample", "A blue quartz shard catches the mine lamp.",
        "Modrý úlomek křemene zachytí světlo důlní lampy.", 2);
    addPickup(world, 48, "siphon_hose", "A spare fuel-safe siphon hose hangs inside the dry magazine.",
        "Ve suchém skladu visí náhradní hadice vhodná pro přečerpávání paliva.", 3);
    addUse(world, 49, "tunnel_lamp", "DARK TUNNEL LAMP MARKERS", "TMAVÉ ZNAČKY V TUNELU",
        "mine_lamp", "quarry_tunnel_lit",
        "Theo's mine lamp reveals the signal cabinet and a harmless cart shadow on the curved wall.",
        "Theova důlní lampa odhalí signální skříň a neškodný stín vozíku na zakřivené stěně.");
    auto& tunnelLamp = ensureHotspot(world, 49, "tunnel_lamp",
        tr("DARK TUNNEL LAMP MARKERS", "TMAVÉ ZNAČKY V TUNELU"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    tunnelLamp.visibleWhen = {e2d::Condition::notFlag("quarry_tunnel_lit")};
    tunnelLamp.visuals = {
        e2d::ArcVisual{{111, 236}, {43, 78}, 3.14159F, 6.28318F, P::darkGray},
        box(68, 192, 86, 48, P::black), circle(111, 211, 6, P::darkGray),
        label(84, 229, tr("NO LIGHT", "BEZ SVĚTLA"), P::lightGray),
    };
    auto& litTunnel = ensureHotspot(world, 49, "tunnel_lamp_complete",
        tr("LIT QUARRY TUNNEL", "OSVĚTLENÝ LOMOVÝ TUNEL"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    litTunnel.visuals = {
        e2d::ArcVisual{{111, 236}, {43, 78}, 3.14159F, 6.28318F, P::lightGray},
        e2d::PolygonVisual{{{77, 236}, {145, 236}, {128, 181}, {94, 181}}, amber, true},
        circle(111, 211, 7, pale),
    };
    addUse(world, 49, "hoist_signal", "BROKEN HOIST SIGNAL", "PŘERUŠENÁ SIGNALIZACE NAVIJÁKU", "multimeter", "hoist_signal_fixed",
        "The meter identifies the crossed pair; Iris restores a steady green signal.",
        "Multimetr najde zkřížený pár a Iris obnoví stálý zelený signál.",
        {e2d::Condition::flag("quarry_tunnel_lit")}, false, 2);
    auto& hoistSignal = ensureHotspot(world, 49, "hoist_signal",
        tr("BROKEN HOIST SIGNAL", "PŘERUŠENÁ SIGNALIZACE NAVIJÁKU"),
        {265, 135, 96, 125}, e2d::HotspotKind::mechanism, 2);
    hoistSignal.visibleWhen = {e2d::Condition::flag("quarry_tunnel_lit"), e2d::Condition::notFlag("hoist_signal_fixed")};
    hoistSignal.visuals = {
        box(275, 156, 76, 81, P::darkGray), box(283, 164, 60, 65, P::black),
        circle(299, 184, 8, danger), circle(327, 184, 8, P::darkGray),
        line(291, 214, 307, 202, signalBlue), line(319, 202, 335, 214, signalBlue),
        label(287, 221, tr("SIGNAL", "SIGNÁL"), danger),
    };
    auto& fixedHoistSignal = ensureHotspot(world, 49, "hoist_signal_complete",
        tr("GREEN HOIST SIGNAL", "ZELENÁ SIGNALIZACE NAVIJÁKU"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    fixedHoistSignal.visuals = {
        box(275, 156, 76, 81, P::darkGray), box(283, 164, 60, 65, P::black),
        circle(299, 184, 8, P::darkGray), circle(327, 184, 8, P::brightGreen),
        line(291, 214, 335, 202, P::brightGreen),
        label(287, 221, tr("SIGNAL", "SIGNÁL"), P::brightGreen),
    };
    gateRight(world, 49, {e2d::Condition::flag("quarry_tunnel_lit"), e2d::Condition::flag("hoist_signal_fixed")},
        "The east landing needs light and a repaired green hoist signal.",
        "Východní stanice vyžaduje světlo a opravenou zelenou signalizaci navijáku.");
    addUse(world, 50, "hoist_pulley", "HOIST PULLEY", "KLADKA NAVIJÁKU", "pulley_pin", "pulley_repaired",
        "The machined pin restores the hoist pulley.", "Obrobený čep obnoví kladku navijáku.", {}, true);
    addContext(world, 50, "hoist_controls", "HOIST CONTROLS", "OVLÁDÁNÍ NAVIJÁKU", "hoist_running", {
        inspect(tr("The hoist lowers a service cage and draws a cable walkway across the ravine.",
            "Naviják spustí servisní klec a natáhne přes rokli kabelovou lávku.")),
        speech(tr("Iris: The red coil proves the trail. Now I need to reach Nightjar before midnight.",
            "Iris: Červená cívka potvrzuje stopu. Teď musím dorazit k Nightjaru před půlnocí."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("hoist_signal_fixed"), e2d::Condition::flag("pulley_repaired"),
        e2d::Condition::flag("brant_secured")}, {e2d::Mutation::setFlag("act2_complete")}, 2, "power");
    gateRight(world, 50, {e2d::Condition::flag("act2_complete")},
        "The hoist needs its signal wire, pulley pin and safe crusher deck.",
        "Naviják potřebuje signalizaci, čep kladky a bezpečnou plošinu drtiče.");
    addFollowUpDialogue(world, 30, "theo", "theo_followup", {
        speech(tr("Theo: The quarry is quiet on my receiver. You stopped them without becoming like them.",
            "Theo: Lom je v mém přijímači tichý. Zastavila jsi je, aniž ses jim podobala.")),
        speech(tr("Iris: Save your strength. Answer when the carrier comes back.",
            "Iris: Šetři síly. Odpověz, až se nosná vlna vrátí."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("theo_briefed"), e2d::Condition::flag("act2_complete")});
    addFollowUpDialogue(world, 38, "nell", "nell_followup", {
        speech(tr("Nell: Kestrel's beacon answered twice. Your repairs are giving the valley breathing room.",
            "Nell: Maják Kestrelu odpověděl dvakrát. Tvoje opravy dávají údolí prostor k nadechnutí.")),
    }, {e2d::Condition::flag("lookout_briefed"), e2d::Condition::flag("act2_complete")});
    addFollowUpDialogue(world, 46, "owen", "owen_followup", {
        speech(tr("Owen: Brant can wait in that empty cage until the rangers arrive. The east hoist is yours.",
            "Owen: Brant může v prázdné kleci počkat na strážce. Východní naviják je tvůj.")),
    }, {e2d::Condition::flag("owen_freed"), e2d::Condition::flag("brant_secured")});
}

void configureLoggingCampArtwork(e2d::WorldDefinition& world) {
    const auto resetRoom = [&world](const int number, const P background, const P floor) -> e2d::RoomDefinition& {
        auto& result = room(world, number);
        result.background = background;
        result.decorations.clear();
        result.solids.clear();
        result.animations.clear();
        addGround(result, floor);
        return result;
    };

    auto& road = resetRoom(51, P::brightBlue, P::brown);
    road.decorations.insert(road.decorations.end(), {
        circle(422, 39, 14, amber),
        line(0, 171, 94, 86, P::darkGray), line(94, 86, 181, 171, P::darkGray),
        line(144, 171, 278, 61, P::lightGray), line(278, 61, 414, 171, P::lightGray),
        box(0, 171, 492, 89, P::green),
        e2d::PolygonVisual{{{117, 260}, {205, 183}, {324, 183}, {430, 260}}, P::brown, true},
        line(150, 252, 225, 190, P::darkGray), line(394, 252, 311, 190, P::darkGray),
        box(315, 229, 102, 31, P::blue), line(322, 237, 409, 237, signalBlue),
        line(169, 221, 203, 221, P::black), line(180, 232, 215, 232, P::black),
        box(65, 174, 8, 86, P::brown),
        e2d::PolygonVisual{{{37, 174}, {69, 137}, {101, 174}}, P::green, true},
        label(332, 245, tr("WASHOUT", "VÝMOL"), pale),
    });
    road.animations.push_back({targetId(51, "runoff"), true, true, {}, {
        {8, {line(323, 239, 367, 239, P::brightCyan), line(373, 248, 408, 248, P::cyan)}},
        {8, {line(334, 239, 378, 239, P::brightCyan), line(363, 248, 398, 248, P::cyan)}},
    }});

    auto& yard = resetRoom(52, P::brightBlue, P::brown);
    yard.decorations.insert(yard.decorations.end(), {
        box(0, 145, 492, 115, P::green),
        box(36, 111, 130, 105, P::red),
        e2d::PolygonVisual{{{20, 111}, {101, 67}, {181, 111}}, P::brown, true},
        box(54, 151, 42, 65, P::black), box(112, 139, 38, 37, P::brightBlue),
        box(326, 124, 134, 92, P::brown),
        e2d::PolygonVisual{{{312, 124}, {392, 82}, {474, 124}}, P::red, true},
        box(344, 154, 38, 62, P::black), box(402, 145, 42, 31, P::brightBlue),
        line(275, 67, 275, 181, P::lightGray), circle(275, 67, 6, amber),
        line(275, 67, 242, 40, P::lightGray), line(275, 67, 307, 40, P::lightGray),
        line(275, 67, 242, 94, P::lightGray), line(275, 67, 307, 94, P::lightGray),
        line(0, 245, 492, 245, P::lightGray), line(0, 253, 492, 253, P::lightGray),
    });
    yard.animations.push_back({targetId(52, "wind_vane"), true, true, {}, {
        {12, {line(275, 67, 239, 62, amber)}},
        {12, {line(275, 67, 311, 72, amber)}},
    }});

    auto& mill = resetRoom(53, P::brown, P::darkGray);
    mill.decorations.insert(mill.decorations.end(), {
        box(0, 0, 492, 220, P::brown),
        line(0, 48, 492, 48, P::red), line(0, 96, 492, 96, P::red),
        box(63, 92, 191, 113, P::darkGray), box(74, 103, 169, 91, P::black),
        circle(111, 168, 28, P::lightGray, false), circle(203, 168, 28, P::lightGray, false),
        line(111, 140, 203, 140, amber), line(111, 196, 203, 196, amber),
        box(278, 115, 168, 90, P::red), box(289, 126, 146, 68, P::black),
        line(301, 151, 419, 151, signalBlue), line(301, 171, 419, 171, P::lightGray),
        line(21, 38, 455, 38, P::lightGray), circle(123, 38, 13, P::lightGray, false),
        circle(356, 38, 13, P::lightGray, false),
        label(106, 113, tr("PLANER", "HOBLOVKA"), amber),
    });

    auto& filing = resetRoom(54, P::brown, P::darkGray);
    filing.decorations.insert(filing.decorations.end(), {
        box(0, 0, 492, 220, P::brown),
        box(34, 40, 182, 134, P::darkGray), box(44, 50, 162, 114, P::black),
        line(76, 61, 76, 151, P::lightGray), circle(76, 70, 12, P::lightGray, false),
        line(121, 64, 181, 147, amber), line(121, 147, 181, 64, amber),
        box(251, 63, 191, 111, P::red), box(261, 73, 171, 91, P::black),
        circle(310, 118, 31, P::lightGray, false), line(310, 87, 310, 149, danger),
        box(356, 92, 54, 43, P::brown, false),
        box(24, 174, 434, 14, P::lightGray),
        label(272, 78, tr("FILING BENCH", "BRUSNÝ STŮL"), amber),
    });
    filing.animations.push_back({targetId(54, "grinder"), true, true, {}, {
        {7, {line(288, 118, 332, 118, signalBlue)}},
        {7, {line(310, 96, 310, 140, signalBlue)}},
    }});

    auto& boiler = resetRoom(55, P::black, P::darkGray);
    boiler.decorations.insert(boiler.decorations.end(), {
        box(0, 0, 492, 220, P::brown),
        box(35, 45, 183, 175, P::red), box(48, 58, 157, 149, P::black),
        e2d::ArcVisual{{126, 80}, {45, 32}, 3.14159F, 6.28318F, P::lightGray},
        circle(126, 116, 27, P::red), circle(126, 116, 14, danger),
        box(269, 73, 151, 147, P::darkGray), box(280, 84, 129, 125, P::black),
        line(344, 84, 344, 209, P::lightGray), circle(344, 118, 24, P::lightGray, false),
        line(344, 118, 358, 104, amber),
        box(291, 169, 105, 19, P::brown), label(303, 174, tr("RESERVE", "REZERVA"), amber),
        line(54, 34, 192, 34, P::lightGray), line(192, 34, 192, 76, P::lightGray),
    });
    boiler.animations.push_back({targetId(55, "pressure"), true, true, {}, {
        {10, {line(344, 118, 358, 104, amber)}},
        {10, {line(344, 118, 363, 114, amber)}},
    }});

    auto& pond = resetRoom(56, P::brightBlue, P::brown);
    pond.decorations.insert(pond.decorations.end(), {
        box(0, 0, 492, 118, P::brightBlue), circle(405, 34, 14, amber),
        box(0, 118, 492, 142, P::blue),
        line(18, 145, 163, 145, signalBlue), line(277, 156, 464, 156, P::brightCyan),
        line(79, 188, 232, 188, P::brightCyan), line(309, 213, 455, 213, signalBlue),
        box(79, 174, 147, 20, P::brown), circle(94, 184, 9, P::darkGray),
        box(319, 197, 126, 22, P::brown), circle(430, 208, 9, P::darkGray),
        box(245, 145, 57, 39, amber), box(253, 153, 41, 23, P::brown),
        line(35, 92, 35, 250, P::brown), line(35, 92, 182, 158, P::lightGray),
        label(236, 130, tr("SERVICE BOX", "SERVISNÍ BEDNA"), pale),
    });
    pond.animations.push_back({targetId(56, "floating_logs"), true, true,
        {e2d::Condition::notFlag("spark_retrieved")}, {
            {9, {line(82, 196, 225, 196, amber), box(245, 145, 57, 39, amber, false)}},
            {9, {line(91, 196, 234, 196, amber), box(237, 145, 57, 39, amber, false)}},
        }});

    auto& bunkhouse = resetRoom(57, P::brown, P::darkGray);
    bunkhouse.decorations.insert(bunkhouse.decorations.end(), {
        box(0, 0, 492, 220, P::brown), line(0, 55, 492, 55, P::red),
        box(34, 72, 178, 109, P::darkGray), box(45, 83, 156, 38, P::blue),
        box(45, 132, 156, 38, P::brightBlue), line(68, 72, 68, 181, P::lightGray),
        line(179, 72, 179, 181, P::lightGray),
        box(271, 72, 178, 109, P::darkGray), box(282, 83, 156, 38, P::red),
        box(282, 132, 156, 38, P::brown), line(305, 72, 305, 181, P::lightGray),
        line(416, 72, 416, 181, P::lightGray),
        e2d::PolygonVisual{{{48, 219}, {73, 195}, {100, 219}}, P::black, true},
        label(34, 183, tr("FOREMAN'S BOOT", "PŘEDÁKOVA BOTA"), amber),
    });

    auto& mess = resetRoom(58, P::brown, P::darkGray);
    mess.decorations.insert(mess.decorations.end(), {
        box(0, 0, 492, 220, P::brown),
        box(39, 49, 128, 92, P::darkGray), box(49, 59, 108, 72, P::black),
        circle(103, 96, 27, P::red), circle(103, 96, 15, danger),
        box(206, 146, 230, 21, P::brown), box(222, 167, 13, 53, P::brown),
        box(408, 167, 13, 53, P::brown),
        circle(276, 137, 11, P::lightGray, false), circle(346, 137, 11, P::lightGray, false),
        box(375, 54, 71, 70, P::darkGray), box(384, 63, 53, 52, P::black),
        label(387, 77, tr("OFFICE", "KANCELÁŘ"), amber),
    });
    mess.animations.push_back({targetId(58, "kettle_steam"), true, true, {}, {
        {8, {e2d::ArcVisual{{276, 121}, {6, 12}, 0.0F, 3.14159F, pale}}},
        {8, {e2d::ArcVisual{{276, 116}, {8, 14}, 0.0F, 3.14159F, P::lightGray}}},
    }});

    auto& office = resetRoom(59, P::brown, P::darkGray);
    office.decorations.insert(office.decorations.end(), {
        box(0, 0, 492, 220, P::brown),
        box(44, 44, 166, 92, P::darkGray), box(55, 55, 144, 70, pale),
        line(65, 68, 188, 68, P::blue), line(65, 82, 168, 82, P::blue),
        line(65, 96, 181, 96, P::red), label(78, 110, tr("23:40?", "23:40?"), P::black),
        box(238, 141, 206, 21, P::brown), box(251, 162, 13, 58, P::brown),
        box(418, 162, 13, 58, P::brown), box(281, 112, 92, 27, pale),
        line(290, 121, 360, 121, P::blue), line(290, 129, 347, 129, P::blue),
        circle(411, 86, 29, P::lightGray, false), line(411, 57, 411, 115, P::lightGray),
        line(382, 86, 440, 86, P::lightGray),
    });

    auto& spur = resetRoom(60, P::brightBlue, P::brown);
    spur.decorations.insert(spur.decorations.end(), {
        box(0, 150, 492, 110, P::green),
        line(0, 238, 492, 190, P::lightGray), line(0, 254, 492, 206, P::lightGray),
        line(184, 220, 472, 252, P::lightGray), line(178, 236, 464, 268, P::lightGray),
        line(61, 234, 438, 197, P::brown), line(92, 251, 459, 215, P::brown),
        box(202, 154, 9, 91, P::darkGray), line(206, 163, 170, 126, amber),
        box(153, 116, 43, 22, amber), label(159, 123, tr("POINTS", "VÝHYBKA"), P::black),
        e2d::PolygonVisual{{{356, 158}, {427, 158}, {455, 176}, {427, 194}, {356, 194}}, P::red, true},
        label(369, 170, tr("TRESTLE", "VIADUKT"), pale),
    });

    auto& engine = resetRoom(61, P::brightBlue, P::brown);
    engine.decorations.insert(engine.decorations.end(), {
        box(0, 157, 492, 103, P::green),
        line(0, 247, 492, 247, P::lightGray), line(0, 257, 492, 257, P::lightGray),
        box(69, 137, 332, 79, P::red), box(105, 93, 135, 65, P::darkGray),
        box(117, 104, 111, 43, P::black), box(262, 105, 83, 53, P::red),
        box(355, 81, 25, 76, P::darkGray),
        circle(127, 222, 34, P::black), circle(127, 222, 18, P::lightGray),
        circle(333, 222, 34, P::black), circle(333, 222, 18, P::lightGray),
        line(127, 222, 333, 222, P::lightGray),
        box(53, 157, 31, 38, amber), box(391, 158, 27, 37, P::brown),
        label(172, 174, tr("BLACK PINE NO. 4", "BLACK PINE Č. 4"), pale),
    });
    engine.animations.push_back({targetId(61, "engine_smoke"), true, true,
        {e2d::Condition::flag("logging_engine_running")}, {
            {7, {circle(368, 65, 9, P::lightGray), circle(382, 49, 12, P::darkGray)}},
            {7, {circle(372, 58, 11, P::lightGray), circle(392, 39, 14, P::darkGray)}},
        }});

    auto& trestle = resetRoom(62, P::brightBlue, P::brown);
    trestle.decorations.insert(trestle.decorations.end(), {
        box(0, 162, 492, 98, P::green),
        line(39, 183, 453, 183, P::brown), line(39, 198, 453, 198, P::brown),
        line(54, 198, 54, 260, P::lightGray), line(438, 198, 438, 260, P::lightGray),
        line(54, 198, 124, 242, P::lightGray), line(124, 242, 194, 198, P::lightGray),
        line(298, 198, 368, 242, P::lightGray), line(368, 242, 438, 198, P::lightGray),
        line(194, 198, 232, 215, danger), line(260, 215, 298, 198, danger),
        box(74, 107, 9, 76, P::brown), circle(78, 97, 13, amber, false),
        line(78, 110, 111, 139, P::lightGray),
        label(92, 105, tr("WHISTLE", "PÍŠŤALA"), amber),
        box(337, 134, 82, 37, P::darkGray), line(349, 152, 407, 152, danger),
        label(345, 137, tr("BRAKE", "BRZDA"), pale),
    });
    trestle.animations.push_back({targetId(62, "trestle_flex"), true, true,
        {e2d::Condition::notFlag("trestle_brake_fixed")}, {
            {9, {line(194, 198, 246, 216, danger), line(246, 216, 298, 198, danger)}},
            {9, {line(194, 198, 246, 221, danger), line(246, 221, 298, 198, danger)}},
        }});

    auto& railCut = resetRoom(63, P::brightBlue, P::brown);
    railCut.decorations.insert(railCut.decorations.end(), {
        box(0, 146, 492, 114, P::green),
        e2d::PolygonVisual{{{0, 146}, {109, 54}, {183, 146}}, P::darkGray, true},
        e2d::PolygonVisual{{{309, 146}, {401, 61}, {492, 146}}, P::lightGray, true},
        line(0, 237, 492, 237, P::lightGray), line(0, 252, 492, 252, P::lightGray),
        box(81, 158, 224, 58, P::red), box(110, 124, 94, 42, P::darkGray),
        box(119, 132, 76, 27, P::black), circle(126, 222, 30, P::black),
        circle(263, 222, 30, P::black), circle(126, 222, 15, P::lightGray),
        circle(263, 222, 15, P::lightGray),
        box(357, 160, 84, 60, P::lightGray), box(367, 170, 64, 40, P::black),
        line(375, 190, 390, 180, signalBlue), line(390, 180, 405, 198, P::brightGreen),
        line(405, 198, 422, 182, signalBlue), circle(427, 204, 4, amber),
    });
    railCut.animations.push_back({targetId(63, "radio_wave"), true, true, {}, {
        {8, {line(376, 190, 393, 177, signalBlue), line(393, 177, 411, 198, P::brightGreen)}},
        {8, {line(376, 193, 393, 185, signalBlue), line(393, 185, 411, 192, P::brightGreen)}},
    }});
}

void configureReservoirArtwork(e2d::WorldDefinition& world) {
    const auto resetRoom = [&world](const int number, const P background, const P floor) -> e2d::RoomDefinition& {
        auto& result = room(world, number);
        result.background = background;
        result.decorations.clear();
        result.solids.clear();
        result.animations.clear();
        addGround(result, floor);
        return result;
    };

    auto& overlook = resetRoom(64, P::brightBlue, P::brown);
    overlook.decorations.insert(overlook.decorations.end(), {
        circle(432, 37, 14, amber),
        line(0, 139, 104, 65, P::darkGray), line(104, 65, 205, 139, P::darkGray),
        line(289, 139, 395, 55, P::lightGray), line(395, 55, 492, 139, P::lightGray),
        box(0, 139, 492, 121, P::blue),
        e2d::PolygonVisual{{{98, 244}, {135, 121}, {362, 121}, {399, 244}}, P::lightGray, true},
        box(145, 136, 207, 18, P::darkGray), box(161, 154, 31, 90, P::black),
        box(231, 154, 31, 90, P::black), box(301, 154, 31, 90, P::black),
        line(176, 163, 176, 237, signalBlue), line(246, 163, 246, 237, signalBlue),
        line(316, 163, 316, 237, signalBlue),
        box(356, 102, 61, 59, P::red), box(365, 111, 43, 41, P::black),
        circle(386, 123, 5, danger), label(361, 92, tr("JONAH", "JONAH"), amber),
        box(31, 206, 90, 16, P::brown), label(39, 211, tr("DAM EAST", "HRÁZ VÝCHOD"), pale),
    });
    overlook.animations.push_back({targetId(64, "spillway_flow"), true, true, {}, {
        {7, {line(168, 174, 185, 236, P::brightCyan), line(238, 174, 255, 236, P::brightCyan)}},
        {7, {line(174, 174, 191, 236, P::brightCyan), line(244, 174, 261, 236, P::brightCyan)}},
    }});
    overlook.animations.push_back({targetId(64, "jonah_beacon"), true, true,
        {e2d::Condition::notFlag("jonah_briefed")}, {
            {5, {circle(386, 123, 8, danger)}}, {9, {circle(386, 123, 3, P::darkGray)}},
        }});

    auto& abutment = resetRoom(65, P::brightBlue, P::brown);
    abutment.decorations.insert(abutment.decorations.end(), {
        box(0, 132, 492, 128, P::blue), box(0, 192, 492, 68, P::lightGray),
        line(0, 192, 492, 192, pale), line(0, 213, 492, 213, P::darkGray),
        box(28, 119, 131, 73, P::darkGray), box(38, 129, 111, 53, P::black),
        box(54, 143, 79, 25, P::red), label(61, 150, tr("RESCUE", "ZÁCHRANA"), pale),
        line(184, 96, 184, 192, P::lightGray), line(309, 96, 309, 192, P::lightGray),
        line(184, 105, 309, 105, P::lightGray), line(184, 137, 309, 137, P::lightGray),
        line(184, 169, 309, 169, P::lightGray),
        line(374, 190, 458, 107, amber), line(397, 190, 481, 107, amber),
        line(388, 174, 411, 174, pale), line(405, 157, 428, 157, pale),
        line(422, 140, 445, 140, pale), line(439, 123, 462, 123, pale),
        label(388, 96, tr("TURBINES", "TURBÍNY"), amber),
    });
    abutment.animations.push_back({targetId(65, "spray_gust"), true, true, {}, {
        {10, {line(321, 152, 354, 139, signalBlue), line(326, 168, 363, 154, P::brightCyan)}},
        {10, {line(326, 145, 363, 132, signalBlue), line(331, 161, 370, 147, P::brightCyan)}},
    }});

    auto& spillway = resetRoom(66, P::brightBlue, P::brown);
    spillway.decorations.insert(spillway.decorations.end(), {
        box(0, 95, 492, 165, P::blue),
        box(0, 205, 492, 28, P::lightGray), line(0, 204, 492, 204, pale),
        line(0, 234, 492, 234, P::darkGray),
        box(66, 101, 13, 104, P::darkGray), box(187, 101, 13, 104, P::darkGray),
        box(308, 101, 13, 104, P::darkGray), box(429, 101, 13, 104, P::darkGray),
        line(72, 109, 193, 146, signalBlue), line(193, 109, 314, 146, signalBlue),
        line(314, 109, 435, 146, signalBlue),
        box(81, 177, 72, 20, P::darkGray), box(202, 177, 72, 20, P::darkGray),
        box(323, 177, 72, 20, P::darkGray),
        label(211, 183, tr("SHIELD 2", "CLONA 2"), amber),
    });
    spillway.animations.push_back({targetId(66, "unsafe_spray"), true, true,
        {e2d::Condition::notFlag("spray_shield_fixed")}, {
            {5, {line(74, 112, 151, 202, P::brightCyan), line(195, 112, 272, 202, signalBlue)}},
            {5, {line(195, 112, 272, 202, P::brightCyan), line(316, 112, 393, 202, signalBlue)}},
            {5, {line(316, 112, 393, 202, P::brightCyan), line(437, 112, 478, 160, signalBlue)}},
        }});

    auto& gatehouse = resetRoom(67, P::brown, P::darkGray);
    gatehouse.decorations.insert(gatehouse.decorations.end(), {
        box(0, 0, 492, 220, P::brown), line(0, 51, 492, 51, P::red),
        box(25, 71, 108, 139, P::lightGray), box(36, 82, 86, 117, P::black),
        circle(79, 123, 31, P::lightGray, false), line(79, 123, 102, 102, danger),
        box(47, 164, 64, 23, P::blue), label(52, 170, tr("GATE", "STAVIDLO"), pale),
        box(159, 82, 98, 128, P::darkGray), box(170, 93, 76, 106, P::black),
        circle(208, 127, 21, P::lightGray, false), circle(208, 127, 6, danger),
        box(180, 166, 56, 21, P::red), label(185, 172, tr("BADGE", "ODZNAK"), pale),
        box(286, 65, 181, 145, P::lightGray), box(297, 76, 159, 123, P::black),
        circle(340, 133, 35, P::lightGray, false), circle(340, 133, 7, amber),
        line(340, 98, 340, 168, P::lightGray), line(305, 133, 375, 133, P::lightGray),
        box(389, 103, 49, 61, P::darkGray), line(399, 114, 428, 153, danger),
        label(304, 81, tr("FALSE OPEN", "FALEŠNĚ OTEVŘENO"), danger),
    });
    gatehouse.animations.push_back({targetId(67, "gate_alarm"), true, true,
        {e2d::Condition::notFlag("spillway_closed")}, {
            {6, {circle(208, 127, 9, danger), line(79, 123, 102, 102, danger)}},
            {6, {circle(208, 127, 5, P::darkGray), line(79, 123, 96, 111, amber)}},
        }});

    auto& turbineUpper = resetRoom(68, P::darkGray, P::brown);
    turbineUpper.decorations.insert(turbineUpper.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray), line(0, 56, 492, 56, P::lightGray),
        box(37, 70, 190, 128, P::lightGray), box(48, 81, 168, 106, P::black),
        circle(132, 134, 42, P::lightGray, false), circle(132, 134, 12, amber),
        line(90, 134, 174, 134, signalBlue), line(132, 92, 132, 176, signalBlue),
        box(278, 61, 170, 124, P::brown), box(288, 71, 150, 104, P::black),
        box(301, 86, 124, 69, P::blue), line(310, 143, 331, 118, signalBlue),
        line(331, 118, 352, 142, P::brightGreen), line(352, 142, 377, 108, signalBlue),
        line(377, 108, 414, 135, danger),
        label(301, 76, tr("AUX POWER", "POMOCNÝ PROUD"), amber),
        line(246, 23, 246, 220, P::lightGray),
    });
    turbineUpper.animations.push_back({targetId(68, "turbine_blur"), true, true, {}, {
        {6, {line(90, 134, 174, 134, signalBlue), line(132, 92, 132, 176, signalBlue)}},
        {6, {line(102, 104, 162, 164, signalBlue), line(102, 164, 162, 104, signalBlue)}},
    }});

    auto& turbineLower = resetRoom(69, P::darkGray, P::brown);
    turbineLower.decorations.insert(turbineLower.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        circle(123, 127, 73, P::lightGray, false), circle(123, 127, 29, P::black),
        line(123, 54, 123, 200, P::lightGray), line(50, 127, 196, 127, P::lightGray),
        box(229, 56, 228, 145, P::lightGray), box(241, 68, 204, 121, P::black),
        box(255, 83, 48, 79, P::darkGray), box(319, 83, 48, 79, P::darkGray),
        box(383, 83, 48, 79, P::darkGray),
        circle(279, 105, 7, danger), circle(343, 105, 7, danger), circle(407, 105, 7, danger),
        line(269, 134, 289, 117, danger), line(333, 134, 353, 117, danger),
        line(397, 134, 417, 117, danger),
        label(265, 170, tr("BAY BREAKERS", "JISTIČE PROSTORU"), amber),
    });
    turbineLower.animations.push_back({targetId(69, "breaker_arcs"), true, true,
        {e2d::Condition::notFlag("bay_isolated")}, {
            {6, {line(292, 75, 306, 63, signalBlue), line(356, 75, 370, 63, signalBlue)}},
            {6, {line(356, 75, 370, 63, signalBlue), line(420, 75, 434, 63, signalBlue)}},
        }});

    auto& pump = resetRoom(70, P::darkGray, P::brown);
    pump.decorations.insert(pump.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        circle(238, 139, 79, P::lightGray, false), circle(238, 139, 31, P::black),
        line(159, 139, 72, 139, signalBlue), line(317, 139, 423, 139, signalBlue),
        box(203, 49, 70, 39, P::brown), box(214, 58, 48, 21, P::black),
        circle(238, 68, 6, danger),
        box(44, 74, 92, 88, P::lightGray), box(54, 84, 72, 68, P::black),
        circle(90, 118, 25, P::lightGray, false), line(90, 118, 104, 104, danger),
        box(356, 70, 98, 96, P::lightGray), box(367, 81, 76, 74, P::black),
        circle(387, 104, 7, danger), circle(423, 104, 7, P::darkGray),
        label(187, 197, tr("EMERGENCY PUMP", "NOUZOVÉ ČERPADLO"), amber),
    });
    pump.animations.push_back({targetId(70, "pump_running_motion"), true, true,
        {e2d::Condition::flag("pump_running")}, {
            {5, {circle(238, 139, 30, P::brightGreen, false), line(238, 108, 238, 170, amber)}},
            {5, {circle(238, 139, 30, P::brightGreen, false), line(207, 139, 269, 139, amber)}},
        }});

    auto& bay = resetRoom(71, P::darkGray, P::brown);
    bay.decorations.insert(bay.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        box(36, 54, 420, 166, P::black), line(36, 54, 456, 54, P::lightGray),
        line(67, 54, 67, 220, P::lightGray), line(425, 54, 425, 220, P::lightGray),
        box(96, 96, 111, 95, P::brown), box(106, 106, 91, 75, P::black),
        box(298, 91, 105, 100, P::lightGray), box(308, 101, 85, 80, P::black),
        line(318, 119, 383, 119, danger), circle(350, 151, 15, P::lightGray, false),
        label(112, 113, tr("LOCKER", "SKŘÍŇKA"), amber),
    });

    auto& intake = resetRoom(72, P::black, P::brown);
    intake.decorations.insert(intake.decorations.end(), {
        e2d::ArcVisual{{246, 225}, {225, 194}, 3.14159F, 6.28318F, P::darkGray},
        box(37, 101, 18, 159, P::brown), box(437, 101, 18, 159, P::brown),
        line(46, 102, 446, 102, P::brown),
        line(73, 210, 419, 210, P::lightGray), line(92, 210, 92, 260, P::lightGray),
        line(400, 210, 400, 260, P::lightGray),
        box(274, 126, 142, 63, P::darkGray), box(284, 136, 122, 43, P::black),
        circle(314, 157, 17, P::lightGray, false), line(331, 157, 391, 157, P::darkGray),
        label(117, 137, tr("NO LIGHT", "BEZ SVĚTLA"), P::darkGray),
    });
    intake.animations.push_back({targetId(72, "water_reflection"), true, true, {}, {
        {9, {line(84, 233, 191, 233, P::blue), line(274, 243, 406, 243, signalBlue)}},
        {9, {line(98, 233, 205, 233, signalBlue), line(261, 243, 393, 243, P::blue)}},
    }});

    auto& shore = resetRoom(73, P::brightBlue, P::brown);
    shore.decorations.insert(shore.decorations.end(), {
        circle(421, 40, 14, amber),
        line(0, 141, 103, 71, P::darkGray), line(103, 71, 218, 141, P::darkGray),
        box(0, 141, 492, 119, P::blue),
        e2d::PolygonVisual{{{0, 222}, {126, 189}, {246, 219}, {372, 181}, {492, 211}, {492, 260}, {0, 260}}, P::green, true},
        line(35, 241, 35, 202, P::green), line(49, 241, 49, 197, P::brightGreen),
        line(442, 241, 442, 201, P::green), line(457, 241, 457, 194, P::brightGreen),
        circle(226, 233, 8, P::lightGray, false), line(218, 233, 206, 244, P::lightGray),
        line(226, 225, 238, 216, P::lightGray),
        line(286, 232, 305, 241, P::black), line(305, 241, 328, 235, P::black),
        label(194, 198, tr("KLINE", "KLINEOVÁ"), amber),
    });
    shore.animations.push_back({targetId(73, "shore_waves"), true, true, {}, {
        {9, {line(64, 166, 207, 166, signalBlue), line(271, 177, 452, 177, P::brightCyan)}},
        {9, {line(78, 166, 221, 166, P::brightCyan), line(256, 177, 437, 177, signalBlue)}},
    }});

    auto& valves = resetRoom(74, P::brightBlue, P::brown);
    valves.decorations.insert(valves.decorations.end(), {
        box(0, 147, 492, 113, P::green),
        box(39, 98, 414, 37, P::lightGray), box(62, 135, 29, 125, P::darkGray),
        box(217, 135, 29, 125, P::darkGray), box(379, 135, 29, 125, P::darkGray),
        circle(76, 117, 31, P::lightGray, false), circle(231, 117, 31, P::lightGray, false),
        circle(393, 117, 31, P::lightGray, false),
        line(76, 86, 76, 148, P::lightGray), line(45, 117, 107, 117, P::lightGray),
        line(231, 86, 231, 148, P::lightGray), line(200, 117, 262, 117, P::lightGray),
        circle(393, 117, 7, danger), box(316, 174, 106, 37, P::darkGray),
        label(324, 185, tr("PUMP INTAKE", "PŘÍVOD ČERPADLA"), amber),
    });

    auto& shaft = resetRoom(75, P::black, P::brown);
    shaft.decorations.insert(shaft.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        e2d::ArcVisual{{254, 221}, {154, 174}, 3.14159F, 6.28318F, P::brown},
        box(109, 88, 14, 172, P::brown), box(385, 88, 14, 172, P::brown),
        line(116, 88, 392, 88, P::brown),
        box(143, 102, 224, 111, P::lightGray, false),
        line(143, 102, 367, 213, P::lightGray), line(367, 102, 143, 213, P::lightGray),
        line(255, 102, 255, 213, P::lightGray),
        line(420, 94, 420, 244, amber), line(444, 94, 444, 244, amber),
        line(420, 112, 444, 112, pale), line(420, 135, 444, 135, pale),
        line(420, 158, 444, 158, pale), line(420, 181, 444, 181, pale),
        line(420, 204, 444, 204, pale), label(407, 75, tr("MINE", "DŮL"), amber),
    });
    shaft.animations.push_back({targetId(75, "shaft_drip"), true, true, {}, {
        {11, {line(185, 103, 185, 121, signalBlue), circle(185, 126, 2, P::brightCyan)}},
        {11, {line(185, 112, 185, 130, signalBlue), circle(185, 135, 2, P::brightCyan)}},
    }});
}

void configureMineArtwork(e2d::WorldDefinition& world) {
    const auto resetRoom = [&world](const int number, const P background, const P floor) -> e2d::RoomDefinition& {
        auto& result = room(world, number);
        result.background = background;
        result.decorations.clear();
        result.solids.clear();
        result.animations.clear();
        addGround(result, floor);
        return result;
    };

    auto& carts = resetRoom(76, P::black, P::brown);
    carts.decorations.insert(carts.decorations.end(), {
        e2d::PolygonVisual{{{0, 55}, {92, 25}, {181, 58}, {276, 31}, {382, 57}, {492, 39}, {492, 260}, {0, 260}}, P::darkGray, true},
        box(43, 72, 13, 188, P::brown), box(433, 72, 13, 188, P::brown), line(49, 72, 439, 72, P::brown),
        line(48, 72, 106, 124, P::brown), line(439, 72, 381, 124, P::brown),
        box(83, 142, 159, 64, P::red), e2d::PolygonVisual{{{75, 142}, {104, 112}, {222, 112}, {250, 142}}, P::brown, true},
        circle(112, 217, 25, P::black), circle(214, 217, 25, P::black),
        circle(112, 217, 12, P::lightGray), circle(214, 217, 12, P::lightGray),
        line(73, 241, 431, 241, P::lightGray), line(89, 241, 89, 260, P::lightGray), line(411, 241, 411, 260, P::lightGray),
        box(303, 108, 97, 94, P::lightGray), box(313, 118, 77, 74, P::black),
        box(326, 145, 51, 23, P::red), label(329, 151, tr("MASK", "MASKA"), pale),
        label(109, 126, tr("ORE CART", "DŮLNÍ VOZÍK"), amber),
    });
    carts.animations.push_back({targetId(76, "mine_lamp_flicker"), true, true, {}, {
        {8, {circle(273, 91, 9, amber)}}, {3, {circle(273, 91, 5, P::brown)}},
    }});

    auto& timber = resetRoom(77, P::black, P::brown);
    timber.decorations.insert(timber.decorations.end(), {
        e2d::PolygonVisual{{{0, 47}, {103, 24}, {201, 49}, {301, 28}, {401, 52}, {492, 34}, {492, 260}, {0, 260}}, P::darkGray, true},
        box(42, 72, 17, 188, P::brown), box(151, 72, 17, 188, P::brown), box(260, 72, 17, 188, P::brown), box(369, 72, 17, 188, P::brown),
        line(50, 72, 377, 72, P::brown), line(50, 72, 159, 151, P::brown),
        line(159, 72, 268, 151, P::brown), line(268, 72, 377, 151, P::brown),
        line(253, 88, 294, 245, P::red), line(271, 88, 312, 245, P::brown),
        line(259, 121, 287, 121, danger), line(267, 150, 295, 150, danger),
        label(231, 94, tr("BRACE 3", "VZPĚRA 3"), amber),
        e2d::PolygonVisual{{{386, 154}, {443, 154}, {478, 174}, {443, 194}, {386, 194}}, signalBlue, true},
        label(395, 166, tr("VENT", "VĚTRÁNÍ"), P::black),
    });
    timber.animations.push_back({targetId(77, "warning_dust"), true, true,
        {e2d::Condition::notFlag("drift_braced")}, {
            {8, {circle(288, 95, 3, P::lightGray), circle(300, 112, 2, P::lightGray)}},
            {8, {circle(292, 119, 3, P::lightGray), circle(304, 141, 2, P::lightGray)}},
        }});

    auto& collapse = resetRoom(78, P::black, P::brown);
    collapse.decorations.insert(collapse.decorations.end(), {
        e2d::PolygonVisual{{{0, 42}, {117, 23}, {228, 51}, {351, 27}, {492, 49}, {492, 260}, {0, 260}}, P::darkGray, true},
        box(42, 73, 15, 187, P::brown), box(420, 73, 15, 187, P::brown), line(49, 73, 427, 73, P::brown),
        e2d::PolygonVisual{{{158, 260}, {184, 201}, {215, 217}, {241, 159}, {273, 211}, {304, 176}, {341, 260}}, P::lightGray, true},
        e2d::PolygonVisual{{{188, 260}, {213, 226}, {247, 239}, {279, 204}, {319, 260}}, P::darkGray, true},
        line(117, 137, 383, 137, P::black), line(119, 145, 380, 145, signalBlue),
        box(342, 108, 78, 38, P::black), label(350, 118, tr("CABLE EAST", "KABEL VÝCHOD"), amber),
    });

    auto& ventilation = resetRoom(79, P::darkGray, P::brown);
    ventilation.decorations.insert(ventilation.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        box(42, 54, 191, 154, P::lightGray), box(54, 66, 167, 130, P::black),
        circle(137, 131, 57, P::lightGray, false), circle(137, 131, 14, P::darkGray),
        line(137, 74, 137, 188, P::lightGray), line(80, 131, 194, 131, P::lightGray),
        box(273, 63, 177, 143, P::brown), box(284, 74, 155, 121, P::black),
        box(297, 91, 58, 72, P::darkGray), circle(326, 112, 8, danger),
        box(370, 91, 55, 72, P::darkGray), box(382, 107, 31, 19, P::red),
        label(295, 173, tr("FAN START", "START VĚTRÁKU"), amber),
        label(363, 173, tr("FILTER", "FILTR"), signalBlue),
    });
    ventilation.animations.push_back({targetId(79, "fan_motion"), true, true,
        {e2d::Condition::flag("ventilation_running")}, {
            {5, {line(137, 74, 137, 188, signalBlue), line(80, 131, 194, 131, signalBlue)}},
            {5, {line(96, 90, 178, 172, signalBlue), line(96, 172, 178, 90, signalBlue)}},
        }});

    auto& copper = resetRoom(80, P::black, P::brown);
    copper.decorations.insert(copper.decorations.end(), {
        e2d::PolygonVisual{{{0, 41}, {94, 18}, {185, 48}, {286, 22}, {389, 51}, {492, 31}, {492, 260}, {0, 260}}, P::darkGray, true},
        e2d::PolygonVisual{{{28, 206}, {74, 91}, {105, 179}, {144, 64}, {181, 201}}, P::brown, true},
        e2d::PolygonVisual{{{287, 205}, {332, 73}, {365, 167}, {404, 51}, {453, 208}}, P::brown, true},
        line(75, 102, 97, 183, signalBlue), line(144, 75, 171, 196, P::brightCyan),
        line(333, 85, 361, 169, signalBlue), line(405, 63, 443, 201, P::brightCyan),
        box(205, 112, 76, 106, P::lightGray), box(216, 123, 54, 84, P::black),
        circle(243, 151, 20, P::lightGray, false), line(243, 151, 258, 137, danger),
        line(190, 233, 302, 233, P::red), label(201, 213, tr("GAS ZONE", "PLYN"), danger),
    });
    copper.animations.push_back({targetId(80, "gas_wisp"), true, true, {}, {
        {9, {e2d::ArcVisual{{222, 91}, {18, 11}, 0.0F, 3.14159F, P::green}}},
        {9, {e2d::ArcVisual{{248, 84}, {22, 13}, 0.0F, 3.14159F, P::brightGreen}}},
    }});

    auto& minePump = resetRoom(81, P::darkGray, P::brown);
    minePump.decorations.insert(minePump.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        circle(209, 139, 76, P::lightGray, false), circle(209, 139, 28, P::black),
        line(133, 139, 54, 139, signalBlue), line(285, 139, 448, 139, signalBlue),
        box(178, 49, 62, 39, P::brown), circle(209, 68, 7, danger),
        box(321, 72, 117, 111, P::lightGray), box(332, 83, 95, 89, P::black),
        circle(360, 118, 21, P::lightGray, false), line(360, 118, 373, 104, danger),
        box(383, 102, 31, 43, P::red), label(326, 190, tr("DRAINAGE", "ODVODNĚNÍ"), amber),
    });
    minePump.animations.push_back({targetId(81, "drainage_motion"), true, true,
        {e2d::Condition::flag("mine_drained")}, {
            {5, {circle(209, 139, 27, P::brightGreen, false), line(209, 112, 209, 166, amber)}},
            {5, {circle(209, 139, 27, P::brightGreen, false), line(182, 139, 236, 139, amber)}},
        }});

    auto& flooded = resetRoom(82, P::black, P::brown);
    flooded.decorations.insert(flooded.decorations.end(), {
        e2d::PolygonVisual{{{0, 46}, {112, 21}, {224, 49}, {348, 24}, {492, 44}, {492, 260}, {0, 260}}, P::darkGray, true},
        box(0, 166, 492, 94, P::blue), line(0, 166, 492, 166, signalBlue),
        line(34, 194, 183, 194, P::brightCyan), line(271, 216, 452, 216, P::brightCyan),
        box(178, 188, 147, 43, P::darkGray), line(188, 197, 315, 197, P::lightGray),
        line(188, 207, 315, 207, P::lightGray), line(188, 217, 315, 217, P::lightGray),
        circle(252, 215, 7, danger), label(194, 174, tr("SUBMERGED GRATE", "PONOŘENÁ MŘÍŽ"), amber),
    });
    flooded.animations.push_back({targetId(82, "flooded_drift_water"), true, true, {}, {
        {8, {line(23, 180, 167, 180, signalBlue), line(286, 231, 463, 231, P::brightCyan)}},
        {8, {line(37, 180, 181, 180, P::brightCyan), line(271, 231, 448, 231, signalBlue)}},
    }});

    auto& survey = resetRoom(83, P::black, P::brown);
    survey.decorations.insert(survey.decorations.end(), {
        e2d::PolygonVisual{{{0, 43}, {104, 22}, {213, 48}, {326, 25}, {492, 46}, {492, 260}, {0, 260}}, P::darkGray, true},
        box(45, 76, 205, 124, P::brown), box(56, 87, 183, 102, P::black),
        box(69, 102, 66, 51, P::blue), line(78, 143, 99, 116, signalBlue), line(99, 116, 123, 138, danger),
        box(151, 102, 73, 51, pale), line(160, 115, 215, 115, P::blue), line(160, 127, 207, 127, P::blue),
        box(293, 137, 148, 58, P::red), e2d::PolygonVisual{{{285, 137}, {316, 108}, {421, 108}, {449, 137}}, P::brown, true},
        circle(319, 206, 22, P::black), circle(416, 206, 22, P::black),
        line(284, 232, 452, 232, P::lightGray), label(78, 91, tr("VOSS SURVEY", "VOSSŮV PRŮZKUM"), amber),
    });

    auto& liftBottom = resetRoom(84, P::black, P::brown);
    liftBottom.decorations.insert(liftBottom.decorations.end(), {
        box(74, 39, 252, 221, P::lightGray, false), box(91, 56, 218, 204, P::darkGray),
        line(200, 56, 200, 260, P::lightGray), line(91, 56, 309, 260, P::lightGray), line(309, 56, 91, 260, P::lightGray),
        box(112, 71, 176, 29, P::black), circle(268, 85, 7, P::darkGray),
        box(339, 91, 122, 112, P::brown), box(350, 102, 100, 90, P::black),
        e2d::ArcVisual{{400, 187}, {42, 68}, 3.14159F, 6.28318F, P::darkGray},
        label(347, 76, tr("MAINT CRAWL", "SERVISNÍ PRŮLEZ"), amber),
        box(17, 136, 48, 73, P::lightGray), box(25, 144, 32, 57, P::black),
        circle(41, 161, 6, danger), label(15, 216, tr("FUSE", "POJISTKA"), pale),
    });

    auto& liftTop = resetRoom(85, P::darkGray, P::brown);
    liftTop.decorations.insert(liftTop.decorations.end(), {
        box(61, 35, 238, 225, P::lightGray, false), box(77, 51, 206, 209, P::darkGray),
        line(180, 51, 180, 260, P::lightGray), line(77, 51, 283, 260, P::lightGray), line(283, 51, 77, 260, P::lightGray),
        box(102, 68, 157, 28, P::black), circle(241, 82, 7, P::brightGreen),
        box(327, 77, 137, 129, P::brown), box(338, 88, 115, 107, P::black),
        line(348, 117, 443, 117, signalBlue), line(348, 141, 443, 141, danger),
        box(358, 157, 75, 24, P::red), label(366, 164, tr("VOSS RADIO", "VOSS RÁDIO"), pale),
        label(329, 62, tr("SUBSTATION", "ROZVODNA"), amber),
    });
    liftTop.animations.push_back({targetId(85, "nightjar_cable"), true, true, {}, {
        {8, {line(348, 117, 395, 117, signalBlue), line(400, 117, 443, 117, P::blue)}},
        {8, {line(348, 117, 390, 117, P::blue), line(395, 117, 443, 117, signalBlue)}},
    }});

    auto& substation = resetRoom(86, P::darkGray, P::brown);
    substation.decorations.insert(substation.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        box(41, 48, 410, 151, P::lightGray), box(54, 61, 384, 125, P::black),
        box(69, 78, 96, 79, P::darkGray), box(197, 78, 96, 79, P::darkGray), box(325, 78, 96, 79, P::darkGray),
        circle(92, 98, 7, amber), circle(220, 98, 7, danger), circle(348, 98, 7, P::darkGray),
        line(101, 132, 150, 105, signalBlue), line(229, 132, 278, 105, danger), line(357, 132, 406, 105, P::lightGray),
        label(76, 164, tr("DAM AUX", "PŘEHRADA"), pale), label(201, 164, tr("QUIET", "TICHO"), danger),
        label(335, 164, tr("LIFT", "VÝTAH"), amber),
        box(129, 202, 235, 18, P::brown), label(177, 207, tr("ROUTING BOARD", "ROZVODNÁ DESKA"), amber),
    });

    auto& switchgear = resetRoom(87, P::darkGray, P::brown);
    switchgear.decorations.insert(switchgear.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        box(42, 43, 408, 164, P::lightGray), box(54, 55, 384, 140, P::black),
        box(72, 73, 88, 104, P::darkGray), box(202, 73, 88, 104, P::darkGray), box(332, 73, 88, 104, P::darkGray),
        line(116, 91, 116, 151, danger), line(246, 91, 246, 151, danger), line(376, 91, 376, 151, danger),
        circle(116, 91, 8, danger), circle(246, 91, 8, danger), circle(376, 91, 8, danger),
        label(91, 159, tr("2", "2"), amber), label(221, 159, tr("1", "1"), amber), label(351, 159, tr("3", "3"), amber),
        e2d::PolylineVisual{{{92, 188}, {221, 188}, {351, 188}}, signalBlue, false},
        label(161, 211, tr("CALDER: 2-1-3", "CALDEROVÁ: 2-1-3"), amber),
    });
    switchgear.animations.push_back({targetId(87, "switchgear_spark"), true, true,
        {e2d::Condition::notFlag("substation_isolated")}, {
            {6, {line(246, 59, 257, 47, signalBlue), line(257, 47, 267, 59, pale)}},
            {6, {line(376, 59, 386, 48, signalBlue), line(386, 48, 397, 59, pale)}},
        }});

    auto& vault = resetRoom(88, P::black, P::brown);
    vault.decorations.insert(vault.decorations.end(), {
        box(0, 0, 492, 220, P::darkGray),
        box(35, 52, 191, 151, P::lightGray), box(47, 64, 167, 127, P::black),
        circle(90, 111, 21, P::red, false), circle(171, 111, 21, P::red, false),
        e2d::PolylineVisual{{{90, 132}, {126, 153}, {171, 132}}, danger, false},
        label(69, 170, tr("QUIET FEED", "PŘÍVOD TICHA"), danger),
        box(266, 52, 191, 151, P::lightGray), box(278, 64, 167, 127, P::black),
        circle(321, 111, 21, P::darkGray, false), circle(402, 111, 21, P::darkGray, false),
        line(321, 132, 402, 132, P::darkGray), label(314, 170, tr("LIFT BUS", "PŘÍVOD VÝTAHU"), amber),
    });
    vault.animations.push_back({targetId(88, "quiet_pulse"), true, true,
        {e2d::Condition::notFlag("quiet_feed_cut")}, {
            {7, {circle(90, 111, 20, danger, false), circle(171, 111, 20, P::red, false)}},
            {7, {circle(90, 111, 20, P::red, false), circle(171, 111, 20, danger, false)}},
        }});

    auto& researchDoor = resetRoom(89, P::darkGray, P::brown);
    researchDoor.decorations.insert(researchDoor.decorations.end(), {
        box(72, 31, 294, 229, P::lightGray), box(88, 47, 262, 213, P::black),
        line(219, 47, 219, 260, P::lightGray),
        box(390, 71, 79, 67, P::lightGray), box(399, 80, 61, 49, P::black),
        circle(415, 99, 7, danger), box(426, 91, 25, 17, P::red),
        box(390, 154, 79, 67, P::lightGray), box(399, 163, 61, 49, P::black),
        circle(415, 182, 7, danger), box(426, 174, 25, 17, P::red),
        label(105, 72, tr("KLINE RESEARCH", "VÝZKUM KLINEOVÉ"), amber),
        label(394, 56, tr("BADGE", "ODZNAK"), pale), label(397, 139, tr("CARD", "KARTA"), pale),
    });

    auto& ridgeLift = resetRoom(90, P::black, P::brown);
    ridgeLift.decorations.insert(ridgeLift.decorations.end(), {
        box(58, 25, 376, 235, P::lightGray, false), box(77, 44, 338, 216, P::darkGray),
        line(246, 44, 246, 260, P::lightGray), line(77, 44, 415, 260, P::lightGray), line(415, 44, 77, 260, P::lightGray),
        box(101, 61, 290, 45, P::black), label(125, 76, tr("RIDGE FREIGHT LIFT", "HŘEBENOVÝ VÝTAH"), amber),
        box(115, 137, 109, 79, P::black), box(126, 148, 87, 57, P::blue),
        line(135, 191, 154, 169, signalBlue), line(154, 169, 177, 193, P::brightGreen), line(177, 193, 203, 160, danger),
        box(281, 137, 104, 79, P::black), circle(333, 176, 31, P::lightGray, false),
        circle(333, 176, 8, P::brightGreen), label(297, 222, tr("ASCEND", "VZHŮRU"), P::brightGreen),
    });
}

void addActThree(e2d::WorldDefinition& world) {
    // The camp is a set of physical work areas around two hubs, not a linear
    // catalogue walk. Every branch gets a visible, two-way ENTER route.
    configureLoggingCampArtwork(world);
    setHorizontalRoute(world, 51, 50, 52);
    setHorizontalRoute(world, 52, 51, std::nullopt);
    for (int branch = 53; branch <= 61; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }
    setHorizontalRoute(world, 62, 60, 63);
    setHorizontalRoute(world, 63, 62, 64);

    addPortal(world, 52, "mill_door", "SAWMILL FLOOR", "PROVOZ PILY",
        {310, 142, 76, 118}, 53, {
            box(318, 153, 58, 107, P::darkGray), box(324, 160, 46, 94, P::black),
            label(329, 174, tr("MILL", "PILA"), amber), circle(363, 210, 3, amber),
        });
    addPortal(world, 52, "bunkhouse_door", "WORKERS' BUNKHOUSE", "UBYTOVNA DĚLNÍKŮ",
        {63, 142, 65, 118}, 57, {
            box(70, 163, 55, 97, P::darkGray), box(76, 170, 43, 84, P::brown),
            label(78, 181, tr("BUNK", "UBYT"), pale), circle(112, 212, 3, amber),
        });
    addPortal(world, 52, "mess_door", "MESS HALL", "JÍDELNA TÁBORA",
        {148, 142, 60, 118}, 58, {
            box(154, 163, 58, 97, P::darkGray), box(160, 170, 46, 84, P::red),
            label(165, 181, tr("MESS", "JÍDLO"), pale), circle(199, 212, 3, amber),
        });
    addPortal(world, 52, "rail_path", "PATH TO RAIL SPUR", "CESTA KE KOLEJOVÉ VLEČCE",
        {401, 142, 83, 118}, 60, {
            e2d::PolygonVisual{{{398, 185}, {446, 185}, {478, 201}, {446, 217}, {398, 217}}, amber, true},
            label(409, 195, tr("RAIL", "KOLEJ"), P::black),
        });

    addPortal(world, 53, "yard_door", "DOOR TO SAWMILL YARD", "DVEŘE NA DVŮR PILY",
        {0, 137, 60, 123}, 52, {box(7, 154, 45, 106, P::red), label(14, 174, tr("YARD", "DVŮR"), pale)});
    addPortal(world, 53, "filing_door", "SAW FILING ROOM", "BRUSÍRNA PIL",
        {292, 137, 50, 123}, 54, {box(299, 154, 43, 106, P::darkGray), label(302, 174, tr("FILES", "PILY"), amber)});
    addPortal(world, 53, "boiler_door", "BOILER HOUSE", "KOTELNA",
        {360, 137, 50, 123}, 55, {box(363, 154, 47, 106, P::red), label(367, 174, tr("FUEL", "PALIVO"), pale)});
    addPortal(world, 53, "pond_door", "DOOR TO LOG POND", "DVEŘE KE KLÁDOVÉMU RYBNÍKU",
        {428, 137, 64, 123}, 56, {box(433, 154, 52, 106, P::blue), label(438, 174, tr("POND", "RYBNÍK"), pale)});
    addPortal(world, 54, "mill_door", "DOOR TO SAWMILL FLOOR", "DVEŘE DO PROVOZU PILY",
        {415, 137, 77, 123}, 53, {box(425, 151, 57, 109, P::red), label(432, 171, tr("MILL", "PILA"), pale)});
    addPortal(world, 55, "mill_door", "DOOR TO SAWMILL FLOOR", "DVEŘE DO PROVOZU PILY",
        {415, 137, 77, 123}, 53, {box(425, 151, 57, 109, P::red), label(432, 171, tr("MILL", "PILA"), pale)});
    addPortal(world, 56, "mill_door", "DOOR TO SAWMILL FLOOR", "DVEŘE DO PROVOZU PILY",
        {0, 137, 67, 123}, 53, {box(7, 151, 50, 109, P::brown), label(14, 171, tr("MILL", "PILA"), pale)});
    addPortal(world, 57, "yard_door", "DOOR TO SAWMILL YARD", "DVEŘE NA DVŮR PILY",
        {415, 137, 77, 123}, 52, {box(425, 151, 57, 109, P::red), label(432, 171, tr("YARD", "DVŮR"), pale)});
    addPortal(world, 58, "yard_door", "DOOR TO SAWMILL YARD", "DVEŘE NA DVŮR PILY",
        {0, 137, 67, 123}, 52, {box(7, 151, 50, 109, P::red), label(14, 171, tr("YARD", "DVŮR"), pale)});
    addPortal(world, 58, "office_door", "CAMP OFFICE", "KANCELÁŘ TÁBORA",
        {404, 137, 88, 123}, 59, {box(414, 151, 68, 109, P::darkGray), label(422, 171, tr("OFFICE", "KANCEL."), amber)});
    addPortal(world, 59, "mess_door", "DOOR TO MESS HALL", "DVEŘE DO JÍDELNY",
        {0, 137, 67, 123}, 58, {box(7, 151, 50, 109, P::red), label(14, 171, tr("MESS", "JÍDLO"), pale)});
    addPortal(world, 60, "yard_path", "PATH TO SAWMILL YARD", "CESTA NA DVŮR PILY",
        {0, 137, 70, 123}, 52, {
            e2d::PolygonVisual{{{7, 190}, {30, 176}, {67, 176}, {67, 204}, {30, 204}}, amber, true},
            label(26, 186, tr("YARD", "DVŮR"), P::black),
        });
    addPortal(world, 60, "engine_path", "PATH TO LOGGING ENGINE", "CESTA K LESNÍ LOKOMOTIVĚ",
        {268, 137, 90, 123}, 61, {
            e2d::PolygonVisual{{{275, 178}, {331, 178}, {359, 195}, {331, 212}, {275, 212}}, P::red, true},
            label(287, 190, tr("ENGINE", "LOKO"), pale),
        });
    addPortal(world, 60, "trestle_path", "TRACK TO TRESTLE", "KOLEJ K VIADUKTU",
        {382, 137, 110, 123}, 62, {
            e2d::PolygonVisual{{{386, 178}, {449, 178}, {482, 195}, {449, 212}, {386, 212}}, amber, true},
            label(398, 190, tr("TRESTLE", "VIADUKT"), P::black),
        });
    addPortal(world, 61, "spur_path", "PATH BACK TO RAIL SPUR", "CESTA ZPĚT K VLEČCE",
        {0, 137, 62, 123}, 60, {box(7, 174, 48, 32, amber), label(13, 185, tr("SPUR", "VLEČKA"), P::black)});

    addCharacter(world, 52, "lila", "LILA MERCER", "LILA MERCEROVÁ", "met_lila", {
        speech(tr("Lila: The ridge road is gone. This logging engine is our only heavy transport.",
            "Lila: Cesta na hřeben je pryč. Tahle lokomotiva je naše jediná těžká doprava.")),
        speech(tr("Lila: Bring me a belt, spark plug, oil and fuel. Align the points and I will set the timing.",
            "Lila: Přines řemen, svíčku, olej a palivo. Srovnej výhybku a já nastavím časování.")),
        speech(tr("Iris: I will make the machine whole. You make it run.",
            "Iris: Já stroj doplním. Ty ho rozběhneš."), e2d::MessageSpeaker::player),
    });
    auto& lila = ensureHotspot(world, 52, "lila", tr("LILA MERCER", "LILA MERCEROVÁ"),
        {230, 137, 68, 123}, e2d::HotspotKind::character, 2);
    lila.interactionArea = {230, 137, 68, 123};
    lila.visuals = {
        circle(266, 170, 11, amber), box(256, 181, 21, 49, P::brightMagenta),
        line(258, 230, 251, 258, P::lightGray), line(274, 230, 281, 258, P::lightGray),
        line(256, 192, 243, 211, amber), line(277, 192, 291, 204, amber),
    };
    addUse(world, 53, "planer_tension", "PLANER BELT TENSIONER", "NAPÍNÁK ŘEMENU HOBLOVKY",
        "wrench", "belt_released", "The tensioner backs off and leaves the drive belt loose and safe.",
        "Napínák povolí a řemen zůstane volný a bezpečný.");
    auto& planer = ensureHotspot(world, 53, "planer_tension",
        tr("PLANER BELT TENSIONER", "NAPÍNÁK ŘEMENU HOBLOVKY"),
        {63, 135, 132, 125}, e2d::HotspotKind::mechanism);
    planer.interactionArea = {63, 135, 132, 125};
    planer.visibleWhen = {e2d::Condition::notFlag("belt_released")};
    planer.visuals = {
        circle(94, 194, 20, P::lightGray, false), circle(159, 194, 20, P::lightGray, false),
        e2d::PolylineVisual{{{94, 174}, {159, 174}, {159, 214}, {94, 214}, {94, 174}}, danger, false},
        line(178, 171, 178, 217, P::lightGray), line(168, 181, 188, 181, amber),
        label(82, 225, tr("TENSIONED", "NAPNUTO"), danger),
    };
    auto& releasedPlaner = ensureHotspot(world, 53, "planer_tension_complete",
        tr("RELEASED PLANER DRIVE", "UVOLNĚNÝ POHON HOBLOVKY"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    releasedPlaner.visuals = {
        circle(94, 194, 20, P::lightGray, false), circle(159, 194, 20, P::lightGray, false),
        e2d::PolylineVisual{{{94, 214}, {124, 226}, {159, 214}}, P::brightGreen, false},
        label(94, 229, tr("SAFE", "VOLNÉ"), P::brightGreen),
    };
    addPickup(world, 53, "drive_belt", "You roll the released drive belt without cracking it.",
        "Uvolněný hnací řemen smotáš bez poškození.", 2, {e2d::Condition::flag("belt_released")});
    addPickup(world, 54, "oil_can", "The oil can is nearly full of heavy machine oil.",
        "Olejnička je téměř plná hustého strojního oleje.", 0);
    addPickup(world, 54, "hand_mirror", "A polished hand mirror survives in the filing-room cabinet.",
        "V brusírně přežilo vyleštěné ruční zrcátko.", 1);
    addUse(world, 55, "reserve_tank", "PROTECTED RESERVE TANK", "CHRÁNĚNÁ REZERVNÍ NÁDRŽ",
        "siphon_hose", "fuel_can_filled", "The hose fills the engine can from the protected reserve. Iris keeps the hose.",
        "Hadice naplní kanystr z chráněné zásoby. Iris si hadici ponechá.");
    world.interactions.back().mutations.push_back(e2d::Mutation::addItem("filled_fuel_can"));
    auto& reserveTank = ensureHotspot(world, 55, "reserve_tank",
        tr("PROTECTED RESERVE TANK", "CHRÁNĚNÁ REZERVNÍ NÁDRŽ"),
        {250, 135, 157, 125}, e2d::HotspotKind::mechanism);
    reserveTank.interactionArea = {250, 135, 157, 125};
    reserveTank.visibleWhen = {e2d::Condition::notFlag("fuel_can_filled")};
    reserveTank.visuals = {
        box(274, 145, 111, 96, P::darkGray), box(284, 155, 91, 76, P::black),
        box(295, 208, 69, 12, amber), line(329, 155, 329, 205, P::lightGray),
        label(299, 185, tr("FUEL", "PALIVO"), amber),
    };
    auto& filledReserve = ensureHotspot(world, 55, "reserve_tank_complete",
        tr("SIPHONED RESERVE", "PŘEČERPANÁ REZERVA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    filledReserve.visuals = {
        box(274, 145, 111, 96, P::darkGray), box(284, 155, 91, 76, P::black),
        box(295, 208, 69, 12, P::brightGreen),
        e2d::PolylineVisual{{{329, 171}, {344, 183}, {356, 229}, {374, 248}}, signalBlue, false},
        label(297, 185, tr("SIPHONED", "ODEBRÁNO"), P::brightGreen),
    };
    addContext(world, 56, "log_pike", "LOG PIKE", "HÁK NA KLÁDY", "spark_retrieved", {
        inspect(tr("The pike draws the floating maintenance box close enough to recover its dry spark plug.",
            "Hák přitáhne plovoucí servisní skříňku a její suchou zapalovací svíčku.")),
    }, {}, {e2d::Mutation::addItem("spark_plug")}, 1, "pickup");
    auto& pike = ensureHotspot(world, 56, "log_pike", tr("LOG PIKE", "HÁK NA KLÁDY"),
        {80, 135, 168, 125}, e2d::HotspotKind::mechanism, 1);
    pike.interactionArea = {80, 135, 168, 125};
    pike.visibleWhen = {e2d::Condition::notFlag("spark_retrieved")};
    pike.visuals = {
        line(92, 239, 224, 151, amber), line(218, 151, 231, 148, P::lightGray),
        line(224, 151, 228, 164, P::lightGray), box(239, 171, 54, 36, P::brown),
        label(96, 225, tr("PIKE", "HÁK"), pale),
    };
    auto& pikeComplete = ensureHotspot(world, 56, "log_pike_complete",
        tr("RECOVERED MAINTENANCE BOX", "VYTAŽENÁ SERVISNÍ BEDNA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    pikeComplete.visuals = {
        line(92, 239, 188, 185, P::lightGray), box(194, 202, 61, 38, P::brown),
        box(202, 210, 45, 22, amber), circle(224, 221, 4, P::brightGreen),
    };
    addHazard(world, 56, "log_pond", "log_pond_safe",
        "A turning log rolls Iris beneath the cold pond.", "Otáčející se kláda stáhne Iris pod studenou hladinu.");
    addPickup(world, 57, "rail_switch_key", "June's clue leads to the switch key in the foreman's boot.",
        "Junina nápověda vede ke klíči od výhybky v předákově botě.", 0,
        {e2d::Condition::flag("met_june")});
    addPickup(world, 57, "logger_token", "You pocket an old stamped logger token.",
        "Schováš si starý ražený dřevařský žeton.", 1);
    addCharacter(world, 58, "june", "JUNE MERCER", "JUNE MERCEROVÁ", "met_june", {
        speech(tr("June: Calder came through here testing railway grounds. Nightjar borrowed every older system on this mountain.",
            "June: Calderová tudy chodila měřit uzemnění tratě. Nightjar si půjčil každý starší systém hory.")),
        speech(tr("June: The mill whistle clears the trestle. Take this ration; guards still follow their stomachs.",
            "June: Píšťala pily uvolní viadukt. Vezmi tu dávku; strážní stále poslouchají žaludek.")),
        speech(tr("Iris: History and supper. Both may save us tonight.",
            "Iris: Historie a večeře. Dnes v noci nás může zachránit obojí."), e2d::MessageSpeaker::player),
    }, {}, {e2d::Mutation::addItem("sealed_ration"), e2d::Mutation::setFlag("whistle_known")});
    auto& june = ensureHotspot(world, 58, "june", tr("JUNE MERCER", "JUNE MERCEROVÁ"),
        {202, 137, 90, 123}, e2d::HotspotKind::character, 2);
    june.interactionArea = {202, 137, 90, 123};
    june.visuals = {
        circle(247, 169, 11, amber), box(237, 180, 21, 50, P::brightMagenta),
        line(239, 230, 232, 258, P::lightGray), line(255, 230, 262, 258, P::lightGray),
        line(237, 192, 219, 207, amber), line(258, 192, 278, 205, amber),
    };
    addUse(world, 59, "carbon_impression", "REVERSED CARBON IMPRESSION", "OBRÁCENÝ OTISK NA KOPÍRÁKU",
        "hand_mirror", "lift_time_known", "In the mirror, the faint pressure marks read RIDGE LIFT / 23:40.",
        "V zrcadle slabé stopy tlaku čtou HŘEBENOVÝ VÝTAH / 23:40.");
    auto& carbon = ensureHotspot(world, 59, "carbon_impression",
        tr("REVERSED CARBON IMPRESSION", "OBRÁCENÝ OTISK NA KOPÍRÁKU"),
        {238, 135, 160, 125}, e2d::HotspotKind::mechanism);
    carbon.interactionArea = {238, 135, 160, 125};
    carbon.visibleWhen = {e2d::Condition::notFlag("lift_time_known")};
    carbon.visuals = {
        box(267, 194, 110, 51, pale), line(278, 207, 364, 207, P::blue),
        line(278, 219, 350, 219, P::blue), label(288, 229, tr("?04:32", "?04:32"), P::red),
    };
    auto& carbonRead = ensureHotspot(world, 59, "carbon_impression_complete",
        tr("READ CARBON IMPRESSION", "PŘEČTENÝ OTISK NA KOPÍRÁKU"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    carbonRead.visuals = {
        box(267, 194, 110, 51, pale), label(279, 204, tr("RIDGE LIFT", "HŘEBEN VÝTAH"), P::black),
        label(299, 223, tr("23:40", "23:40"), danger), circle(372, 239, 4, P::brightGreen),
    };
    addUse(world, 60, "rail_points", "RAIL POINTS", "VÝHYBKA", "rail_switch_key", "rail_points_aligned",
        "The switch key locks the points onto the east reservoir line.",
        "Klíč uzamkne výhybku na východní trať k přehradě.");
    auto& railPoints = ensureHotspot(world, 60, "rail_points", tr("RAIL POINTS", "VÝHYBKA"),
        {112, 135, 142, 125}, e2d::HotspotKind::mechanism);
    railPoints.interactionArea = {112, 135, 142, 125};
    railPoints.visibleWhen = {e2d::Condition::notFlag("rail_points_aligned")};
    railPoints.visuals = {
        line(122, 239, 237, 199, P::lightGray), line(122, 249, 237, 209, P::lightGray),
        line(157, 242, 237, 238, danger), box(165, 156, 10, 73, P::darkGray),
        line(170, 164, 143, 139, amber), label(137, 225, tr("WEST", "ZÁPAD"), danger),
    };
    auto& pointsAligned = ensureHotspot(world, 60, "rail_points_complete",
        tr("ALIGNED EAST RAIL POINTS", "SROVNANÁ VÝCHODNÍ VÝHYBKA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    pointsAligned.visuals = {
        line(122, 239, 237, 199, P::brightGreen), line(122, 249, 237, 209, P::brightGreen),
        line(157, 242, 237, 209, P::brightGreen), box(165, 156, 10, 73, P::darkGray),
        line(170, 164, 197, 139, P::brightGreen), label(137, 225, tr("EAST", "VÝCHOD"), P::brightGreen),
    };
    addUse(world, 61, "engine_belt", "ENGINE DRIVE", "POHON LOKOMOTIVY", "drive_belt", "engine_belt_installed",
        "The planer belt settles around the engine pulleys.", "Řemen z hoblovky se usadí na řemenicích lokomotivy.", {}, true, 0);
    addUse(world, 61, "engine_ignition", "ENGINE IGNITION", "ZAPALOVÁNÍ LOKOMOTIVY", "spark_plug", "engine_plug_installed",
        "The dry spark plug seats in the cleaned cylinder head.", "Suchá svíčka zapadne do vyčištěné hlavy válce.", {}, true, 1);
    addUse(world, 61, "engine_bearings", "ENGINE BEARINGS", "LOŽISKA LOKOMOTIVY", "oil_can", "engine_oiled",
        "Heavy oil reaches every marked bearing cup.", "Hustý olej dorazí do každé označené maznice.", {}, true, 2);
    addUse(world, 61, "engine_fuel_tank", "ENGINE FUEL TANK", "PALIVOVÁ NÁDRŽ LOKOMOTIVY",
        "filled_fuel_can", "engine_fueled", "The protected reserve fuel fills the engine tank without a drop wasted.",
        "Palivo z chráněné zásoby naplní nádrž lokomotivy beze ztráty jediné kapky.", {}, true, 3);
    auto& engineBelt = ensureHotspot(world, 61, "engine_belt", tr("ENGINE DRIVE", "POHON LOKOMOTIVY"),
        {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    engineBelt.visibleWhen = {e2d::Condition::notFlag("engine_belt_installed")};
    engineBelt.visuals = {
        circle(94, 201, 18, P::lightGray, false), circle(134, 201, 18, P::lightGray, false),
        line(94, 183, 134, 219, danger), line(94, 219, 134, 183, danger),
        label(79, 157, tr("BELT", "ŘEMEN"), pale),
    };
    auto& beltInstalled = ensureHotspot(world, 61, "engine_belt_complete",
        tr("INSTALLED ENGINE BELT", "NAMONTOVANÝ ŘEMEN"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    beltInstalled.visuals = {
        circle(94, 201, 18, P::lightGray, false), circle(134, 201, 18, P::lightGray, false),
        e2d::PolylineVisual{{{94, 183}, {134, 183}, {134, 219}, {94, 219}, {94, 183}}, P::brightGreen, false},
    };
    auto& engineIgnition = ensureHotspot(world, 61, "engine_ignition",
        tr("ENGINE IGNITION", "ZAPALOVÁNÍ LOKOMOTIVY"),
        {164, 135, 96, 125}, e2d::HotspotKind::mechanism, 1);
    engineIgnition.visibleWhen = {e2d::Condition::notFlag("engine_plug_installed")};
    engineIgnition.visuals = {
        box(187, 171, 51, 59, P::black), line(212, 181, 212, 214, P::lightGray),
        line(201, 181, 223, 181, danger), circle(212, 218, 7, P::darkGray),
        label(180, 157, tr("PLUG", "SVÍČKA"), pale),
    };
    auto& plugInstalled = ensureHotspot(world, 61, "engine_ignition_complete",
        tr("INSTALLED SPARK PLUG", "NAMONTOVANÁ SVÍČKA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    plugInstalled.visuals = {
        box(187, 171, 51, 59, P::black), line(212, 177, 212, 218, pale),
        line(201, 185, 223, 185, P::brightGreen), circle(212, 220, 7, P::brightGreen),
    };
    auto& engineBearings = ensureHotspot(world, 61, "engine_bearings",
        tr("ENGINE BEARINGS", "LOŽISKA LOKOMOTIVY"),
        {265, 135, 96, 125}, e2d::HotspotKind::mechanism, 2);
    engineBearings.visibleWhen = {e2d::Condition::notFlag("engine_oiled")};
    engineBearings.visuals = {
        circle(293, 205, 14, P::darkGray, false), circle(333, 205, 14, P::darkGray, false),
        line(293, 191, 333, 219, danger), label(284, 157, tr("OIL", "OLEJ"), pale),
    };
    auto& bearingsOiled = ensureHotspot(world, 61, "engine_bearings_complete",
        tr("OILED ENGINE BEARINGS", "NAMAZANÁ LOŽISKA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    bearingsOiled.visuals = {
        circle(293, 205, 14, P::brightGreen, false), circle(333, 205, 14, P::brightGreen, false),
        line(293, 205, 333, 205, amber),
    };
    auto& engineFuel = ensureHotspot(world, 61, "engine_fuel_tank",
        tr("ENGINE FUEL TANK", "PALIVOVÁ NÁDRŽ LOKOMOTIVY"),
        {366, 135, 96, 125}, e2d::HotspotKind::mechanism, 3);
    engineFuel.visibleWhen = {e2d::Condition::notFlag("engine_fueled")};
    engineFuel.visuals = {
        box(382, 176, 63, 49, P::darkGray), box(390, 184, 47, 33, P::black),
        line(400, 206, 427, 206, danger), circle(414, 170, 6, amber),
        label(389, 157, tr("FUEL", "PALIVO"), pale),
    };
    auto& engineFueled = ensureHotspot(world, 61, "engine_fuel_tank_complete",
        tr("FILLED ENGINE TANK", "NAPLNĚNÁ NÁDRŽ"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    engineFueled.visuals = {
        box(382, 176, 63, 49, P::darkGray), box(390, 184, 47, 33, P::black),
        box(394, 198, 39, 15, P::brightGreen), circle(414, 170, 6, P::brightGreen),
    };
    addContext(world, 61, "engine_start", "ENGINE STARTER", "STARTÉR LOKOMOTIVY", "logging_engine_running", {
        speech(tr("Lila: Timing set. Give her the crank.", "Lila: Časování je hotové. Zatoč klikou.")),
        inspect(tr("The old engine fires, shakes loose thirty years of dust, and settles into a hard idle.",
            "Stará lokomotiva naskočí, setřese třicet let prachu a ustálí se v tvrdém volnoběhu.")),
    }, {e2d::Condition::flag("met_lila"), e2d::Condition::flag("engine_belt_installed"),
        e2d::Condition::flag("engine_plug_installed"), e2d::Condition::flag("engine_oiled"),
        e2d::Condition::flag("engine_fueled"), e2d::Condition::flag("rail_points_aligned")}, {}, 3, "power");
    auto& engineStarter = ensureHotspot(world, 61, "engine_start",
        tr("ENGINE STARTER", "STARTÉR LOKOMOTIVY"),
        {404, 135, 80, 125}, e2d::HotspotKind::mechanism, 3);
    engineStarter.interactionArea = {404, 135, 80, 125};
    engineStarter.visibleWhen = {
        e2d::Condition::flag("met_lila"), e2d::Condition::flag("engine_belt_installed"),
        e2d::Condition::flag("engine_plug_installed"), e2d::Condition::flag("engine_oiled"),
        e2d::Condition::flag("engine_fueled"), e2d::Condition::flag("rail_points_aligned"),
        e2d::Condition::notFlag("logging_engine_running"),
    };
    engineStarter.visuals = {
        circle(444, 200, 20, P::lightGray, false), line(444, 200, 461, 185, amber),
        label(418, 157, tr("START", "START"), P::brightGreen),
    };
    auto& engineRunning = ensureHotspot(world, 61, "engine_start_complete",
        tr("RUNNING LOGGING ENGINE", "BĚŽÍCÍ LESNÍ LOKOMOTIVA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    engineRunning.visuals = {
        circle(444, 200, 20, P::brightGreen, false), line(444, 200, 461, 185, P::brightGreen),
        circle(224, 183, 5, amber), circle(245, 183, 5, P::brightGreen),
        label(196, 158, tr("RUNNING", "BĚŽÍ"), P::brightGreen),
    };
    addContext(world, 62, "mill_whistle", "MILL WHISTLE CABLE", "LANKO PÍŠŤALY PILY", "trestle_guard_diverted", {
        inspect(tr("The old whistle rolls across the valley. The guard leaves the trestle to investigate.",
            "Stará píšťala se rozlehne údolím. Strážný opustí viadukt a jde pátrat.")),
    }, {e2d::Condition::flag("whistle_known")}, {}, 0, "warning");
    auto& whistle = ensureHotspot(world, 62, "mill_whistle",
        tr("MILL WHISTLE CABLE", "LANKO PÍŠŤALY PILY"),
        {48, 135, 132, 125}, e2d::HotspotKind::mechanism);
    whistle.interactionArea = {48, 135, 132, 125};
    whistle.visibleWhen = {e2d::Condition::flag("whistle_known"), e2d::Condition::notFlag("trestle_guard_diverted")};
    whistle.visuals = {
        circle(82, 164, 16, amber, false), line(82, 180, 82, 230, P::lightGray),
        line(82, 230, 111, 250, amber), label(100, 155, tr("PULL", "ZATÁHNI"), pale),
    };
    auto& whistleSounded = ensureHotspot(world, 62, "mill_whistle_complete",
        tr("SOUNDED MILL WHISTLE", "SPUŠTĚNÁ PÍŠŤALA PILY"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    whistleSounded.visuals = {
        circle(82, 164, 16, P::brightGreen, false), line(82, 180, 82, 230, P::lightGray),
        e2d::ArcVisual{{82, 164}, {28, 22}, 4.71239F, 1.5708F, amber},
        label(102, 155, tr("CLEAR", "VOLNO"), P::brightGreen),
    };
    addUse(world, 62, "brake_linkage", "BRAKE LINKAGE", "TÁHLO BRZDY", "wrench", "trestle_brake_fixed",
        "The wrench replaces the linkage pin and restores the engine brake.",
        "Klíč nahradí čep táhla a obnoví brzdu lokomotivy.", {}, false, 2);
    auto& brake = ensureHotspot(world, 62, "brake_linkage", tr("BRAKE LINKAGE", "TÁHLO BRZDY"),
        {245, 135, 122, 125}, e2d::HotspotKind::mechanism, 2);
    brake.interactionArea = {245, 135, 122, 125};
    brake.visibleWhen = {e2d::Condition::notFlag("trestle_brake_fixed")};
    brake.visuals = {
        box(268, 183, 76, 41, P::darkGray), circle(285, 203, 12, P::lightGray, false),
        circle(329, 203, 12, P::lightGray, false), line(297, 196, 317, 211, danger),
        label(266, 164, tr("MISSING PIN", "CHYBÍ ČEP"), danger),
    };
    auto& brakeFixed = ensureHotspot(world, 62, "brake_linkage_complete",
        tr("REPAIRED BRAKE LINKAGE", "OPRAVENÉ TÁHLO BRZDY"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    brakeFixed.visuals = {
        box(268, 183, 76, 41, P::darkGray), circle(285, 203, 12, P::brightGreen, false),
        circle(329, 203, 12, P::brightGreen, false), line(297, 203, 317, 203, P::brightGreen),
        circle(307, 203, 4, amber), label(270, 164, tr("BRAKE SAFE", "BRZDA OK"), P::brightGreen),
    };
    addHazard(world, 62, "rotten_trestle", "trestle_brake_fixed",
        "The unrepaired engine tears through the flexing trestle.", "Neopravená lokomotiva prorazí prohýbající se viadukt.");
    gateRight(world, 62, {e2d::Condition::flag("logging_engine_running"),
            e2d::Condition::flag("trestle_guard_diverted"), e2d::Condition::flag("trestle_brake_fixed")},
        "The running engine needs a clear trestle and repaired brake linkage.",
        "Jedoucí lokomotiva potřebuje volný viadukt a opravené táhlo brzdy.");
    addContext(world, 63, "portable_radio", "PORTABLE RADIO", "PŘENOSNÉ RÁDIO", "elias_contacted", {
        speech(tr("Elias: Black Pine, this is regional dispatch. Kestrel Six is down and losing beacon power.",
            "Elias: Black Pine, zde oblastní dispečink. Kestrel Six je na zemi a ztrácí napájení majáku.")),
        speech(tr("Iris: I hear you. I am following the jammer toward Nightjar. Keep that channel open.",
            "Iris: Slyším vás. Sleduji rušení k Nightjaru. Udržte kanál otevřený."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("logging_engine_running")}, {e2d::Mutation::setFlag("railway_complete")});
    auto& portableRadio = ensureHotspot(world, 63, "portable_radio",
        tr("PORTABLE RADIO", "PŘENOSNÉ RÁDIO"),
        {340, 135, 126, 125}, e2d::HotspotKind::mechanism);
    portableRadio.interactionArea = {340, 135, 126, 125};
    portableRadio.visibleWhen = {e2d::Condition::flag("logging_engine_running"), e2d::Condition::notFlag("elias_contacted")};
    portableRadio.visuals = {
        box(357, 160, 84, 60, P::lightGray), box(367, 170, 64, 40, P::black),
        circle(379, 191, 13, P::lightGray, false),
        line(399, 183, 425, 183, signalBlue), line(399, 194, 431, 194, danger),
        circle(427, 204, 4, amber), label(372, 225, tr("SIGNAL", "SIGNÁL"), amber),
    };
    auto& radioAnswered = ensureHotspot(world, 63, "portable_radio_complete",
        tr("OPEN DISPATCH CHANNEL", "OTEVŘENÝ KANÁL DISPEČINKU"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    radioAnswered.visuals = {
        box(357, 160, 84, 60, P::lightGray), box(367, 170, 64, 40, P::black),
        circle(379, 191, 13, P::brightGreen, false),
        e2d::PolylineVisual{{{399, 192}, {407, 180}, {415, 198}, {423, 184}, {431, 192}}, P::brightGreen, false},
        circle(427, 204, 4, P::brightGreen), label(370, 225, tr("ELIAS", "ELIAS"), P::brightGreen),
    };

    // The dam is a pair of connected work hubs. The catalogue order does not
    // represent its stairs, control doors, intake tunnel or drained-bay route.
    configureReservoirArtwork(world);
    setHorizontalRoute(world, 64, 63, 65);
    setHorizontalRoute(world, 65, 64, 66);
    setHorizontalRoute(world, 66, 65, 67);
    setHorizontalRoute(world, 67, 66, std::nullopt);
    for (int branch = 68; branch <= 72; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }
    setHorizontalRoute(world, 72, std::nullopt, 73);
    setHorizontalRoute(world, 73, 72, 74);
    setHorizontalRoute(world, 74, 73, std::nullopt);
    setHorizontalRoute(world, 75, std::nullopt, std::nullopt);

    addPortal(world, 65, "turbine_stairs", "STAIRS TO TURBINE HALL", "SCHODY DO TURBÍNOVÉ HALY",
        {360, 132, 124, 128}, 68, {
            line(374, 248, 458, 151, amber), line(397, 248, 481, 151, amber),
            line(387, 230, 411, 230, pale), line(403, 211, 427, 211, pale),
            line(419, 192, 443, 192, pale), line(435, 173, 459, 173, pale),
            label(375, 137, tr("TURBINES", "TURBÍNY"), amber),
        });
    addPortal(world, 68, "abutment_stairs", "STAIRS TO WEST ABUTMENT", "SCHODY K ZÁPADNÍ OPĚŘE",
        {0, 132, 64, 128}, 65, {
            line(8, 151, 52, 247, amber), line(28, 151, 72, 247, amber),
            line(19, 174, 42, 174, pale), line(28, 195, 51, 195, pale),
            label(8, 137, tr("DAM", "HRÁZ"), amber),
        });
    addPortal(world, 68, "lower_stairs", "STAIRS TO LOWER TURBINE HALL", "SCHODY DO DOLNÍ TURBÍNOVÉ HALY",
        {304, 132, 74, 128}, 69, {
            line(313, 151, 356, 247, amber), line(333, 151, 376, 247, amber),
            line(324, 176, 346, 176, pale), line(334, 198, 356, 198, pale),
            label(315, 137, tr("LOWER", "DOLŮ"), pale),
        });
    addPortal(world, 68, "pump_door", "CONTROL DOOR TO PUMP GALLERY", "ŘÍDICÍ DVEŘE DO ČERPACÍ GALERIE",
        {396, 132, 96, 128}, 70, {
            box(406, 150, 76, 110, P::lightGray), box(415, 159, 58, 95, P::black),
            circle(463, 207, 4, amber), label(421, 174, tr("PUMP", "ČERPADLO"), amber),
        });
    addPortal(world, 69, "upper_stairs", "STAIRS TO UPPER TURBINE HALL", "SCHODY DO HORNÍ TURBÍNOVÉ HALY",
        {0, 132, 68, 128}, 68, {
            line(8, 247, 51, 151, amber), line(28, 247, 71, 151, amber),
            line(19, 222, 42, 222, pale), line(29, 201, 52, 201, pale),
            label(8, 137, tr("UPPER", "NAHORU"), pale),
        });
    addPortal(world, 69, "pump_door", "DOOR TO PUMP GALLERY", "DVEŘE DO ČERPACÍ GALERIE",
        {408, 132, 84, 128}, 70, {
            box(418, 150, 64, 110, P::lightGray), box(427, 159, 46, 95, P::black),
            circle(464, 207, 4, amber), label(432, 174, tr("PUMP", "ČERP."), amber),
        });
    addPortal(world, 70, "upper_door", "DOOR TO UPPER TURBINE HALL", "DVEŘE DO HORNÍ TURBÍNOVÉ HALY",
        {0, 132, 50, 128}, 68, {box(6, 150, 39, 110, P::brown), label(8, 171, tr("UP", "HORNÍ"), pale)});
    addPortal(world, 70, "lower_door", "DOOR TO LOWER TURBINE HALL", "DVEŘE DO DOLNÍ TURBÍNOVÉ HALY",
        {66, 132, 50, 128}, 69, {box(72, 150, 39, 110, P::brown), label(74, 171, tr("LOW", "DOLNÍ"), pale)});
    addPortal(world, 70, "bay_door", "DOOR TO MAINTENANCE BAY", "DVEŘE DO SERVISNÍHO PROSTORU",
        {386, 132, 42, 128}, 71, {box(390, 150, 34, 110, P::blue), label(393, 171, tr("BAY", "PROST."), pale)});
    addPortal(world, 70, "intake_door", "DOOR TO INTAKE TUNNEL", "DVEŘE DO PŘÍVODNÍHO TUNELU",
        {444, 132, 48, 128}, 72, {box(448, 150, 39, 110, P::black), label(451, 171, tr("INTAKE", "PŘÍV."), amber)});
    addPortal(world, 71, "pump_door", "DOOR TO PUMP GALLERY", "DVEŘE DO ČERPACÍ GALERIE",
        {0, 132, 62, 128}, 70, {box(7, 150, 48, 110, P::brown), label(13, 171, tr("PUMP", "ČERP."), pale)});
    auto& shaftRoute = addPortal(world, 71, "shaft_route", "DRAINED PASSAGE TO EAST SHAFT",
        "ODČERPANÁ CESTA K VÝCHODNÍ ŠACHTĚ", {397, 132, 95, 128}, 75, {
            e2d::PolygonVisual{{{401, 183}, {452, 183}, {483, 200}, {452, 217}, {401, 217}}, amber, true},
            label(412, 195, tr("SHAFT", "ŠACHTA"), P::black),
        }, {e2d::Condition::flag("pump_running"), e2d::Condition::flag("taken_magnet_cord")});
    world.addInteraction({e2d::Verb::context, shaftRoute.id, std::nullopt,
        {e2d::Condition::notFlag("taken_magnet_cord")},
        {inspect(tr("The east passage remains flooded. Start the pump, then recover the magnet before leaving the bay.",
            "Východní průchod zůstává zatopený. Spusť čerpadlo a před odchodem vytáhni magnet."))},
        {}, 10, {}});
    addPortal(world, 72, "pump_door", "DOOR TO PUMP GALLERY", "DVEŘE DO ČERPACÍ GALERIE",
        {0, 132, 62, 128}, 70, {box(7, 150, 48, 110, P::brown), label(13, 171, tr("PUMP", "ČERP."), pale)});
    addPortal(world, 75, "bay_route", "DRAINED PASSAGE TO MAINTENANCE BAY",
        "ODČERPANÁ CESTA DO SERVISNÍHO PROSTORU", {0, 132, 74, 128}, 71, {
            e2d::PolygonVisual{{{7, 183}, {31, 168}, {68, 168}, {68, 198}, {31, 198}}, amber, true},
            label(27, 178, tr("BAY", "PROST."), P::black),
        });
    auto& mineLadder = addPortal(world, 75, "mine_ladder", "LADDER DOWN TO ORE CART CHAMBER",
        "ŽEBŘÍK DOLŮ DO KOMORY S VOZÍKEM", {405, 132, 79, 128}, 76, {
            line(420, 142, 420, 252, amber), line(444, 142, 444, 252, amber),
            line(420, 160, 444, 160, pale), line(420, 183, 444, 183, pale),
            line(420, 206, 444, 206, pale), line(420, 229, 444, 229, pale),
            label(407, 137, tr("MINE", "DŮL"), amber),
        }, {e2d::Condition::flag("mine_access_open")});
    mineLadder.visibleWhen = {e2d::Condition::flag("mine_access_open")};

    addPickup(world, 65, "insulated_boots", "The rescue locker holds dry insulated boots.",
        "Záchranná skříňka obsahuje suché izolační boty.", 0);
    addPickup(world, 65, "turbine_badge", "Jonah's turbine badge lies on the safe side of the rail.",
        "Jonahův odznak turbíny leží na bezpečné straně zábradlí.", 1);
    addUse(world, 66, "spray_shield", "LOOSE SPRAY SHIELD", "UVOLNĚNÁ VODNÍ CLONA", "wrench", "spray_shield_fixed",
        "The shield locks into its safe timing position.", "Clona se zajistí v bezpečné časované poloze.");
    auto& sprayShield = ensureHotspot(world, 66, "spray_shield",
        tr("LOOSE SPRAY SHIELD", "UVOLNĚNÁ VODNÍ CLONA"),
        {63, 135, 126, 125}, e2d::HotspotKind::mechanism);
    sprayShield.interactionArea = {63, 135, 126, 125};
    sprayShield.visibleWhen = {e2d::Condition::notFlag("spray_shield_fixed")};
    sprayShield.visuals = {
        box(83, 173, 82, 24, P::darkGray), line(91, 185, 157, 171, danger),
        circle(92, 185, 6, P::lightGray, false), circle(156, 173, 6, P::lightGray, false),
        label(76, 210, tr("LOOSE SHIELD", "VOLNÁ CLONA"), danger),
    };
    auto& fixedShield = ensureHotspot(world, 66, "spray_shield_complete",
        tr("SECURED SPRAY SHIELD", "ZAJIŠTĚNÁ VODNÍ CLONA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    fixedShield.visuals = {
        box(83, 173, 82, 24, P::darkGray), line(91, 185, 157, 185, P::brightGreen),
        circle(92, 185, 6, P::brightGreen, false), circle(156, 185, 6, P::brightGreen, false),
        label(84, 210, tr("SHIELD SAFE", "CLONA OK"), P::brightGreen),
    };
    addHazard(world, 66, "spillway_spray", "spray_shield_fixed",
        "The unsecured spray shield sweeps Iris into the spillway.", "Nezajištěná vodní clona smete Iris do přelivu.");
    gateRight(world, 66, {e2d::Condition::flag("spray_shield_fixed")},
        "The loose spray shield makes the walk unsafe.", "Uvolněná vodní clona činí chodník nebezpečným.");
    addUse(world, 67, "gatehouse_reader", "GATEHOUSE BADGE READER", "ČTEČKA DOMKU STAVIDEL",
        "turbine_badge", "gatehouse_open", "Jonah's badge opens the control vestibule.",
        "Jonahův odznak otevře vestibul ovládání.");
    auto& gateReader = ensureHotspot(world, 67, "gatehouse_reader",
        tr("GATEHOUSE BADGE READER", "ČTEČKA DOMKU STAVIDEL"),
        {142, 135, 100, 125}, e2d::HotspotKind::mechanism);
    gateReader.interactionArea = {142, 135, 100, 125};
    gateReader.visibleWhen = {e2d::Condition::notFlag("gatehouse_open")};
    gateReader.visuals = {
        box(170, 166, 54, 63, P::darkGray), box(178, 174, 38, 47, P::black),
        circle(197, 188, 7, danger), box(183, 204, 28, 9, P::red),
        label(174, 235, tr("LOCKED", "ZAMČENO"), danger),
    };
    auto& gateReaderOpen = ensureHotspot(world, 67, "gatehouse_reader_complete",
        tr("OPEN GATEHOUSE VESTIBULE", "OTEVŘENÝ VESTIBUL DOMKU"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    gateReaderOpen.visuals = {
        box(170, 166, 54, 63, P::darkGray), box(178, 174, 38, 47, P::black),
        circle(197, 188, 7, P::brightGreen), box(183, 204, 28, 9, P::brightGreen),
        label(180, 235, tr("OPEN", "OTEVŘENO"), P::brightGreen),
    };
    addPickup(world, 67, "spillway_crank", "You lift the removable emergency crank from Jonah's control locker.",
        "Z Jonahovy ovládací skříňky zvedneš odnimatelnou nouzovou kliku.", 1,
        {e2d::Condition::flag("gatehouse_open")});
    addUse(world, 67, "spillway_crank_socket", "SPILLWAY CRANK SOCKET", "OBJÍMKA KLIKY PŘELIVU",
        "spillway_crank", "spillway_closed", "Iris inserts the emergency crank and closes the false-open command by hand.",
        "Iris zasune nouzovou kliku a ručně zruší falešný povel k otevření.",
        {e2d::Condition::flag("gatehouse_open")}, true, 2);
    auto& crankSocket = ensureHotspot(world, 67, "spillway_crank_socket",
        tr("SPILLWAY CRANK SOCKET", "OBJÍMKA KLIKY PŘELIVU"),
        {260, 135, 105, 125}, e2d::HotspotKind::mechanism, 2);
    crankSocket.interactionArea = {260, 135, 105, 125};
    crankSocket.visibleWhen = {e2d::Condition::flag("gatehouse_open"), e2d::Condition::notFlag("spillway_closed")};
    crankSocket.visuals = {
        circle(313, 190, 31, P::lightGray, false), circle(313, 190, 8, P::black),
        line(313, 159, 313, 221, danger), line(282, 190, 344, 190, danger),
        label(273, 231, tr("CRANK SOCKET", "OBJÍMKA KLIKY"), amber),
    };
    auto& spillwayClosed = ensureHotspot(world, 67, "spillway_crank_socket_complete",
        tr("CLOSED SPILLWAY GATE", "UZAVŘENÉ STAVIDLO PŘELIVU"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    spillwayClosed.visuals = {
        box(297, 76, 159, 31, P::black), label(315, 86, tr("GATE CLOSED", "STAVIDLO ZAVŘENO"), P::brightGreen),
        circle(313, 190, 31, P::brightGreen, false), circle(313, 190, 8, amber),
        line(313, 159, 313, 221, P::brightGreen), line(282, 190, 344, 190, P::brightGreen),
        label(283, 231, tr("GATE CLOSED", "HRÁZ ZAVŘENA"), P::brightGreen),
        line(79, 123, 57, 109, P::brightGreen),
    };
    addCharacter(world, 67, "jonah", "JONAH REED", "JONAH REED", "jonah_briefed", {
        speech(tr("Jonah: That flood command came from the ridge, not this gatehouse.",
            "Jonah: Ten povel k záplavě přišel z hřebene, ne z tohoto domku.")),
        speech(tr("Jonah: Drain the maintenance bay and I can open the east mine shaft.",
            "Jonah: Odčerpej servisní prostor a já otevřu východní důlní šachtu.")),
        speech(tr("Iris: Voss is erasing his route. We will preserve it instead.",
            "Iris: Voss maže svou cestu. My ji naopak zachováme."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("spillway_closed")});
    auto& jonah = ensureHotspot(world, 67, "jonah", tr("JONAH REED", "JONAH REED"),
        {376, 135, 96, 125}, e2d::HotspotKind::character, 2);
    jonah.interactionArea = {376, 135, 96, 125};
    jonah.visibleWhen = {e2d::Condition::flag("spillway_closed")};
    jonah.visuals = {
        circle(424, 171, 11, amber), box(414, 182, 21, 49, P::brightCyan),
        line(416, 231, 409, 258, P::lightGray), line(432, 231, 439, 258, P::lightGray),
        line(414, 194, 397, 207, amber), line(435, 194, 452, 207, amber),
    };
    addContext(world, 68, "power_diagram", "AUXILIARY POWER DIAGRAM", "SCHÉMA POMOCNÉHO NAPÁJENÍ",
        "dam_diagram_read", {inspect(tr("The diagram gives a safe breaker order and links the dam feed to the mine substation.",
            "Schéma uvádí bezpečné pořadí jističů a spojuje přehradu s důlní rozvodnou."))});
    auto& powerDiagram = ensureHotspot(world, 68, "power_diagram",
        tr("AUXILIARY POWER DIAGRAM", "SCHÉMA POMOCNÉHO NAPÁJENÍ"),
        {171, 135, 111, 125}, e2d::HotspotKind::mechanism);
    powerDiagram.interactionArea = {171, 135, 111, 125};
    powerDiagram.visibleWhen = {e2d::Condition::notFlag("dam_diagram_read")};
    powerDiagram.visuals = {
        box(183, 151, 87, 77, pale), box(191, 159, 71, 61, P::blue),
        line(198, 207, 211, 184, signalBlue), line(211, 184, 227, 203, P::brightGreen),
        line(227, 203, 244, 174, signalBlue), line(244, 174, 256, 196, danger),
        label(188, 232, tr("READ ORDER", "ČTI POŘADÍ"), amber),
    };
    auto& diagramRead = ensureHotspot(world, 68, "power_diagram_complete",
        tr("READ AUXILIARY POWER DIAGRAM", "PŘEČTENÉ SCHÉMA POMOCNÉHO NAPÁJENÍ"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    diagramRead.visuals = {
        box(183, 151, 87, 77, pale), box(191, 159, 71, 61, P::black),
        circle(204, 178, 5, P::brightGreen), circle(227, 190, 5, P::brightGreen),
        circle(250, 202, 5, P::brightGreen), line(204, 178, 227, 190, signalBlue),
        line(227, 190, 250, 202, signalBlue), label(194, 232, tr("1-2-3", "1-2-3"), P::brightGreen),
    };
    addPickup(world, 69, "pump_gasket", "A fresh pump gasket remains in the service cabinet.",
        "V servisní skříňce zůstalo nové těsnění čerpadla.", 0);
    addContext(world, 69, "bay_breakers", "FLOODED-BAY BREAKERS", "JISTIČE ZATOPENÉHO PROSTORU",
        "bay_isolated", {inspect(tr("Following the diagram, Iris opens the three breakers. The water stops arcing.",
            "Podle schématu Iris vypne tři jističe. Voda přestane jiskřit."))},
        {e2d::Condition::flag("dam_diagram_read")}, {}, 2, "power");
    auto& bayBreakers = ensureHotspot(world, 69, "bay_breakers",
        tr("FLOODED-BAY BREAKERS", "JISTIČE ZATOPENÉHO PROSTORU"),
        {238, 135, 150, 125}, e2d::HotspotKind::mechanism, 2);
    bayBreakers.interactionArea = {238, 135, 150, 125};
    bayBreakers.visibleWhen = {e2d::Condition::flag("dam_diagram_read"), e2d::Condition::notFlag("bay_isolated")};
    bayBreakers.visuals = {
        box(250, 155, 126, 83, P::lightGray), box(259, 164, 108, 65, P::black),
        box(268, 176, 20, 36, P::darkGray), box(303, 176, 20, 36, P::darkGray),
        box(338, 176, 20, 36, P::darkGray),
        line(278, 181, 278, 205, danger), line(313, 181, 313, 205, danger),
        line(348, 181, 348, 205, danger), label(273, 218, tr("LIVE BAY", "PROUD V PROST."), danger),
    };
    auto& bayIsolated = ensureHotspot(world, 69, "bay_breakers_complete",
        tr("ISOLATED MAINTENANCE BAY", "ODPOJENÝ SERVISNÍ PROSTOR"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    bayIsolated.visuals = {
        box(250, 155, 126, 83, P::lightGray), box(259, 164, 108, 65, P::black),
        box(268, 176, 20, 36, P::darkGray), box(303, 176, 20, 36, P::darkGray),
        box(338, 176, 20, 36, P::darkGray),
        line(278, 205, 278, 181, P::brightGreen), line(313, 205, 313, 181, P::brightGreen),
        line(348, 205, 348, 181, P::brightGreen), label(278, 218, tr("ISOLATED", "ODPOJENO"), P::brightGreen),
    };
    addPickup(world, 70, "dry_cell", "A charged dry cell waits beside the emergency starter.",
        "Vedle nouzového startéru čeká nabitý suchý článek.", 0);
    addUse(world, 70, "pump_flange", "EMERGENCY PUMP FLANGE", "PŘÍRUBA NOUZOVÉHO ČERPADLA",
        "pump_gasket", "pump_gasket_installed", "The new gasket seals the cracked pump flange.",
        "Nové těsnění utěsní prasklou přírubu čerpadla.", {}, true, 0);
    addUse(world, 70, "pump_starter", "EMERGENCY PUMP STARTER", "STARTÉR NOUZOVÉHO ČERPADLA",
        "dry_cell", "pump_battery_installed", "The charged dry cell wakes the starter lamp.",
        "Nabitý suchý článek rozsvítí kontrolku startéru.", {}, true, 2);
    auto& pumpFlange = ensureHotspot(world, 70, "pump_flange",
        tr("EMERGENCY PUMP FLANGE", "PŘÍRUBA NOUZOVÉHO ČERPADLA"),
        {125, 135, 90, 125}, e2d::HotspotKind::mechanism);
    pumpFlange.interactionArea = {125, 135, 90, 125};
    pumpFlange.visibleWhen = {e2d::Condition::notFlag("pump_gasket_installed")};
    pumpFlange.visuals = {
        circle(170, 194, 29, P::lightGray, false), circle(170, 194, 13, P::black),
        line(150, 174, 190, 214, danger), line(190, 174, 150, 214, danger),
        label(140, 228, tr("NO GASKET", "BEZ TĚSNĚNÍ"), danger),
    };
    auto& flangeSealed = ensureHotspot(world, 70, "pump_flange_complete",
        tr("SEALED PUMP FLANGE", "UTĚSNĚNÁ PŘÍRUBA ČERPADLA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    flangeSealed.visuals = {
        circle(170, 194, 29, P::brightGreen, false), circle(170, 194, 13, P::black),
        circle(170, 194, 20, amber, false), label(145, 228, tr("SEALED", "TĚSNÍ"), P::brightGreen),
    };
    auto& pumpStarter = ensureHotspot(world, 70, "pump_starter",
        tr("EMERGENCY PUMP STARTER", "STARTÉR NOUZOVÉHO ČERPADLA"),
        {230, 135, 90, 125}, e2d::HotspotKind::mechanism, 2);
    pumpStarter.interactionArea = {230, 135, 90, 125};
    pumpStarter.visibleWhen = {e2d::Condition::notFlag("pump_battery_installed")};
    pumpStarter.visuals = {
        box(249, 168, 52, 61, P::darkGray), box(257, 176, 36, 45, P::black),
        circle(275, 188, 7, danger), box(262, 204, 26, 9, P::red),
        label(245, 233, tr("NO CELL", "BEZ ČLÁNKU"), danger),
    };
    auto& starterReady = ensureHotspot(world, 70, "pump_starter_complete",
        tr("POWERED PUMP STARTER", "NAPÁJENÝ STARTÉR ČERPADLA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    starterReady.visuals = {
        box(249, 168, 52, 61, P::darkGray), box(257, 176, 36, 45, P::black),
        circle(275, 188, 7, P::brightGreen), box(262, 204, 26, 9, P::brightGreen),
        label(253, 233, tr("READY", "PŘIPRAVEN"), P::brightGreen),
    };

    addUse(world, 72, "intake_markings", "DARK INTAKE-TUNNEL MARKINGS", "TMAVÉ ZNAČKY PŘÍVODNÍHO TUNELU",
        "hand_crank_torch", "intake_tunnel_lit",
        "The hand-crank torch reveals Kline's chalk warning and the removable bypass wheel.",
        "Ruční svítilna odhalí Klineové křídové varování a odnímatelné kolo obtoku.");
    auto& intakeMarkings = ensureHotspot(world, 72, "intake_markings",
        tr("DARK INTAKE-TUNNEL MARKINGS", "TMAVÉ ZNAČKY PŘÍVODNÍHO TUNELU"),
        {88, 135, 150, 125}, e2d::HotspotKind::mechanism);
    intakeMarkings.interactionArea = {88, 135, 150, 125};
    intakeMarkings.visibleWhen = {e2d::Condition::notFlag("intake_tunnel_lit")};
    intakeMarkings.visuals = {
        e2d::ArcVisual{{164, 218}, {55, 70}, 3.14159F, 6.28318F, P::darkGray},
        box(116, 176, 96, 54, P::black), label(134, 193, tr("NO LIGHT", "BEZ SVĚTLA"), P::darkGray),
    };
    auto& intakeLit = ensureHotspot(world, 72, "intake_markings_complete",
        tr("KLINE'S CHALK WARNING", "KLINEOVÉ KŘÍDOVÉ VAROVÁNÍ"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    intakeLit.visuals = {
        e2d::PolygonVisual{{{72, 247}, {127, 142}, {276, 142}, {334, 247}}, amber, false},
        label(96, 159, tr("THE FIELD FOLLOWS", "POLE NÁSLEDUJE"), pale),
        label(112, 178, tr("THE CARRIER", "NOSNOU VLNU"), signalBlue),
        circle(319, 197, 24, P::lightGray, false), line(295, 197, 343, 197, P::lightGray),
        line(319, 173, 319, 221, P::lightGray),
    };
    addPickup(world, 72, "valve_wheel", "You detach the redundant bypass valve wheel.",
        "Odpojíš kolo nepotřebného obtokového ventilu.", 0,
        {e2d::Condition::flag("intake_tunnel_lit")});
    addUse(world, 74, "intake_valve", "PUMP INTAKE VALVE", "PŘÍVODNÍ VENTIL ČERPADLA", "valve_wheel", "pump_intake_open",
        "The wheel opens the intake in the direction shown by the turbine diagram.",
        "Kolo otevře přívod ve směru označeném na schématu turbíny.",
        {e2d::Condition::flag("dam_diagram_read")}, true);
    auto& intakeValve = ensureHotspot(world, 74, "intake_valve",
        tr("PUMP INTAKE VALVE", "PŘÍVODNÍ VENTIL ČERPADLA"),
        {280, 135, 150, 125}, e2d::HotspotKind::mechanism);
    intakeValve.interactionArea = {280, 135, 150, 125};
    intakeValve.visibleWhen = {e2d::Condition::flag("dam_diagram_read"), e2d::Condition::notFlag("pump_intake_open")};
    intakeValve.visuals = {
        box(306, 174, 96, 55, P::darkGray), circle(354, 201, 28, P::lightGray, false),
        circle(354, 201, 7, danger), line(354, 173, 354, 229, P::lightGray),
        line(326, 201, 382, 201, P::lightGray), label(307, 237, tr("WHEEL MISSING", "CHYBÍ KOLO"), danger),
    };
    auto& intakeOpen = ensureHotspot(world, 74, "intake_valve_complete",
        tr("OPEN PUMP INTAKE", "OTEVŘENÝ PŘÍVOD ČERPADLA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    intakeOpen.visuals = {
        box(306, 174, 96, 55, P::darkGray), circle(354, 201, 28, P::brightGreen, false),
        circle(354, 201, 7, amber), line(334, 181, 374, 221, P::brightGreen),
        line(374, 181, 334, 221, P::brightGreen), label(325, 237, tr("INTAKE OPEN", "PŘÍVOD OTEVŘEN"), P::brightGreen),
    };
    room(world, 74).animations.push_back({targetId(74, "intake_pressure"), true, true,
        {e2d::Condition::flag("pump_intake_open")}, {
            {7, {line(39, 106, 185, 106, signalBlue), circle(393, 117, 9, P::brightGreen)}},
            {7, {line(112, 126, 263, 126, P::brightCyan), circle(393, 117, 6, P::brightGreen)}},
        }});
    addContext(world, 70, "pump_controls", "EMERGENCY PUMP CONTROLS", "OVLÁDÁNÍ NOUZOVÉHO ČERPADLA",
        "pump_running", {inspect(tr("The primed pump catches and lowers the maintenance-bay water in four visible stages.",
            "Zavodněné čerpadlo se rozběhne a ve čtyřech stupních sníží vodu v servisním prostoru."))},
        {e2d::Condition::flag("pump_gasket_installed"), e2d::Condition::flag("pump_battery_installed"),
            e2d::Condition::flag("pump_intake_open"), e2d::Condition::flag("bay_isolated")}, {}, 3, "power");
    auto& pumpControls = ensureHotspot(world, 70, "pump_controls",
        tr("EMERGENCY PUMP CONTROLS", "OVLÁDÁNÍ NOUZOVÉHO ČERPADLA"),
        {335, 135, 35, 125}, e2d::HotspotKind::mechanism, 3);
    pumpControls.interactionArea = {335, 135, 35, 125};
    pumpControls.visibleWhen = {
        e2d::Condition::flag("pump_gasket_installed"), e2d::Condition::flag("pump_battery_installed"),
        e2d::Condition::flag("pump_intake_open"), e2d::Condition::flag("bay_isolated"),
        e2d::Condition::notFlag("pump_running"),
    };
    pumpControls.visuals = {
        box(339, 171, 27, 59, P::darkGray), circle(352, 189, 9, P::brightGreen),
        line(352, 204, 352, 221, amber), label(337, 234, tr("START", "START"), P::brightGreen),
    };
    auto& pumpRunning = ensureHotspot(world, 70, "pump_controls_complete",
        tr("RUNNING EMERGENCY PUMP", "BĚŽÍCÍ NOUZOVÉ ČERPADLO"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    pumpRunning.visuals = {
        box(339, 171, 27, 59, P::darkGray), circle(352, 189, 9, P::brightGreen),
        line(344, 210, 360, 210, P::brightGreen), label(334, 234, tr("RUNNING", "BĚŽÍ"), P::brightGreen),
    };
    addPickup(world, 71, "magnet_cord", "Wearing insulated boots, Iris retrieves the magnet from the shallow locker.",
        "V izolačních botách Iris vytáhne magnet z mělké skříňky.", 0,
        {e2d::Condition::flag("pump_running"), e2d::Condition::has("insulated_boots")});
    addHazard(world, 71, "electrified_water", "bay_isolated",
        "Current flashes through the flooded maintenance bay.", "Zatopeným servisním prostorem projede proud.");
    auto& liveBayWater = ensureHotspot(world, 71, "live_bay_water",
        tr("DEEP ELECTRIFIED WATER", "HLUBOKÁ VODA POD PROUDEM"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    liveBayWater.visibleWhen = {e2d::Condition::notFlag("bay_isolated")};
    liveBayWater.visuals = {
        box(36, 125, 420, 135, P::blue), line(36, 125, 456, 125, P::brightCyan),
        line(79, 161, 192, 161, signalBlue), line(269, 180, 426, 180, signalBlue),
        line(302, 118, 316, 102, pale), line(316, 102, 329, 119, signalBlue),
        label(172, 139, tr("LIVE WATER", "VODA POD PROUDEM"), danger),
    };
    auto& isolatedBayWater = ensureHotspot(world, 71, "isolated_bay_water",
        tr("ISOLATED FLOODED BAY", "ODPOJENÝ ZATOPENÝ PROSTOR"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    isolatedBayWater.visibleWhen = {e2d::Condition::flag("bay_isolated"), e2d::Condition::notFlag("pump_running")};
    isolatedBayWater.visuals = {
        box(36, 164, 420, 96, P::blue), line(36, 164, 456, 164, signalBlue),
        line(71, 191, 211, 191, P::brightCyan), line(279, 211, 429, 211, P::brightCyan),
        label(171, 176, tr("POWER ISOLATED", "PROUD ODPOJEN"), amber),
    };
    auto& drainedBay = ensureHotspot(world, 71, "drained_bay_water",
        tr("DRAINED MAINTENANCE BAY", "ODČERPANÝ SERVISNÍ PROSTOR"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    drainedBay.visibleWhen = {e2d::Condition::flag("pump_running")};
    drainedBay.visuals = {
        box(36, 226, 420, 34, P::blue), line(36, 226, 456, 226, P::brightCyan),
        line(75, 244, 214, 244, signalBlue), line(275, 251, 422, 251, signalBlue),
        label(166, 231, tr("BAY DRAINED", "PROSTOR ODČERPÁN"), P::brightGreen),
    };
    room(world, 71).animations.push_back({targetId(71, "residual_ripple"), true, true,
        {e2d::Condition::flag("pump_running")}, {
            {9, {line(71, 244, 187, 244, P::brightCyan)}},
            {9, {line(86, 244, 202, 244, signalBlue)}},
        }});
    addContext(world, 75, "shaft_grille", "EAST SHAFT GRILLE", "MŘÍŽ VÝCHODNÍ ŠACHTY", "mine_access_open", {
        speech(tr("Jonah: Water is down. I have released the east grille. Watch the mine gas below.",
            "Jonah: Voda klesla. Uvolnil jsem východní mříž. Dole pozor na důlní plyn.")),
    }, {e2d::Condition::flag("jonah_briefed"), e2d::Condition::flag("pump_running"),
        e2d::Condition::flag("taken_magnet_cord")}, {e2d::Mutation::setFlag("reservoir_complete")}, 2, "unlock");
    auto& shaftGrille = ensureHotspot(world, 75, "shaft_grille",
        tr("EAST SHAFT GRILLE", "MŘÍŽ VÝCHODNÍ ŠACHTY"),
        {205, 135, 145, 125}, e2d::HotspotKind::mechanism, 2);
    shaftGrille.interactionArea = {205, 135, 145, 125};
    shaftGrille.visibleWhen = {e2d::Condition::notFlag("mine_access_open")};
    shaftGrille.visuals = {
        box(218, 151, 119, 94, P::lightGray, false),
        line(218, 151, 337, 245, P::lightGray), line(337, 151, 218, 245, P::lightGray),
        line(278, 151, 278, 245, P::lightGray), box(263, 188, 30, 23, P::red),
        circle(278, 199, 5, amber), label(230, 231, tr("GRILLE LOCKED", "MŘÍŽ ZAVŘENA"), danger),
    };
    auto& shaftOpen = ensureHotspot(world, 75, "shaft_grille_complete",
        tr("OPEN EAST SHAFT", "OTEVŘENÁ VÝCHODNÍ ŠACHTA"),
        {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    shaftOpen.visuals = {
        box(218, 151, 119, 22, P::lightGray, false),
        line(218, 151, 248, 173, P::lightGray), line(337, 151, 307, 173, P::lightGray),
        box(233, 181, 89, 64, P::black), label(239, 231, tr("SHAFT OPEN", "ŠACHTA OTEVŘENA"), P::brightGreen),
        circle(278, 199, 5, P::brightGreen),
    };

    // The mine follows the workings shown on Voss's survey map. Ventilation is
    // a side room, the survey chamber is the ore-cart hub, and a maintenance
    // crawl reaches the substation before the freight cage has power.
    configureMineArtwork(world);
    setHorizontalRoute(world, 76, std::nullopt, 77);
    setHorizontalRoute(world, 77, 76, 78);
    setHorizontalRoute(world, 78, 77, 80);
    setHorizontalRoute(world, 79, std::nullopt, std::nullopt);
    setHorizontalRoute(world, 80, 78, 81);
    setHorizontalRoute(world, 81, 80, 82);
    setHorizontalRoute(world, 82, 81, 83);
    setHorizontalRoute(world, 83, 82, 84);
    for (int branch = 84; branch <= 90; ++branch) {
        setHorizontalRoute(world, branch, std::nullopt, std::nullopt);
    }

    addPortal(world, 76, "shaft_ladder", "LADDER UP TO EAST ACCESS SHAFT",
        "ŽEBŘÍK NAHORU DO VÝCHODNÍ ŠACHTY", {0, 132, 64, 128}, 75, {
            line(15, 143, 15, 252, amber), line(39, 143, 39, 252, amber),
            line(15, 163, 39, 163, pale), line(15, 186, 39, 186, pale),
            line(15, 209, 39, 209, pale), line(15, 232, 39, 232, pale),
            label(7, 137, tr("SHAFT", "ŠACHTA"), amber),
        });
    auto& cartReturn = addPortal(world, 76, "cart_shortcut", "WORKING ORE-CART SHORTCUT",
        "FUNKČNÍ ZKRATKA DŮLNÍM VOZÍKEM", {83, 108, 168, 132}, 83, {
            box(93, 145, 141, 59, P::red), circle(119, 216, 22, P::black),
            circle(209, 216, 22, P::black), label(108, 126, tr("SURVEY CART", "PRŮZKUMNÝ VOZÍK"), amber),
        }, {e2d::Condition::flag("mine_cart_ready")});
    cartReturn.visibleWhen = {e2d::Condition::flag("mine_cart_ready")};
    addPortal(world, 77, "vent_spur", "BLUE-MARKED VENTILATION SPUR",
        "MODŘE ZNAČENÁ ODBOČKA VĚTRÁNÍ", {386, 137, 98, 123}, 79, {
            e2d::PolygonVisual{{{386, 172}, {443, 172}, {478, 192}, {443, 212}, {386, 212}}, signalBlue, true},
            label(398, 184, tr("VENT", "VĚTRÁNÍ"), P::black),
        });
    addPortal(world, 79, "gallery_door", "DOOR TO TIMBER GALLERY", "DVEŘE DO VYZTUŽENÉ CHODBY",
        {0, 137, 61, 123}, 77, {box(7, 153, 46, 107, P::brown), label(10, 172, tr("GALLERY", "CHODBA"), pale)});
    auto& pumpPassage = addPortal(world, 79, "pump_passage", "VENT DUCT TO MINE PUMP",
        "VĚTRACÍ KANÁL K DŮLNÍMU ČERPADLU", {432, 137, 60, 123}, 81, {
            e2d::ArcVisual{{461, 260}, {27, 91}, 3.14159F, 6.28318F, P::lightGray},
            label(438, 172, tr("PUMP", "ČERP."), amber),
        }, {e2d::Condition::flag("ventilation_running")});
    pumpPassage.visibleWhen = {e2d::Condition::flag("ventilation_running")};
    auto& ventPassage = addPortal(world, 81, "vent_passage", "RUNNING VENT DUCT",
        "BĚŽÍCÍ VĚTRACÍ KANÁL", {0, 137, 61, 123}, 79, {
            e2d::ArcVisual{{30, 260}, {28, 91}, 3.14159F, 6.28318F, P::lightGray},
            label(8, 172, tr("VENT", "VĚTR."), amber),
        }, {e2d::Condition::flag("ventilation_running")});
    ventPassage.visibleWhen = {e2d::Condition::flag("ventilation_running")};

    auto& surveyCart = ensureHotspot(world, 83, "cart_shortcut",
        tr("SURVEY ORE CART", "PRŮZKUMNÝ DŮLNÍ VOZÍK"), {293, 137, 159, 123}, e2d::HotspotKind::mechanism, 3);
    surveyCart.interactionArea = {293, 137, 159, 123};
    surveyCart.visuals = {
        box(302, 151, 130, 49, P::red), circle(323, 211, 21, P::black), circle(411, 211, 21, P::black),
        line(295, 235, 446, 235, P::lightGray), label(317, 132, tr("CART WEST", "VOZÍK ZÁPAD"), amber),
    };
    world.addInteraction({e2d::Verb::context, surveyCart.id, std::nullopt,
        {e2d::Condition::notFlag("mine_cart_ready")},
        {inspect(tr("Iris releases the survey cart brake. The old cable rolls it back to the ore chamber and leaves a usable shortcut.",
            "Iris uvolní brzdu průzkumného vozíku. Staré lano ho sveze ke komoře s rudou a vytvoří použitelnou zkratku."))},
        {e2d::Mutation::setFlag("mine_cart_ready"), e2d::Mutation::moveTo(std::string{screen(76).id})}, 30, {}, "climb"});
    world.addInteraction({e2d::Verb::context, surveyCart.id, std::nullopt,
        {e2d::Condition::flag("mine_cart_ready")}, {},
        {e2d::Mutation::moveTo(std::string{screen(76).id})}, 20, {}, "climb"});

    addPortal(world, 84, "survey_door", "DOOR TO SURVEY CHAMBER", "DVEŘE DO PRŮZKUMNÉ KOMORY",
        {0, 137, 62, 123}, 83, {box(7, 153, 47, 107, P::brown), label(9, 172, tr("SURVEY", "PRŮZK."), pale)});
    auto& bottomCage = addPortal(world, 84, "freight_cage", "POWERED FREIGHT CAGE",
        "NAPÁJENÁ NÁKLADNÍ KLEC", {80, 137, 246, 123}, 85, {
            box(91, 148, 218, 112, P::darkGray), line(200, 148, 200, 260, P::lightGray),
            line(91, 148, 309, 260, P::lightGray), line(309, 148, 91, 260, P::lightGray),
            label(122, 161, tr("CAGE TO UPPER", "KLEC NAHORU"), amber),
        }, {e2d::Condition::flag("lift_fuse_installed"), e2d::Condition::flag("lift_powered")});
    bottomCage.visibleWhen = {e2d::Condition::flag("lift_fuse_installed"), e2d::Condition::flag("lift_powered")};
    addPortal(world, 84, "maintenance_crawl", "MAINTENANCE CRAWL TO SUBSTATION",
        "SERVISNÍ PRŮLEZ DO ROZVODNY", {345, 137, 139, 123}, 86, {
            e2d::ArcVisual{{411, 260}, {62, 89}, 3.14159F, 6.28318F, P::darkGray},
            line(356, 240, 466, 240, amber), label(349, 172, tr("SUBSTATION", "ROZVODNA"), amber),
        });
    addPortal(world, 85, "freight_cage", "FREIGHT CAGE DOWN", "NÁKLADNÍ KLEC DOLŮ",
        {61, 137, 190, 123}, 84, {box(77, 148, 206, 112, P::darkGray),
            line(180, 148, 180, 260, P::lightGray), label(110, 161, tr("CAGE DOWN", "KLEC DOLŮ"), amber)});
    addPortal(world, 85, "substation_door", "DOOR TO SUBSTATION", "DVEŘE DO ROZVODNY",
        {407, 137, 77, 123}, 86, {box(414, 151, 63, 109, P::black),
            label(417, 172, tr("SUBST.", "ROZV."), amber)});
    addPortal(world, 86, "bottom_crawl", "CRAWL TO LOWER LIFT", "PRŮLEZ K DOLNÍMU VÝTAHU",
        {0, 137, 58, 123}, 84, {e2d::ArcVisual{{29, 260}, {27, 88}, 3.14159F, 6.28318F, P::brown},
            label(5, 172, tr("CRAWL", "PRŮLEZ"), pale)});
    auto& upperLiftDoor = addPortal(world, 86, "upper_lift_door", "DOOR TO UPPER LIFT", "DVEŘE K HORNÍMU VÝTAHU",
        {78, 137, 62, 123}, 85, {box(85, 151, 48, 109, P::darkGray), label(88, 172, tr("LIFT", "VÝTAH"), amber)},
        {e2d::Condition::flag("lift_powered")});
    upperLiftDoor.visibleWhen = {e2d::Condition::flag("lift_powered")};
    addPortal(world, 86, "switchgear_door", "SWITCHGEAR AISLE", "ULIČKA ROZVADĚČŮ",
        {315, 137, 70, 123}, 87, {box(322, 151, 56, 109, P::red), label(325, 172, tr("SWITCH", "ROZV."), pale)});
    addPortal(world, 86, "vault_door", "CABLE VAULT", "KABELOVÁ KOMORA",
        {407, 137, 77, 123}, 88, {box(414, 151, 63, 109, P::blue), label(421, 172, tr("VAULT", "KABELY"), pale)});
    addPortal(world, 87, "substation_return", "RETURN TO SUBSTATION", "ZPĚT DO ROZVODNY",
        {0, 137, 62, 123}, 86, {box(7, 151, 48, 109, P::brown), label(9, 172, tr("BACK", "ZPĚT"), pale)});
    addPortal(world, 88, "substation_return", "RETURN TO SUBSTATION", "ZPĚT DO ROZVODNY",
        {0, 137, 62, 123}, 86, {box(7, 151, 48, 109, P::brown), label(9, 172, tr("BACK", "ZPĚT"), pale)});
    auto& researchRoute = addPortal(world, 88, "research_door", "PASSAGE TO RESEARCH DOOR",
        "CHODBA K VÝZKUMNÝM DVEŘÍM", {422, 137, 70, 123}, 89, {
            box(429, 151, 56, 109, P::darkGray), label(432, 172, tr("RESEARCH", "VÝZKUM"), amber),
        }, {e2d::Condition::flag("flood_order_heard")});
    researchRoute.visibleWhen = {e2d::Condition::flag("flood_order_heard")};
    addPortal(world, 89, "vault_return", "RETURN TO CABLE VAULT", "ZPĚT DO KABELOVÉ KOMORY",
        {0, 137, 62, 123}, 88, {box(7, 151, 48, 109, P::brown), label(9, 172, tr("VAULT", "KABELY"), pale)});
    auto& ridgeRoute = addPortal(world, 89, "ridge_lift_door", "OPEN DOOR TO RIDGE LIFT",
        "OTEVŘENÉ DVEŘE K HŘEBENOVÉMU VÝTAHU", {80, 137, 286, 123}, 90, {
            box(88, 151, 262, 109, P::black), line(219, 151, 219, 260, P::lightGray),
            label(123, 172, tr("RIDGE LIFT", "HŘEBENOVÝ VÝTAH"), amber),
        }, {e2d::Condition::flag("research_door_open")});
    ridgeRoute.visibleWhen = {e2d::Condition::flag("research_door_open")};
    addPortal(world, 90, "research_return", "RETURN TO RESEARCH DOOR",
        "ZPĚT K VÝZKUMNÝM DVEŘÍM", {0, 137, 62, 123}, 89, {
            box(7, 151, 48, 109, P::brown), label(9, 172, tr("BACK", "ZPĚT"), pale),
        });

    addPickup(world, 76, "respirator", "You take the respirator body from the emergency cabinet.",
        "Vezmeš tělo respirátoru z nouzové skříňky.", 0);
    addUse(world, 77, "timber_brace", "LOOSE TIMBER BRACE", "UVOLNĚNÁ VÝDŘEVA", "wrench", "drift_braced",
        "The marked brace tightens until the warning dust stops.", "Označená vzpěra se dotáhne a varovný prach ustane.");
    auto& timberBrace = ensureHotspot(world, 77, "timber_brace",
        tr("LOOSE TIMBER BRACE", "UVOLNĚNÁ VÝDŘEVA"), {164, 135, 96, 125}, e2d::HotspotKind::mechanism);
    timberBrace.visibleWhen = {e2d::Condition::notFlag("drift_braced")};
    timberBrace.visuals = {
        line(203, 151, 237, 248, danger), line(217, 145, 251, 242, P::brown),
        line(202, 177, 230, 177, amber), line(210, 202, 239, 202, amber),
        label(179, 229, tr("LOOSE BRACE", "VOLNÁ VZPĚRA"), danger),
    };
    auto& braceComplete = ensureHotspot(world, 77, "timber_brace_complete",
        tr("SECURED TIMBER BRACE", "ZAJIŠTĚNÁ VÝDŘEVA"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    braceComplete.visuals = {
        box(210, 90, 16, 170, P::brown), line(218, 90, 268, 140, P::brown),
        circle(218, 174, 5, P::brightGreen), label(177, 229, tr("BRACE SECURE", "VZPĚRA DRŽÍ"), P::brightGreen),
    };
    gateRight(world, 77, {e2d::Condition::flag("drift_braced")},
        "The marked timber brace must be secured before the collapsed drift.",
        "Před zavalenou chodbou je nutné zajistit označenou výdřevu.");
    addHazard(world, 78, "rockfall", "drift_braced",
        "The unsupported drift gives way above Iris.", "Nezajištěná chodba se nad Iris zřítí.");
    addPickup(world, 79, "filter_housing", "You take the empty respirator cartridge from the ventilation locker.",
        "Z větrací skříňky vezmeš prázdnou respirátorovou vložku.", 1,
        {e2d::Condition::has("respirator")});
    addUse(world, 79, "respirator_filter", "RESPIRATOR FILTER", "FILTR RESPIRÁTORU", "charcoal", "respirator_fitted",
        "Clean charcoal packs the filter housing and completes the respirator.",
        "Čisté uhlí naplní pouzdro filtru a dokončí respirátor.",
        {e2d::Condition::has("respirator"), e2d::Condition::has("filter_housing")}, true, 0);
    world.interactions.back().mutations.push_back(e2d::Mutation::removeItem("filter_housing"));
    auto& respiratorFilter = ensureHotspot(world, 79, "respirator_filter",
        tr("RESPIRATOR FILTER", "FILTR RESPIRÁTORU"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    respiratorFilter.visibleWhen = {e2d::Condition::has("respirator"), e2d::Condition::has("filter_housing"),
        e2d::Condition::notFlag("respirator_fitted")};
    respiratorFilter.visuals = {
        box(82, 174, 58, 50, P::lightGray), circle(111, 199, 18, P::black),
        line(93, 199, 129, 199, danger), label(74, 230, tr("PACK FILTER", "NAPLŇ FILTR"), amber),
    };
    auto& respiratorReady = ensureHotspot(world, 79, "respirator_filter_complete",
        tr("FITTED RESPIRATOR", "DOKONČENÝ RESPIRÁTOR"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    respiratorReady.visuals = {
        box(82, 174, 58, 50, P::lightGray), circle(111, 199, 18, P::brightGreen, false),
        line(93, 199, 129, 199, amber), label(84, 230, tr("FILTER OK", "FILTR OK"), P::brightGreen),
    };
    addUse(world, 79, "fan_starter", "VENTILATION FAN STARTER", "STARTÉR VĚTRÁKU", "multimeter", "ventilation_running",
        "The meter finds a dead starter contact; Iris bridges it and the fan accelerates.",
        "Multimetr najde mrtvý kontakt; Iris ho propojí a větrák zrychlí.", {}, false, 2);
    auto& fanStarter = ensureHotspot(world, 79, "fan_starter",
        tr("VENTILATION FAN STARTER", "STARTÉR VĚTRÁKU"), {265, 135, 96, 125}, e2d::HotspotKind::mechanism, 2);
    fanStarter.visibleWhen = {e2d::Condition::notFlag("ventilation_running")};
    fanStarter.visuals = {
        box(284, 169, 58, 59, P::darkGray), box(293, 178, 40, 41, P::black),
        circle(306, 191, 6, danger), line(316, 207, 328, 187, danger),
        label(279, 233, tr("DEAD CONTACT", "MRTVÝ KONTAKT"), danger),
    };
    auto& fanRunning = ensureHotspot(world, 79, "fan_starter_complete",
        tr("RUNNING VENTILATION FAN", "BĚŽÍCÍ VĚTRÁK"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    fanRunning.visuals = {
        box(284, 169, 58, 59, P::darkGray), box(293, 178, 40, 41, P::black),
        circle(306, 191, 6, P::brightGreen), line(316, 207, 328, 187, P::brightGreen),
        label(286, 233, tr("FAN RUNNING", "VĚTRÁK BĚŽÍ"), P::brightGreen),
    };
    addHazard(world, 80, "mine_gas", "respirator_fitted",
        "The mine lamp shrinks to blue and the gas takes Iris's breath.",
        "Plamen důlní lampy zmodrá a plyn vezme Iris dech.");
    addPickup(world, 80, "copper_bus_bar", "Behind the gas haze lies a cut copper bus bar.",
        "Za plynovým oparem leží uříznutá měděná přípojnice.", 0,
        {e2d::Condition::flag("respirator_fitted")});
    addContext(world, 81, "mine_pump", "MINE DRAINAGE PUMP", "DŮLNÍ ODVODŇOVACÍ ČERPADLO", "mine_drained", {
        inspect(tr("Ventilation lets Iris restart the drainage pump. The flooded drift slows to a shallow current.",
            "Větrání umožní Iris spustit odvodnění. Proud v zatopené chodbě zeslábne.")),
    }, {e2d::Condition::flag("ventilation_running")}, {}, 2, "power");
    auto& minePumpControl = ensureHotspot(world, 81, "mine_pump",
        tr("MINE DRAINAGE PUMP", "DŮLNÍ ODVODŇOVACÍ ČERPADLO"), {270, 137, 92, 123}, e2d::HotspotKind::mechanism, 2);
    minePumpControl.visibleWhen = {e2d::Condition::flag("ventilation_running"), e2d::Condition::notFlag("mine_drained")};
    minePumpControl.visuals = {
        box(289, 169, 54, 59, P::darkGray), circle(316, 188, 8, amber),
        box(303, 204, 26, 11, P::red), label(288, 233, tr("START PUMP", "SPUSŤ ČERP."), amber),
    };
    auto& minePumpRunning = ensureHotspot(world, 81, "mine_pump_complete",
        tr("RUNNING MINE PUMP", "BĚŽÍCÍ DŮLNÍ ČERPADLO"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    minePumpRunning.visuals = {
        box(289, 169, 54, 59, P::darkGray), circle(316, 188, 8, P::brightGreen),
        box(303, 204, 26, 11, P::brightGreen), label(290, 233, tr("DRAINING", "ODVODŇUJE"), P::brightGreen),
    };
    gateRight(world, 81, {e2d::Condition::flag("mine_drained")},
        "The flooded drift is impassable until the mine pump drains it.",
        "Zatopená chodba je neprůchodná, dokud ji důlní čerpadlo neodvodní.");
    addUse(world, 82, "submerged_grate", "SUBMERGED GRATE", "PONOŘENÁ MŘÍŽ", "magnet_cord", "lift_fuse_retrieved",
        "The magnet swings once, catches, and brings the lift fuse out of the water.",
        "Magnet se zhoupne, zachytí a vytáhne z vody pojistku výtahu.",
        {e2d::Condition::flag("mine_drained"), e2d::Condition::has("insulated_boots")}, false);
    // The retrieved fuse becomes a carried item without consuming the reusable magnet.
    world.interactions.back().mutations.push_back(e2d::Mutation::addItem("lift_fuse"));
    auto& submergedGrate = ensureHotspot(world, 82, "submerged_grate",
        tr("SUBMERGED GRATE", "PONOŘENÁ MŘÍŽ"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    submergedGrate.visibleWhen = {e2d::Condition::flag("mine_drained"), e2d::Condition::has("insulated_boots"),
        e2d::Condition::notFlag("lift_fuse_retrieved")};
    submergedGrate.visuals = {
        box(73, 188, 76, 42, P::darkGray), line(82, 197, 140, 197, P::lightGray),
        line(82, 208, 140, 208, P::lightGray), line(82, 219, 140, 219, P::lightGray),
        circle(111, 215, 7, danger), label(77, 234, tr("FUSE BELOW", "POJISTKA DOLE"), amber),
    };
    auto& grateCleared = ensureHotspot(world, 82, "submerged_grate_complete",
        tr("CLEARED DRAIN GRATE", "VYČIŠTĚNÁ ODTOKOVÁ MŘÍŽ"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    grateCleared.visuals = {
        box(73, 188, 76, 42, P::darkGray), line(82, 197, 140, 197, P::brightGreen),
        line(82, 208, 140, 208, P::brightGreen), line(82, 219, 140, 219, P::brightGreen),
        label(79, 234, tr("FUSE FOUND", "POJISTKA NALEZENA"), P::brightGreen),
    };
    gateRight(world, 82, {e2d::Condition::flag("lift_fuse_retrieved")},
        "The survey chamber route is under the grate; retrieve the lift fuse before crossing.",
        "Cesta do průzkumné komory vede kolem mříže; před přechodem vytáhni pojistku výtahu.");
    addPickup(world, 83, "mine_map", "You unfold Voss's marked mine map.", "Rozložíš Vossovu označenou důlní mapu.", 0);
    addPickup(world, 83, "research_badge", "Kline's research badge was abandoned in haste.",
        "Klineové výzkumný odznak byl opuštěn ve spěchu.", 1);
    addPickup(world, 83, "punched_card", "The punched card has meaningful holes on both orientations.",
        "Děrný štítek má smysluplné otvory v obou orientacích.", 2);
    addUse(world, 84, "lift_fuse_box", "FREIGHT LIFT FUSE BOX", "POJISTKOVÁ SKŘÍŇ VÝTAHU", "lift_fuse", "lift_fuse_installed",
        "The recovered fuse wakes the cage lamp, but the motor feed remains dark.",
        "Získaná pojistka rozsvítí lampu klece, ale napájení motoru zůstane temné.", {}, true);
    auto& liftFuseBox = ensureHotspot(world, 84, "lift_fuse_box",
        tr("FREIGHT LIFT FUSE BOX", "POJISTKOVÁ SKŘÍŇ VÝTAHU"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    liftFuseBox.visibleWhen = {e2d::Condition::notFlag("lift_fuse_installed")};
    liftFuseBox.visuals = {
        box(70, 166, 54, 63, P::lightGray), box(78, 174, 38, 47, P::black),
        circle(97, 188, 7, danger), box(84, 204, 26, 9, P::red),
        label(70, 234, tr("NO FUSE", "BEZ POJISTKY"), danger),
    };
    auto& fuseInstalled = ensureHotspot(world, 84, "lift_fuse_box_complete",
        tr("FUSED FREIGHT LIFT", "JIŠTĚNÝ NÁKLADNÍ VÝTAH"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    fuseInstalled.visuals = {
        box(70, 166, 54, 63, P::lightGray), box(78, 174, 38, 47, P::black),
        circle(97, 188, 7, amber), box(84, 204, 26, 9, amber),
        label(74, 234, tr("FUSE OK", "POJISTKA OK"), amber),
    };
    addContext(world, 87, "isolation_order", "SWITCHGEAR ISOLATORS", "ODPOJOVAČE ROZVADĚČE", "substation_isolated", {
        inspect(tr("Calder's arrows guide a safe isolation order. The black feed falls quiet.",
            "Calderiny šipky vedou bezpečným pořadím odpojení. Černý přívod ztichne.")),
    }, {e2d::Condition::flag("nightjar_signal_found")}, {}, 1, "power");
    auto& isolationOrder = ensureHotspot(world, 87, "isolation_order",
        tr("SWITCHGEAR ISOLATORS", "ODPOJOVAČE ROZVADĚČE"), {171, 137, 92, 123}, e2d::HotspotKind::mechanism, 1);
    isolationOrder.visibleWhen = {e2d::Condition::flag("nightjar_signal_found"),
        e2d::Condition::notFlag("substation_isolated")};
    isolationOrder.visuals = {
        box(182, 164, 70, 66, P::lightGray), box(191, 173, 52, 48, P::black),
        line(202, 184, 202, 211, danger), line(217, 184, 217, 211, danger), line(232, 184, 232, 211, danger),
        label(188, 234, tr("2 - 1 - 3", "2 - 1 - 3"), amber),
    };
    auto& isolationComplete = ensureHotspot(world, 87, "isolation_order_complete",
        tr("ISOLATED SWITCHGEAR", "ODPOJENÝ ROZVADĚČ"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    isolationComplete.visuals = {
        box(182, 164, 70, 66, P::lightGray), box(191, 173, 52, 48, P::black),
        line(202, 211, 202, 184, P::brightGreen), line(217, 211, 217, 184, P::brightGreen),
        line(232, 211, 232, 184, P::brightGreen), label(188, 234, tr("ISOLATED", "ODPOJENO"), P::brightGreen),
    };
    addUse(world, 88, "quiet_field_feed", "QUIET FIELD FEED", "PŘÍVOD TICHÉHO POLE", "wrench", "quiet_feed_cut",
        "The wrench disconnects the black Quiet Field feed from the mine system.",
        "Klíč odpojí černý přívod Tichého pole od důlního systému.",
        {e2d::Condition::flag("substation_isolated")});
    auto& quietFeed = ensureHotspot(world, 88, "quiet_field_feed",
        tr("QUIET FIELD FEED", "PŘÍVOD TICHÉHO POLE"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    quietFeed.visibleWhen = {e2d::Condition::flag("substation_isolated"), e2d::Condition::notFlag("quiet_feed_cut")};
    quietFeed.visuals = {
        circle(84, 189, 14, P::red, false), circle(138, 189, 14, P::red, false),
        e2d::PolylineVisual{{{84, 203}, {111, 218}, {138, 203}}, danger, false},
        label(73, 230, tr("QUIET LIVE", "TICHO POD PROUDEM"), danger),
    };
    auto& quietCut = ensureHotspot(world, 88, "quiet_field_feed_complete",
        tr("CUT QUIET FIELD FEED", "ODPOJENÝ PŘÍVOD TICHÉHO POLE"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    quietCut.visuals = {
        circle(84, 189, 14, P::darkGray, false), circle(138, 189, 14, P::darkGray, false),
        line(84, 203, 100, 212, P::darkGray), line(122, 212, 138, 203, P::darkGray),
        label(82, 230, tr("FEED CUT", "PŘÍVOD ODPOJEN"), P::brightGreen),
    };
    addUse(world, 88, "lift_bus", "LIFT BUS CIRCUIT", "OBVOD PŘÍPOJNICE VÝTAHU", "copper_bus_bar", "lift_powered",
        "The copper bar completes the lift circuit. The cage motor hums above.",
        "Měděná přípojnice dokončí obvod výtahu. Motor klece nahoře zabzučí.",
        {e2d::Condition::flag("quiet_feed_cut")}, true, 2);
    auto& liftBus = ensureHotspot(world, 88, "lift_bus",
        tr("LIFT BUS CIRCUIT", "OBVOD PŘÍPOJNICE VÝTAHU"), {265, 135, 96, 125}, e2d::HotspotKind::mechanism, 2);
    liftBus.visibleWhen = {e2d::Condition::flag("quiet_feed_cut"), e2d::Condition::notFlag("lift_powered")};
    liftBus.visuals = {
        circle(286, 189, 14, P::darkGray, false), circle(340, 189, 14, P::darkGray, false),
        line(300, 189, 326, 189, P::darkGray), label(275, 230, tr("BUS MISSING", "CHYBÍ PŘÍPOJNICE"), danger),
    };
    auto& liftPowered = ensureHotspot(world, 88, "lift_bus_complete",
        tr("POWERED LIFT BUS", "NAPÁJENÁ PŘÍPOJNICE VÝTAHU"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    liftPowered.visuals = {
        circle(286, 189, 14, P::brightGreen, false), circle(340, 189, 14, P::brightGreen, false),
        box(300, 183, 26, 12, amber), label(283, 230, tr("LIFT POWER", "VÝTAH NAPÁJEN"), P::brightGreen),
    };
    addContext(world, 85, "kade_radio", "VOSS FIELD RADIO", "VOSSOVO POLNÍ RÁDIO", "flood_order_heard", {
        speech(tr("Kade: The east drift is flooded. Kline's badge and card are still below, but nobody can reach them.",
            "Kade: Východní chodba je zatopená. Klineové odznak a karta jsou stále dole, ale nikdo se k nim nedostane.")),
        speech(tr("Voss: Then seal the research passage and report to the observatory. Nightjar fires at midnight.",
            "Voss: Pak uzavři výzkumnou chodbu a hlas se v observatoři. Nightjar spustí o půlnoci.")),
        speech(tr("Iris: Their flood failed. Now I know which door they fear.",
            "Iris: Jejich záplava selhala. Teď vím, kterých dveří se bojí."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("lift_powered")}, {}, 2, "talk");
    auto& kadeRadio = ensureHotspot(world, 85, "kade_radio",
        tr("VOSS FIELD RADIO", "VOSSOVO POLNÍ RÁDIO"), {270, 137, 92, 123}, e2d::HotspotKind::mechanism, 2);
    kadeRadio.visibleWhen = {e2d::Condition::flag("lift_powered"), e2d::Condition::notFlag("flood_order_heard")};
    kadeRadio.visuals = {
        box(295, 168, 64, 56, P::red), box(304, 177, 46, 38, P::black),
        line(310, 190, 344, 190, signalBlue), line(310, 202, 338, 202, danger),
        circle(344, 213, 4, amber), label(292, 229, tr("VOSS CHANNEL", "VOSSŮV KANÁL"), danger),
    };
    auto& radioHeard = ensureHotspot(world, 85, "kade_radio_complete",
        tr("MONITORED VOSS CHANNEL", "ODPOSLECHNUTÝ VOSSŮV KANÁL"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    radioHeard.visuals = {
        box(295, 168, 64, 56, P::red), box(304, 177, 46, 38, P::black),
        line(310, 196, 344, 196, P::brightGreen), circle(344, 213, 4, P::brightGreen),
        label(299, 229, tr("ORDER HEARD", "ROZKAZ SLYŠEN"), P::brightGreen),
    };
    addUse(world, 89, "research_reader", "RESEARCH BADGE READER", "ČTEČKA VÝZKUMNÉHO ODZNAKU",
        "research_badge", "research_badge_presented", "The reader accepts Kline's emergency authority.",
        "Čtečka přijme Klineové nouzové oprávnění.");
    addUse(world, 89, "code_reader", "PUNCHED-CARD READER", "ČTEČKA DĚRNÉHO ŠTÍTKU",
        "punched_card", "research_door_open", "Turned upside down, the card exposes Kline's emergency code and retracts the bolts.",
        "Obrácený štítek odhalí Klineové nouzový kód a zasune závory.",
        {e2d::Condition::flag("research_badge_presented"), e2d::Condition::flag("lift_time_known")}, false, 2);
    auto& badgeReader = ensureHotspot(world, 89, "research_reader",
        tr("RESEARCH BADGE READER", "ČTEČKA VÝZKUMNÉHO ODZNAKU"), {63, 135, 96, 125}, e2d::HotspotKind::mechanism);
    badgeReader.visibleWhen = {e2d::Condition::notFlag("research_badge_presented")};
    badgeReader.visuals = {box(82, 166, 58, 63, P::lightGray), box(91, 175, 40, 45, P::black),
        circle(111, 188, 7, danger), label(81, 234, tr("BADGE FIRST", "NEJPRVE ODZNAK"), danger)};
    auto& codeReader = ensureHotspot(world, 89, "code_reader",
        tr("PUNCHED-CARD READER", "ČTEČKA DĚRNÉHO ŠTÍTKU"), {265, 135, 96, 125}, e2d::HotspotKind::mechanism, 2);
    codeReader.visibleWhen = {e2d::Condition::flag("research_badge_presented"), e2d::Condition::notFlag("research_door_open")};
    codeReader.visuals = {box(284, 166, 58, 63, P::lightGray), box(293, 175, 40, 45, P::black),
        box(300, 185, 26, 17, pale), circle(307, 190, 2, P::black), circle(318, 197, 2, P::black),
        label(279, 234, tr("TURN CARD", "OBRAŤ KARTU"), amber)};
    auto& researchOpen = ensureHotspot(world, 89, "code_reader_complete",
        tr("OPEN RESEARCH DOOR", "OTEVŘENÉ VÝZKUMNÉ DVEŘE"), {0, 0, 0, 0}, e2d::HotspotKind::scenery);
    researchOpen.visuals = {
        box(88, 151, 118, 109, P::black), box(232, 151, 118, 109, P::black),
        line(219, 151, 219, 260, P::brightGreen), circle(313, 188, 7, P::brightGreen),
        label(151, 172, tr("DOOR OPEN", "DVEŘE OTEVŘENY"), P::brightGreen),
    };
    addContext(world, 90, "ridge_lift", "RIDGE FREIGHT LIFT", "HŘEBENOVÝ NÁKLADNÍ VÝTAH", "act3_complete", {
        speech(tr("Voss: Leave my phase coil in the cage and you may walk away before midnight.",
            "Voss: Nech mou fázovou cívku v kleci a můžeš před půlnocí odejít.")),
        speech(tr("Iris: Kestrel Six did not get that choice. Neither do you.",
            "Iris: Kestrel Six tu možnost nedostal. Ty také ne."), e2d::MessageSpeaker::player),
        inspect(tr("The lift climbs through old mine strata toward the observatory and Nightjar.",
            "Výtah stoupá starými důlními vrstvami k observatoři a Nightjaru.")),
    }, {e2d::Condition::flag("lift_fuse_installed"), e2d::Condition::flag("lift_powered"),
        e2d::Condition::flag("research_door_open"), e2d::Condition::has("red_phase_coil")},
        {e2d::Mutation::moveTo(std::string{screen(91).id})}, 2, "climb");
    addFollowUpDialogue(world, 52, "lila", "lila_followup", {
        speech(tr("Lila: The engine made the reservoir run and is still holding pressure. I will relay Mara's calls east.",
            "Lila: Lokomotiva zvládla cestu k nádrži a stále drží tlak. Budu předávat Mařina volání na východ.")),
    }, {e2d::Condition::flag("met_lila"), e2d::Condition::flag("logging_engine_running")});
    addFollowUpDialogue(world, 58, "june", "june_history_heard", {
        speech(tr("June: That silted badge predates the enamel one. Black Pine belonged to working voices before Nightjar borrowed its wires.",
            "June: Ten odznak z nánosu je starší než smaltovaný. Black Pine patřil pracujícím hlasům dřív, než si Nightjar vypůjčil jeho dráty.")),
        speech(tr("Iris: Then we will return every wire to the people who kept it alive.",
            "Iris: Pak vrátíme každý drát lidem, kteří ho udržovali při životě."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("met_june"), e2d::Condition::has("old_relay_badge"),
        e2d::Condition::has("logger_token")});
    addFollowUpDialogue(world, 67, "jonah", "jonah_followup", {
        speech(tr("Jonah: The false flood command is isolated and the east shaft is stable. I will keep it that way.",
            "Jonah: Falešný povel k záplavě je odpojený a východní šachta je stabilní. Udržím ji tak.")),
    }, {e2d::Condition::flag("jonah_briefed"), e2d::Condition::flag("reservoir_complete")});
}

void addActFour(e2d::WorldDefinition& world) {
    addUse(world, 91, "tracking_camera", "TRACKING CAMERA", "SLEDUJÍCÍ KAMERA", "hand_mirror", "camera_blinded",
        "The mirror returns one hard flash. The tracking camera iris closes.",
        "Zrcátko vrátí ostrý záblesk. Clona sledující kamery se zavře.");
    addContext(world, 91, "staff_passage", "BADGED STAFF PASSAGE", "SLUŽEBNÍ CHODBA NA ODZNAK",
        "staff_passage_taken", {inspect(tr("Kline's badge opens the quiet staff corridor behind the patrol courtyard.",
            "Klineové odznak otevře tichou služební chodbu za hlídaným nádvořím."))},
        {e2d::Condition::flag("camera_blinded"), e2d::Condition::has("research_badge")},
        {e2d::Mutation::moveTo(std::string{screen(96).id})}, 3, "unlock");
    addHazard(world, 92, "paired_patrol", "courtyard_patrol_diverted",
        "Kade and Morrow catch Iris in the searchlight.", "Kade a Morrow chytí Iris ve světlometu.");
    addPickup(world, 93, "nightjar_patch", "You save a cloth Nightjar patch from Voss's temporary bunk.",
        "Z Vossova provizorního lůžka zachráníš látkovou nášivku Nightjar.", 0);
    addUse(world, 94, "kitchen_bait", "BACK-DOOR BAIT", "NÁVNADA U ZADNÍCH DVEŘÍ", "sealed_ration", "guard_bait_placed",
        "The sealed ration waits outside the kitchen without revealing Iris's route.",
        "Uzavřená dávka čeká před kuchyní, aniž prozradí Irisinu cestu.", {}, true);
    addContext(world, 94, "kitchen_timer", "MECHANICAL KITCHEN TIMER", "MECHANICKÁ KUCHYŇSKÁ MINUTKA",
        "courtyard_patrol_diverted", {inspect(tr("The timer rings at the back door. One guard follows the sound and ration away from the courtyard.",
            "Minutka zazvoní u zadních dveří. Jeden strážný následuje zvuk a dávku pryč z nádvoří."))},
        {e2d::Condition::flag("guard_bait_placed")}, {}, 2, "warning");
    gateRight(world, 94, {e2d::Condition::flag("courtyard_patrol_diverted")},
        "The paired patrol still covers the infirmary route.", "Dvojice stráží stále kryje cestu k ošetřovně.");
    addPickup(world, 95, "first_aid_kit", "The observatory first-aid kit is complete.",
        "Lékárnička observatoře je úplná.", 0);
    addContext(world, 95, "kline_recording", "KLINE'S RECORDING", "KLINEOVÉ NAHRÁVKA", "calder_warning_known", {
        speech(tr("Kline recording: If you restore the carrier, find Calder's reel. The field follows the protected signal.",
            "Nahrávka Klineové: Pokud obnovíš nosnou vlnu, najdi Calderové kotouč. Pole následuje chráněný signál.")),
        speech(tr("Iris: She expected somebody to survive Voss's demonstration.",
            "Iris: Čekala, že někdo Vossovu demonstraci přežije."), e2d::MessageSpeaker::player),
    });
    addContext(world, 96, "project_portraits", "NIGHTJAR PROJECT DATES", "DATA PROJEKTU NIGHTJAR",
        "archive_dates_known", {inspect(tr("The portraits date Calder's warning before Voss falsified the final safety report.",
            "Portréty datují Calderové varování před Vossovo zfalšování závěrečné bezpečnostní zprávy."))});
    addContext(world, 97, "archive_drawers", "FOUR ARCHIVE DRAWERS", "ČTYŘI ARCHIVNÍ ZÁSUVKY",
        "archive_open", {inspect(tr("The four project dates align the drawers. A cipher lens and Calder archive reel unlock together.",
            "Čtyři data srovnají zásuvky. Šifrovací čočka a Calderové archivní kotouč se odemknou současně."))},
        {e2d::Condition::flag("archive_dates_known")},
        {e2d::Mutation::addItem("cipher_lens"), e2d::Mutation::addItem("archive_reel")}, 1, "unlock");
    addPickup(world, 98, "phase_prism", "You remove the phase prism from Voss's calibration rig.",
        "Vyjmeš fázový hranol z Vossovy kalibrační soupravy.", 0);
    addContext(world, 98, "ventilation_duct", "LAB VENTILATION DUCT", "VĚTRACÍ KANÁL LABORATOŘE",
        "kline_located", {speech(tr("Kline: If someone hears me, the holding room is below the test cell. Do not overload the field.",
            "Klineová: Jestli mě někdo slyší, zadržovací místnost je pod zkušební komorou. Pole nepřetěžuj."))});
    addUse(world, 102, "security_keypad", "MIRRORED SECURITY KEYPAD", "ZRCADLENÁ BEZPEČNOSTNÍ KLÁVESNICE",
        "hand_mirror", "security_office_open", "The mirror reveals the keypad without placing Iris before its camera.",
        "Zrcátko odhalí klávesnici, aniž Iris vstoupí před její kameru.",
        {e2d::Condition::flag("camera_blinded")});
    addPickup(world, 102, "dome_key", "The security drawer releases the instrument-dome key.",
        "Bezpečnostní zásuvka vydá klíč od přístrojové kopule.", 1,
        {e2d::Condition::flag("security_office_open")});
    addUse(world, 99, "dome_lock", "INSTRUMENT DOME LOCK", "ZÁMEK PŘÍSTROJOVÉ KOPULE", "dome_key", "dome_open",
        "The key releases the dome drive and exposes its north marks.",
        "Klíč uvolní pohon kopule a odhalí severní značky.");
    addContext(world, 99, "dome_drive", "DOME DRIVE", "POHON KOPULE", "dome_aligned", {
        inspect(tr("The dome slit rotates onto true north. The archive reader wakes and a calibration fork slides free.",
            "Štěrbina kopule se otočí na pravý sever. Čtečka archivu ožije a uvolní kalibrační ladičku.")),
    }, {e2d::Condition::flag("dome_open"), e2d::Condition::has("compass")},
        {e2d::Mutation::addItem("calibration_fork")}, 2, "power");
    addContext(world, 100, "telescope", "LANDMARK TELESCOPE", "TELESKOP ORIENTAČNÍCH BODŮ",
        "tower_alignment_known", {
            speech(tr("Nell: Quarry crane, split pine, summit beacon. Hold those three and the azimuth is true.",
                "Nell: Lomový jeřáb, rozštípnutá borovice, vrcholový maják. Drž ty tři a azimut sedí.")),
            inspect(tr("The sight confirms the tower north mark. The old fog horn wakes below.",
                "Zaměřovač potvrdí severní značku věže. Dole se probudí stará mlhová siréna.")),
        }, {e2d::Condition::flag("dome_aligned"), e2d::Condition::flag("lookout_briefed")},
        {e2d::Mutation::setFlag("fog_horn_ready"), e2d::Mutation::addItem("alignment_chart")}, 2, "warning");
    addCharacter(world, 101, "sable", "SABLE DUNN", "SABLE DUNNOVÁ", "sable_persuaded", {
        speech(tr("Sable: Voss said the protected carrier would keep aircraft safe.",
            "Sable: Voss tvrdil, že chráněná nosná vlna udrží letadla v bezpečí.")),
        speech(tr("Iris: Theo saw the sabotage before the storm. Kline recorded the warning. Kestrel Six is already down.",
            "Iris: Theo viděl sabotáž před bouří. Klineová nahrála varování. Kestrel Six už spadl."), e2d::MessageSpeaker::player),
        speech(tr("Sable: Then he lied to all of us. I am killing my jammer rack. Kline is in holding below the test cell.",
            "Sable: Pak lhal nám všem. Vypínám svou rušičku. Klineová je zadržena pod zkušební komorou.")),
    }, {e2d::Condition::flag("calder_warning_known"), e2d::Condition::has("survey_notebook"),
        e2d::Condition::flag("tower_alignment_known")}, {e2d::Mutation::setFlag("jammer_disabled")});
    addUse(world, 103, "ante_badge", "BUNKER BADGE LOCK", "ODZNAKOVÝ ZÁMEK BUNKRU", "research_badge", "ante_badge_open",
        "Kline's badge clears the first lock.", "Klineové odznak uvolní první zámek.");
    addUse(world, 103, "ante_phrase", "CALDER PHRASE LOCK", "ZÁMEK CALDEROVÉ FRÁZE", "cipher_lens", "ante_phrase_open",
        "Through the lens, the punched card reads RUTH / OPEN CHANNEL.",
        "Přes čočku děrný štítek čte RUTH / OTEVŘENÝ KANÁL.",
        {e2d::Condition::flag("archive_open")}, false, 1);
    addUse(world, 103, "ante_tone", "CALIBRATION TONE LOCK", "ZÁMEK KALIBRAČNÍHO TÓNU",
        "calibration_fork", "bunker_door_open", "The fork answers the third lock. The heavy door irises apart.",
        "Ladička odpoví třetímu zámku. Těžké dveře se rozevřou.",
        {e2d::Condition::flag("ante_badge_open"), e2d::Condition::flag("ante_phrase_open"),
            e2d::Condition::flag("sable_persuaded")}, false, 2);
    gateRight(world, 103, {e2d::Condition::flag("bunker_door_open")},
        "Nightjar's three locks need Kline's badge, Calder's phrase and the calibration tone.",
        "Tři zámky Nightjaru potřebují Klineové odznak, Calderové frázi a kalibrační tón.");

    addUse(world, 104, "decon_reader", "DECONTAMINATION BADGE READER", "ČTEČKA DEKONTAMINACE",
        "research_badge", "decon_authorized", "The badge authorizes a harmless air cycle.",
        "Odznak povolí neškodný vzduchový cyklus.");
    addContext(world, 104, "decon_cycle", "DECONTAMINATION CYCLE", "DEKONTAMINAČNÍ CYKLUS",
        "decon_complete", {inspect(tr("Fans rise, warning lamps count down, and the inner door opens after the clean-air cycle.",
            "Větráky zrychlí, kontrolky odpočítají čas a po čistém cyklu se otevřou vnitřní dveře."))},
        {e2d::Condition::flag("decon_authorized")}, {}, 2, "power");
    gateRight(world, 104, {e2d::Condition::flag("decon_complete")},
        "The two-door decontamination interlock is still closed.", "Dvoukřídlá dekontaminační propusť je stále zavřená.");
    addUse(world, 105, "guard_intercom", "BUNKER INTERCOM", "INTERKOM BUNKRU", "archive_reel", "bunker_guards_sealed",
        "Calder's recorded voice calls Kade and Morrow into decontamination; Iris seals the outer door.",
        "Calderové hlas zavolá Kadea a Morrowa do dekontaminace; Iris uzavře vnější dveře.");
    addHazard(world, 105, "bunker_patrol", "bunker_guards_sealed",
        "The paired patrol detains Iris in the holding room.", "Dvojice stráží zadrží Iris v zadržovací místnosti.");
    addUse(world, 106, "diagnostic_coil", "DIAGNOSTIC COIL CRADLE", "DIAGNOSTICKÉ LOŽE CÍVKY",
        "red_phase_coil", "diagnostic_coil_ready", "The red coil enters the isolated diagnostic rig, not the live machine.",
        "Červená cívka vstoupí do oddělené diagnostiky, ne do živého stroje.");
    addUse(world, 106, "diagnostic_prism", "DIAGNOSTIC PRISM MOUNT", "DIAGNOSTICKÝ DRŽÁK HRANOLU",
        "phase_prism", "inversion_calculated", "With coil and prism together, the rig plots a stable inversion curve.",
        "S cívkou a hranolem dohromady souprava vykreslí stabilní inverzní křivku.",
        {e2d::Condition::flag("diagnostic_coil_ready")}, false, 2);
    addContext(world, 107, "fork_sequence", "CALIBRATION POSITIONS", "KALIBRAČNÍ POLOHY",
        "protected_sequence_known", {inspect(tr("The cipher colours order the fork positions. The protected-carrier sequence is 4-1-3.",
            "Barvy šifry seřadí polohy ladičky. Sekvence chráněné nosné vlny je 4-1-3."))},
        {e2d::Condition::flag("inversion_calculated"), e2d::Condition::has("calibration_fork"),
            e2d::Condition::has("cipher_lens")}, {}, 2, "power");
    addUse(world, 108, "test_cell_player", "TEST-CELL REEL PLAYER", "PŘEHRÁVAČ ZKUŠEBNÍ KOMORY",
        "archive_reel", "calder_testimony_heard", "Calder's voice explains that the field silenced navigation and medical telemetry in every test.",
        "Calderové hlas vysvětlí, že pole při každém testu umlčelo navigaci i zdravotní telemetrii.",
        {e2d::Condition::flag("protected_sequence_known")});
    addUse(world, 109, "seized_rack", "SEIZED TOOL RACK", "ZADŘENÝ STOJAN NÁŘADÍ", "wrench", "machine_rack_open",
        "The wrench frees the rack holding Nightjar's emergency service parts.",
        "Klíč uvolní stojan s nouzovými servisními díly Nightjaru.");
    addPickup(world, 109, "coolant_hose", "You take the pressure-rated replacement coolant hose.",
        "Vezmeš náhradní tlakovou chladicí hadici.", 1, {e2d::Condition::flag("machine_rack_open")});
    addPickup(world, 109, "grounding_clamp", "The high-current grounding clamp is heavy but essential.",
        "Silnoproudá zemnicí svorka je těžká, ale nezbytná.", 2, {e2d::Condition::flag("machine_rack_open")});
    addPickup(world, 109, "calder_photo", "Behind the rack is Ruth Calder beside the first protected-carrier rig.",
        "Za stojanem je fotografie Ruth Calderové u první soupravy chráněné nosné vlny.", 3,
        {e2d::Condition::flag("machine_rack_open")});
    addUse(world, 110, "capacitor_banks", "CAPACITOR BANKS 4-1-3", "KONDENZÁTOROVÉ BLOKY 4-1-3",
        "grounding_clamp", "capacitors_grounded", "The clamp follows 4-1-3. Three immense arcs collapse safely into ground.",
        "Svorka následuje 4-1-3. Tři mohutné výboje bezpečně zmizí do země.",
        {e2d::Condition::flag("protected_sequence_known")});
    addHazard(world, 110, "capacitor_discharge", "capacitors_grounded",
        "A charged bank discharges through the ungrounded walkway.", "Nabitý blok se vybije přes neuzemněnou lávku.");
    gateRight(world, 110, {e2d::Condition::flag("capacitors_grounded")},
        "The charged banks block the cooling gallery.", "Nabité kondenzátorové bloky blokují chladicí galerii.");
    addUse(world, 111, "split_coolant_line", "SPLIT COOLANT LINE", "PRASKLÉ CHLADICÍ VEDENÍ",
        "coolant_hose", "cooling_diverted", "The new hose seals, then Iris diverts cooling from the Quiet Field into the emergency dump.",
        "Nová hadice těsní a Iris odvede chlazení z Tichého pole do nouzové výpusti.", {}, true);
    addHazard(world, 111, "coolant_steam", "cooling_diverted",
        "A steam lane opens across Iris before the split hose is replaced.",
        "Před výměnou hadice se přes Iris otevře proud páry.");
    addUse(world, 112, "archive_deck", "COMMAND ARCHIVE DECK", "MECHANIKA VELITELSKÉHO ARCHIVU",
        "archive_reel", "command_archive_loaded", "Calder's reel mounts beside Voss's live command log.",
        "Calderové kotouč se připojí vedle Vossova živého velitelského záznamu.");
    addUse(world, 112, "archive_decoder", "ARCHIVE DECODER", "DEKODÉR ARCHIVU", "cipher_lens", "evidence_copied",
        "The lens aligns the records. Voss's admission and the Nightjar archive copy to an evidence spool.",
        "Čočka srovná záznamy. Vossovo přiznání a archiv Nightjaru se zkopírují na důkazní kotouč.",
        {e2d::Condition::flag("command_archive_loaded"), e2d::Condition::flag("cooling_diverted")}, false, 2);
    world.interactions.back().mutations.push_back(e2d::Mutation::addItem("evidence_spool"));
    addCharacter(world, 113, "miriam", "DR. MIRIAM KLINE", "DR. MIRIAM KLINEOVÁ", "kline_freed", {
        speech(tr("Kline: Voss moved the remaining charge to the summit. Do not overload it—make the field follow Calder's carrier.",
            "Klineová: Voss přesunul zbývající náboj na vrchol. Nepřetěžuj ho—přinuť pole následovat Calderové nosnou vlnu.")),
        speech(tr("Iris: Coil, prism, beacon reference, then 4-1-3 through the protected carrier.",
            "Iris: Cívka, hranol, reference majáku a pak 4-1-3 přes chráněnou nosnou vlnu."), e2d::MessageSpeaker::player),
        speech(tr("Kline: Exactly. Take my override key. Let his own transmitter expose him.",
            "Klineová: Přesně. Vezmi můj nouzový klíč. Ať ho odhalí jeho vlastní vysílač.")),
    }, {e2d::Condition::flag("calder_testimony_heard"), e2d::Condition::flag("evidence_copied")},
        {e2d::Mutation::addItem("override_key")});
    addUse(world, 113, "miriam", "DR. MIRIAM KLINE", "DR. MIRIAM KLINEOVÁ", "first_aid_kit", "kline_treated",
        "Iris cleans Kline's cut and secures her wrist. The remaining clean cloth stays in the kit.",
        "Iris Klineové vyčistí ránu a zpevni zápěstí. Zbývající čistá látka zůstane v lékárničce.",
        {e2d::Condition::flag("kline_freed")}, false, 2);
    addUse(world, 114, "dark_stair", "DARK EMERGENCY STAIR", "TEMNÉ NOUZOVÉ SCHODIŠTĚ",
        "hand_crank_torch", "emergency_stair_lit", "The crank torch catches the red stair markers as Voss seals the lower blast door.",
        "Ruční svítilna zachytí červené značky schodů, zatímco Voss zavírá spodní pancéřové dveře.",
        {e2d::Condition::flag("kline_freed")});
    addUse(world, 115, "summit_override", "SUMMIT OVERRIDE LOCK", "NOUZOVÝ ZÁMEK VRCHOLU",
        "override_key", "summit_override_open", "Kline's key gives local control to the protected-carrier circuit.",
        "Klineové klíč předá místní ovládání obvodu chráněné nosné vlny.",
        {e2d::Condition::flag("emergency_stair_lit")});
    addContext(world, 115, "summit_sequence", "SUMMIT ACCESS SEQUENCE", "SEKVENCE PŘÍSTUPU NA VRCHOL",
        "act4_complete", {
            inspect(tr("Iris enters 4-1-3. The lock opens and reports eighteen minutes until Voss's final pulse.",
                "Iris zadá 4-1-3. Zámek se otevře a hlásí osmnáct minut do Vossova posledního pulzu.")),
        }, {e2d::Condition::flag("summit_override_open"), e2d::Condition::flag("protected_sequence_known")}, {}, 2, "unlock");
    gateRight(world, 115, {e2d::Condition::flag("act4_complete")},
        "The summit lock needs Kline's override and protected sequence 4-1-3.",
        "Vrcholový zámek potřebuje Klineové nouzový klíč a chráněnou sekvenci 4-1-3.");
}

void addActFive(e2d::WorldDefinition& world) {
    addContext(world, 116, "broken_ground", "BROKEN GROUNDING CABLE", "PŘERUŠENÉ UZEMNĚNÍ",
        "summit_ground_fault_found", {warning(tr("The copper grounding path is broken above. Every following strike will seek the tower steel.",
            "Měděná zemnicí cesta je nahoře přerušená. Každý další blesk si najde ocel věže."))});
    addContext(world, 117, "windbreak", "STONE WINDBREAK", "KAMENNÝ VĚTRNÝ KRYT", "ledge_crossed", {
        inspect(tr("Iris waits under the marked stone through one rockfall, then follows the fixed handline across.",
            "Iris přečká jeden sesuv pod označeným kamenem a pak přejde po pevném laně.")),
    }, {e2d::Condition::flag("summit_ground_fault_found")}, {}, 1, "warning");
    addHazard(world, 117, "falling_rock", "ledge_crossed",
        "The warning pebbles become a fatal fall of rock.", "Varovné kamínky se změní ve smrtící sesuv.");
    addUse(world, 118, "broken_ground_strap", "BROKEN COPPER GROUND STRAP", "PRASKLÝ MĚDĚNÝ ZEMNICÍ PÁS",
        "grounding_clamp", "summit_ground_clamped", "The clamp bridges the broken copper strap before the next flash.",
        "Svorka překlene prasklý měděný pás před dalším zábleskem.",
        {e2d::Condition::flag("ledge_crossed")});
    addUse(world, 118, "ground_clamp_bolt", "GROUND CLAMP BOLT", "ŠROUB ZEMNICÍ SVORKY", "wrench", "summit_grounded",
        "The wrench tightens the clamp. A major strike now races visibly into the mountain.",
        "Klíč dotáhne svorku. Mohutný blesk nyní viditelně sjede do hory.",
        {e2d::Condition::flag("summit_ground_clamped")}, false, 2);
    addHazard(world, 118, "lightning", "summit_grounded",
        "Lightning finds the ungrounded gallery steel.", "Blesk si najde neuzemněnou ocel galerie.");
    gateRight(world, 118, {e2d::Condition::flag("summit_grounded")},
        "The tower remains lethal until the grounding strap is clamped and tightened.",
        "Věž zůstává smrtelná, dokud není zemnicí pás sepnutý a dotažený.");
    addUse(world, 119, "tower_feed", "TOWER PHASE FEED", "FÁZOVÝ PŘÍVOD VĚŽE", "red_phase_coil", "tower_coil_installed",
        "The recovered red coil enters the summit feed and changes its pulse from red to amber.",
        "Získaná červená cívka vstoupí do vrcholového přívodu a její pulz se změní z červeného na žlutý.", {}, true);
    addCharacter(world, 119, "sable_summit", "SABLE DUNN", "SABLE DUNNOVÁ", "transmitter_key_received", {
        speech(tr("Sable: Voss dropped this transmitter key when he ran. I have the lower guards contained.",
            "Sable: Voss upustil tenhle klíč vysílače, když utíkal. Spodní stráže držím pod kontrolou.")),
        speech(tr("Iris: Get Kline to Mara's channel. I will make one clear opening.",
            "Iris: Dostaň Klineovou na Mařin kanál. Já vytvořím jeden čistý průchod."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("sable_persuaded"), e2d::Condition::flag("tower_coil_installed")},
        {e2d::Mutation::addItem("transmitter_key")});
    // The field case guarantees progress even if a future alternate Sable route is added.
    addContext(world, 119, "voss_field_case", "VOSS'S FIELD CASE", "VOSSOVO POLNÍ POUZDRO",
        "field_case_checked", {inspect(tr("Voss's spare transmitter key lies beneath a soaked demonstration contract.",
            "Vossův náhradní klíč vysílače leží pod promočenou smlouvou o demonstraci."))},
        {e2d::Condition::flag("tower_coil_installed")}, {e2d::Mutation::addItem("transmitter_key")}, 3, "pickup");
    addUse(world, 120, "sheltered_ladder", "SHELTERED LADDER SIDE", "KRYTÁ STRANA ŽEBŘÍKU", "compass", "mid_tower_crossed",
        "The compass identifies the lee side. Iris climbs while Mara and Elias break through in fragments.",
        "Kompas určí závětrnou stranu. Iris stoupá a útržky hlasů Mary a Eliase pronikají rušením.");
    gateRight(world, 120, {e2d::Condition::flag("mid_tower_crossed")},
        "The exposed ladder side is too dangerous in this wind.", "Návětrná strana žebříku je v tomto větru příliš nebezpečná.");
    addUse(world, 121, "waveguide_prism", "WAVEGUIDE PRISM MOUNT", "DRŽÁK HRANOLU VLNOVODU",
        "phase_prism", "tower_prism_installed", "The phase prism locks into the microwave waveguide.",
        "Fázový hranol zapadne do mikrovlnného vlnovodu.", {}, true);
    addUse(world, 121, "waveguide_tuning", "REFLECTED-POWER TUNING", "LADĚNÍ ODRAŽENÉHO VÝKONU",
        "calibration_fork", "waveguide_tuned", "The fork finds the protected carrier and the reflected-power meter settles.",
        "Ladička najde chráněnou nosnou vlnu a měřidlo odraženého výkonu se ustálí.",
        {e2d::Condition::flag("tower_prism_installed"), e2d::Condition::flag("inversion_calculated")}, false, 2);
    gateRight(world, 121, {e2d::Condition::flag("waveguide_tuned")},
        "The waveguide still needs the phase prism and calibration fork.",
        "Vlnovod stále potřebuje fázový hranol a kalibrační ladičku.");
    addContext(world, 122, "beacon_housing", "CRACKED BEACON HOUSING", "PRASKLÝ KRYT MAJÁKU",
        "beacon_crystal_removed", {inspect(tr("Iris opens the cracked housing and lifts out its clouded reference crystal.",
            "Iris otevře prasklý kryt a vyjme zakalený referenční krystal."))},
        {e2d::Condition::flag("waveguide_tuned")}, {e2d::Mutation::addItem("beacon_crystal")}, 0, "unlock");
    addUse(world, 122, "beacon_cleaning", "CLOUDED BEACON CRYSTAL", "ZAKALENÝ KRYSTAL MAJÁKU",
        "first_aid_kit", "beacon_crystal_cleaned", "A clean cloth from the first-aid kit clears smoke residue without scratching the crystal.",
        "Čistá látka z lékárničky odstraní kouřový povlak bez poškrábání krystalu.",
        {e2d::Condition::flag("beacon_crystal_removed"), e2d::Condition::has("beacon_crystal")}, false, 1);
    addUse(world, 122, "beacon_socket", "BEACON REFERENCE SOCKET", "OBJÍMKA REFERENCE MAJÁKU",
        "beacon_crystal", "beacon_reference_ready", "The clean crystal locks home. The red rotation becomes a steady green pulse.",
        "Čistý krystal zapadne na místo. Červené otáčení se změní ve stálý zelený pulz.",
        {e2d::Condition::flag("beacon_crystal_cleaned")}, true, 2);
    gateRight(world, 122, {e2d::Condition::flag("beacon_reference_ready")},
        "The protected carrier needs a clean beacon-crystal reference.",
        "Chráněná nosná vlna potřebuje čistou referenci krystalu majáku.");
    addUse(world, 123, "azimuth_mount", "ANTENNA AZIMUTH MOUNT", "AZIMUTOVÝ DRŽÁK ANTÉNY", "wrench", "antenna_aligned",
        "Using Nell's chart, Iris rotates the mount onto the true north mark.",
        "Podle Nellina plánu Iris otočí držák na značku pravého severu.",
        {e2d::Condition::flag("tower_alignment_known"), e2d::Condition::has("alignment_chart")});
    addUse(world, 123, "local_motor_lock", "LOCAL MOTOR OVERRIDE", "MÍSTNÍ NOUZOVÉ OVLÁDÁNÍ MOTORU",
        "override_key", "antenna_control_locked", "Kline's key locks local control before Voss can reverse the antenna motor.",
        "Klineové klíč uzamkne místní ovládání dřív, než Voss obrátí motor antény.",
        {e2d::Condition::flag("antenna_aligned")}, false, 2);
    gateRight(world, 123, {e2d::Condition::flag("antenna_control_locked")},
        "The antenna must be aligned and its local motor control locked.",
        "Anténa musí být vyrovnaná a její místní ovládání motoru uzamčené.");
    addCharacter(world, 124, "voss", "GIDEON VOSS", "GIDEON VOSS", "voss_confronted", {
        speech(tr("Voss: In seconds the valley will hear perfect silence. One failed helicopter will be forgotten.",
            "Voss: Za pár sekund uslyší údolí dokonalé ticho. Na jeden spadlý vrtulník se zapomene.")),
        speech(tr("Iris: Silence is not control. It is every call you chose not to hear.",
            "Iris: Ticho není kontrola. Je to každé volání, které ses rozhodl neslyšet."), e2d::MessageSpeaker::player),
        speech(tr("Voss: You cannot tune my field with maintenance scraps.",
            "Voss: Moje pole nenaladíš servisními zbytky.")),
        speech(tr("Iris: They are the mountain's working memory. And Calder left the channel open.",
            "Iris: Jsou pracovní pamětí téhle hory. A Calderová nechala kanál otevřený."), e2d::MessageSpeaker::player),
    });
    addUse(world, 124, "transmitter_lock", "SUMMIT TRANSMITTER LOCK", "ZÁMEK VRCHOLOVÉHO VYSÍLAČE",
        "transmitter_key", "transmitter_unlocked", "Voss's key gives Iris the protected-carrier controls.",
        "Vossův klíč předá Iris ovládání chráněné nosné vlny.",
        {e2d::Condition::flag("voss_confronted")});
    addUse(world, 124, "evidence_loader", "EVIDENCE REEL BAY", "MECHANIKA DŮKAZNÍHO KOTOUČE",
        "evidence_spool", "evidence_loaded", "The complete Nightjar archive and Voss's admission wait beneath the rescue call.",
        "Úplný archiv Nightjaru a Vossovo přiznání čekají pod záchranným voláním.",
        {e2d::Condition::flag("transmitter_unlocked")}, false, 2);

    auto& finalConsole = ensureHotspot(world, 124, "protected_carrier_console",
        tr("PROTECTED-CARRIER CONSOLE", "PANEL CHRÁNĚNÉ NOSNÉ VLNY"),
        {276, 122, 152, 138}, e2d::HotspotKind::mechanism, 3);
    const std::vector<e2d::Condition> finalConditions{
        e2d::Condition::flag("transmitter_unlocked"), e2d::Condition::flag("tower_coil_installed"),
        e2d::Condition::flag("waveguide_tuned"), e2d::Condition::flag("beacon_reference_ready"),
        e2d::Condition::flag("antenna_control_locked"), e2d::Condition::flag("protected_sequence_known"),
    };
    auto keeperConditions = finalConditions;
    keeperConditions.insert(keeperConditions.end(), {
        e2d::Condition::flag("evidence_loaded"), e2d::Condition::flag("theo_followup"),
        e2d::Condition::flag("nell_followup"), e2d::Condition::flag("owen_followup"),
        e2d::Condition::flag("lila_followup"), e2d::Condition::flag("june_history_heard"),
        e2d::Condition::flag("jonah_followup"), e2d::Condition::flag("sable_persuaded"),
        e2d::Condition::flag("kline_treated"), e2d::Condition::has("pine_bird"),
        e2d::Condition::has("relay_badge"), e2d::Condition::has("ranger_patch"),
        e2d::Condition::has("old_relay_badge"), e2d::Condition::has("quartz_sample"),
        e2d::Condition::has("logger_token"), e2d::Condition::has("nightjar_patch"),
        e2d::Condition::has("calder_photo"),
    });
    world.addInteraction({e2d::Verb::context, finalConsole.id, std::nullopt, keeperConditions, {}, {
        e2d::Mutation::win(tr(
            "KEEPER OF BLACK PINE. Iris keys 4-1-3 and opens Calder's carrier with the complete Nightjar record beneath it. Kestrel Six answers. Every rescued voice joins Mara's channel, Ruth Calder's name returns to the ridge, and the community chooses Iris to keep Black Pine open for everyone.",
            "STRÁŽKYNĚ BLACK PINE. Iris zadá 4-1-3 a otevře Calderové nosnou vlnu s úplným záznamem Nightjaru. Kestrel Six odpoví. Každý zachráněný hlas se připojí k Mařinu kanálu, jméno Ruth Calderové se vrátí na hřeben a komunita zvolí Iris strážkyní Black Pine pro všechny."))}, 60, {}});
    auto evidenceConditions = finalConditions;
    evidenceConditions.push_back(e2d::Condition::flag("evidence_loaded"));
    world.addInteraction({e2d::Verb::context, finalConsole.id, std::nullopt, evidenceConditions, {}, {
        e2d::Mutation::win(tr(
            "OPEN CHANNEL. Iris keys 4-1-3. The Quiet Field folds into Calder's carrier; Voss's confession rides beneath the rescue call. Kestrel Six answers, the beacon turns green, and voices return across Black Pine one by one.",
            "OTEVŘENÝ KANÁL. Iris zadá 4-1-3. Tiché pole se složí do Calderové nosné vlny; Vossovo přiznání letí pod záchranným voláním. Kestrel Six odpoví, maják zezelená a hlasy se jeden po druhém vracejí přes Black Pine."))}, 50, {}});
    auto rescueConditions = finalConditions;
    rescueConditions.push_back(e2d::Condition::notFlag("evidence_loaded"));
    world.addInteraction({e2d::Verb::context, finalConsole.id, std::nullopt, rescueConditions, {}, {
        e2d::Mutation::win(tr(
            "CARRIER RESTORED. Iris keys 4-1-3 and the Quiet Field collapses into a clean rescue carrier. Kestrel Six answers and the Black Pine beacon turns green. Voss is stopped; some proof remains hidden in Nightjar.",
            "NOSNÁ VLNA OBNOVENA. Iris zadá 4-1-3 a Tiché pole se zhroutí do čisté záchranné nosné vlny. Kestrel Six odpoví a maják Black Pine zezelená. Voss je zastaven; část důkazů zůstává skrytá v Nightjaru."))}, 40, {}});
}

void addHints(e2d::WorldDefinition& world) {
    int priority = 1000;
    const auto next = [&world, &priority](const std::string_view flag, const char* en, const char* cs,
        std::vector<e2d::Condition> prerequisites = {}) {
        addHint(world, flag, en, cs, priority--, std::move(prerequisites));
    };
    next("mission_started", "Answer the pulsing emergency phone at Storm Gate Trailhead with ENTER.",
        "U Výchoziště u bouřkové brány zvedni pulzující nouzový telefon klávesou ENTER.");
    next("taken_patch_cable", "At Storm Gate Trailhead, TAKE the patch cable from the damaged toolbox.",
        "U Výchoziště u bouřkové brány SEBER propojovací kabel z poškozené skříňky.");
    next("cabin_entered", "At Lower Switchback use the yellow CABIN sign, then press ENTER at the highlighted cabin door.",
        "U Dolní serpentiny použij žlutou šipku CHATA a potom stiskni ENTER u zvýrazněných dveří chaty.");
    next("met_mara", "Inside the caretaker cabin, walk to Mara and speak with ENTER.",
        "Uvnitř správcovské chaty dojdi k Maře a promluv klávesou ENTER.");
    next("key_revealed", "EXAMINE Mara's desk after speaking with her.", "Po rozhovoru s Marou PROZKOUMEJ její stůl.");
    next("taken_brass_key", "TAKE the brass yard key revealed on Mara's desk.", "SEBER mosazný klíč odkrytý na Mařině stole.");
    next("taken_site_map", "Use the RADIO door in Mara's cabin and TAKE her annotated site map from the console.",
        "Použij dveře RÁDIO v Mařině chatě a SEBER její popsanou mapu z pultu.");
    next("taken_ceramic_fuse", "Use the CELLAR hatch in Mara's cabin and TAKE the ceramic fuse beside the Nightjar crate.",
        "Použij poklop SKLEP v Mařině chatě a SEBER keramickou pojistku vedle bedny Nightjar.");
    next("taken_hand_crank_torch", "In the Root Cellar, TAKE the blue hand-crank torch.",
        "Ve sklepě SEBER modrou ruční svítilnu.");
    next("taken_wrench", "Leave by the front door, follow the SHED sign and TAKE the 17 mm wrench.",
        "Vyjdi hlavními dveřmi, sleduj šipku KŮLNA a SEBER montážní klíč 17 mm.");
    next("taken_lineman_gloves", "In the Tool Shed, TAKE the insulated lineman gloves.",
        "V kůlně SEBER izolované elektrikářské rukavice.");
    next("taken_pruning_saw", "In the Tool Shed, TAKE the folding pruning saw, then follow the MAST sign.",
        "V kůlně SEBER skládací prořezávací pilu a potom sleduj šipku STOŽÁR.");
    next("vehicle_gate_open", "Follow the service road east and USE the brass key on the Vehicle Gate.",
        "Pokračuj po servisní cestě na východ a u Vjezdové brány POUŽIJ mosazný klíč.");
    next("cable_patched", "At Relay Yard West enter the labelled TRENCH and USE the patch cable on the blue terminals.",
        "V Západní části areálu vstup do označeného VÝKOPU a POUŽIJ kabel na modré svorky.");
    next("fuse_installed", "At Relay Yard East follow GENERATOR and USE the ceramic fuse on the MAIN holder.",
        "Ve Východní části areálu sleduj GENERÁTOR a POUŽIJ keramickou pojistku na HLAVNÍ držák.");
    next("battery_linked", "From the Generator Shed enter BATTERY and USE the wrench on the loose bus link.",
        "Z Generátorovny vstup do BATERIE a POUŽIJ klíč na uvolněnou spojnici.");
    next("fuel_valve_open", "Return to Relay Yard East, follow PUMP and USE the wrench on the fuel valve.",
        "Vrať se do Východní části areálu, sleduj ČERPADLO a POUŽIJ klíč na palivový ventil.");
    next("taken_siphon_hose", "At the Fuel Pump, TAKE the coiled siphon hose before leaving.",
        "U Palivového čerpadla před odchodem SEBER stočenou přečerpávací hadici.");
    next("feeder_isolated", "At Relay Yard East follow TRANSFORMER and USE the lineman gloves on the feeder isolator.",
        "Ve Východní části areálu sleduj TRANSFORMÁTOR a POUŽIJ elektrikářské rukavice na odpojovač.");
    next("workshop_open", "From the Generator Shed enter WORKSHOP and USE the brass key on Calder's cabinet.",
        "Z Generátorovny vstup do DÍLNY a POUŽIJ mosazný klíč na Calderové skříň.");
    next("nightjar_signal_found", "At Relay Yard West enter HALL and USE the multimeter on the Nightjar trunk.",
        "V Západní části areálu vstup do SÁLU a POUŽIJ multimetr na trase Nightjar.");
    next("power_on", "Return through YARD and GENERATOR, then operate the repaired MAIN lever with ENTER.",
        "Vrať se přes AREÁL a GENERÁTOR a spusť opravenou HLAVNÍ páku klávesou ENTER.");
    next("mast_calibrated", "Return to the Weather Mast Clearing and USE the multimeter on its test leads.",
        "Vrať se na Mýtinu s meteostožárem a POUŽIJ multimetr na testovací vývody.");
    next("act1_complete", "In the Local Control Room, operate the direction console after tracing Nightjar.",
        "V Místním velíně spusť směrový panel po proměření trasy Nightjar.");
    next("forest_route_entered", "Return to Old Service Road Fork and press ENTER at the FOREST barrier.",
        "Vrať se na Rozcestí staré servisní cesty a stiskni ENTER u závory LES.");
    next("survey_ribbon_recorded", "On North Service Road, EXAMINE the red survey ribbon tied to the broken branch.",
        "Na Severní servisní cestě PROZKOUMEJ červenou průzkumnickou stuhu na zlomené větvi.");
    next("bandage_cache_found", "At Burned Pine Stand, EXAMINE the ash-covered ranger boot.",
        "U Spáleného borového porostu PROZKOUMEJ popelem pokrytou botu strážce.");
    next("taken_bandage_roll", "TAKE the sealed bandage revealed in the ranger boot.",
        "SEBER uzavřený obvaz odkrytý v botě strážce.");
    next("fir_cut", "Continue east and USE the pruning saw on the Fallen Fir.",
        "Pokračuj na východ a na Padlou jedli POUŽIJ prořezávací pilu.");
    next("taken_signal_flare", "At Cold Creek follow BLIND, then TAKE the signal flare from the emergency box.",
        "U Studeného potoka sleduj POSED a pak SEBER signální světlici z nouzové skříňky.");
    next("theo_freed", "Follow THEO to Mossy Hollow and USE the pruning saw on the branch pinning him.",
        "Sleduj šipku THEO do Mechové prohlubně a na větev, která ho svírá, POUŽIJ pilu.");
    next("theo_rescued", "In Mossy Hollow, USE the bandage roll on Theo's wound.",
        "V Mechové prohlubni POUŽIJ obvaz na Theovo zranění.");
    next("theo_briefed", "Speak to the bandaged Theo with ENTER, then follow CACHE.",
        "Promluv s ošetřeným Theem klávesou ENTER a potom sleduj šipku SKRÝŠ.");
    next("taken_climbing_rope", "In Ranger Cache, TAKE Theo's climbing rope.",
        "Ve Skrýši strážců SEBER Theovo horolezecké lano.");
    next("taken_iron_hook", "In Ranger Cache, TAKE the iron service hook.",
        "Ve Skrýši strážců SEBER železný servisní hák.");
    next("taken_mine_lamp", "In Ranger Cache, TAKE the rugged mine lamp.",
        "Ve Skrýši strážců SEBER odolnou důlní lampu.");
    next("taken_compass", "In Ranger Cache, TAKE Theo's compass.",
        "Ve Skrýši strážců SEBER Theův kompas.");
    next("taken_charcoal", "From Burned Pine Stand follow KILN LOOP and TAKE clean charcoal at the ruin.",
        "Od Spáleného porostu sleduj OKRUH K MILÍŘI a v ruině SEBER čisté dřevěné uhlí.");
    next("echo_route_solved", "Return to Cold Creek, follow ECHO GROVE and USE Theo's compass on the 017 marker.",
        "Vrať se ke Studenému potoku, sleduj HÁJ OZVĚN a na značce 017 POUŽIJ Theův kompas.");
    next("quarry_trace_found", "Follow the green 017 path and USE the multimeter on the buried cable posts.",
        "Sleduj zelenou cestu 017 a na sloupcích zakopaného kabelu POUŽIJ multimetr.");
    next("bear_gone", "At Bear Meadow, USE the signal flare from the marked UPWIND edge; approaching the bear only makes Iris retreat.",
        "Na Medvědí louce POUŽIJ světlici u označené NÁVĚTRNÉ hrany; při přiblížení k medvědovi Iris pouze ustoupí.");
    next("weather_data_read", "At Firebreak Junction follow WEATHER and USE the hand-crank torch on the recorder.",
        "Na Rozcestí proseku sleduj METEO a na záznamníku POUŽIJ ruční svítilnu.");
    next("lookout_briefed", "Return to Firebreak Junction, follow LOOKOUT and speak to Nell.",
        "Vrať se na Rozcestí proseku, sleduj HLÁSKU a promluv s Nell.");
    next("hook_fixed", "At Firebreak Junction follow RAVINE, then USE the iron hook on the anchor eye.",
        "Na Rozcestí proseku sleduj ROKLI a potom na kotevním oku POUŽIJ železný hák.");
    next("ravine_rope_fixed", "At the ravine lip, USE the climbing rope on the fixed hook.",
        "Na hraně rokle POUŽIJ horolezecké lano na upevněný hák.");
    next("ravine_descended", "Press ENTER at the labelled DESCEND rope to reach Ravine Floor West.",
        "Stiskni ENTER u lana označeného SESTOUPIT a dostaň se na Západní dno rokle.");
    next("culvert_lit", "Continue east and USE the hand-crank torch on the dark Culvert markers.",
        "Pokračuj na východ a na tmavých značkách v Propustku POUŽIJ ruční svítilnu.");
    next("sluice_closed", "At Waterfall Shelf, USE the wrench on the small sluice.",
        "Na Římse za vodopádem POUŽIJ montážní klíč na malé stavidlo.");
    next("taken_quarry_office_key", "After closing the sluice, TAKE the quarry office key from the quiet grate.",
        "Po zavření stavidla SEBER klíč od kanceláře lomu z klidné mříže.");
    next("quarry_gate_open", "At Ravine Floor East follow QUARRY and USE its office key on the gate.",
        "Na Východním dně rokle sleduj LOM a na bráně POUŽIJ klíč od kanceláře.");
    next("owen_freed", "Enter OFFICE through the open gate and speak to Owen with ENTER.",
        "Vstup otevřenou bránou do KANCELÁŘE a promluv s Owenem klávesou ENTER.");
    next("horn_sounded", "Follow CRUSHER and pull Owen's crusher horn with ENTER.",
        "Sleduj DRTIČ a klávesou ENTER zatáhni za houkačku podle Owenova plánu.");
    next("brant_secured", "After the horn sounds, USE the brass key on Brant's inspection cage.",
        "Po zaznění houkačky POUŽIJ mosazný klíč na Brantovu kontrolní klec.");
    next("taken_red_phase_coil", "Enter MAGAZINE and TAKE the pulsing red phase coil.",
        "Vstup do SKLADU a SEBER pulzující červenou fázovou cívku.");
    next("taken_survey_notebook", "In the Equipment Magazine, TAKE Voss's survey notebook.",
        "Ve Skladu vybavení SEBER Vossův průzkumnický zápisník.");
    next("quarry_tunnel_lit", "Return to CRUSHER, follow TUNNEL and USE Theo's mine lamp on its dark markers.",
        "Vrať se k DRTIČI, sleduj TUNEL a na tmavých značkách POUŽIJ Theovu důlní lampu.");
    next("hoist_signal_fixed", "In Quarry Tunnel, USE the multimeter on the broken hoist signal.",
        "V Lomovém tunelu POUŽIJ multimetr na přerušenou signalizaci navijáku.");
    next("pulley_repaired", "Continue east and USE Owen's pulley pin on the East Hoist pulley.",
        "Pokračuj na východ a na kladce Východního navijáku POUŽIJ Owenův čep.");
    next("act2_complete", "At East Hoist Landing, operate the repaired hoist controls with ENTER.",
        "Ve Východní stanici navijáku spusť opravené ovládání klávesou ENTER.");
    next("met_lila", "Continue east to Sawmill Yard and speak to Lila with ENTER.",
        "Pokračuj na východ na Dvůr pily a promluv s Lilou klávesou ENTER.");
    next("belt_released", "In Sawmill Yard enter MILL and USE the wrench on the planer's tensioner.",
        "Na Dvoře pily vstup do PILY a na napínáku hoblovky POUŽIJ montážní klíč.");
    next("taken_drive_belt", "On Sawmill Floor, TAKE the now-loose drive belt beside the planer.",
        "V Provozu pily SEBER nyní uvolněný hnací řemen vedle hoblovky.");
    next("taken_oil_can", "On Sawmill Floor enter FILES and TAKE the oil can.",
        "V Provozu pily vstup do BRUSÍRNY a SEBER olejničku.");
    next("taken_hand_mirror", "In Saw Filing Room, TAKE the hand mirror beside the filing bench.",
        "V Brusírně pil SEBER ruční zrcátko vedle brusného stolu.");
    next("fuel_can_filled", "Return to MILL, enter FUEL and USE the siphon hose on the protected reserve tank.",
        "Vrať se do PILY, vstup do KOTELNY a na chráněné nádrži POUŽIJ přečerpávací hadici.");
    next("spark_retrieved", "Return to MILL, enter POND and press ENTER at the log pike; stay away from the moving logs.",
        "Vrať se do PILY, vstup k RYBNÍKU a stiskni ENTER u háku na klády; nepřibližuj se k pohyblivým kládám.");
    next("met_june", "Return through MILL and YARD, enter MESS and speak to June with ENTER.",
        "Vrať se přes PILU a DVŮR, vstup do JÍDELNY a promluv s June klávesou ENTER.");
    next("lift_time_known", "From Mess Hall enter OFFICE and USE the hand mirror on the reversed carbon page.",
        "Z Jídelny vstup do KANCELÁŘE a na obráceném kopíráku POUŽIJ ruční zrcátko.");
    next("taken_rail_switch_key", "Return to YARD, enter BUNK and TAKE the rail switch key revealed in the foreman's boot.",
        "Vrať se na DVŮR, vstup do UBYTOVNY a SEBER klíč od výhybky odkrytý v předákově botě.");
    next("rail_points_aligned", "Return to YARD, follow RAIL and USE the switch key on the west points.",
        "Vrať se na DVŮR, sleduj KOLEJ a na západní výhybce POUŽIJ klíč.");
    next("engine_belt_installed", "At Rail Spur West follow ENGINE and USE the drive belt on the engine drive.",
        "Na Západní vlečce sleduj LOKOMOTIVU a na pohonu POUŽIJ hnací řemen.");
    next("engine_plug_installed", "On the Logging Engine, USE the spark plug on its ignition.",
        "U Lesní lokomotivy POUŽIJ zapalovací svíčku na zapalování.");
    next("engine_oiled", "On the Logging Engine, USE the oil can on the marked bearings.",
        "U Lesní lokomotivy POUŽIJ olejničku na označená ložiska.");
    next("engine_fueled", "On the Logging Engine, USE the filled fuel can on its fuel tank.",
        "U Lesní lokomotivy POUŽIJ plný kanystr na palivovou nádrž.");
    next("logging_engine_running", "When all four repair stations are green, press ENTER at the engine START control.",
        "Až budou všechna čtyři místa opravy zelená, stiskni ENTER u ovládání START lokomotivy.");
    next("trestle_guard_diverted", "Return to SPUR, follow TRESTLE and pull the mill-whistle cable with ENTER.",
        "Vrať se na VLEČKU, sleduj VIADUKT a klávesou ENTER zatáhni za lanko píšťaly.");
    next("trestle_brake_fixed", "At Trestle Approach, USE the wrench on the brake linkage marked MISSING PIN.",
        "U Příjezdu k viaduktu POUŽIJ montážní klíč na táhlo brzdy označené CHYBÍ ČEP.");
    next("elias_contacted", "Cross east after the whistle and brake are safe, then answer the portable radio with ENTER.",
        "Po zajištění píšťaly a brzdy přejdi na východ a zvedni přenosné rádio klávesou ENTER.");
    next("taken_insulated_boots", "Continue east to West Abutment and TAKE the insulated boots from the RESCUE locker.",
        "Pokračuj na východ k Západní opěře a SEBER izolační boty ze skříňky ZÁCHRANA.");
    next("taken_turbine_badge", "At West Abutment, TAKE Jonah's turbine badge beside the rescue locker.",
        "U Západní opěry SEBER Jonahův turbínový odznak vedle záchranné skříňky.");
    next("spray_shield_fixed", "Continue east and USE the wrench on the LOOSE SHIELD before crossing Spillway Walk.",
        "Pokračuj na východ a před přechodem přelivu POUŽIJ klíč na VOLNOU CLONU.");
    next("gatehouse_open", "Cross the now-safe spillway and USE Jonah's turbine badge on the Gatehouse reader.",
        "Přejdi nyní bezpečný přeliv a na čtečce Domku stavidel POUŽIJ Jonahův turbínový odznak.");
    next("taken_spillway_crank", "Inside the open Gatehouse vestibule, TAKE the emergency spillway crank.",
        "V otevřeném vestibulu Domku stavidel SEBER nouzovou kliku přelivu.");
    next("spillway_closed", "In the Gatehouse, USE the emergency crank on the large CRANK SOCKET.",
        "V Domku stavidel POUŽIJ nouzovou kliku na velkou OBJÍMKU KLIKY.");
    next("jonah_briefed", "After the gate closes, speak to Jonah in the Gatehouse with ENTER.",
        "Po uzavření stavidla promluv v Domku stavidel s Jonahem klávesou ENTER.");
    next("dam_diagram_read", "Return west to the abutment, follow TURBINES and read the AUX POWER diagram with ENTER.",
        "Vrať se na západ k opěře, sleduj TURBÍNY a klávesou ENTER přečti schéma POMOCNÉHO PROUDU.");
    next("taken_pump_gasket", "In Turbine Hall Upper take the LOWER stairs and TAKE the pump gasket.",
        "V Horní turbínové hale sejdi po schodech DOLŮ a SEBER těsnění čerpadla.");
    next("bay_isolated", "In Turbine Hall Lower, operate the three BAY BREAKERS with ENTER in the diagram order.",
        "V Dolní turbínové hale ovládej klávesou ENTER tři JISTIČE PROSTORU v pořadí ze schématu.");
    next("taken_dry_cell", "From Turbine Hall Lower enter PUMP and TAKE the dry-cell battery.",
        "Z Dolní turbínové haly vstup do ČERPACÍ GALERIE a SEBER suchý článek.");
    next("pump_gasket_installed", "In Pump Gallery, USE the gasket on the pump flange marked NO GASKET.",
        "V Čerpací galerii POUŽIJ těsnění na přírubě označené BEZ TĚSNĚNÍ.");
    next("pump_battery_installed", "In Pump Gallery, USE the dry cell on the starter marked NO CELL.",
        "V Čerpací galerii POUŽIJ suchý článek na startéru označeném BEZ ČLÁNKU.");
    next("intake_tunnel_lit", "From Pump Gallery enter INTAKE and USE the hand-crank torch on the dark markings.",
        "Z Čerpací galerie vstup do PŘÍVODU a na tmavých značkách POUŽIJ ruční svítilnu.");
    next("taken_valve_wheel", "In the lit Intake Tunnel, TAKE the removable valve wheel revealed by Kline's warning.",
        "V osvětleném Přívodním tunelu SEBER odnímatelné ventilové kolo odkryté Klineové varováním.");
    next("pump_intake_open", "Continue east through Reservoir Shore and USE the valve wheel on PUMP INTAKE.",
        "Pokračuj na východ přes Břeh nádrže a na PŘÍVODU ČERPADLA POUŽIJ ventilové kolo.");
    next("pump_running", "Return through the intake tunnel to PUMP and press ENTER at the green START control.",
        "Vrať se přívodním tunelem k ČERPADLU a stiskni ENTER u zeleného ovládání START.");
    next("taken_magnet_cord", "In Pump Gallery enter BAY and TAKE the magnet from the now-shallow locker water.",
        "V Čerpací galerii vstup do PROSTORU a SEBER magnet z nyní mělké vody u skříňky.");
    next("mine_access_open", "Follow the SHAFT route from the drained bay and ask Jonah to raise the grille with ENTER.",
        "Sleduj cestu ŠACHTA z odčerpaného prostoru a klávesou ENTER požádej Jonaha o zvednutí mříže.");
    next("taken_respirator", "At East Access Shaft take the MINE ladder down and TAKE the respirator from the cabinet.",
        "Ve Východní šachtě slez po žebříku DŮL a SEBER respirátor ze skříňky.");
    next("drift_braced", "Walk east to Timber Gallery and USE the wrench on the red-marked LOOSE BRACE.",
        "Jdi na východ do Výdřevové chodby a POUŽIJ klíč na červeně označenou VOLNOU VZPĚRU.");
    next("taken_filter_housing", "At Timber Gallery enter the blue VENT spur and TAKE the empty filter housing.",
        "Ve Výdřevové chodbě vstup do modré odbočky VĚTRÁNÍ a SEBER prázdné pouzdro filtru.");
    next("respirator_fitted", "In Ventilation Room USE charcoal on PACK FILTER to complete the respirator.",
        "Ve Větrací strojně POUŽIJ uhlí na NAPLŇ FILTR a dokonči respirátor.");
    next("ventilation_running", "In Ventilation Room USE the multimeter on the DEAD CONTACT fan starter.",
        "Ve Větrací strojně POUŽIJ multimetr na MRTVÝ KONTAKT startéru větráku.");
    next("taken_copper_bus_bar", "Return through GALLERY, walk east through the braced collapse and TAKE the copper bus in the GAS ZONE.",
        "Vrať se přes CHODBU, jdi na východ zajištěným závalem a SEBER měděnou přípojnici v PLYNU.");
    next("mine_drained", "Continue east to Mine Pump Station and press ENTER at START PUMP.",
        "Pokračuj na východ k Důlnímu čerpadlu a stiskni ENTER u SPUSŤ ČERP.");
    next("lift_fuse_retrieved", "Walk east into the drained drift and USE the magnet cord on FUSE BELOW.",
        "Jdi na východ do odvodněné chodby a POUŽIJ magnet na laně u POJISTKA DOLE.");
    next("taken_mine_map", "Continue east to Survey Chamber and TAKE Voss's mine map from the survey board.",
        "Pokračuj na východ do Průzkumné komory a SEBER Vossovu důlní mapu z tabule.");
    next("taken_research_badge", "In Survey Chamber TAKE Kline's research badge.",
        "V Průzkumné komoře SEBER Klineové výzkumný odznak.");
    next("taken_punched_card", "In Survey Chamber TAKE the punched card.",
        "V Průzkumné komoře SEBER děrný štítek.");
    next("lift_fuse_installed", "Walk east to Freight Lift Bottom and USE the recovered fuse on NO FUSE.",
        "Jdi na východ k Dolní stanici výtahu a POUŽIJ nalezenou pojistku na BEZ POJISTKY.");
    next("substation_isolated", "At the lower lift enter SUBSTATION through MAINT CRAWL, then enter SWITCH and press ENTER at 2-1-3.",
        "U dolního výtahu vstup SERVISNÍM PRŮLEZEM do ROZVODNY, pak do ROZV. a stiskni ENTER u 2-1-3.");
    next("quiet_feed_cut", "Return to SUBSTATION, enter VAULT and USE the wrench on QUIET LIVE.",
        "Vrať se do ROZVODNY, vstup do KABELŮ a POUŽIJ klíč na TICHO POD PROUDEM.");
    next("lift_powered", "In Cable Vault USE the copper bus bar on BUS MISSING.",
        "V Kabelové komoře POUŽIJ měděnou přípojnici na CHYBÍ PŘÍPOJNICE.");
    next("flood_order_heard", "Return through SUBSTATION and CRAWL, ride the powered CAGE TO UPPER, then press ENTER at VOSS CHANNEL.",
        "Vrať se přes ROZVODNU a PRŮLEZ, jeď napájenou KLECÍ NAHORU a stiskni ENTER u VOSSŮV KANÁL.");
    next("research_badge_presented", "Enter SUBSTATION, then VAULT and RESEARCH; USE Kline's badge on BADGE FIRST.",
        "Vstup do ROZVODNY, pak do KABELŮ a VÝZKUMU; POUŽIJ Klineové odznak na NEJPRVE ODZNAK.");
    next("research_door_open", "At the research door USE the punched card on TURN CARD.",
        "U výzkumných dveří POUŽIJ děrný štítek na OBRAŤ KARTU.");
    next("act3_complete", "Enter the open RIDGE LIFT door and press ENTER at ASCEND.",
        "Vstup otevřenými dveřmi HŘEBENOVÉHO VÝTAHU a stiskni ENTER u VZHŮRU.");
    next("courtyard_patrol_diverted", "Place June's ration and set the timer in the Observatory Kitchen.",
        "Polož Juninu dávku a nastav minutku v Kuchyni observatoře.");
    next("archive_open", "Read the project dates in Archive Hall, then align the four drawers in Records Room.",
        "Přečti data projektu v Archivní hale a srovnej čtyři zásuvky ve Spisovně.");
    next("dome_aligned", "Get the dome key from Security Office, then unlock and align Instrument Dome north.",
        "Získej klíč v Bezpečnostní kanceláři, potom odemkni a srovnej Přístrojovou kopuli na sever.");
    next("sable_persuaded", "Use the Telescope Platform, then confront Sable in Communications Lab with your evidence.",
        "Použij Teleskopovou plošinu a pak konfrontuj Sable v Komunikační laboratoři se získanými důkazy.");
    next("bunker_door_open", "In Nightjar Antechamber, use Kline's badge, cipher lens and calibration fork on the three locks.",
        "V Předsíni Nightjaru použij Klineové odznak, šifrovací čočku a kalibrační ladičku na tři zámky.");
    next("protected_sequence_known", "Diagnose coil and prism in Phase Laboratory, then operate the fork positions in Calibration Chamber.",
        "Diagnostikuj cívku a hranol ve Fázové laboratoři a pak ovládej polohy ladičky v Kalibrační komoře.");
    next("evidence_copied", "Ground Capacitor Hall, divert Cooling Gallery, then copy the Command Archive.",
        "Uzemni Kondenzátorovou halu, odveď chlazení v Chladicí galerii a zkopíruj Velitelský archiv.");
    next("act4_complete", "Free Kline, light Emergency Stair, and enter 4-1-3 at Summit Access Lock.",
        "Osvoboď Klineovou, rozsviť Nouzové schodiště a zadej 4-1-3 do Zámku přístupu na vrchol.");
    next("summit_grounded", "At Lightning Gallery, USE the grounding clamp, then the wrench, on the broken copper strap.",
        "V Bleskové galerii POUŽIJ zemnicí svorku a potom klíč na prasklý měděný pás.");
    next("waveguide_tuned", "Install the coil at Black Pine Tower Base; install the prism and tune it on Microwave Deck.",
        "Namontuj cívku u Paty věže Black Pine; namontuj hranol a nalaď ho na Mikrovlnné plošině.");
    next("antenna_control_locked", "Remove the Beacon Ring crystal, clean it with the first-aid kit, reinstall it, then align and lock the antenna.",
        "Vyjmi krystal na Prstenci majáku, očisti ho lékárničkou, vrať ho a potom srovnej a uzamkni anténu.");
    next("transmitter_unlocked", "Confront Voss in Summit Control Capsule and USE his key on the transmitter lock.",
        "Konfrontuj Vosse ve Vrcholové řídicí kabině a POUŽIJ jeho klíč na zámek vysílače.");
    next("evidence_loaded", "Optionally USE the evidence spool on the reel bay, then press ENTER at the carrier console.",
        "Volitelně POUŽIJ důkazní kotouč v mechanice a pak stiskni ENTER u panelu nosné vlny.");
}

} // namespace

e2d::RendererTheme buildTheme() {
    e2d::RendererTheme theme;
    theme.frame = amber;
    theme.panel = P::black;
    theme.panelPattern = P::red;
    theme.text = pale;
    theme.dimText = P::lightGray;
    theme.accent = signalBlue;
    theme.selected = P::brightMagenta;
    theme.danger = danger;
    theme.playerSkin = amber;
    theme.playerShirt = P::brightRed;
    theme.playerPants = signalBlue;
    return theme;
}

e2d::WorldDefinition buildWorld() {
    e2d::WorldDefinition world;
    addPresentation(world);
    addItems(world);
    addScreens(world);
    addActOne(world);
    addActTwo(world);
    addActThree(world);
    addActFour(world);
    addActFive(world);
    addHints(world);
    return world;
}

} // namespace black_pine
