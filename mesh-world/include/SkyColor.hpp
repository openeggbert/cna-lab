// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>

namespace MeshWorld {

// S201 (sky/day-night/weather, 2026-07-11) — the sky background color for a
// given hour of day (0-24, wraps like TimeOfDay::hours() itself). Pure
// function, no rendering dependencies -- same "pure logic in the root
// build" split TimeOfDay.hpp/CelestialPosition.hpp already established.
//
// {r,g,b} in [0,1], matching this codebase's existing color convention
// (WorldMap::zone_color() and friends return std::array<float,3>, not a
// dedicated color struct -- reused here rather than inventing a new type).
//
// 9 fixed keyframes -- the 4 main ones (midnight/dawn/noon/dusk, at the
// same 00:00/06:00/12:00/18:00 hours CelestialPosition.hpp's sun arc uses)
// plus 5 more tightly bracketing dawn/dusk so their warm glow fades to
// full day/night color within roughly an hour instead of lingering for
// several (a real bug found via user report: TimeOfDay's own default
// starting hour, 08:00, used to render as pink -- see .cpp for the full
// writeup) -- linearly interpolated between the two nearest keyframes,
// deliberately simple, matching the stylized, non-astronomical model the
// rest of this S-series slice already uses. Not yet a real gradient sky
// dome (S203, a separate, larger, ask-first task) -- S202 wires this into
// the app's flat clear color, which is still a real, correct fix for "the
// sky is currently black".
std::array<float, 3> sky_color(double hours);

} // namespace MeshWorld
