# NEXT.md — Iron Shadows continuity document

Primary short-term continuity doc for autonomous/resumed work sessions. Read this before
`plan.md`/`plan/plan_NN-*.md`. Update it whenever project state changes materially — do not wait
until the end of a session to reconstruct it from memory.

## Where things stand (as of this entry)

Repo: `/rv/data/development/github.com/openeggbert/iron-shadows`, branch `develop` (not `master` —
someone switched branches outside this session at some point; both exist, `develop` is ahead).
**No remote configured, nothing pushed** — commit locally only until explicitly told otherwise.

Gates M0-M9 (see `plan.md` milestone order) are done at prototype fidelity, including M9's own
literal "ten-minute soak" criterion (`plan_39-vertical-slice-gates.md` `IS-39-010`/`049`) — see
its entry below. **Gate M10 is IN PROGRESS** (production assets/collision, baked lighting, one
dynamic sun, limited shadows, audio, UI) — the on-screen HUD piece is done; dynamic sun/shadows,
the baked lightmap, and audio remain — see "What changed most recently" below for the locked
architecture and implementation order.

- M0/M1: workspace preflight + running procedural scaffold.
- M2: warehouse building loads as a real CNJ model (`assets/source/mc3/warehouse.mc3.xml` →
  `mc3togltf` → `cna_tool_gltf_to_cnj` → `Content.Load<Model>()`), replacing its procedural box.
- M3: sedan loads as 4 composed CNJ models (`vehicle_body`/`cabin`/`windshield`/`wheel` — one MC3
  file per part, not one multi-object scene, because `cna_tool_gltf_to_cnj` does not bake
  per-object node transforms into vertex data; see `plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md`
  `IS-10-004b` for the confirmed upstream gap).
- M4: Jolt Physics v5.6.0 (MIT, pinned commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`) is
  selected and integrated behind `IronShadows::Physics::PhysicsWorld` (PIMPL, no Jolt types leak
  out of `src/Physics/PhysicsWorld.cpp`). **Beyond the M4 gate's own scope**, `PlayerController`
  and `VehicleController` are actually driven by physics (not just standalone-prototyped).
- M5: a second, genuinely different district (`Countryside`, alongside the original
  `WarehouseBlock`) plus `IronShadows::DistrictManager`, which owns the currently loaded
  `PrototypeWorld` and its static physics bodies and drives a synchronous loading-screen
  transition between them.
- M6: one skinned test character plays "Idle"/"Walk" (crossfading between them), "Dialogue", and
  "EnterVehicle"/"ExitVehicle" clips, replacing the procedural player box. Required extending the
  `cna-extended` sibling repo (owner's explicit go-ahead) with a new ECS component/system pair,
  since it had no wrapper for the skinned-playback path cna's own glTF/CNJ tools actually
  populate, later extended with crossfade blending.
- M7: the one existing mission is now data-driven — `assets/missions/prologue.mission.json`
  defines its 5 states/objectives/transitions, loaded via new `MissionDefinition`/
  `LoadMissionDefinition` (parsed with sharp-runtime's own `System::Text::Json`), replacing
  `PrototypeMission`'s hardcoded switch statements while preserving its enum-based public API and
  `SaveGame` compatibility exactly.
- M8: a short in-engine intro cutscene (camera-track only) plays alongside the opening dialogue,
  panning from an establishing shot of the warehouse delivery target to the exact framing of the
  normal gameplay camera at the player's spawn — skippable, save-safe, and hands control back to
  gameplay automatically once finished.
- M9: 2 `TrafficVehicle`s follow a fixed waypoint loop and brake for each other/the player's
  vehicle; 2 `Pedestrian`s walk fixed sidewalk paths and flee the player's vehicle within a fixed
  radius; `PoliceSystem` runs a full `Clear -> Dispatched -> Chasing -> (one escalation) -> Clear`
  state machine off a simplified fixed-radius "witness" check (not real line-of-sight) — all
  WarehouseBlock-only, ticking every frame regardless of dialogue/cutscenes, deterministically
  unit-tested end to end, and confirmed surviving a ~16.3-minute continuous soak with no crash —
  see "What changed most recently" below.

Plan size: 2,148 tasks across 41 group files under `plan/`, down from an original 6,380-task
AAA/open-world-scoped draft — see `plan.md`'s "Locked scope decisions" section for the ten
scope decisions that drove that cut (Mafia-1 (2002) content scope, Mafia-1-era system fidelity,
no in-house editor suite beyond Mesh Craft, manual MC3 authoring, baked lighting target, etc.).

## What changed most recently (this session)

**Gate M10 is now IN PROGRESS** (a much bigger, qualitatively different milestone than M0-M9 --
production assets/collision, baked lighting, one dynamic sun, limited shadows, audio, UI). Before
starting, did concrete research against CNA's actual source (not assumptions) to lock a feasible
architecture for each piece -- see `plan_39-vertical-slice-gates.md` `IS-39-011`'s own inline note
for the full research writeup (dual-texture lightmap math, why `BasicEffect`'s built-in lighting is
a no-op on the SOFTWARE backend, why real shadow-mapping isn't achievable without modifying CNA,
SpriteFont/SoundEffect API details). The user explicitly chose the higher-effort option for two key
decisions: a REAL lightmap texture bake (not a vertex-color approximation), and real CC0 sound
assets via WebSearch/WebFetch (not synthesized placeholder audio) -- both tracked in
`assets/licenses/asset-registry.csv`.

Planned implementation order: **UI/HUD (done, see below) -> dynamic sun + shadow decals (next) ->
baked lightmap (biggest, most novel) -> audio (needs external CC0 asset sourcing)**.

### UI/HUD done this session (`plan_28` `IS-28-001`/`002`)

- New `include/`/`src/UI/BitmapFont.hpp`/`.cpp`: `BuildBitmapFont8x8(GraphicsDevice&)` builds a
  real `Microsoft::Xna::Framework::Graphics::SpriteFont` at runtime from the public-domain
  "font8x8" bitmap font (8x8 monochrome glyphs, printable ASCII range U+0020-U+007E only --
  fetched directly from its authoritative source, https://github.com/dhepper/font8x8, Public
  Domain, recorded in `assets/licenses/asset-registry.csv`) -- CNA has no XNB font content
  pipeline, and rather than vendoring a new TTF-rasterization dependency or sourcing an external
  font asset/license, this avoids both. The bit-unpacking/atlas-layout math is pulled into its own
  GraphicsDevice-independent `BuildFont8x8AtlasPixels()` so it's headlessly unit-testable (a
  standalone ASCII-art diagnostic hand-verified the 'A'/'a'/'1' glyph bit patterns and bit order --
  bit 0 = leftmost column -- before any of this was written).
- `IronShadowsGame` gained `spriteBatch_`/`hudFont_` (both `std::optional`, constructed in
  `Initialize()`), and `Draw()` now draws a real on-screen HUD each frame (objective + driving
  speed + dialogue subtitle/prompt + cutscene skip prompt + wanted status + transient status),
  replacing the window-title-only display (the title itself stays, for window-manager/taskbar
  visibility, but is no longer the sole UI).
- New test: `TestBitmapFontGlyphAtlas` (exact bit-pattern check against 'A', independent of any
  GraphicsDevice). Verified: full `ctest` suite, `./scripts/check-syntax.sh`, and a `--smoke` run
  with no crash while the HUD drew every frame -- noted the CPU software rasterizer has a real,
  measurable per-frame text-drawing cost (a 30-frame smoke run went from ~10s to ~15s wall-clock
  after this change), not yet profiled against `docs/performance-targets.md`.
- Not done yet: menus (pause/settings/save-load/restart/quit), gamepad/rebinding, layout
  scaling/safe-area, a dedicated on-screen interaction prompt (e.g. "Press E") beyond dialogue/
  cutscene prompts -- all real `plan_28` scope, not attempted in this pass.

### Earlier this session: implemented gate M9 (traffic, pedestrians, one police-response scenario), then closed its ten-minute soak gap

Implemented gate M9 at first-pass/prototype fidelity, entirely within Iron Shadows itself.
Adopted a "kinematic movers + one shared waypoint
helper + a simplified radius-based police witness check" architecture -- the smallest slice that
proves each of the three mechanics (`plan_19`/`plan_20`/`plan_21`/`plan_22`), deliberately not the
full lane-graph/navmesh/vision-cone/multi-tier-wanted machinery those plan files describe at their
fullest scope.

- New `include/`/`src/World/WaypointPath.hpp`/`.cpp`: `WaypointPath` is a hand-authored, ordered
  list of `Vector3` points plus a `loop` flag -- not a graph, no branching, no portals. The shared
  `AdvanceAlongPath()` free function moves a mover toward its current target at a given speed,
  advancing (and wrapping, if `loop`) to the next target once within `arrivalRadius`; both
  `TrafficVehicle` and `Pedestrian` below call it as their only path-following logic. Hand-verified
  via a standalone diagnostic that starting a mover exactly AT its first waypoint (as `Reset()`
  does for both movers) immediately wraps to the second waypoint on the very first call -- this
  surprised an early version of the corresponding unit test, which had the wrong mental model.
- `PrototypeWorld::BuildWarehouseBlock()` gained `trafficLoop_` (a 4-point rectangular loop using
  both `road_north_south` lanes, X = +-3) and `sidewalkPaths_` (two 2-point back-and-forth paths
  along `sidewalk_west`/`sidewalk_east`, X = +-7.5), both new `GetTrafficLoop()`/`GetSidewalkPaths()`
  accessors. `BuildCountryside()` was NOT touched -- both stay empty there, so Countryside has no
  ambient traffic/pedestrians by design.
- New `include/`/`src/Gameplay/TrafficVehicle.hpp`/`.cpp`: a kinematic (non-Jolt) mover that
  accelerates toward a cruise speed (6 units/s^2) and brakes (12 units/s^2) toward zero when an
  externally-computed `obstacleDistanceAhead` falls inside a 10-unit braking distance / 3-unit
  minimum gap.
- New `include/`/`src/Gameplay/Pedestrian.hpp`/`.cpp`: walks a fixed sidewalk `WaypointPath` at
  1.6 units/s, overridden by a 4-second "flee directly away from the last-known threat position"
  state (2.5x speed) that keeps running off its own timer even after the threat is no longer
  reported present each frame.
- New `include/`/`src/Gameplay/PoliceSystem.hpp`/`.cpp`: a `Clear -> Dispatched -> Chasing` state
  machine. Clear: a witnessed offense (player speeding >70 km/h, or within 2.5 units of a witness,
  while any traffic-vehicle/pedestrian position is within a 15-unit "witness radius") triggers
  dispatch. Dispatched: a fixed 2-second delay before the chase actually starts. Chasing: up to 2
  patrol cars drive straight toward the player's current position (ignoring roads -- a documented
  simplification, not real route-following) at 9 units/s; a second car joins after 20 seconds still
  chasing (the one locked escalation tier); the chase resolves back to Clear once the CLOSEST
  patrol car has stayed beyond 40 units from the player for 3 sustained seconds.
- `IronShadowsGame` wiring: new `trafficVehicles_`/`pedestrians_`/`police_` members;
  `RespawnTrafficAndPedestrians()` (re)populates them from the current district's `WaypointPath`
  data and resets `police_`, called from `Initialize()`, `HandleDistrictArrival()`,
  `LoadPrototype()`, and `ResetPrototype()` (none of this ambient state is part of `SaveGame` --
  deliberately never persisted, matching the locked M9 scope). All three systems tick every frame
  gated only on `!transitioning` in `Update()` (they keep running through dialogue/cutscenes,
  matching Mafia 1's own ambient-city feel) -- a local `DistanceAheadIfInLane()` helper computes
  each `TrafficVehicle`'s obstacle distance against other traffic vehicles and the player's own
  vehicle when driving. `Draw()` gained a `renderer_.DrawTraffic(...)` call after the normal
  `Draw()`. The window title now appends "Police dispatched..."/"WANTED" while not Clear.
- `PrototypeRenderer` gained a new `ActorPose` struct and `DrawTraffic()` method, plus three new
  colored-box meshes (`trafficVehicleMesh_`, `pedestrianMesh_`, `policeCarMesh_`) distinct in shape
  and color from the player's own sedan/character meshes, so all three actor kinds stay visually
  distinguishable even as plain debug geometry.
- New tests in `tests/CoreTests.cpp`: `TestWaypointPathAdvancesAndWraps`,
  `TestTrafficVehicleAcceleratesAndBrakes`, `TestPedestrianFleesAndResumesPath`, and
  `TestPoliceSystemFullCycle` (the big one -- exercises the FULL Clear -> Dispatched -> Chasing ->
  escalate -> resolve cycle against hand-computed, standalone-diagnostic-confirmed position/timer
  values, including two negative cases: not driving, and a witness outside the radius, both
  correctly never triggering a chase).
- Verification performed: standalone diagnostics confirmed `AdvanceAlongPath`'s wrap-on-arrival
  behavior (see above) before the test was corrected to match it. Full `compile-software` rebuild
  (clean, one pre-existing unrelated warning only), all three `ctest` targets pass,
  `./scripts/check-syntax.sh` passes on every file, two short `--smoke` runs (30 and 90 draw
  frames, ~10s and ~25s wall-clock), and the gate's own "ten-minute soak" criterion: a
  `--smoke 3000` run, launched in the background and timed via `date +%s` before/after, ran for
  980 real seconds (~16.3 simulated minutes) with `trafficVehicles_`/`pedestrians_`/`police_` all
  ticking every frame throughout, and exited cleanly (exit 0, no crash, no error, no asset-fallback
  message) -- confirmed CNA's `Game::Tick()` uses a fixed 1/60s timestep accumulator fed by real
  elapsed wall-clock time (`cna/src/Microsoft/Xna/Framework/Game.cpp`), so simulated game time
  tracks real time 1:1 for this workload; there is no way to "compress" a ten-minute soak into a
  shorter wall-clock run, it genuinely has to run that long. This closes `plan_39` `IS-39-010`/
  `049` -- gate M9 is now fully done. **Still not verified**: memory-leak growth specifically over
  the soak run (only crash/stall-freedom was checked, no periodic memory sampling), and, as with
  every other visual milestone this session, there is no display to check how any of this
  actually looks.

### Earlier this session: implemented gate M8 (one in-engine cutscene)

Implemented gate M8 (one in-engine cutscene), entirely within Iron Shadows itself. Scoped down to
a camera-track-only sequence player (`plan_26-cutscenes-and-cinematic-sequencing.md`'s own IS-26-002
lists animation/dialogue/audio/event/fade tracks too, but a camera track alone is enough to satisfy
gate M8's own wording -- "play, skip, save-safe finalize, hand control back" -- without inventing
a general timeline system in one pass).

- New `CutsceneCameraKeyframe`/`CutsceneSequence` structs + `LoadCutsceneSequence`
  (`include/`/`src/Cutscenes/CutsceneSequence.hpp/.cpp`): a sequence is a named, versioned list of
  camera keyframes (time/position/lookAt), parsed with the same sharp-runtime `System::Text::Json`
  used for missions. Validation (mirroring `LoadMissionDefinition`'s inline-validation
  convention): rejects an empty keyframe list, a first keyframe not at time 0, non-strictly-
  ascending keyframe times, and a duration shorter than the last keyframe's time.
- New `CutscenePlayer` (`include/`/`src/Cutscenes/CutscenePlayer.hpp/.cpp`): `Start`/`Update`/
  `Skip`/`IsActive`/`GetCameraPosition`/`GetCameraLookAt`. `Update()` advances elapsed time and
  finishes (`IsActive()` becomes false) the instant it reaches the sequence's duration; `Skip()`
  jumps straight to the same terminal (last-keyframe) state a natural finish would produce, per
  IS-26-004's explicit requirement that skip apply the *same* terminal state, not some other
  "just stop" behavior. Camera position/look-at are linearly interpolated between the two
  keyframes bracketing the current elapsed time (`Vector3::Lerp`), clamped to the first/last
  keyframe outside their time range.
- `assets/cutscenes/prologue_intro.cutscene.json` (new directory): a 2.5-second pan from
  `(25, 12, -34)` looking at `(0, 2, -34)` (an elevated establishing shot of the warehouse
  delivery target) to `(0, 4.65, 27.5)` looking at `(0, 1.25, 20)` -- the second keyframe was
  computed BY HAND to exactly reproduce `Draw()`'s own normal-gameplay camera formula
  (`target = playerPos + (0,-0.45,0); camera = target - forward*7.5 + (0,3.4,0)`) at the player's
  actual spawn point `(0, 1.70, 20)`, yaw 0 -- so the cut back to the ordinary follow camera has
  zero visible pop, verified by a standalone diagnostic confirming the file parses to these exact
  numbers.
- `IronShadowsGame::Initialize()` starts the cutscene (loaded from file, falling back to an
  identical hardcoded sequence built inline on any load/validation failure, same convention as
  dialogue/mission) right after starting the opening dialogue. `Update()` ticks
  `cutscene_.Update(deltaSeconds)` every frame (gated only on `!transitioning`, independent of
  dialogue's own advancement pace -- the cutscene has its own fixed 2.5s duration and finishes on
  its own regardless of how fast the player clicks through dialogue lines); the existing Enter-key
  handler now checks dialogue first (unchanged) and, only if dialogue is NOT active, treats Enter
  as "skip the cutscene" instead. `Draw()` gained an `if (cutscene_.IsActive())` branch at the top
  of its camera-selection logic that overrides `camera`/`target` with the cutscene's interpolated
  values instead of the normal player/vehicle follow camera. The existing on-foot/driving
  gameplay-freeze conditions (`!dialogue_.IsActive() && ... && !transitioning`) all gained a
  `&& !cutscene_.IsActive()` clause, reusing the exact same freeze mechanism dialogue already
  established rather than inventing a second one. `LoadPrototype()` and `ResetPrototype()` both
  force `cutscene_.Skip()` defensively (save-safety, IS-26-016): nothing ever saves mid-cutscene
  by construction (it only plays for 2.5s at the very start of a fresh game), but this guarantees
  loading/resetting can never leave an active cutscene camera fighting a freshly restored,
  unrelated player/vehicle position.
- New tests in `tests/CoreTests.cpp`: `TestCutscenePlayerAdvancesAndFinishes` (fixed-time updates,
  asserts exact linear-interpolation numbers halfway through, then the exact terminal keyframe
  once finished), `TestCutscenePlayerSkipAppliesTerminalState` (Skip() partway through must
  produce the *identical* terminal camera state a natural finish would), and
  `TestCutsceneValidationRejectsMalformedData` (5 malformed-data cases + a missing file, all
  correctly rejected, plus one well-formed sequence that still loads afterward).
- Verification performed: a standalone diagnostic (not committed) confirmed the real committed
  file parses to the exact intended keyframe values before writing the corresponding test. Full
  `compile-software` rebuild (clean, no new warnings), all three `ctest` targets pass,
  `./scripts/check-syntax.sh` passes on every file, a `--smoke 20` run (covers only the first
  ~0.3 simulated seconds -- mid-cutscene) and a `--smoke 200` run (covers ~3.3 simulated seconds,
  past the cutscene's 2.5s duration, confirming it naturally finishes and the game keeps running
  normally afterward) both exit 0 with no cutscene-load fallback message. **Not verified**: the
  actual Enter-press skip path in a real running game -- smoke mode's dialogue never becomes
  inactive on its own (nothing auto-advances it), so the skip branch was only exercised by the
  standalone unit tests above, not an end-to-end `IronShadowsGame` run -- and, as with every other
  visual milestone this session, there is no display to check how the pan actually looks.

### Earlier this session: implemented gate M7 (one data-driven mission)

Implemented gate M7 (one data-driven mission), entirely within Iron Shadows itself (no cross-repo
work needed this time). Found that `assets/missions/prologue.mission.json` already existed as a
hand-written stub from the original scaffold, with a comment saying "this file defines the
intended future data-driven form" and "currently mirrored by PrototypeMission.cpp" — this session
made that comment true rather than aspirational.

- New `MissionCondition` enum + `MissionStateDefinition`/`MissionDefinition` structs +
  `LoadMissionDefinition` (`include/`/`src/Missions/MissionDefinition.hpp/.cpp`): a mission is a
  named, versioned list of states, each with an id, objective text, a named transition condition
  (`dialogue_finished`/`player_near_vehicle`/`player_driving`/`player_driving_in_warehouse_goal`,
  or none for a terminal state), and a next-state id. Parsed with sharp-runtime's own
  `System::Text::Json` (`JsonDocument`/`JsonElement`, backed by vendored `nlohmann::json`) —
  already a linked dependency via `SHARP_RUNTIME`, discovered by grepping for an existing JSON
  library before considering hand-rolling a parser (matching this session's established "reuse
  existing sharp-runtime/CNA machinery" habit).
- Deliberately NOT a general condition/action expression language: conditions are a small, fixed,
  engine-evaluated set by name (`plan_24-mission-framework-and-scripting.md`'s own explicit
  non-goal — "no embedded Lua VM, no script API surface to secure").
- Inline validation (not a separate tool/script, matching "smallest coherent slice"): rejects
  duplicate state ids, an `initialState`/`next` that doesn't match any state id, an unrecognized
  condition name, and an empty state list, each with an actionable error message.
- `assets/missions/prologue.mission.json` updated from its old stub (just `id`/`objective` per
  state, no transitions) to the real, active definition: the exact same 5-state flow
  (introduction → reach_vehicle → enter_vehicle → drive_to_warehouse → completed) the hardcoded
  C++ already implemented.
- `PrototypeMission` (`include/`/`src/Missions/PrototypeMission.hpp/.cpp`) rewritten to look up
  its current state's objective text and transition condition against a `MissionDefinition`
  instead of a hardcoded switch statement. Kept `PrototypeMissionState` as a fixed enum
  (Introduction/ReachVehicle/EnterVehicle/DriveToWarehouse/Completed) purely for `SaveGame`'s
  existing int-based `mission_state` field and public-API compatibility — a bidirectional
  enum↔state-id mapping bridges the two; `LoadMission()` rejects any mission file introducing a
  state id outside that fixed set of 5. Ships with a hardcoded default (constructor-initialized,
  identical to the original switch-based flow) so existing callers/tests that never call
  `LoadMission()` — including the pre-existing `TestMissionFlow` — keep working completely
  unchanged, matching `DialogueSystem::LoadFallbackPrologue()`'s "never fully fail" convention.
- `IronShadowsGame::Initialize()` calls `mission_.LoadMission(assetRoot_ + "/missions/prologue.mission.json", error)`
  right before `mission_.Reset()` (moved after the load, so `Reset()` picks up whichever
  definition — loaded file or fallback — ended up active), printing a fallback message on failure
  the same way dialogue loading already does.
- New tests in `tests/CoreTests.cpp`: `TestMissionLoadsCommittedFile` (loads the real committed
  file via a new `IRON_SHADOWS_SOURCE_ASSET_DIR` compile definition — added to both the CMake test
  target and `scripts/check-syntax.sh` — and drives it through the identical flow
  `TestMissionFlow` already proves against the hardcoded default) and
  `TestMissionValidationRejectsMalformedData` (6 cases: dangling `next`, dangling `initialState`,
  duplicate id, unknown condition, empty states, missing file — all correctly rejected — plus one
  well-formed minimal mission that still loads correctly afterward, proving a run of failures
  doesn't corrupt the loader's own state).
- Verification performed: a standalone diagnostic (not committed) confirmed the real committed
  file parses to the exact expected 5-state structure before writing the test. Full
  `compile-software` rebuild (clean, no new warnings), all three `ctest` targets pass,
  `./scripts/check-syntax.sh` passes on every file, and a `--smoke 20` run exits 0 with **no**
  mission-load fallback message printed (confirming the real file loads successfully at runtime,
  not just in the standalone test/diagnostic). **Not verified**: save/load resuming correctly
  mid-mission with the new data-driven system was reasoned through (the int-enum save format and
  `SetState()`/`GetState()` are byte-for-byte unchanged) but has no new dedicated test, and there
  is no display to check the objective-text window title actually updates on screen.

### Earlier this session: implemented gate M6 (one skinned character)

Implemented gate M6's first proving slice (one skinned character), after asking the owner two
consolidated architecture questions first (both authorized): (1) hand-author a rigged glTF and
bypass MC3 entirely for this one asset, since Mesh Craft has no rigging/skinning authoring support
at all (confirmed by checking `mc3.xsd` for any skin/bone/joint concept and finding none); (2)
extend the `cna-extended` sibling repo with a real `Model`/`AnimationPlayer` ECS wrapper, rather
than reusing the existing Avatar-specific path or building a purely-local Iron Shadows-side
workaround.

**In `cna-extended`** (commit `1c91d75`, its own repo, own git history — see that repo's own
`NEXT.md` for the full writeup):

- New `ModelAnimationComponentEXT` (`include/CNA/Extended/World3DEXT/ModelAnimationComponentEXT.hpp`):
  a non-owning `Model*` + an owned `AnimationPlayer` (built from the model's own `SkinningData`,
  retrieved via `model.getTagProperty()`) + a `ClipNameEXT` string. No separate `EffectEXT` field:
  cna's glTF/CNJ importer already assigns a real `SkinnedEffect`/`SkinnedPbrEffect` to every
  skinned `ModelMeshPart` at load time.
- New `ModelAnimationSystem3DEXT` (an `EntityUpdateSystem`): each frame, starts `ClipNameEXT` from
  the beginning if it names a different clip than the one currently playing (**a hard cut, no
  blending** — not in either animation system in cna-extended today), otherwise advances
  `AnimationPlayer::Update()`.
- `RenderSystem3DEXT::Draw()` gained a third branch: pushes `PlayerEXT.GetSkinTransforms()` onto
  every `SkinnedEffect`/`SkinnedPbrEffect` found across the model's meshes, then calls the
  *existing* `Model::Draw(world, view, projection)` — no hand-rolled draw loop needed (unlike
  `SkinnedModelComponentEXT`, since `SkinnedModelEXT` has no `Draw()` of its own but a real `Model`
  does).
- Tests reuse cna's own proven-good `kSkinnedAnimatedGltf` fixture (`RuntimeGltfModelTests.cpp`,
  a 2-bone skinned/textured/animated triangle) verbatim rather than authoring a new one: 3 new unit
  tests (clip start/switch/hard-cut/unknown-name) plus a real pixel-level render test. Full
  `cna-extended` suite re-run clean: 2367 tests, 1 unrelated pre-existing parallel-run flake
  (`TexturePackerFileReaderTests`, passes solo).

**In Iron Shadows**:

- `assets/source/gltf/gen_test_character_gltf.py` (new) generates `assets/source/gltf/test_character.gltf`:
  a blocky 3-bone/3-box test humanoid (Root/LeftLeg/RightLeg, torso+head box rigid to Root, one
  leg box per side rigid to its own hip-pivot bone), hand-authored directly as glTF/base64 buffers
  via a Python script (not by hand-editing JSON — too error-prone for binary buffer offsets), with
  "Idle" (static) and "Walk" (alternating leg-swing) clips. Converted via `cna_tool_gltf_to_cnj` to
  `assets/generated/models/cnj/test_character.cnj` (3 mesh parts, 3 bones, 2 clips).
  **First attempt had no material/texture at all and crashed the game at runtime**
  (`SoftwareGraphicsBackend::DrawIndexedPrimitivesEx: TextureEnabled=true but texture0 is null`) —
  a skinned mesh with no material still gets `TextureEnabled=true` from cna's importer, which then
  needs a real bound texture. Fixed by adding a trivial 1x1 white PNG (same bytes cna's own test
  fixtures use) to every primitive's material.
- `PrototypeRenderer` gained `characterModel_`/`characterWorld_`/`characterAnimComponent_` and two
  new methods: `Initialize(...)`'s new `characterModel` parameter builds a minimal, dedicated
  `CNA::Extended::ECS::World` (just `ModelAnimationSystem3DEXT`, one entity, one
  `ModelAnimationComponentEXT`) so the *real* new cna-extended classes actually drive animation
  state — not a hand-rolled reimplementation of the same logic. `UpdateCharacterAnimation(deltaSeconds,
  clipName)` sets `ClipNameEXT` and ticks that ECS world; call once per frame from gameplay
  `Update()`, not `Draw()` (`Draw()` has no time step of its own). `Draw()`'s player branch draws
  the character via a direct `Model::Draw()` call (after pushing bone transforms onto its
  `SkinnedEffect`s by hand) — matching the *existing* pattern `warehouseModel_`/`vehicleModels_`
  already use, **not** `RenderSystem3DEXT`/`Camera3DEXT`: Iron Shadows has no other ECS-driven
  rendering to justify bridging its own view/projection into `Camera3DEXT` when it already has
  the exact matrices ready to use.
- `IronShadowsGame::Initialize()` loads `test_character.cnj` the same try/catch way as
  warehouse/vehicle (procedural-box fallback on failure). `Update()`'s on-foot branch computes
  `playerIsMoving` from the current frame's `OnFootInput` and calls
  `renderer_.UpdateCharacterAnimation(deltaSeconds, playerIsMoving ? "Walk" : "Idle")` — a hard
  cut between the two, no blend.
- Verification performed: a standalone diagnostic program (not committed) loaded the CNJ through
  `ContentManager`/`AnimationPlayer` directly and confirmed the "Walk" clip's leg-swing numbers
  match hand-derived pivot-rotation math (`y≈0.084`, `z≈±0.380` at the swing extremes) — the legs
  really do swing in opposite phase around the hip joint, not just "move some amount". Full
  `compile-software` rebuild (clean), all three `ctest` targets pass, `./scripts/check-syntax.sh`
  passes on every file, and a `--smoke 30` run exits 0 with `[IronShadows] Loaded generated
  test_character.cnj` logged. **A scary-looking apparent hang during this verification turned out
  to be real system load, not a bug**: this machine is shared with several other concurrent
  Claude/Codex sessions (`load average` ~5 on 16 cores at the time), and the CPU software
  rasterizer got visibly slower per frame under that contention — confirmed by running longer and
  observing continued forward progress (236 frames in 60 real seconds) rather than a stuck call,
  and by reproducing the same apparent slowdown with the character-drawing code fully stubbed out
  to a no-op (so it could not have been the cause). If a future session sees `--smoke` runs that
  seem to hang, check `uptime`/`ps aux --sort=-%cpu` for contention before assuming a regression.
- **Explicitly not done** (see `plan/plan_18-characters-skeletons-and-animation.md`/`plan_39`'s
  gate M6 entry for the full itemized list): a dialogue-pose clip (dialogue leaves
  the character in whatever locomotion clip it was already playing), vehicle entry/exit animation
  (the character just is not drawn while driving, matching the box it replaced), a general
  named-bone skeleton convention (this rig is a one-off, not reusable), and any interactive/visual
  check of how the character actually looks or moves — this environment has no display.

**Follow-up in the same session: clip blending added.** The paragraph above originally shipped
with a hard cut only; blending was added right after (`cna-extended` commit `2ff3cff`, iron-shadows
same commit as the rest of this M6 work). `ModelAnimationComponentEXT` gained `BlendDurationEXT`
(seconds, default 0.25; 0 = hard cut), `BlendFromSkinTransformsEXT` (a frozen outgoing-pose
snapshot), and `BlendedSkinTransformsEXT` (the actual per-frame output — `RenderSystem3DEXT` and
Iron Shadows' `PrototypeRenderer` both read this now, not `PlayerEXT.GetSkinTransforms()`
directly). `ModelAnimationSystem3DEXT::Update()` snapshots the outgoing pose the instant
`ClipNameEXT` changes to a different clip, then writes a per-bone `Matrix::Lerp` between that
snapshot and the new clip's live pose each frame. `Matrix::Lerp` (matching real XNA's own
`Matrix.Lerp`) is a simple per-component interpolation, not true rotation-aware blending — a
documented simplification, adequate for a short crossfade between similar poses like Idle/Walk,
not a substitute for a real blend-space/layered system. Verified: 2 new `cna-extended` tests
(a hand-verified two-named-clip glTF fixture, proving the blend math numerically at
t=0/halfway/finished, plus a `BlendDurationEXT=0` hard-cut case) — full suite 2369/2369, including
the previously-flaky `TexturePackerFileReaderTests` case now passing (confirms that was a
parallel-run isolation flake, not a regression). In Iron Shadows: full rebuild, all `ctest`
targets pass, `check-syntax.sh` clean, `--smoke 20` exits 0. Not verified: how the crossfade
actually looks, since this environment has no display.

**Second follow-up in the same session: a dialogue-pose clip added.** `gen_test_character_gltf.py`
gained a third clip, "Dialogue" — a static "parade rest" leg stance rotated about the local Z axis
(a different axis than Walk's X-axis swing, so it reads as a genuinely distinct pose, not a frozen
mid-walk frame), regenerated into `test_character.cnj` (now 3 clips: Idle/Walk/Dialogue).
`IronShadowsGame::Update` calls `renderer_.UpdateCharacterAnimation(deltaSeconds, "Dialogue")`
whenever `dialogue_.IsActive() && !playerDriving_ && !transitioning` — placed right after the
existing gameplay-update block (which only runs the Walk/Idle call when dialogue is NOT active,
so the two calls never race for the same frame). Verified: a standalone diagnostic (not
committed) confirmed the Dialogue pose's left-foot position matches hand-derived pivot-rotation
math exactly (`(-0.0747, 0.00876, 0)` at an 8° Z-axis rotation about the hip pivot). Full rebuild,
all `ctest` targets pass, `check-syntax.sh` clean, `--smoke 20` exits 0 — and since smoke mode
never dismisses the opening dialogue, this run exercised the new Dialogue clip continuously for
its whole duration, a stronger check than the Idle/Walk clips get (those only run once dialogue
ends, which smoke mode never triggers). Not verified: how the pose actually looks, since this
environment has no display.

**Gate M6 status after both follow-ups**: locomotion (with blending) and dialogue pose were done;
vehicle entry/exit animation was the one remaining piece -- **also completed as a third
same-session follow-up, gate M6 is now fully done** (at prototype fidelity; see the entry
directly below).

### Third follow-up in the same session: vehicle entry/exit animation added

`gen_test_character_gltf.py` gained two more one-shot clips: "EnterVehicle" (standing -> sitting,
both legs bending forward *together* via `quat_x`, unlike Walk's alternating phase) and
"ExitVehicle" (the reverse). Each is authored as a 1-second clip but only ever played for 0.5s
(`IronShadowsGame::kVehicleTransitionSeconds`) — deliberately so the motion is still visibly *in
progress*, not already at its end pose, at the moment the game switches away, and so `LoopEXT`'s
default-true modulo wraparound (the same "boundary" gotcha noted in
`ModelAnimationSystem3DEXTTests.cpp`) never has a chance to trigger. `test_character.cnj`
regenerated with 5 clips total (Idle/Walk/Dialogue/EnterVehicle/ExitVehicle).

Added a small `VehicleTransitionState` (`None`/`Entering`/`Exiting`) enum + two new
`IronShadowsGame` fields (`vehicleTransitionState_`, `vehicleTransitionSecondsRemaining_`).
`HandleInteraction()`:
- Entering: instead of instantly flipping `playerDriving_` true, starts `Entering` and keeps the
  character visible/on-foot (still `drawPlayer = !playerDriving_ == true`) while "EnterVehicle"
  plays; `playerDriving_` only flips true (hiding the character) once the clip finishes, in the
  new `Update()` tick below.
- Exiting: flips `playerDriving_` false *immediately* (character becomes visible right where the
  car is) and starts `Exiting` while "ExitVehicle" plays — nothing left to flip once it finishes.
- Ignores a new interaction entirely while `vehicleTransitionState_ != None` (no double-triggering
  mid-clip).

`Update()` gained a small tick block (`!transitioning && vehicleTransitionState_ != None`): plays
the relevant clip via `renderer_.UpdateCharacterAnimation`, counts down
`vehicleTransitionSecondsRemaining_`, and on reaching zero either flips `playerDriving_` true
(Entering finishing) or does nothing further (Exiting finishing, already flipped) before resetting
the state to `None`. The existing on-foot `player_.Update()`/Walk-Idle call is now gated behind
`vehicleTransitionState_ == None` too (suppressed mid-clip, matching how dialogue already freezes
movement), and `CheckDistrictExit()` is skipped mid-clip as a defensive guard (position doesn't
actually change during the 0.5s window since input is suppressed, but avoids stacking an unrelated
district transition on top of an in-progress vehicle one regardless). `LoadPrototype()`/
`ResetPrototype()` both reset `vehicleTransitionState_` to `None` for safety (no mid-clip state is
ever saved).

Verified: two standalone diagnostics (not committed) loaded the regenerated CNJ and confirmed both
clips' foot-position math at t=0/0.5s matches hand-derived pivot-rotation values exactly (e.g.
`EnterVehicle@0.5s` and `ExitVehicle@0.5s` both land at the same halfway pose, as expected by
symmetry — a real, independent numeric cross-check, not just "it compiled"). Full rebuild, all
`ctest` targets pass, `check-syntax.sh` clean, `--smoke 20` exits 0. **The state machine itself
(as opposed to the underlying clip math) was verified by careful manual code review of
`HandleInteraction()`/`Update()`'s new branches, tracing through both the Entering and Exiting
paths frame-by-frame on paper — not by an automated test.** Smoke mode never presses 'E', and
`IronShadowsGame` is not unit-testable headlessly today (it's a real `Game` subclass, not a
pure-logic class like `PlayerController`/`DistrictManager`), so there was no way to exercise the
actual interaction flow in this environment. **First priority if a display/interactive input ever
becomes available**: walk up to the sedan, press E, and watch whether the enter/exit animation
and the `playerDriving_` handoff actually look and feel right — this is the single least-verified
piece of the entire M6 body of work.

### Earlier this session: implemented gate M5 (second district)

- `include/IronShadows/Core/WorldTypes.hpp` gained `DistrictId` (`WarehouseBlock`, `Countryside`)
  and `DistrictExit` (a `TriggerZone` plus the target district id/entry position/entry yaw).
- `PrototypeWorld` is now district-parameterized: its constructor takes a `DistrictId` (defaults to
  `WarehouseBlock`), `BuildCityBlock()` was renamed `BuildWarehouseBlock()` and now also sets
  `districtExit_` (a trigger behind the map pointing to `Countryside`), and a new
  `BuildCountryside()` builds a distinct farmland district (ground, dirt road, barn, farmhouse,
  silo, fence posts) with its own spawn points and its own exit back to `WarehouseBlock`.
  `BuildPhysicsStaticBodies()` now returns the created body handles (`[[nodiscard]]`) instead of
  discarding them, so a caller can destroy them later.
- New `IronShadows::DistrictManager` (`include/`/`src/World/DistrictManager.cpp`) owns the current
  `PrototypeWorld` and its static body handles. `RequestTransition()` swaps to the current
  district's own exit target (destroy old bodies → construct new `PrototypeWorld` → rebuild
  bodies, all synchronous) and starts a 0.6s minimum-display-time loading-screen timer;
  `LoadDistrict(id)` is a direct-load bypass used by save/load restore and the reset key;
  `ConsumeArrival()` reports true exactly once when the timer elapses.
- `PrototypeRenderer` gained `RebuildStaticGeometry()` (extracted from `Initialize()`'s static-mesh
  loop) so a district swap only rebuilds the static city mesh, not vehicle/player meshes or CNJ
  content.
- `SaveSnapshot`/`SaveGame` gained a `districtId` field (additive text key, no format version
  bump; defaults to `WarehouseBlock` if absent so old saves still load).
- `IronShadowsGame` replaced its raw `PrototypeWorld world_` with `districtManager_`, added
  `CheckDistrictExit()` (tests the player/vehicle position against the current district's exit
  trigger and calls `RequestTransition`) and `HandleDistrictArrival()` (repositions player/vehicle
  at the new district's spawn, re-snaps the player onto the vehicle if they were driving, rebuilds
  renderer geometry). Gameplay update/mission logic is gated behind `!IsTransitioning()`; `Draw()`
  shows a plain dark clear (no 3D scene) while transitioning; `SavePrototype()`/`LoadPrototype()`/
  `ResetPrototype()` all go through `DistrictManager` now.
- `PhysicsWorld` gained `GetBodyCount()` (wraps `JPH::PhysicsSystem::GetNumBodies()`) — test/
  diagnostic only, for leak detection across a district swap.
- New test `tests/CoreTests.cpp::TestDistrictTransition` (the `IS-13-042` task): does two full
  round trips (warehouse → countryside → warehouse → countryside) and asserts the district id
  after each transition, the loading screen's timing/one-shot `ConsumeArrival()` behavior, and
  that the physics body count returns to exactly its prior value every time the *same* district is
  revisited. (First draft of this test asserted equal body counts after a one-way swap, which is
  wrong since the two districts have different numbers of static bodies — caught immediately by
  the test itself failing, then corrected to only compare counts on same-district revisits.)
- Verification performed: full `compile-software` rebuild (clean, no new warnings), all three
  `ctest` targets pass, `./scripts/check-syntax.sh` passes on every file, and
  `./cmake-build-compile-software/iron_shadows --smoke 120` exits 0 with correct asset-loading
  logs. **Not verified**: actually walking/driving through an exit trigger interactively (no
  display access in this environment), or how the loading screen looks beyond a dark clear +
  "Loading..." window title.

### Earlier this session: wiring PhysicsWorld into gameplay (gate M4 follow-up)

Wired `PhysicsWorld` into actual gameplay (previously it was a proven-but-standalone M4
prototype):

- `PlayerController` owns a `Physics::CharacterHandle` (a `JPH::CharacterVirtual` capsule,
  radius 0.35, cylinder half-height 0.5). `Update()` keeps its existing yaw/turn/sprint/direction
  math unchanged and hands the resulting desired velocity to `PhysicsWorld::MoveCharacter`, then
  reads position/grounded back. `Reset()`/`SetPosition()`/`SetYaw()` now take a
  `Physics::PhysicsWorld&` and teleport the physics capsule.
- `VehicleController` owns a `Physics::VehicleHandle` (chassis half-extents 1.05x0.325x2.1
  matching `PrototypeRenderer`'s body box; 4 wheels, radius 0.33, matching the existing wheel
  local offsets). `Update()` translates `VehicleInput` into `SetVehicleInput` (mirroring Jolt's
  own `VehicleConstraintTest` sample's forward/brake-on-direction-reversal logic) and reads
  position/yaw/speed back from physics instead of simulating them itself.
- `PrototypeWorld::BuildPhysicsStaticBodies()` builds real Jolt static bodies from the world's
  existing `colliders_` list, called once in `IronShadowsGame::Initialize()`.

**Two real bugs found and fixed during this migration** (not hypothetical — confirmed via a
throwaway diagnostic program and via `ctest` failures before the fix):

1. The render-only ground box in `PrototypeWorld::BuildCityBlock()` is deliberately
   `collidable=false` (so the unrelated XZ-only `CanOccupy()`/`ResolveHorizontalMotion()` checks,
   still used for the vehicle-exit safe-position check, don't reject the entire map). This meant
   `BuildPhysicsStaticBodies()` never created a floor at all — the vehicle/character had nothing
   to stand on. Fixed with a dedicated `groundCollider_` member set alongside the render box,
   independent of `colliders_`, and physics now creates one extra static body for it.
2. Iron Shadows' own `ForwardFromYaw(yaw)` convention (`WorldTypes.hpp`; local -Z is "forward" at
   yaw 0, matching how the CNJ sedan parts are authored — cabin/windshield/front-wheels all sit at
   negative local Z) does not match Jolt's default vehicle-forward axis `(0,0,1)`. Fixed by
   setting `VehicleConstraintSettings::mForward` and each `WheelSettingsWV::mWheelForward` to
   `(0,0,-1)`, and negating the angle `GetVehicleYaw()` extracts from Jolt's rotation. **Verified
   empirically**, not just derived on paper: `tests/PhysicsTests.cpp`'s `TestVehicleDrivesForward`
   asserts a vehicle created with identity rotation reports ~0 yaw and that forward throttle moves
   it toward -Z (matching `ForwardFromYaw(0) == (0,0,-1)`), not just "moved some distance".

New tests: `tests/CoreTests.cpp` gained `TestPlayerMotion` (walks forward from spawn; separately,
walks straight at the hotel's static collider for 5 simulated seconds and asserts no tunneling).
`TestVehicleMotion` was updated to build real physics static bodies and drive on them.

All three `ctest` targets pass: `iron_shadows_core_tests`, `iron_shadows_missing_asset_fallback`,
`iron_shadows_physics_tests`.

**Explicitly not verified — no display/interactive input in this environment:**

- Actual driving/handling *feel* — Jolt's default `VehicleEngineSettings`/`VehicleTransmissionSettings`
  (500 Nm max torque, 6000 RPM, 5-speed auto, default suspension spring/damper) are used as-is,
  not retuned for this sedan's mass/size. Might feel too fast, too slow, too twitchy, or too
  floaty. **First thing to check with a real display.**
- Visual alignment: does the rendered warehouse/sedan CNJ model line up with where the physics
  bodies actually are? Does the player's on-foot body mesh (drawn at `playerPosition + (0,-0.95,0)`,
  an offset tuned for the old fixed-Y system) look right sitting on the new physics-driven capsule
  center position? Camera framing (`target = player_.GetPosition() + (0,-0.45,0)` etc.) also
  unverified against the new position semantics.
- Steering feel (turn sharpness, whether "left" visually turns left) — the yaw-convention fix was
  verified for straight-line forward/backward direction only, not for turning under steering
  input.
- Vehicle spawn (`vehicleSpawn_` Y=0.65) and player spawn (`playerSpawn_` Y=1.70) are old
  constants tuned for the pre-physics fixed-height system; physics will now pull them down to
  their real resting height over the first fraction of a second after gameplay starts (dialogue
  gates `Update()`'s physics-stepping branches, so nothing falls *during* the opening dialogue,
  only once it ends). Diagnostic testing showed the vehicle settles to roughly Y≈0.84 with all 4
  wheels in contact — a ~0.19-unit initial drop, likely unnoticeable, but not visually confirmed.

## Known architectural notes for future sessions

- `PhysicsWorld::Step()` must be called exactly once per game frame. It currently is, because
  `IronShadowsGame::Update()` only ever calls one of `PlayerController::Update()` /
  `VehicleController::Update()` per frame (mutually exclusive on `playerDriving_`), and each of
  those calls `physics.Step(deltaSeconds)` once internally. **If a future change makes both
  callable in the same frame, physics would step twice — restructure to a single explicit
  `physics_.Step()` call in `IronShadowsGame::Update()` instead before that happens.**
- `JPH::CharacterVirtual::Update()` (the non-Extended variant, which is what `PhysicsWorld::Step()`
  calls) does **not** auto-accumulate gravity into the character's own velocity — only the
  caller-supplied `SetLinearVelocity()` value is used for movement each step; gravity is only used
  internally for the ground-reaction-impulse calculation. `PlayerController::Update()` works around
  this with a small constant downward bias (`Vector3(0,-4,0)` added to desired velocity) since this
  prototype has no jump mechanic. If jumping is ever added, this needs a real accumulated
  vertical-velocity state machine (see Jolt's `ExtendedUpdate()` doc comments for the intended
  pattern), not just a bigger constant.
- Dynamic rigid bodies (the vehicle chassis) DO get normal Jolt gravity/integration on every
  `physicsSystem.Update()` call regardless of which controller triggered it, since `Step()`
  advances the whole `PhysicsSystem`, not just one body. So the parked vehicle settles under
  gravity even while the player is walking (not driving) and vice versa — this was relied upon,
  not accidental.
- No debug renderer/wireframe overlay exists yet (`plan_15` `IS-15-029`) — if handling feels wrong
  once visually tested, that would be the fastest way to see suspension/contact/capsule state.

## Recommended next session starting point

**First priority if a display/interactive input becomes available:** actually play the game
(`./scripts/run.sh compile-software` or a real backend preset) and check the items in "Explicitly
not verified" above. Camera offsets, body-mesh offset, Jolt's default vehicle tuning, the loading
screen (behind the map at roughly `(0,0.5,-47)`), the skinned test character's look/scale/
Idle-Walk switching (walk forward and watch the legs alternate), the Dialogue pose, walking up to
the sedan and pressing E to watch the enter/exit animation and `playerDriving_` handoff, and —
**especially** — the new intro cutscene's camera pan and whether pressing Enter mid-cutscene
(after dialogue has finished) actually skips it cleanly are all unverified visually — this
environment has no display, so everything above has only ever been checked via assertions/logs,
never by actually seeing it happen.

**Gates M6, M7, M8, and M9 (`plan/plan_39-vertical-slice-gates.md` `IS-39-007`/`008`/`009`/`010`)
are all now fully done at prototype fidelity**, including M9's own literal "ten-minute soak"
wording (verified via a 980-second/~16.3-minute `--smoke 3000` background run with no crash) —
see the entries above for each. Remaining sub-tasks in `plan_18` (a real named-bone skeleton
convention, layered animation/bone masks, IK, root motion, sit/drive/steer poses while actually
driving), `plan_24` (typed mission variables, a fuller condition/action set, failure/retry
policies, checkpoints beyond plain save/load, the campaign graph), `plan_26` (animation/dialogue/
audio/event/fade tracks beyond the camera-only track, a timeline debug overlay), and
`plan_19`/`plan_20`/`plan_21`/`plan_22` (real lane graph/signals, real vision-cone witness
perception, 10-20 pedestrians instead of 2, 3-5 traffic vehicles instead of 2, local avoidance,
route-following patrol cars, save/load persistence of NPC/wanted state, debug views — see each
file's own "Gate M9 status" note) are real but not gate-blocking — see each file for its itemized
list.

**If continuing headless/autonomous work, gate M10 is already in progress** (see "What changed
most recently" above for the full locked architecture) — continue in this order:

1. **Dynamic sun + limited shadows** (next): a single shared sun direction/intensity applied as a
   CPU-computed per-actor brightness scalar (NOT `BasicEffect`'s built-in `DirectionalLight0` --
   confirmed a no-op on the SOFTWARE backend, see `plan_39` `IS-39-011`'s research note) for
   dynamic actors (player/vehicle/traffic/pedestrians/police); a simple dark, semi-transparent
   ground decal beneath the player and vehicle for "shadows" (confirmed AlphaBlend IS implemented
   on the SOFTWARE backend) -- a period-appropriate "blob shadow," not real shadow-mapping
   (confirmed not achievable without modifying CNA itself).
2. **Baked lightmap** (biggest, most novel): `DualTextureEffect` (confirmed fully implemented on
   the SOFTWARE backend, formula `finalColor = vertexColor * (texture0*2) * texture1 * diffuse`,
   both textures sampled at the SAME uv) with `texture0` = a flat 50%-gray 1x1 texture (so
   `texture0*2` = identity) and `texture1` = a lightmap atlas baked IN-PROCESS (no external tool,
   no JSON round-trip) directly from `PrototypeWorld::GetBoxes()` at renderer build time -- one
   flat-shaded tile per box face, uploaded via `Texture2D(device,w,h)` + `SetData()`. Needs a new
   vertex format for the static city mesh (`VertexPositionColorTexture`, already exists in CNA)
   carrying one UV per vertex into the atlas. Scope: the procedural box city only, not MC3-sourced
   models (no lightmap UV channel in that pipeline yet).
3. **Audio** (needs external CC0 asset sourcing via WebSearch/WebFetch, tracked in
   `assets/licenses/asset-registry.csv` per the user's explicit choice): `SoundEffect::FromStream`
   (confirmed decodes WAV/OGG/MP3 directly via SDL3_mixer's `MIX_LoadAudio_IO`, no XNB conversion
   needed) + `SoundEffectInstance`/`CreateInstance()` for looped ambience/engine hum, `Play()` for
   one-shots (footstep/horn/siren). Wrap in try/catch for `NoAudioHardwareException` (confirmed
   this environment may have no audio device), matching the established optional-asset fallback
   convention.

This is a good point to also revisit the user's own concrete feedback earlier this session
("doesn't look like Mafia 1") once the lightmap/sun/shadow pieces land — M10 is exactly the
milestone where visual fidelity work is in scope, unlike M0-M9's deliberately placeholder-grade
debug-box rendering.

Other open items worth picking up opportunistically (not blocking, not sequenced):

- `plan_26` `IS-26-002` (animation/dialogue/audio/event/fade tracks beyond the camera-only track
  this session added), `IS-26-018` (a timeline debug overlay).
- `plan_24` itself (see above) — typed mission variables, richer conditions/actions, failure/retry,
  a real checkpoint/retry system distinct from plain save/load, the campaign dependency graph.
- `plan_18` `IS-18-001` (a real named-bone skeleton convention, not this one-off test rig),
  `IS-18-006`/`007`/`008` (sit/drive/steer poses while actually driving, bone masks, additive
  layers), `IS-18-034` (report an unknown clip name instead of silently holding the last pose),
  `IS-18-037` (persist animation-clip state in `SaveGame` once clips stop being purely
  input-derived).
- `plan_13` `IS-13-014` (loading-screen progress feedback — `DistrictManager::GetTransitionProgress()`
  exists but isn't drawn yet), `IS-13-016` (fade instead of hard cut), `IS-13-022`/`023` (per-district
  mutable world state — no doors/pickups/NPCs exist yet to need this), `IS-13-034`/`035`
  (background/async district loading), `IS-13-044` (a real many-iteration soak test, not just the
  two round trips `TestDistrictTransition` currently does).
- `plan_15` `IS-15-006`/`007` (deferred body creation, sleep/activation tuning), `IS-15-009`/`010`
  (real MC3-attribute-driven collision role + layer/mask system, currently everything is one
  "static box" treatment), `IS-15-022` (steps/slopes/stairs).
- `plan_39` `IS-39-028` (standalone GLB validation step), `IS-39-032` (collision derived from MC3
  `collision` attribute instead of the separate procedural AABB — needs the sidecar/MCB metadata
  compiler from `plan_10` `IS-10-001`/`002`).
- The upstream `cna_tool_gltf_to_cnj` node-transform-baking gap (`IS-10-004b`) — currently worked
  around in Iron Shadows by hand-composing multi-part props; a real fix belongs in the `cna`
  sibling repo, out of scope unless explicitly asked to cross into that repo.

## Useful commands

```bash
./scripts/preflight.sh compile-software      # verify CNA/sharp-runtime/cna-extended/jolt/mesh-craft
./scripts/check-syntax.sh                    # fast syntax-only pass over every .cpp
cmake --preset compile-software && cmake --build --preset compile-software   # -j4, ccache
ctest --preset compile-software --output-on-failure
./cmake-build-compile-software/iron_shadows --smoke 60     # headless-safe smoke run
./cmake-build-compile-software/iron_shadows_physics_tests  # standalone physics prototypes
```

Jolt lives at `~/deps/jolt` (shared checkout, not a repo sibling) — clone with
`git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt` if
missing. Build directories (`cmake-build-*`) are persistent and gitignored; reuse them, don't
delete and recreate. Cap all builds at `-j4` (already the case in `CMakePresets.json`) per
`CLAUDE.md`'s SSD/RAM guidance.
