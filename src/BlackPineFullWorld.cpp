#include "BlackPineWorld.hpp"

#include "BlackPineContent.hpp"

#include "explore2d/Renderer.hpp"

#include <algorithm>
#include <array>
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

    const std::string code = "SCREEN " + screenPrefix(spec.number).substr(1, 3);
    result.decorations.push_back(box(8, 35, 144, 17, P::black));
    result.decorations.push_back(label(15, 40, tr(code, "OBRAZOVKA " + code.substr(7)), amber));

    const float motionX = 455.0F - static_cast<float>(spec.number % 37);
    result.animations.push_back({targetId(spec.number, "scene_motion"), true, true, {}, {
        {6 + spec.number % 5, {circle(motionX, 67 + spec.number % 29, 3, danger)}},
        {6 + (spec.number + 2) % 5, {circle(motionX, 67 + spec.number % 29, 3, signalBlue)}},
    }});
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
    ItemSpec{"pump_gasket", "PUMP GASKET", "TĚSNĚNÍ ČERPADLA", "A fresh emergency-pump gasket.", "Nové těsnění nouzového čerpadla."},
    ItemSpec{"dry_cell", "DRY-CELL BATTERY", "SUCHÝ ČLÁNEK", "A charged pump starter cell.", "Nabitý článek startéru čerpadla."},
    ItemSpec{"magnet_cord", "MAGNET ON CORD", "MAGNET NA ŠŇŮŘE", "A retrieval magnet on strong cord.", "Vytahovací magnet na pevné šňůře."},
    ItemSpec{"valve_wheel", "VALVE WHEEL", "VENTILOVÉ KOLO", "A detachable bypass wheel.", "Odnímatelné kolo obtoku."},
    ItemSpec{"respirator", "RESPIRATOR", "RESPIRÁTOR", "A mask body awaiting filter charcoal.", "Tělo masky čekající na filtrační uhlí."},
    ItemSpec{"copper_bus_bar", "COPPER BUS BAR", "MĚDĚNÁ PŘÍPOJNICE", "A heavy cut copper link.", "Těžká měděná spojnice."},
    ItemSpec{"lift_fuse", "LIFT FUSE", "POJISTKA VÝTAHU", "A sealed freight-lift fuse.", "Utěsněná pojistka nákladního výtahu."},
    ItemSpec{"mine_map", "MINE MAP", "DŮLNÍ MAPA", "A marked underground route map.", "Označená mapa podzemních tras.", false},
    ItemSpec{"research_badge", "KLINE'S RESEARCH BADGE", "KLINEOVÉ VÝZKUMNÝ ODZNAK", "Emergency Nightjar access.", "Nouzový přístup do Nightjaru."},
    ItemSpec{"punched_card", "PUNCHED CODE CARD", "DĚRNÝ KÓDOVÝ ŠTÍTEK", "A reversible emergency code card.", "Oboustranný nouzový kódový štítek."},
    ItemSpec{"nightjar_patch", "NIGHTJAR PATCH", "NÁŠIVKA NIGHTJAR", "An optional cloth insignia.", "Volitelná látková nášivka.", false},
    ItemSpec{"first_aid_kit", "FIRST-AID KIT", "LÉKÁRNIČKA", "A complete observatory kit.", "Úplná lékárnička observatoře."},
    ItemSpec{"cipher_lens", "CIPHER LENS", "ŠIFROVACÍ ČOČKA", "A coloured Nightjar decoding lens.", "Barevná dekódovací čočka Nightjaru."},
    ItemSpec{"archive_reel", "ARCHIVE REEL", "ARCHIVNÍ KOTOUČ", "Calder's magnetic project archive.", "Calderové magnetický projektový archiv."},
    ItemSpec{"phase_prism", "PHASE PRISM", "FÁZOVÝ HRANOL", "The second stolen field component.", "Druhá ukradená součást pole."},
    ItemSpec{"calibration_fork", "CALIBRATION FORK", "KALIBRAČNÍ LADIČKA", "A precisely tuned Nightjar fork.", "Přesně naladěná ladička Nightjaru."},
    ItemSpec{"dome_key", "INSTRUMENT DOME KEY", "KLÍČ OD PŘÍSTROJOVÉ KOPULE", "The security office dome key.", "Klíč od kopule z bezpečnostní kanceláře."},
    ItemSpec{"coolant_hose", "COOLANT HOSE", "CHLADICÍ HADICE", "A pressure-rated replacement hose.", "Náhradní tlaková hadice."},
    ItemSpec{"grounding_clamp", "GROUNDING CLAMP", "ZEMNICÍ SVORKA", "A heavy high-current clamp.", "Těžká silnoproudá svorka."},
    ItemSpec{"evidence_spool", "EVIDENCE SPOOL", "DŮKAZNÍ KOTOUČ", "A copy of Nightjar records and Voss's admission.", "Kopie záznamů Nightjaru a Vossova přiznání."},
    ItemSpec{"override_key", "EMERGENCY OVERRIDE KEY", "NOUZOVÝ OVLÁDACÍ KLÍČ", "Kline's local-control key.", "Klineové klíč místního ovládání."},
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
        result.hotspots.push_back({sceneTarget, tr("SCENE CLUES", "STOPY VE SCÉNĚ"),
            {327, 84, 157, 166}, e2d::HotspotKind::scenery, {}, {
                box(436 - static_cast<float>(spec.number % 51), 205, 18, 10, signalBlue, false)}});

        if (index > 0) {
            result.exits.push_back({e2d::Direction::left, std::string{content::screens[index - 1].id}, {462, 232}, {}, {}});
        }
        if (index + 1 < content::screens.size()) {
            result.exits.push_back({e2d::Direction::right, std::string{content::screens[index + 1].id}, {8, 232}, {}, {}});
        }
        world.addRoom(std::move(result));

        world.addInteraction({e2d::Verb::examine, sceneTarget, std::nullopt, {},
            {inspect(tr(std::string{spec.englishStory}, std::string{spec.czechStory}))}, {}, 0, {}});
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

void addHazard(
    e2d::WorldDefinition& world,
    const int screenNumber,
    const std::string_view name,
    const std::string_view safeFlag,
    std::string englishDeath,
    std::string czechDeath)
{
    room(world, screenNumber).hazards.push_back({targetId(screenNumber, name), {314, 112, 92, 68},
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
    addPickup(world, 1, "patch_cable", "You coil the weatherproof cable from the damaged toolbox.",
        "Z poškozené skříňky smotáš kabel odolný proti počasí.", 0);
    addPickup(world, 1, "field_note", "The folded note gives the generator repair order.",
        "Složená poznámka uvádí pořadí opravy generátoru.", 1);
    addPickup(world, 2, "pine_bird", "You save a tiny carved bird from the wet noticeboard.",
        "Z mokré nástěnky zachráníš malého vyřezávaného ptáčka.", 0);

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
    addPickup(world, 8, "site_map", "Mara's annotations name the relay, quarry, dam and lookout.",
        "Mařiny poznámky označují převaděč, lom, přehradu a hlásku.", 0,
        {e2d::Condition::flag("met_mara")});
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

    addUse(world, 14, "vehicle_gate", "VEHICLE GATE", "VJEZDOVÁ BRÁNA", "brass_key", "vehicle_gate_open",
        "The old master key drops the gate chain. You keep it for the workshop.",
        "Starý hlavní klíč uvolní řetěz brány. Necháš si ho pro dílnu.");
    gateRight(world, 14, {e2d::Condition::flag("vehicle_gate_open")},
        "The locked vehicle gate blocks the relay yard.", "Zamčená vjezdová brána blokuje areál převaděče.");
    addUse(world, 17, "blue_terminals", "BLUE TERMINALS", "MODRÉ SVORKY", "patch_cable", "cable_patched",
        "Iris kneels and bridges the deliberately empty blue terminals.",
        "Iris se skloní a propojí úmyslně prázdné modré svorky.", {}, true);
    addUse(world, 18, "main_fuse_holder", "MAIN FUSE HOLDER", "DRŽÁK HLAVNÍ POJISTKY", "ceramic_fuse", "fuse_installed",
        "The ceramic fuse locks into the MAIN holder with a clean click.",
        "Keramická pojistka čistě zaklapne do HLAVNÍHO držáku.", {}, true);
    addUse(world, 19, "battery_bus", "BATTERY BUS LINK", "SPOJNICE AKUMULÁTORŮ", "wrench", "battery_linked",
        "The wrench secures the loose battery bus without crossing the acid stain.",
        "Klíčem upevníš spojení akumulátorů, aniž vstoupíš do kyseliny.");
    addUse(world, 20, "fuel_valve", "FUEL SUPPLY VALVE", "PŘÍVODNÍ VENTIL PALIVA", "wrench", "fuel_valve_open",
        "The seized valve turns and fuel rises in the sight glass.",
        "Zadřený ventil se otočí a palivo stoupne v průhledítku.");
    addPickup(world, 20, "siphon_hose", "You take the empty fuel-safe siphon hose.",
        "Vezmeš prázdnou hadici vhodnou pro přečerpávání paliva.", 2);
    addUse(world, 21, "fallen_feeder", "FALLEN FEEDER ISOLATOR", "ODPOJOVAČ SPADLÉHO PŘÍVODU",
        "lineman_gloves", "feeder_isolated", "Insulated hands open the feeder switch. The blue arcs die.",
        "Izolované ruce otevřou odpojovač. Modré výboje zhasnou.");
    addHazard(world, 4, "live_feeder", "feeder_isolated",
        "The fallen feeder arcs through Iris before she can pull away.",
        "Spadlý přívod zasáhne Iris dřív, než stačí ucuknout.");

    auto& cabinet = ensureHotspot(world, 22, "locked_cabinet", tr("CALDER'S CABINET", "CALDEROVÉ SKŘÍŇ"),
        {91, 132, 116, 128}, e2d::HotspotKind::mechanism, 0);
    world.addInteraction({e2d::Verb::use, cabinet.id, std::string{"brass_key"},
        {e2d::Condition::notFlag("workshop_open")},
        {inspect(tr("The key opens Calder's cabinet. Her multimeter and enamel badge are still inside.",
            "Klíč otevře Calderové skříň. Uvnitř zůstal multimetr a smaltovaný odznak."))},
        {e2d::Mutation::setFlag("workshop_open"), e2d::Mutation::addItem("multimeter"),
            e2d::Mutation::addItem("relay_badge")}, 30, {}, "unlock"});
    addUse(world, 23, "nightjar_trunk", "NIGHTJAR TRUNK", "TRASA NIGHTJAR", "multimeter", "nightjar_signal_found",
        "The meter proves that a timed signal lives on a physically disconnected trunk.",
        "Měřidlo dokáže, že na fyzicky odpojené trase žije časovaný signál.");
    addContext(world, 18, "main_lever", "MAIN LEVER", "HLAVNÍ PÁKA", "power_on", {
        inspect(tr("The generator coughs twice, catches, and drives power through every repaired link.",
            "Generátor dvakrát zakašle, chytne se a žene proud všemi opravenými spoji.")),
    }, {e2d::Condition::flag("fuse_installed"), e2d::Condition::flag("cable_patched"),
        e2d::Condition::flag("battery_linked"), e2d::Condition::flag("fuel_valve_open"),
        e2d::Condition::flag("feeder_isolated")}, {}, 2, "power");
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
    gateRight(world, 24, {e2d::Condition::flag("act1_complete")},
        "The control-room trace is incomplete. F1 lists the missing repair.",
        "Trasování ve velínu není hotové. F1 ukáže chybějící opravu.");
}

void addActTwo(e2d::WorldDefinition& world) {
    addPickup(world, 26, "bandage_roll", "A sealed ranger bandage waits inside the hidden boot cache.",
        "V ukryté schránce čeká uzavřený obvaz strážců.", 0);
    addUse(world, 27, "fallen_fir", "FALLEN FIR", "PADLÁ JEDLE", "pruning_saw", "fir_cut",
        "The saw frees a short section that rolls into a permanent step.",
        "Pila uvolní krátký díl, který se skutálí do podoby trvalého schodu.");
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
    gateRight(world, 30, {e2d::Condition::flag("theo_briefed")},
        "Theo still needs to be freed, bandaged and questioned.",
        "Thea je ještě nutné osvobodit, ošetřit a vyslechnout.");
    addPickup(world, 31, "climbing_rope", "Theo's climbing rope is dry inside the cache.",
        "Theovo horolezecké lano je ve skladu suché.", 0, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "iron_hook", "You take the iron service-anchor hook.",
        "Vezmeš železný hák pro servisní kotvy.", 1, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "mine_lamp", "A rugged mine lamp joins the pack.",
        "Do batohu přibude odolná důlní lampa.", 2, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 31, "compass", "Theo's liquid compass settles without a tremor.",
        "Theův kapalinový kompas se ustálí bez zachvění.", 3, {e2d::Condition::flag("theo_briefed")});
    addPickup(world, 32, "charcoal", "You bag clean hardwood charcoal for a future filter.",
        "Nabereš čisté dřevěné uhlí pro budoucí filtr.", 0);
    addUse(world, 33, "bearing_route", "ECHO GROVE BEARING", "NÁMĚR V HÁJI OZVĚN", "compass", "echo_route_solved",
        "With the mast bearing, Iris chooses north-east, north, then east.",
        "Podle náměru ze stožáru Iris zvolí severovýchod, sever a východ.",
        {e2d::Condition::flag("mast_calibrated")});
    gateRight(world, 33, {e2d::Condition::flag("echo_route_solved")},
        "The identical pines loop back. Compass bearing 017 is needed.",
        "Stejné borovice vedou zpět. Je potřeba kompas a náměr 017.");
    addUse(world, 34, "cable_posts", "BURIED CABLE POSTS", "SLOUPKY ZAKOPANÉHO KABELU", "multimeter", "quarry_trace_found",
        "Three rising readings point away from the tower and into the quarry.",
        "Tři rostoucí hodnoty míří od věže do lomu.");
    addUse(world, 35, "bear_wind", "UPWIND EDGE", "NÁVĚTRNÁ HRANA", "signal_flare", "bear_gone",
        "The flare burns from upwind. The bear sniffs, turns and leaves unharmed.",
        "Světlice hoří proti větru. Medvěd zavětří, otočí se a bez úhony odejde.", {}, true);
    addHazard(world, 35, "bear", "bear_gone",
        "Iris ignores the bear's warning once too often.", "Iris příliš dlouho ignoruje medvědovo varování.");
    gateRight(world, 35, {e2d::Condition::flag("bear_gone")},
        "The bear blocks the path. Observe the wind and use the flare.",
        "Cestu blokuje medvěd. Sleduj vítr a použij světlici.");
    addCharacter(world, 38, "nell", "NELL HARKER", "NELL HARKEROVÁ", "lookout_briefed", {
        speech(tr("Nell: Three false surveyors, one quarry hoist, and a red light moving underground.",
            "Nell: Tři falešní průzkumníci, jeden lomový naviják a červené světlo v podzemí.")),
        speech(tr("Nell: Kestrel Six is flashing beyond the ridge. Weak, but alive. I marked the ravine route.",
            "Nell: Kestrel Six bliká za hřebenem. Slabě, ale žije. Označila jsem cestu roklí.")),
        speech(tr("Iris: Keep watching the beacon. I will open the old infrastructure.",
            "Iris: Sleduj maják. Já otevřu starou infrastrukturu."), e2d::MessageSpeaker::player),
    });
    gateRight(world, 38, {e2d::Condition::flag("lookout_briefed")},
        "Nell must mark the safe ravine approach.", "Nell musí označit bezpečný přístup roklí.");
    addUse(world, 39, "anchor_eye", "ANCHOR EYE", "KOTEVNÍ OKO", "iron_hook", "hook_fixed",
        "The iron hook seats behind the service anchor with a solid knock.",
        "Železný hák pevně zapadne za servisní kotvu.", {}, true);
    addUse(world, 39, "fixed_hook", "FIXED IRON HOOK", "UPEVNĚNÝ ŽELEZNÝ HÁK", "climbing_rope", "ravine_rope_fixed",
        "Iris ties, tests and fixes the climbing rope down the ravine wall.",
        "Iris lano uváže, vyzkouší a upevní dolů po stěně rokle.",
        {e2d::Condition::flag("hook_fixed")}, true, 1);
    gateRight(world, 39, {e2d::Condition::flag("ravine_rope_fixed")},
        "The ravine descent needs the iron hook and climbing rope.",
        "Sestup do rokle potřebuje železný hák a horolezecké lano.");
    addHazard(world, 40, "flooded_span", "hoist_running",
        "The broken span drops Iris into the flooded ravine.", "Zřícený most shodí Iris do rozvodněné rokle.");
    addUse(world, 43, "sluice", "SMALL SLUICE", "MALÉ STAVIDLO", "wrench", "sluice_closed",
        "The wrench closes the sluice and weakens the waterfall current.",
        "Klíč zavře stavidlo a oslabí proud vodopádu.");
    addPickup(world, 43, "quarry_office_key", "The quarry office key comes free from the quiet grate.",
        "Klíč od kanceláře lomu se uvolní z klidné mříže.", 2,
        {e2d::Condition::flag("sluice_closed")});
    addUse(world, 45, "quarry_gate", "QUARRY GATE", "BRÁNA LOMU", "quarry_office_key", "quarry_gate_open",
        "The rusted key opens the quarry. Voss's field radio lights at once.",
        "Rezavý klíč otevře lom. Vossovo polní rádio se okamžitě rozsvítí.");
    gateRight(world, 45, {e2d::Condition::flag("quarry_gate_open")},
        "The quarry gate needs the key hidden at the waterfall sluice.",
        "Brána lomu potřebuje klíč ukrytý u stavidla za vodopádem.");
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
    gateRight(world, 47, {e2d::Condition::flag("brant_secured")},
        "Brant and the active crusher block the magazine.", "Brant a aktivní drtič blokují sklad.");
    addPickup(world, 48, "red_phase_coil", "You lift Nightjar's pulsing red phase coil into its padded case.",
        "Uložíš pulzující červenou fázovou cívku Nightjaru do pouzdra.", 0);
    addPickup(world, 48, "survey_notebook", "Voss's notebook links the false survey crew to Nightjar.",
        "Vossův zápisník spojuje falešné průzkumníky s Nightjarem.", 1);
    addPickup(world, 48, "quartz_sample", "A blue quartz shard catches the mine lamp.",
        "Modrý úlomek křemene zachytí světlo důlní lampy.", 2);
    addUse(world, 49, "hoist_signal", "BROKEN HOIST SIGNAL", "PŘERUŠENÁ SIGNALIZACE NAVIJÁKU", "multimeter", "hoist_signal_fixed",
        "The meter identifies the crossed pair; Iris restores a steady green signal.",
        "Multimetr najde zkřížený pár a Iris obnoví stálý zelený signál.");
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
}

void addActThree(e2d::WorldDefinition& world) {
    addCharacter(world, 52, "lila", "LILA MERCER", "LILA MERCEROVÁ", "met_lila", {
        speech(tr("Lila: The ridge road is gone. This logging engine is our only heavy transport.",
            "Lila: Cesta na hřeben je pryč. Tahle lokomotiva je naše jediná těžká doprava.")),
        speech(tr("Lila: Bring me a belt, spark plug, oil and fuel. Align the points and I will set the timing.",
            "Lila: Přines řemen, svíčku, olej a palivo. Srovnej výhybku a já nastavím časování.")),
        speech(tr("Iris: I will make the machine whole. You make it run.",
            "Iris: Já stroj doplním. Ty ho rozběhneš."), e2d::MessageSpeaker::player),
    });
    addUse(world, 53, "planer_tension", "PLANER BELT TENSIONER", "NAPÍNÁK ŘEMENU HOBLOVKY",
        "wrench", "belt_released", "The tensioner backs off and leaves the drive belt loose and safe.",
        "Napínák povolí a řemen zůstane volný a bezpečný.");
    addPickup(world, 53, "drive_belt", "You roll the released drive belt without cracking it.",
        "Uvolněný hnací řemen smotáš bez poškození.", 2, {e2d::Condition::flag("belt_released")});
    addPickup(world, 54, "oil_can", "The oil can is nearly full of heavy machine oil.",
        "Olejnička je téměř plná hustého strojního oleje.", 0);
    addPickup(world, 54, "hand_mirror", "A polished hand mirror survives in the filing-room cabinet.",
        "V brusírně přežilo vyleštěné ruční zrcátko.", 1);
    addUse(world, 55, "reserve_tank", "PROTECTED RESERVE TANK", "CHRÁNĚNÁ REZERVNÍ NÁDRŽ",
        "siphon_hose", "engine_fueled", "The hose fills the engine can from the protected reserve. Iris keeps the hose.",
        "Hadice naplní kanystr z chráněné zásoby. Iris si hadici ponechá.");
    addContext(world, 56, "log_pike", "LOG PIKE", "HÁK NA KLÁDY", "spark_retrieved", {
        inspect(tr("The pike draws the floating maintenance box close enough to recover its dry spark plug.",
            "Hák přitáhne plovoucí servisní skříňku a její suchou zapalovací svíčku.")),
    }, {}, {e2d::Mutation::addItem("spark_plug")}, 1, "pickup");
    addHazard(world, 56, "log_pond", "spark_retrieved",
        "A turning log rolls Iris beneath the cold pond.", "Otáčející se kláda stáhne Iris pod studenou hladinu.");
    addPickup(world, 57, "rail_switch_key", "June's clue leads to the switch key in the foreman's boot.",
        "Junina nápověda vede ke klíči od výhybky v předákově botě.", 0);
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
    addUse(world, 59, "carbon_impression", "REVERSED CARBON IMPRESSION", "OBRÁCENÝ OTISK NA KOPÍRÁKU",
        "hand_mirror", "lift_time_known", "In the mirror, the faint pressure marks read RIDGE LIFT / 23:40.",
        "V zrcadle slabé stopy tlaku čtou HŘEBENOVÝ VÝTAH / 23:40.");
    addUse(world, 60, "rail_points", "RAIL POINTS", "VÝHYBKA", "rail_switch_key", "rail_points_aligned",
        "The switch key locks the points onto the east reservoir line.",
        "Klíč uzamkne výhybku na východní trať k přehradě.");
    addUse(world, 61, "engine_belt", "ENGINE DRIVE", "POHON LOKOMOTIVY", "drive_belt", "engine_belt_installed",
        "The planer belt settles around the engine pulleys.", "Řemen z hoblovky se usadí na řemenicích lokomotivy.", {}, true, 0);
    addUse(world, 61, "engine_ignition", "ENGINE IGNITION", "ZAPALOVÁNÍ LOKOMOTIVY", "spark_plug", "engine_plug_installed",
        "The dry spark plug seats in the cleaned cylinder head.", "Suchá svíčka zapadne do vyčištěné hlavy válce.", {}, true, 1);
    addUse(world, 61, "engine_bearings", "ENGINE BEARINGS", "LOŽISKA LOKOMOTIVY", "oil_can", "engine_oiled",
        "Heavy oil reaches every marked bearing cup.", "Hustý olej dorazí do každé označené maznice.", {}, true, 2);
    addContext(world, 61, "engine_start", "ENGINE STARTER", "STARTÉR LOKOMOTIVY", "logging_engine_running", {
        speech(tr("Lila: Timing set. Give her the crank.", "Lila: Časování je hotové. Zatoč klikou.")),
        inspect(tr("The old engine fires, shakes loose thirty years of dust, and settles into a hard idle.",
            "Stará lokomotiva naskočí, setřese třicet let prachu a ustálí se v tvrdém volnoběhu.")),
    }, {e2d::Condition::flag("met_lila"), e2d::Condition::flag("engine_belt_installed"),
        e2d::Condition::flag("engine_plug_installed"), e2d::Condition::flag("engine_oiled"),
        e2d::Condition::flag("engine_fueled"), e2d::Condition::flag("rail_points_aligned")}, {}, 3, "power");
    gateRight(world, 61, {e2d::Condition::flag("logging_engine_running")},
        "Lila still needs every engine part, fuel and the aligned rail points.",
        "Lila stále potřebuje všechny části lokomotivy, palivo a srovnanou výhybku.");
    addContext(world, 62, "mill_whistle", "MILL WHISTLE CABLE", "LANKO PÍŠŤALY PILY", "trestle_guard_diverted", {
        inspect(tr("The old whistle rolls across the valley. The guard leaves the trestle to investigate.",
            "Stará píšťala se rozlehne údolím. Strážný opustí viadukt a jde pátrat.")),
    }, {e2d::Condition::flag("whistle_known")}, {}, 0, "warning");
    addUse(world, 62, "brake_linkage", "BRAKE LINKAGE", "TÁHLO BRZDY", "wrench", "trestle_brake_fixed",
        "The wrench replaces the linkage pin and restores the engine brake.",
        "Klíč nahradí čep táhla a obnoví brzdu lokomotivy.", {}, false, 2);
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

    addPickup(world, 65, "insulated_boots", "The rescue locker holds dry insulated boots.",
        "Záchranná skříňka obsahuje suché izolační boty.", 0);
    addPickup(world, 65, "turbine_badge", "Jonah's turbine badge lies on the safe side of the rail.",
        "Jonahův odznak turbíny leží na bezpečné straně zábradlí.", 1);
    addUse(world, 66, "spray_shield", "LOOSE SPRAY SHIELD", "UVOLNĚNÁ VODNÍ CLONA", "wrench", "spray_shield_fixed",
        "The shield locks into its safe timing position.", "Clona se zajistí v bezpečné časované poloze.");
    addHazard(world, 66, "spillway_spray", "spray_shield_fixed",
        "The unsecured spray shield sweeps Iris into the spillway.", "Nezajištěná vodní clona smete Iris do přelivu.");
    gateRight(world, 66, {e2d::Condition::flag("spray_shield_fixed")},
        "The loose spray shield makes the walk unsafe.", "Uvolněná vodní clona činí chodník nebezpečným.");
    addUse(world, 67, "gatehouse_reader", "GATEHOUSE BADGE READER", "ČTEČKA DOMKU STAVIDEL",
        "turbine_badge", "gatehouse_open", "Jonah's badge opens the control vestibule.",
        "Jonahův odznak otevře vestibul ovládání.");
    addContext(world, 67, "spillway_crank", "SPILLWAY CRANK", "KLIKA PŘELIVU", "spillway_closed", {
        inspect(tr("Iris inserts the emergency crank and closes the false-open command by hand.",
            "Iris zasune nouzovou kliku a ručně zruší falešný povel k otevření.")),
    }, {e2d::Condition::flag("gatehouse_open")}, {}, 2, "power");
    addCharacter(world, 67, "jonah", "JONAH REED", "JONAH REED", "jonah_briefed", {
        speech(tr("Jonah: That flood command came from the ridge, not this gatehouse.",
            "Jonah: Ten povel k záplavě přišel z hřebene, ne z tohoto domku.")),
        speech(tr("Jonah: Drain the maintenance bay and I can open the east mine shaft.",
            "Jonah: Odčerpej servisní prostor a já otevřu východní důlní šachtu.")),
        speech(tr("Iris: Voss is erasing his route. We will preserve it instead.",
            "Iris: Voss maže svou cestu. My ji naopak zachováme."), e2d::MessageSpeaker::player),
    }, {e2d::Condition::flag("spillway_closed")});
    addContext(world, 68, "power_diagram", "AUXILIARY POWER DIAGRAM", "SCHÉMA POMOCNÉHO NAPÁJENÍ",
        "dam_diagram_read", {inspect(tr("The diagram gives a safe breaker order and links the dam feed to the mine substation.",
            "Schéma uvádí bezpečné pořadí jističů a spojuje přehradu s důlní rozvodnou."))});
    addPickup(world, 69, "pump_gasket", "A fresh pump gasket remains in the service cabinet.",
        "V servisní skříňce zůstalo nové těsnění čerpadla.", 0);
    addContext(world, 69, "bay_breakers", "FLOODED-BAY BREAKERS", "JISTIČE ZATOPENÉHO PROSTORU",
        "bay_isolated", {inspect(tr("Following the diagram, Iris opens the three breakers. The water stops arcing.",
            "Podle schématu Iris vypne tři jističe. Voda přestane jiskřit."))},
        {e2d::Condition::flag("dam_diagram_read")}, {}, 2, "power");
    addPickup(world, 70, "dry_cell", "A charged dry cell waits beside the emergency starter.",
        "Vedle nouzového startéru čeká nabitý suchý článek.", 0);
    addUse(world, 70, "pump_flange", "EMERGENCY PUMP FLANGE", "PŘÍRUBA NOUZOVÉHO ČERPADLA",
        "pump_gasket", "pump_gasket_installed", "The new gasket seals the cracked pump flange.",
        "Nové těsnění utěsní prasklou přírubu čerpadla.", {}, true, 0);
    addUse(world, 70, "pump_starter", "EMERGENCY PUMP STARTER", "STARTÉR NOUZOVÉHO ČERPADLA",
        "dry_cell", "pump_battery_installed", "The charged dry cell wakes the starter lamp.",
        "Nabitý suchý článek rozsvítí kontrolku startéru.", {}, true, 2);
    addPickup(world, 72, "valve_wheel", "You detach the redundant bypass valve wheel.",
        "Odpojíš kolo nepotřebného obtokového ventilu.", 0);
    addUse(world, 74, "intake_valve", "PUMP INTAKE VALVE", "PŘÍVODNÍ VENTIL ČERPADLA", "valve_wheel", "pump_intake_open",
        "The wheel opens the intake in the direction shown by the turbine diagram.",
        "Kolo otevře přívod ve směru označeném na schématu turbíny.",
        {e2d::Condition::flag("dam_diagram_read")}, true);
    addContext(world, 70, "pump_controls", "EMERGENCY PUMP CONTROLS", "OVLÁDÁNÍ NOUZOVÉHO ČERPADLA",
        "pump_running", {inspect(tr("The primed pump catches and lowers the maintenance-bay water in four visible stages.",
            "Zavodněné čerpadlo se rozběhne a ve čtyřech stupních sníží vodu v servisním prostoru."))},
        {e2d::Condition::flag("pump_gasket_installed"), e2d::Condition::flag("pump_battery_installed"),
            e2d::Condition::flag("pump_intake_open"), e2d::Condition::flag("bay_isolated")}, {}, 3, "power");
    addPickup(world, 71, "magnet_cord", "Wearing insulated boots, Iris retrieves the magnet from the shallow locker.",
        "V izolačních botách Iris vytáhne magnet z mělké skříňky.", 0,
        {e2d::Condition::flag("pump_running"), e2d::Condition::has("insulated_boots")});
    addHazard(world, 71, "electrified_water", "bay_isolated",
        "Current flashes through the flooded maintenance bay.", "Zatopeným servisním prostorem projede proud.");
    addContext(world, 75, "shaft_grille", "EAST SHAFT GRILLE", "MŘÍŽ VÝCHODNÍ ŠACHTY", "mine_access_open", {
        speech(tr("Jonah: Water is down. I have released the east grille. Watch the mine gas below.",
            "Jonah: Voda klesla. Uvolnil jsem východní mříž. Dole pozor na důlní plyn.")),
    }, {e2d::Condition::flag("jonah_briefed"), e2d::Condition::flag("pump_running"),
        e2d::Condition::flag("taken_magnet_cord")}, {e2d::Mutation::setFlag("reservoir_complete")}, 2, "unlock");
    gateRight(world, 75, {e2d::Condition::flag("mine_access_open")},
        "Jonah cannot open the shaft until the spillway and flooded bay are controlled.",
        "Jonah nemůže otevřít šachtu, dokud nejsou přeliv a zatopený prostor pod kontrolou.");

    addPickup(world, 76, "respirator", "You take the respirator body from the emergency cabinet.",
        "Vezmeš tělo respirátoru z nouzové skříňky.", 0);
    addUse(world, 77, "timber_brace", "LOOSE TIMBER BRACE", "UVOLNĚNÁ VÝDŘEVA", "wrench", "drift_braced",
        "The marked brace tightens until the warning dust stops.", "Označená vzpěra se dotáhne a varovný prach ustane.");
    gateRight(world, 77, {e2d::Condition::flag("drift_braced")},
        "The marked timber brace must be secured before the collapsed drift.",
        "Před zavalenou chodbou je nutné zajistit označenou výdřevu.");
    addHazard(world, 78, "rockfall", "drift_braced",
        "The unsupported drift gives way above Iris.", "Nezajištěná chodba se nad Iris zřítí.");
    addUse(world, 79, "respirator_filter", "RESPIRATOR FILTER", "FILTR RESPIRÁTORU", "charcoal", "respirator_fitted",
        "Clean charcoal packs the filter housing and completes the respirator.",
        "Čisté uhlí naplní pouzdro filtru a dokončí respirátor.", {e2d::Condition::has("respirator")}, true, 0);
    addUse(world, 79, "fan_starter", "VENTILATION FAN STARTER", "STARTÉR VĚTRÁKU", "multimeter", "ventilation_running",
        "The meter finds a dead starter contact; Iris bridges it and the fan accelerates.",
        "Multimetr najde mrtvý kontakt; Iris ho propojí a větrák zrychlí.", {}, false, 2);
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
    addUse(world, 82, "submerged_grate", "SUBMERGED GRATE", "PONOŘENÁ MŘÍŽ", "magnet_cord", "lift_fuse_retrieved",
        "The magnet swings once, catches, and brings the lift fuse out of the water.",
        "Magnet se zhoupne, zachytí a vytáhne z vody pojistku výtahu.",
        {e2d::Condition::flag("mine_drained"), e2d::Condition::has("insulated_boots")}, false);
    // The retrieved fuse becomes a carried item without consuming the reusable magnet.
    world.interactions.back().mutations.push_back(e2d::Mutation::addItem("lift_fuse"));
    addPickup(world, 83, "mine_map", "You unfold Voss's marked mine map.", "Rozložíš Vossovu označenou důlní mapu.", 0);
    addPickup(world, 83, "research_badge", "Kline's research badge was abandoned in haste.",
        "Klineové výzkumný odznak byl opuštěn ve spěchu.", 1);
    addPickup(world, 83, "punched_card", "The punched card has meaningful holes on both orientations.",
        "Děrný štítek má smysluplné otvory v obou orientacích.", 2);
    addUse(world, 84, "lift_fuse_box", "FREIGHT LIFT FUSE BOX", "POJISTKOVÁ SKŘÍŇ VÝTAHU", "lift_fuse", "lift_fuse_installed",
        "The recovered fuse wakes the cage lamp, but the motor feed remains dark.",
        "Získaná pojistka rozsvítí lampu klece, ale napájení motoru zůstane temné.", {}, true);
    addContext(world, 87, "isolation_order", "SWITCHGEAR ISOLATORS", "ODPOJOVAČE ROZVADĚČE", "substation_isolated", {
        inspect(tr("Calder's arrows guide a safe isolation order. The black feed falls quiet.",
            "Calderiny šipky vedou bezpečným pořadím odpojení. Černý přívod ztichne.")),
    }, {e2d::Condition::flag("nightjar_signal_found")}, {}, 1, "power");
    addUse(world, 88, "quiet_field_feed", "QUIET FIELD FEED", "PŘÍVOD TICHÉHO POLE", "wrench", "quiet_feed_cut",
        "The wrench disconnects the black Quiet Field feed from the mine system.",
        "Klíč odpojí černý přívod Tichého pole od důlního systému.",
        {e2d::Condition::flag("substation_isolated")});
    addUse(world, 88, "lift_bus", "LIFT BUS CIRCUIT", "OBVOD PŘÍPOJNICE VÝTAHU", "copper_bus_bar", "lift_powered",
        "The copper bar completes the lift circuit. The cage motor hums above.",
        "Měděná přípojnice dokončí obvod výtahu. Motor klece nahoře zabzučí.",
        {e2d::Condition::flag("quiet_feed_cut")}, true, 2);
    addUse(world, 89, "research_reader", "RESEARCH BADGE READER", "ČTEČKA VÝZKUMNÉHO ODZNAKU",
        "research_badge", "research_badge_presented", "The reader accepts Kline's emergency authority.",
        "Čtečka přijme Klineové nouzové oprávnění.");
    addUse(world, 89, "code_reader", "PUNCHED-CARD READER", "ČTEČKA DĚRNÉHO ŠTÍTKU",
        "punched_card", "research_door_open", "Turned upside down, the card exposes Kline's emergency code and retracts the bolts.",
        "Obrácený štítek odhalí Klineové nouzový kód a zasune závory.",
        {e2d::Condition::flag("research_badge_presented"), e2d::Condition::flag("lift_time_known")}, false, 2);
    addContext(world, 90, "ridge_lift", "RIDGE FREIGHT LIFT", "HŘEBENOVÝ NÁKLADNÍ VÝTAH", "act3_complete", {
        speech(tr("Voss: Leave my phase coil in the cage and you may walk away before midnight.",
            "Voss: Nech mou fázovou cívku v kleci a můžeš před půlnocí odejít.")),
        speech(tr("Iris: Kestrel Six did not get that choice. Neither do you.",
            "Iris: Kestrel Six tu možnost nedostal. Ty také ne."), e2d::MessageSpeaker::player),
        inspect(tr("The lift climbs through old mine strata toward the observatory and Nightjar.",
            "Výtah stoupá starými důlními vrstvami k observatoři a Nightjaru.")),
    }, {e2d::Condition::flag("lift_fuse_installed"), e2d::Condition::flag("lift_powered"),
        e2d::Condition::flag("research_door_open"), e2d::Condition::has("red_phase_coil")}, {}, 2, "climb");
    gateRight(world, 90, {e2d::Condition::flag("act3_complete")},
        "The ridge lift needs its fuse, mine power and Kline's emergency access.",
        "Hřebenový výtah potřebuje pojistku, důlní napájení a Klineové nouzový přístup.");
}

void addActFour(e2d::WorldDefinition& world) {
    addUse(world, 91, "tracking_camera", "TRACKING CAMERA", "SLEDUJÍCÍ KAMERA", "hand_mirror", "camera_blinded",
        "The mirror returns one hard flash. The tracking camera iris closes.",
        "Zrcátko vrátí ostrý záblesk. Clona sledující kamery se zavře.");
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
        {e2d::Mutation::setFlag("fog_horn_ready")}, 2, "warning");
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
    addContext(world, 122, "beacon_crystal", "CRACKED BEACON HOUSING", "PRASKLÝ KRYT MAJÁKU",
        "beacon_reference_ready", {inspect(tr("Iris removes the crystal, cleans it with cloth, and reseats it. The red rotation becomes a steady green pulse.",
            "Iris vyjme krystal, očistí ho látkou a vrátí zpět. Červené otáčení se změní ve stálý zelený pulz."))},
        {e2d::Condition::flag("waveguide_tuned")}, {}, 2, "power");
    gateRight(world, 122, {e2d::Condition::flag("beacon_reference_ready")},
        "The protected carrier needs a clean beacon-crystal reference.",
        "Chráněná nosná vlna potřebuje čistou referenci krystalu majáku.");
    addUse(world, 123, "azimuth_mount", "ANTENNA AZIMUTH MOUNT", "AZIMUTOVÝ DRŽÁK ANTÉNY", "wrench", "antenna_aligned",
        "Using Nell's chart, Iris rotates the mount onto the true north mark.",
        "Podle Nellina plánu Iris otočí držák na značku pravého severu.",
        {e2d::Condition::flag("tower_alignment_known")});
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
            "OPEN CHANNEL. Iris keys 4-1-3 and the Quiet Field collapses into a clean rescue carrier. Kestrel Six answers and the Black Pine beacon turns green. Voss is stopped; some proof remains hidden in Nightjar.",
            "OTEVŘENÝ KANÁL. Iris zadá 4-1-3 a Tiché pole se zhroutí do čisté záchranné nosné vlny. Kestrel Six odpoví a maják Black Pine zezelená. Voss je zastaven; část důkazů zůstává skrytá v Nightjaru."))}, 40, {}});
}

void addHints(e2d::WorldDefinition& world) {
    int priority = 1000;
    const auto next = [&world, &priority](const std::string_view flag, const char* en, const char* cs,
        std::vector<e2d::Condition> prerequisites = {}) {
        addHint(world, flag, en, cs, priority--, std::move(prerequisites));
    };
    next("taken_patch_cable", "At screen 001, TAKE the patch cable from the damaged toolbox.",
        "Na obrazovce 001 SEBER propojovací kabel z poškozené skříňky.");
    next("met_mara", "Reach Mara in the caretaker cabin and speak with ENTER.",
        "Dojdi za Marou do správcovské chaty a promluv klávesou ENTER.");
    next("key_revealed", "EXAMINE Mara's desk after speaking with her.", "Po rozhovoru s Marou PROZKOUMEJ její stůl.");
    next("taken_brass_key", "TAKE the brass yard key revealed on Mara's desk.", "SEBER mosazný klíč odkrytý na Mařině stole.");
    next("vehicle_gate_open", "USE the brass key on the vehicle gate at screen 014.", "Na obrazovce 014 POUŽIJ mosazný klíč na vjezdovou bránu.");
    next("cable_patched", "USE the patch cable on the blue terminals in screen 017.", "Na obrazovce 017 POUŽIJ kabel na modré svorky.");
    next("fuse_installed", "USE the ceramic fuse on the MAIN holder in screen 018.", "Na obrazovce 018 POUŽIJ keramickou pojistku na HLAVNÍ držák.");
    next("battery_linked", "USE the wrench on the battery bus in screen 019.", "Na obrazovce 019 POUŽIJ klíč na spojnici akumulátorů.");
    next("fuel_valve_open", "USE the wrench on the fuel valve in screen 020.", "Na obrazovce 020 POUŽIJ klíč na palivový ventil.");
    next("feeder_isolated", "USE the lineman gloves on the feeder isolator in screen 021.", "Na obrazovce 021 POUŽIJ elektrikářské rukavice na odpojovač.");
    next("workshop_open", "USE the brass key on Calder's cabinet in screen 022 to obtain the multimeter.",
        "Na obrazovce 022 POUŽIJ mosazný klíč na Calderové skříň a získej multimetr.");
    next("power_on", "Return to screen 018 and operate the repaired generator's MAIN lever with ENTER.",
        "Vrať se na obrazovku 018 a spusť opravený generátor klávesou ENTER u HLAVNÍ páky.");
    next("mast_calibrated", "Return to screen 011 and USE the multimeter on the powered weather mast.",
        "Vrať se na obrazovku 011 a POUŽIJ multimetr na napájený meteostožár.");
    next("act1_complete", "At screen 024, operate the direction console after tracing the Nightjar trunk.",
        "Na obrazovce 024 spusť směrový panel po proměření trasy Nightjar.");
    next("theo_briefed", "Free Theo in screen 030 with the saw and bandage, then speak to him.",
        "Na obrazovce 030 osvoboď Thea pilou a obvazem a pak s ním promluv.");
    next("echo_route_solved", "USE Theo's compass with bearing 017 in Echo Grove, screen 033.",
        "Na obrazovce 033 v Háji ozvěn POUŽIJ Theův kompas a náměr 017.");
    next("bear_gone", "At screen 035, USE the signal flare from the upwind edge.",
        "Na obrazovce 035 POUŽIJ signální světlici z návětrné hrany.");
    next("lookout_briefed", "Speak to Nell at the fire lookout, screen 038.", "Promluv s Nell na požární hlásce, obrazovka 038.");
    next("ravine_rope_fixed", "At screen 039, fix the iron hook first, then the climbing rope.",
        "Na obrazovce 039 upevni nejprve železný hák a potom lano.");
    next("brant_secured", "Free Owen, sound the crusher horn, then USE the brass key on Brant's cage.",
        "Osvoboď Owena, spusť houkačku a POUŽIJ mosazný klíč na Brantovu klec.");
    next("act2_complete", "Repair the hoist signal and pulley at screens 049–050, then run the controls.",
        "Na obrazovkách 049–050 oprav signalizaci a kladku navijáku a pak ho spusť.");
    next("logging_engine_running", "Collect belt, plug, oil and fuel, align screen 060 points, then start screen 061 engine.",
        "Seber řemen, svíčku, olej a palivo, srovnej výhybku 060 a spusť lokomotivu 061.");
    next("trestle_brake_fixed", "At screen 062, sound June's whistle and repair the brake linkage.",
        "Na obrazovce 062 spusť Juninu píšťalu a oprav táhlo brzdy.");
    next("spillway_closed", "Use Jonah's badge and operate the spillway crank at screen 067.",
        "Použij Jonahův odznak a ovládej kliku přelivu na obrazovce 067.");
    next("pump_running", "Isolate the bay, fit the pump gasket and cell, open screen 074 intake, then return to screen 070.",
        "Odpoj prostor, namontuj těsnění a článek, otevři přívod 074 a vrať se na 070.");
    next("mine_access_open", "Retrieve the magnet in drained screen 071, then ask Jonah to open screen 075 shaft.",
        "V odčerpané obrazovce 071 vezmi magnet a požádej Jonaha o otevření šachty 075.");
    next("respirator_fitted", "At screen 079, USE charcoal on the respirator before entering the gas zone.",
        "Na obrazovce 079 POUŽIJ uhlí na respirátor před vstupem do plynu.");
    next("lift_powered", "Ground screen 087 switchgear, cut the black feed, then install the copper bus at 088.",
        "Odpoj rozvaděče 087, přeruš černý přívod a namontuj měděnou přípojnici na 088.");
    next("act3_complete", "Open research door 089 and operate the powered ridge lift at screen 090.",
        "Otevři výzkumné dveře 089 a spusť napájený hřebenový výtah 090.");
    next("courtyard_patrol_diverted", "Place June's ration and set the kitchen timer at screen 094.",
        "Polož Juninu dávku a nastav kuchyňskou minutku na obrazovce 094.");
    next("archive_open", "Read project dates at 096, then align the four drawers at 097.",
        "Přečti data projektu na 096 a srovnej čtyři zásuvky na 097.");
    next("dome_aligned", "Get the dome key from security 102, return to 099, unlock and align the dome north.",
        "Získej klíč v kanceláři 102, vrať se na 099, odemkni a srovnej kopuli na sever.");
    next("sable_persuaded", "Use Nell's telescope at 100, then confront Sable at 101 with the accumulated evidence.",
        "Použij Nellin teleskop na 100 a pak konfrontuj Sable na 101 se získanými důkazy.");
    next("bunker_door_open", "At 103, use Kline's badge, cipher lens and calibration fork on the three locks.",
        "Na 103 použij Klineové odznak, šifrovací čočku a kalibrační ladičku na tři zámky.");
    next("protected_sequence_known", "Diagnose coil and prism at 106, then operate the fork positions at 107.",
        "Diagnostikuj cívku a hranol na 106 a pak ovládej polohy ladičky na 107.");
    next("evidence_copied", "Ground capacitors, divert cooling, then copy the command archive at 112.",
        "Uzemni kondenzátory, odveď chlazení a zkopíruj velitelský archiv na 112.");
    next("act4_complete", "Free Kline, light the emergency stair, and enter 4-1-3 at summit lock 115.",
        "Osvoboď Klineovou, rozsviť nouzové schody a zadej 4-1-3 do zámku 115.");
    next("summit_grounded", "At 118, USE the grounding clamp, then the wrench, on the broken copper strap.",
        "Na 118 POUŽIJ zemnicí svorku a potom klíč na prasklý měděný pás.");
    next("waveguide_tuned", "Install the coil at 119; install prism and tune with the fork at 121.",
        "Namontuj cívku na 119; namontuj hranol a nalaď ho ladičkou na 121.");
    next("antenna_control_locked", "Clean the beacon at 122, then align and lock the antenna controls at 123.",
        "Očisti maják na 122, potom srovnej a uzamkni ovládání antény na 123.");
    next("transmitter_unlocked", "Confront Voss at 124 and USE his transmitter key on the console lock.",
        "Konfrontuj Vosse na 124 a POUŽIJ jeho klíč vysílače na zámek panelu.");
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
