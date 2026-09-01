# Sky, day/night & weather

The S-series (`plan.md`, S101-S1103, 48 tasks), built 2026-07-11 at explicit user
request ("obloha je černá... bude to mít i mraky... bude se střídat den a noc...
pridej nejaky mechanismus pocasi dest snih nebo klik vitr"). Everything here lives
entirely in the rendering layer (`apps/mesh-world-app` + `WorldRenderer`/MeshCraft),
not `namespace MeshWorld::Map` — a deliberately separate concern from the planetary
map subsystem `map.md` covers.

Same split every subsystem in this codebase already uses: **pure logic (state,
formulas, state machines) lives in the root `MeshWorldLib` build, fully unit-tested
without a GPU; only the final "hand this to `Mc3Renderer`" step is renderer-gated**
(`#ifdef MESH_WORLD_HAS_RENDERER`, only compiled in `apps/mesh-world-app`'s separate
CMake project). `WorldRendererTests.cpp`/`CelestialPositionTests.cpp`/
`WeatherTests.cpp`/`ParticleSystemTests.cpp`/`SnowAccumulationTests.cpp` cover the
pure half; nothing here has ever been visually verified with a real GPU (this
sandboxed dev environment has none) — see S1103 below.

## Time model

`TimeOfDay` (`include/TimeOfDay.hpp`/`src/TimeOfDay.cpp`) — a simple day/night clock,
not persisted (resets fresh every `start_explore()` call, same "explore-session
state, not saved world state" precedent `camera_`/`vel_y_` already establish).
`kDefaultDayLengthRealMinutes = 24.0` (1 real minute = 1 in-game hour). `advance()`
handles multi-day-wrap correctly for a single large call (e.g. after a long pause),
not just the common per-frame case.

Every other pure state machine in this system (`Weather`, `SnowAccumulation`) takes
the exact same `elapsed_real_seconds` unit and does the exact same
real-seconds→in-game-hours conversion, so a caller (`update_explore()`) can pass the
same per-frame `dt` to all of them.

## Celestial positions

`CelestialPosition.hpp`/`.cpp` — `SkyAngle{elevation_deg, azimuth_deg}`, a simple,
deliberately non-astronomical model:

- `sun_position(hours)`: a single sinusoidal arc. Rises due East (azimuth 90°) at
  06:00, zeniths due South (elevation 90°) at 12:00, sets due West at 18:00, nadir due
  North at midnight.
- `moon_position(hours)`: the sun's exact antipode (elevation negated, azimuth +180°)
  — approximates a full moon's real relationship to the sun, but does NOT model the
  moon's own ~29.5-day drift relative to the sun.
- `moon_phase_fraction(day, hours, lunar_cycle_days=8.0)`: 0=new, 0.25=first quarter,
  0.5=full, 0.75=last quarter — an 8 in-game-day cycle (short enough to notice within
  a normal play session), using continuous day progress (`day + hours/24`) so phase
  advances smoothly, not just once at midnight.
- `generate_star_field(seed, count, max_count=800)`: a fixed, seeded set of star
  positions (same `SkyAngle` type), generated once at world load and held for the
  session. Seeded from `std::hash<std::string>{}(world_name)` (`apps/mesh-world-app`),
  NOT from time — the same world name always shows the same stars. Uses a local
  splitmix64-based hash (not `Map::noise`'s), kept independent of the `Map::`
  subsystem. Elevation sampled via `asin(2u-1)` (not uniform in `[-90,90]`) for a
  proper uniform scatter across the sky sphere.

**Axis convention** (this project's own, no external convention to match): azimuth
0°/North = -Z, 90°/East = +X, 180°/South = +Z, 270°/West = -X; elevation = +Y (up).
`sky_angle_to_xyz()` (`WorldRenderer.cpp`, internal) converts a `SkyAngle` to a
camera-relative XYZ position at a given distance — shared by the sun, moon, and star
rendering (factored out once star rendering became the 3rd near-identical use).

## Sky color

`sky_color(hours)` (`SkyColor.hpp`/`.cpp`) — 9 keyframes (midnight navy → dawn
orange-pink → noon sky blue → dusk orange-red → midnight), linearly interpolated.
Wired into `main.cpp`'s `Draw()` as the GPU clear color.

## Rendering: sun, moon, stars, clouds, particles

All five live in `WorldRenderer.hpp`/`.cpp`, each with a pure "where/how bright"
compute function plus a renderer-gated `render_*()` method that builds a small,
never-persisted synthetic `Mc3Document` fresh every frame (same technique
`render_placements()` already established for `ModelPlacement` instances) and calls
`inject_materials()` + `renderer.render(doc, local_cam)` with the camera zeroed
(everything already computed camera-relative).

- **Sun** (`compute_sun_render_state()`/`render_sun()`): a bright `sun_glow` sphere at
  5000m "infinity" distance, brightness fading linearly across a ±10° horizon band.
- **Moon** (`compute_moon_render_state()`/`render_moon()`): a `moon_glow` sphere +
  a dark `moon_shadow` sphere offset perpendicular to the view direction, scaled by
  `illuminated_fraction` — the "eclipsing sphere" trick for phases without textures (0
  offset/fully dark at new moon, fully clear/fully bright at full moon). Brightness
  combines the moon's own horizon fade with a daylight-dimming factor (fades toward a
  0.15 floor as the sun climbs, never fully invisible during the day).
- **Stars** (`visible_star_count()`/`render_stars()`): draws the FIRST N entries of
  `generate_star_field()`'s stable ordering, where N is the exact inverse of the
  sun's own horizon-fade brightness — the visible set only ever grows/shrinks from one
  end as the sky darkens/brightens, never re-sampled (no flicker).
- **Clouds** (`compute_cloud_puffs()`/`render_clouds()`): 0/8/24 puffs for
  Clear/PartlyCloudy/(Overcast,Rain,Snow), each a 3-icosphere `Mc3Object::makeGroup()`
  cluster (same composition technique `ObjectDefinitionLibrary.cpp`'s tree canopies
  use). Scattered via **Vogel's disk-sampling method** (`angle = index·137.50776° +
  drift`, `radius_frac = sqrt((index+0.5)/count)`) — no RNG needed, fully deterministic
  from index+count. Drift rate scales with `wind.strength` only (direction is
  deliberately unused — a whole-field rotation has no natural compass mapping).
- **Precipitation particles** (`ParticleSystem`/`render_particles()`): the first real
  particle infrastructure in either this repo or mesh-craft. A fixed-cap pool
  (`kMaxParticles=1500`, quality tiers 200/600/1500) recycled within a 60m-radius,
  40m-tall disk around the camera, camera-relative with floating-origin re-homing
  (particles shift by the camera's own frame-to-frame delta, so they stay visually
  anchored in world space). Rain = fast straight-down streaks (thin cylinders) plus a
  wind-driven horizontal push; snow = slow drifting flakes (small icospheres), drift
  biased toward the wind direction with per-spawn jitter. Mutually exclusive, gated by
  `WeatherState`.

## Weather state machine

`Weather` (`Weather.hpp`/`.cpp`) — `WeatherState{Clear, PartlyCloudy, Overcast, Rain,
Snow}`. Transitions every `[3,8)` randomized in-game hours, with a 0.5h crossfade
(`transition_progress()`, derived from elapsed time since the transition started, not
stored/incremented directly — correctly carries leftover time across a transition
boundary within one large `advance()` call). `state()`/`previous_state()` expose the
crossfade endpoints for a renderer to blend between.

**Temperature gate** (S603): each transition picks from a 4-state warm pool
(`Clear`/`PartlyCloudy`/`Overcast`/`Rain`) or cold pool (same three + `Snow`), by
whether the LOCAL temperature (sampled fresh every call from the map layer's own
temperature field at the player's current position — `apps/mesh-world-app`'s
`local_temperature_c()`, reusing `ChunkPipeline.cpp`'s own tile-sampling technique) is
at/below freezing (0°C, inclusive).

**Seeding**: deliberately NOT deterministic like `generate_star_field()` — weather has
no "same world always shows the same weather" requirement, so it's seeded from
`steady_clock` at each `start_explore()`, matching this project's own "entropy is
time-based, non-reproducible by design" principle.

## Wind

Folded directly into `Weather` (not a separate class) — `WindState{direction_deg,
strength}`. `Weather::wind()` interpolates between the previously-rolled wind and a
freshly-rolled target using the EXACT SAME crossfade progress the weather transition
itself uses ("paired with the weather-transition timer"), rolled fresh every time a
transition fires. Biased by the NEW weather state: `Overcast`/`Rain`/`Snow` sample
strength from `[0.4, 1]`, `Clear`/`PartlyCloudy` from `[0, 0.5]` (deliberately
overlapping ranges). Direction interpolates along the shorter angular path.

Consumers: cloud drift rate (above), particle horizontal drift (above), and tree sway
(below).

## Tree sway

`compute_tree_sway_rotation_deg()` (pure) + `WorldRenderer::apply_tree_sway()`
(renderer-gated) — a sine-wave rotation, amplitude scaled by `wind.strength`,
decomposed into Euler X/Z components from `wind.direction_deg`, pushed onto every
`"trunk"`/`"canopy"` sub-object (the names every tree definition in
`ObjectDefinitionLibrary.cpp` already uses) via
`Mc3Renderer::scene_renderer().setAnimOverrides()`. Called every frame
unconditionally (even at zero wind — `setAnimOverrides()` replaces the FULL override
map each call, so a zero-wind frame still needs to push a zero-rotation override to
clear the previous frame's nonzero one).

**Known v1 limitation, confirmed via research (`SceneRenderer.cpp:608-619`), not
assumed**: `AnimOverride` is keyed by object NAME against one shared entry, applied to
EVERY object anywhere in the document with that name — so every tree instance, of
every species, everywhere in the scene, sways in lockstep/in-phase, not independently.
A real per-instance phase offset needs extending `SceneRenderer`'s own
instance-drawing path to key overrides by instance identity, not just name — a
separate, larger, cross-repo (mesh-craft) change, not attempted as part of this
backlog (same "ask before crossing into mesh-craft" precedent S203/S303 both
established: neither was attempted here either, for the same reason).

## Snow accumulation

`SnowAccumulation` (pure, root build) — a single GLOBAL depth scalar `[0,1]`, not
tracked per-object (with potentially hundreds of trees/props/roofs in view,
per-instance bookkeeping would be real complexity for no visible benefit). Builds from
0 to 1 over 4 continuous in-game hours of `WeatherState::Snow` + at/below-freezing;
melts over 6 hours once EITHER temperature rises above freezing OR 12 hours pass
without fresh snowfall (a deliberate stylized simplification, not a claim that real
snow melts from time alone while still freezing).

`WorldRenderer::render_snow_accumulation()` (renderer-gated) overlays thin white boxes
(reusing the already-registered `"snow"` material) on chunk-embedded objects whose
name is `is_snow_eligible_chunk_object_name()` — currently only
`SmallHouseBlockGenerator.cpp`'s own `"roof_"`-prefixed roof boxes, the one real,
reusable per-object naming convention found by research before writing this (no
interior-room generator exists anywhere in this codebase, so "excludes interiors" is
satisfied vacuously, not by new exclusion logic — see plan.md's own S1001/S1004
writeup for the full research finding). Plus a small white sphere near the
approximated top of every visible `ModelPlacement` instance (tree canopies/ground
props), positioned at `kApproxOutdoorObjectHeightM * placement.scale` above its base —
a stylized approximation, since `PlacementInstance` carries no real per-definition
geometry height at this layer.

**Known v1 limitation**: other generators' rooftops (e.g. `ApartmentBlockGenerator`)
aren't tagged with any recognizable name prefix today, so they don't get snow in this
pass. Extending this is later, straightforward work (tag more object names in more
generators), not attempted here to keep this task's own scope bounded.

## Manual visual-verification checklist (S1103)

Everything above compiles, links, and passes its own unit tests, but has never
actually been SEEN — this sandboxed dev environment has no GPU/display. A human with
a real display running `apps/mesh-world-app` should confirm, over one play session:

- [ ] The sky is blue during the day (not black), with a visible dawn/dusk color shift
      and a dark-but-not-black night sky.
- [ ] The sun is visible during the day, fades out near/below the horizon (not a hard
      pop), and does NOT appear to sit on the terrain (the far-plane bug this session
      already found and fixed once).
- [ ] The moon is visible at night (and faintly during the day), and its
      illuminated shape changes over several in-game days (new → crescent → quarter →
      gibbous → full → back down) — one full cycle takes 8 in-game days
      (`lunar_cycle_days`), i.e. roughly 8 real-world minutes at the default day
      length.
- [ ] Stars fade in as the sun sets and fade out as it rises, never appear as a harsh
      on/off pop.
- [ ] Mouse look (free-look) works smoothly in both axes (not inverted — this session
      already found and fixed one inversion bug) and returns to normal
      when the player moves.
- [ ] Over a long enough session, weather visibly cycles through multiple states (the
      HUD's own "Weather ..." readout is the easiest way to confirm this without
      waiting to visually recognize each state) — clouds appear/thicken for
      `PartlyCloudy`/`Overcast`/`Rain`/`Snow`, rain/snow particles fall during
      `Rain`/`Snow`.
- [ ] Trees visibly sway more in stronger wind (HUD "Wind ..." readout) — and, per the
      known v1 limitation above, ALL trees sway in the same phase/direction at once
      (confirm this is what's actually seen, not independent motion, since that would
      indicate the limitation description here is stale).
- [ ] After sustained snow at freezing temperatures, snow-cap overlays appear on
      house roofs and on top of trees/props (HUD "Snow N% accumulated" readout tracks
      this numerically) — and they gradually disappear again once it warms up or a
      long time passes without fresh snow.
- [ ] Performance stays acceptable at the default particle quality (Medium, 600
      particles) during active rain/snow.

If anything above doesn't match, it's a real bug this session's own unit tests could
not have caught (they test the pure logic, not the actual rendered pixels) — file it
the same way the pink-sky/sun-on-terrain/inverted-mouse-look bugs earlier in this
S-series were found and fixed: a precise description (ideally a screenshot) is usually
enough to diagnose the root cause without needing to reproduce the GPU environment.
