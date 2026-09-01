// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Style.hpp"

namespace MeshWorld {

Style make_style_desert_outpost() {
    Style s;
    s.id   = "desert_outpost";
    s.name = "Desert Outpost";
    s.palette = {
        // park — sand and sandstone
        {"park.ground",          "sand"},
        {"park.path",            "sand_wet"},
        {"park.fountain.base",   "rock_sandstone"},
        {"park.fountain.basin",  "rock_sandstone"},
        {"park.fountain.water",  "water"},
        {"park.flowerbed.a",     "sand_dune"},
        {"park.flowerbed.b",     "gravel_riverbed"},
        {"park.lamp",            "wood_natural"},
        // road — sandy surface, sandstone pavers
        {"road.ground",          "sand"},
        {"road.surface",         "sand_dune"},
        {"road.sidewalk",        "rock_sandstone"},
        {"road.curb",            "rock_sandstone"},
        {"road.marking",         "paint_white"},
        {"road.lamp",            "wood_natural"},
        {"road.drain",           "rock_sandstone"},
        // residential block — cream/sandstone walls, flat roofs
        {"block.ground",         "sand"},
        {"block.facade.0",       "plaster_cream"},
        {"block.facade.1",       "rock_sandstone"},
        {"block.facade.2",       "plaster_yellow"},
        {"block.roof.0",         "plaster_cream"},
        {"block.roof.1",         "rock_sandstone"},
        {"block.fence",          "wood_natural"},
        {"block.gate_post",      "rock_sandstone"},
        {"block.path",           "sand_wet"},
    };
    return s;
}

} // namespace MeshWorld
