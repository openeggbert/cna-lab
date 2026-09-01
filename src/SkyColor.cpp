// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "SkyColor.hpp"

#include <cmath>
#include <cstddef>

namespace MeshWorld {

namespace {
struct Keyframe {
    double                hour;
    std::array<float, 3>  color;
};

// Midnight (deep navy) -> dawn (warm orange-pink, matches
// CelestialPosition.hpp's own 06:00 sunrise) -> noon (sky blue) -> dusk
// (deeper orange-red, matches the 18:00 sunset) -> back to midnight at
// hour 24. Deliberately simple, hand-picked colors, not a physically
// simulated atmosphere.
//
// Bug fix (2026-07-11, user-reported "why is the sky pink?"): the original
// version had ONLY these 4 keyframes, so dawn's warm glow linearly
// interpolated toward noon's blue over a full 6-hour span -- at
// TimeOfDay's own default starting hour (08:00, just 2h past dawn), the
// sky was still 2/3 of the way toward dawn's own orange-pink, i.e.
// genuinely pink, not blue. Real dawn/dusk glow is brief (well under an
// hour in practice); the fix adds 4 more keyframes tight around 06:00/
// 18:00 so the glow fades to full day/night color quickly instead of
// lingering for hours. 06:00/12:00/18:00/24:00 themselves are unchanged
// (still exactly match CelestialPosition.hpp's own sunrise/noon/sunset/
// midnight), so this only narrows the transition, it doesn't move it.
constexpr Keyframe kKeyframes[] = {
    {0.0,  {0.02f, 0.02f, 0.08f}},  // midnight -- deep navy
    {5.0,  {0.05f, 0.05f, 0.12f}},  // pre-dawn -- still mostly night
    {6.0,  {0.90f, 0.55f, 0.40f}},  // dawn -- peak warm glow
    {7.5,  {0.55f, 0.65f, 0.85f}},  // morning -- already mostly day
    {12.0, {0.45f, 0.65f, 0.95f}},  // noon -- full sky blue
    {16.5, {0.55f, 0.65f, 0.85f}},  // afternoon -- still day
    {18.0, {0.85f, 0.40f, 0.30f}},  // dusk -- peak warm glow
    {19.5, {0.10f, 0.08f, 0.18f}},  // evening -- already mostly night
    {24.0, {0.02f, 0.02f, 0.08f}},  // midnight -- deep navy (wraps to 0.0)
};
constexpr int kKeyframeCount = sizeof(kKeyframes) / sizeof(kKeyframes[0]);
} // namespace

std::array<float, 3> sky_color(double hours) {
    double h = std::fmod(hours, 24.0);
    if (h < 0.0) h += 24.0;

    for (int i = 0; i + 1 < kKeyframeCount; ++i) {
        const Keyframe& a = kKeyframes[i];
        const Keyframe& b = kKeyframes[i + 1];
        if (h < a.hour || h > b.hour) continue;

        const double t = (h - a.hour) / (b.hour - a.hour);
        return {
            static_cast<float>(a.color[0] + (b.color[0] - a.color[0]) * t),
            static_cast<float>(a.color[1] + (b.color[1] - a.color[1]) * t),
            static_cast<float>(a.color[2] + (b.color[2] - a.color[2]) * t),
        };
    }
    return kKeyframes[0].color;  // unreachable given h in [0,24) and the last keyframe at 24.0
}

} // namespace MeshWorld
