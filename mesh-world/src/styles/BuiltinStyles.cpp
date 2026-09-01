// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "BuiltinStyles.hpp"
#include "StyleRegistry.hpp"
#include "Style.hpp"

namespace MeshWorld {

Style make_style_central_europe_small_city();
Style make_style_nordic_town();
Style make_style_desert_outpost();
Style make_style_jungle_village();

void register_builtin_styles() {
    auto& reg = StyleRegistry::instance();
    reg.register_style(make_style_central_europe_small_city());
    reg.register_style(make_style_nordic_town());
    reg.register_style(make_style_desert_outpost());
    reg.register_style(make_style_jungle_village());
}

} // namespace MeshWorld
