# Missing pieces for full Craft parity across all three backends

This documents a concrete gap found while assessing whether `cna-craft` can be brought to full
[fogleman/Craft](https://github.com/fogleman/Craft) feature parity (per the `plan.md` §11
backlog) while running identically on all three of CNA's 3D-capable backends
(`EASYGL`/`VULKAN`/`BGFX`). Short answer: the core gameplay (movement, collision, chunk data,
breaking/placing, terrain) is already backend-agnostic, since it's written against the
`Microsoft::Xna::Framework` API. The gap is in Craft's *visual fidelity* features — ambient
occlusion, day/night sky dome fog blending, dynamic lighting — which Craft implements via custom
GLSL shaders (`shaders/block_vertex.glsl`, `shaders/block_fragment.glsl`,
`shaders/sky_vertex.glsl`, `shaders/sky_fragment.glsl`). Replicating those depends on CNA's
`ShaderEffect`, and support for it is uneven across backends.

## Custom shader support by backend

- **EASYGL** — real, working runtime GLSL compilation. `ShaderEffect` is implemented in
  `EasyGLGraphicsBackend.cpp` (compiles GLSL source strings at runtime). This is the
  straightforward path — closest to how Craft itself works (also GLSL, also OpenGL).
- **VULKAN** — `ShaderEffect` has a real implementation, but the constructor takes
  **pre-compiled SPIR-V bytes**, not GLSL source (see `examples/vulkan_shader_effect_test.cpp:14,216`
  in `../cna`). There is no runtime GLSL-to-SPIR-V compilation path. Using custom shaders on
  Vulkan means standing up a SPIR-V build/authoring toolchain (e.g. `glslang`/`shaderc` offline,
  committing the compiled bytecode) rather than just writing `.glsl` files.
- **BGFX** — no real `ShaderEffect` support today. `BgfxGraphicsBackend.cpp:237-253` shows only a
  stub `BgfxEffectBackend` — `Bind()` is empty, no shader compilation happens at all. Custom
  shaders on BGFX would require new engine-level work in CNA itself, not just in `cna-craft`.

**Implication:** AO baking, day/night-driven sky dome fog, and any other custom-shader visual
feature can only be built cleanly against `EASYGL` right now. `VULKAN` can get there but needs a
SPIR-V toolchain investment. `BGFX` can't get there at all until CNA's own BGFX backend grows
real shader support — that's upstream work on CNA, out of `cna-craft`'s scope.

**Recommendation:** treat `EASYGL` as the primary/reference target for full-fidelity work
(§11.2/§11.3 of `plan.md` — AO, day/night sky dome). Treat `VULKAN` as a secondary target that
should work once shaders are precompiled to SPIR-V. Treat `BGFX` as `BasicEffect`-only (no AO, no
dynamic day/night lighting) until CNA's BGFX backend gets real shader support — file that as a
CNA-side task, not a `cna-craft` one.

## SQLite: added 2026-07-10 (user decision), delta persistence implemented

**Update**: this section originally said SQLite wasn't present anywhere in the dependency chain.
That's still true of `../cna`/`../sharp-runtime` themselves, but as of 2026-07-10 the user
explicitly approved adding it directly to `cna-craft`'s own `CMakeLists.txt` (not to CNA), via
`find_package(SQLite3 REQUIRED)` against the system package — confirmed present and discoverable
on this dev machine (`libsqlite3-dev`, CMake's built-in `FindSQLite3` module gives the
`SQLite::SQLite3` imported target directly, no `FetchContent`/vendoring/submodule needed). See
`src/CnaCraft/Persistence/WorldStore.{hpp,cpp}` for the resulting delta-storage implementation
(CRAFT_PARITY.md §4.1/§4.2, plan.md §12.1 item 15) — a `block(x,y,z,w)` table (Craft's real
`block(p,q,x,y,z,w)` schema minus the `p,q` chunk-address columns cna-craft's World doesn't have),
storing only player-driven edits over the regenerated procedural terrain, exactly like Craft's own
delta model. Only the engine-agnostic `CnaCraftWorlds` library and its main smoke test remain
SQLite-free by design (a separate `cna_craft_persistence_smoke_test` target covers `WorldStore`
specifically, since it needs real disk I/O unlike the rest of `CnaCraftWorlds`'s tests).

## BGFX build cost

Configuring `cna-craft` with `-DCNA_GRAPHICS_BACKEND=BGFX` triggers a real network fetch:
`../cna/CMakeLists.txt:116-123` declares
`FetchContent_Declare(bgfx_cmake, GIT_REPOSITORY .../bkaradzic/bgfx.cmake.git, GIT_TAG master, GIT_SHALLOW TRUE, GIT_SUBMODULES_RECURSE TRUE)`.
`GIT_TAG master` is a floating tag (not pinned to a commit), and there is no vendored/pre-fetched
copy of bgfx anywhere in the tree. This means:

- Every **fresh** build directory does a real shallow clone (+ submodules) over the network on
  first configure — this hung for 2+ minutes in a sandboxed/network-constrained environment.
- It's cached under that build directory's `_deps/`, so it's a one-time-per-build-dir cost, not
  a per-invocation one.
- There is no offline fallback if network access is restricted at configure time.

This isn't a fundamental blocker, but it's worth knowing before choosing BGFX for a CI pipeline or
an offline dev environment.

## Summary

| Backend | Gameplay (movement/collision/terrain/blocks) | Custom shaders (AO, day/night, fog) | Notes |
|---|---|---|---|
| EASYGL | ✅ works today | ✅ real GLSL support | reference/primary target |
| VULKAN | ✅ works today | ⚠️ works, but needs precompiled SPIR-V | extra toolchain investment |
| BGFX | ✅ works today | ❌ `ShaderEffect` is a stub in CNA | needs upstream CNA work first; first configure needs network access |

Full Craft feature parity, identically on all three backends, is not realistic today without also
extending CNA's own BGFX backend. A phased, backend-tiered approach (per the table above) is the
realistic path — see `plan.md` §11 for the feature-by-feature backlog this applies to.
