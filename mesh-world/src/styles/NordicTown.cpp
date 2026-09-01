// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Style.hpp"

namespace MeshWorld {

Style make_style_nordic_town() {
    Style s;
    s.id   = "nordic_town";
    s.name = "Nordic Town";
    s.palette = {
        // park — snow-covered
        {"park.ground",          "snow"},
        {"park.path",            "cobblestone_path"},
        {"park.fountain.base",   "stone_granite"},
        {"park.fountain.basin",  "stone_granite"},
        {"park.fountain.water",  "ice_sheet"},
        {"park.flowerbed.a",     "snow"},
        {"park.flowerbed.b",     "snow"},
        {"park.lamp",            "metal_dark"},
        // road — cleared asphalt, stone kerbs
        {"road.ground",          "snow"},
        {"road.surface",         "asphalt"},
        {"road.sidewalk",        "cobblestone_light"},
        {"road.curb",            "stone_wall"},
        {"road.marking",         "road_marking_white"},
        {"road.lamp",            "metal_chrome"},
        {"road.drain",           "metal_grate"},
        // residential block — light plaster + dark details
        {"block.ground",         "snow"},
        {"block.facade.0",       "plaster_white"},
        {"block.facade.1",       "plaster_grey"},
        {"block.facade.2",       "brick_dark"},
        {"block.roof.0",         "roof_tile_grey"},
        {"block.roof.1",         "roof_tile_grey"},
        {"block.fence",          "metal_dark"},
        {"block.gate_post",      "stone_granite"},
        {"block.path",           "cobblestone_path"},
    };
    return s;
}

} // namespace MeshWorld
