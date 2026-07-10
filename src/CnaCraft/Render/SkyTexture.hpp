#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft::Render {

// The sky gradient texture (CRAFT_PARITY.md §5.3, plan.md §12.1 item 33) —
// a faithful, downsampled recreation of Craft's real textures/sky.png,
// built the same way this project's dye-block colors were (real RGB values
// sampled directly from Craft's own shipped asset, embedded as constants —
// keeps the "no binary art assets, everything procedural/embedded" project
// convention while matching the real thing's look).
//
// Layout matches Craft's sampling convention exactly (shaders/sky_*.glsl,
// src/cube.c make_sphere):
//   U (x axis) = time of day in [0,1) — 0/1 = midnight, 0.25 = dawn,
//       0.5 = noon, ~0.85 = dusk. The whole sky dome shares ONE U per frame
//       (Craft passes it as the `timer` uniform; here it's baked into the
//       dome's vertex UVs each frame, see SkyDome::Update).
//   V (y axis) = elevation band: 0 = straight down (nadir), 0.5 = horizon,
//       1 = straight up (zenith) — Craft's `t = 1 - acos(y)/pi` sphere
//       texcoord, same orientation after its own flip_image_vertical.
// Sampled with LinearClamp, matching Craft's GL_LINEAR + GL_CLAMP_TO_EDGE.
Microsoft::Xna::Framework::Graphics::Texture2D BuildProceduralSkyTexture(
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

// CPU-side bilinear sample of the same gradient the texture is built from —
// used by CnaCraftGame::Draw for the clear color and BasicEffect's FogColor,
// so fog fades toward the *actual* sky color at the horizon for the current
// time of day (Craft's block_fragment.glsl samples the sky texture per
// fragment at `vec2(timer, fog_height)`; BasicEffect's fixed-function fog
// only has one flat color per draw, so cna-craft uses the horizon band
// (elevationT=0.5) for everything — a documented simplification, see
// CRAFT_PARITY.md §5.2).
Microsoft::Xna::Framework::Color SampleSkyColor(float timeOfDay, float elevationT);

}
