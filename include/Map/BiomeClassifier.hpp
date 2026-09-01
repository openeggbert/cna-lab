// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "ZoneType.hpp"

namespace MeshWorld::Map {

// Maps (elevation_m, temperature_c, moisture [0..1], sea_level_m,
// cavity_noise [0..1]) → ZoneType. Pure static; no state. Moisture can be 0
// when not yet computed (M060 deferred).
class BiomeClassifier {
public:
    // M331 (MAP21) -- `cavity_noise` is a caller-supplied, spatially-varying
    // 0..1 score (1.0 = strongly cave-like), NOT derived from the other 4
    // scalar parameters -- classify() has no world-position/entropy
    // awareness of its own (deliberately: it stays a pure function of local
    // climate, reusable exactly as every existing caller/test already
    // relies on). The one real caller that populates it is MapBuilder::
    // setBiomeField() (Map::noise::worley_f1() sampled at this cell's real
    // world position, see its own doc comment for why Worley specifically).
    // Defaults to 0.0 so every pre-M331 caller (every direct classify()
    // unit test, any future caller that doesn't care about caves) is
    // completely unaffected -- ZoneType::cave simply never triggers unless
    // a caller explicitly supplies a real cavity score.
    static MeshWorld::ZoneType classify(double elevation_m,
                                        double temperature_c,
                                        double moisture,
                                        double sea_level_m,
                                        double cavity_noise = 0.0);
};

} // namespace MeshWorld::Map
