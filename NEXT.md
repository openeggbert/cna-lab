# NEXT.md — Iron Shadows continuity document

Primary short-term continuity doc for autonomous/resumed work sessions. Read this before
`plan.md`/`plan/plan_NN-*.md`. Update it whenever project state changes materially — do not wait
until the end of a session to reconstruct it from memory.

## Where things stand (as of this entry)

Repo: `/rv/data/development/github.com/openeggbert/iron-shadows`, branch `develop` (not `master` —
someone switched branches outside this session at some point; both exist, `develop` is ahead).
**No remote configured, nothing pushed** — commit locally only until explicitly told otherwise.

Gates M0-M5 (see `plan.md` milestone order) are done; M6 has its first proving slice done but is
not fully done (see below):

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
- M6 (partial): one skinned test character plays "Idle"/"Walk" clips, replacing the procedural
  player box. Required extending the `cna-extended` sibling repo (owner's explicit go-ahead) with
  a new ECS component/system pair, since it had no wrapper for the skinned-playback path cna's own
  glTF/CNJ tools actually populate. Blending, dialogue pose, and vehicle entry/exit animation are
  NOT done — see "What changed most recently" below.

Plan size: 2,148 tasks across 41 group files under `plan/`, down from an original 6,380-task
AAA/open-world-scoped draft — see `plan.md`'s "Locked scope decisions" section for the ten
scope decisions that drove that cut (Mafia-1 (2002) content scope, Mafia-1-era system fidelity,
no in-house editor suite beyond Mesh Craft, manual MC3 authoring, baked lighting target, etc.).

## What changed most recently (this session)

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
  gate M6 entry for the full itemized list): clip blending, a dialogue-pose clip (dialogue leaves
  the character in whatever locomotion clip it was already playing), vehicle entry/exit animation
  (the character just is not drawn while driving, matching the box it replaced), a general
  named-bone skeleton convention (this rig is a one-off, not reusable), and any interactive/visual
  check of how the character actually looks or moves — this environment has no display.

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
screen (behind the map at roughly `(0,0.5,-47)`), and now the skinned test character's look/scale/
Idle-Walk switching (walk forward and watch the legs alternate) are all unverified visually — this
environment has no display, so everything above has only ever been checked via assertions/logs,
never by actually seeing it happen.

**If continuing headless/autonomous work, gate M6 (`plan/plan_39-vertical-slice-gates.md`
`IS-39-007`) still has real work left before it's fully done** — see `plan/plan_18-characters-skeletons-and-animation.md`
for the itemized list, but the three biggest pieces are:
1. **Clip blending** — neither `AnimationSystem3DEXT` (Avatar path) nor the new
   `ModelAnimationSystem3DEXT` (added this session, general `Model`/`AnimationPlayer` path) support
   it; both only do a hard cut. This is real work in the `cna-extended` sibling repo (cross into it
   again with the same kind of explicit go-ahead this session got, or ask first) — likely a linear
   blend between two `AnimationPlayer`-computed pose sets over a short window, driven by
   `ModelAnimationComponentEXT` gaining a second "outgoing" clip + blend-weight field.
2. **A dialogue-pose clip** — dialogue currently leaves the character in whatever locomotion clip
   it was already playing. Needs a new clip authored in `gen_test_character_gltf.py` (or a real
   asset once one exists) plus a call from wherever `DialogueSystem`'s active/inactive state is
   already read in `IronShadowsGame::Update()`.
3. **Vehicle entry/exit animation** — the character is currently just not drawn at all while
   `playerDriving_` is true (same as the box it replaced). Needs an actual enter/exit clip and a
   state machine in `IronShadowsGame`/`PrototypeRenderer` for "play this clip once, non-looping,
   then switch to driving-hidden" rather than the current binary show/hide.

Before any of that, note the *real* rig used today (`assets/source/gltf/test_character.gltf`) is a
minimal 3-bone/3-box placeholder, not a proper reusable skeleton (`plan_18` `IS-18-001` is still
open) — if a real character artist/pipeline becomes available, replacing this rig entirely (rather
than extending it) is probably the right call once MC3 gains rigging support or a better
hand-authoring path is found.

Other open items worth picking up opportunistically (not blocking, not sequenced):

- `plan_18` `IS-18-001` (a real named-bone skeleton convention, not this one-off test rig),
  `IS-18-006`/`007`/`008` (enter/exit/sit/drive clips, bone masks, additive layers — mostly
  blocked on IS-18-004's blending landing first), `IS-18-034` (report an unknown clip name instead
  of silently holding the last pose), `IS-18-037` (persist animation-clip state in `SaveGame` once
  clips stop being purely input-derived).
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
- Alternatively, if M6 blending/dialogue-pose/vehicle-entry feels like a large enough chunk of new
  cross-repo work to warrant asking first (it plausibly is, matching the same scope-question
  pattern used to start this session's M6 work), gate M7 (`plan_39` `IS-39-008`, one data-driven
  mission) does not depend on any of it and could be picked up instead.

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
