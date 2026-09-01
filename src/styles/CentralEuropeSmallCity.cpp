// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Style.hpp"

namespace MeshWorld {

Style make_style_central_europe_small_city() {
    Style s;
    s.id   = "central_europe_small_city";
    s.name = "Central European Small City";
    s.palette = {
        // park
        {"park.ground",          "grass_park"},
        {"park.path",            "path_gravel"},
        {"park.fountain.base",   "stone_granite"},
        {"park.fountain.basin",  "stone_light"},
        {"park.fountain.water",  "water"},
        {"park.flowerbed.a",     "flower_red"},
        {"park.flowerbed.b",     "flower_yellow"},
        {"park.lamp",            "metal_lamp_ornate"},
        // road
        {"road.ground",          "grass_strip"},
        {"road.surface",         "asphalt"},
        {"road.sidewalk",        "pavement_slab"},
        {"road.curb",            "stone_curb"},
        {"road.marking",         "road_marking_white"},
        {"road.lamp",            "metal_lamp"},
        {"road.drain",           "metal_grate"},
        // residential block
        {"block.ground",         "grass_garden"},
        {"block.facade.0",       "brick_red"},
        {"block.facade.1",       "plaster_cream"},
        {"block.facade.2",       "plaster_yellow"},
        {"block.roof.0",         "roof_tile_red"},
        {"block.roof.1",         "roof_tile_grey"},
        {"block.fence",          "wood_fence"},
        {"block.gate_post",      "stone_post"},
        {"block.path",           "path_stone"},
    };
    return s;
}

} // namespace MeshWorld
