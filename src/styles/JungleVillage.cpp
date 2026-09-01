// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Style.hpp"

namespace MeshWorld {

Style make_style_jungle_village() {
    Style s;
    s.id   = "jungle_village";
    s.name = "Jungle Village";
    s.palette = {
        // park — lush jungle floor
        {"park.ground",          "jungle_floor"},
        {"park.path",            "dirt"},
        {"park.fountain.base",   "rock_mossy"},
        {"park.fountain.basin",  "rock_mossy"},
        {"park.fountain.water",  "water_still"},
        {"park.flowerbed.a",     "flower_red"},
        {"park.flowerbed.b",     "flower_bluebell"},
        {"park.lamp",            "wood_natural"},
        // road — dirt track through jungle
        {"road.ground",          "jungle_floor"},
        {"road.surface",         "dirt"},
        {"road.sidewalk",        "dirt"},
        {"road.curb",            "rock_mossy"},
        {"road.marking",         "paint_white"},
        {"road.lamp",            "wood_natural"},
        {"road.drain",           "rock_mossy"},
        // residential block — wood and jungle materials
        {"block.ground",         "jungle_floor"},
        {"block.facade.0",       "wood_natural"},
        {"block.facade.1",       "plaster_cream"},
        {"block.facade.2",       "plaster_green"},
        {"block.roof.0",         "wood_natural"},
        {"block.roof.1",         "wood_counter"},
        {"block.fence",          "wood_natural"},
        {"block.gate_post",      "wood_natural"},
        {"block.path",           "dirt"},
    };
    return s;
}

} // namespace MeshWorld
