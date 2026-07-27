#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>

namespace IronShadows
{
    using Microsoft::Xna::Framework::Vector3;

    // Gate M10 (plan_39 IS-39-011's own locked research note): Iron Shadows' single shared
    // "dynamic sun" -- one fixed directional light for this vertical slice (no day/night cycle;
    // that is real plan_08/plan_31 scope, not attempted here). Both the per-actor CPU brightness
    // tint below (see PrototypeRenderer::DrawMesh()'s own comment) and the baked lightmap (a later
    // step) read this same direction/intensity, so static and dynamic geometry read as
    // consistently lit. Components are pre-normalized by hand rather than computed via
    // Vector3::Normalize() at startup, so this stays a plain literal constant -- matching how
    // cutscene camera keyframes etc. are authored as literal values elsewhere in this codebase.
    inline const Vector3 kSunDirection(-0.5997F, -0.5997F, -0.5298F);
    inline constexpr float kSunIntensity = 0.75F;
    inline constexpr float kSunAmbientFloor = 0.35F;

    // How much daylight reaches a surface facing `normal`, clamped to [0,1] -- shared by the
    // per-actor CPU brightness tint below and the baked lightmap's per-face tile brightness
    // (LightmapMesh.hpp), so static and dynamic geometry read as consistently lit.
    [[nodiscard]] inline float ComputeBrightnessForNormal(const Vector3& normal)
    {
        const float dot = std::max(0.0F, Vector3::Dot(normal, -kSunDirection));
        return std::clamp(kSunAmbientFloor + kSunIntensity * dot, 0.0F, 1.0F);
    }

    // A single scalar approximating how much daylight reaches a mostly-upward-facing dynamic
    // actor (player, vehicle, traffic, pedestrians, police) -- a deliberate simplification (one
    // uniform brightness per actor, not real per-face N-dot-L shading). CNA's BasicEffect/
    // PbrEffect/SkinnedEffect built-in lighting (DirectionalLight0/EnableDefaultLighting) is a
    // no-op on the SOFTWARE backend Iron Shadows targets (confirmed by reading its own source),
    // so this is computed on the CPU instead and applied as a DiffuseColor multiplier, which the
    // SOFTWARE backend DOES apply unconditionally (vertexColor*diffuseColor*texture0, independent
    // of the lighting-enabled flag).
    [[nodiscard]] inline float ComputeSunBrightness()
    {
        return ComputeBrightnessForNormal(Vector3::Up);
    }
}
