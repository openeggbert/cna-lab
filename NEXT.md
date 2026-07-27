# NEXT.md — Iron Shadows continuity document

Primary short-term continuity doc for autonomous/resumed work sessions. Read this before
`plan.md`/`plan/plan_NN-*.md`. Update it whenever project state changes materially — do not wait
until the end of a session to reconstruct it from memory.

## Where things stand (as of this entry)

Repo: `/rv/data/development/github.com/openeggbert/iron-shadows`, branch `develop` (not `master` —
someone switched branches outside this session at some point; both exist, `develop` is ahead).
**No remote configured, nothing pushed** — commit locally only until explicitly told otherwise.

Gates M0-M5 (see `plan.md` milestone order) are done:

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
  transition between them — see "What changed most recently" below.

Plan size: 2,148 tasks across 41 group files under `plan/`, down from an original 6,380-task
AAA/open-world-scoped draft — see `plan.md`'s "Locked scope decisions" section for the ten
scope decisions that drove that cut (Mafia-1 (2002) content scope, Mafia-1-era system fidelity,
no in-house editor suite beyond Mesh Craft, manual MC3 authoring, baked lighting target, etc.).

## What changed most recently (this session)

Implemented gate M5 (second district):

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
not verified" above. Camera offsets, body-mesh offset, and Jolt's default vehicle tuning are the
most likely things to need a quick numeric tweak once someone can see them. Also walk/drive
through the warehouse block's exit trigger (behind the map at roughly `(0,0.5,-47)`) and confirm
the loading screen appears, the countryside district looks right, and the return trip back to the
warehouse block works too — this has only been verified via `TestDistrictTransition`'s physics/
state-machine assertions, never by actually seeing it happen.

**If continuing headless/autonomous work, the next milestone per `plan.md`'s own order is M6 — one
skinned character** (`plan/plan_39-vertical-slice-gates.md` `IS-39-007`): one skinned character
plays blended locomotion, dialogue pose, and vehicle entry/exit animations using
`cna-extended`'s Transform3/skinned-playback. This will need: a skinned-mesh-capable MC3 export
(or a hand-authored test rig if MC3/Mesh Craft's skinning support needs investigation first —
check `cna-extended`'s existing skinned-playback API/tests before assuming a gap), replacing the
player's current invisible-capsule-with-a-box-mesh representation, and wiring animation state to
`PlayerController`'s existing on-foot/dialogue/vehicle-entry states rather than inventing a new
state machine. Scope the first pass down to one locomotion blend (idle/walk/run) proving the
skinned-playback pipeline end to end, before adding dialogue-pose and vehicle-entry animations.

Other open items worth picking up opportunistically (not blocking, not sequenced):

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
  sibling repo, out of scope for this repo's own autonomous session unless explicitly asked to
  cross into that repo.

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
