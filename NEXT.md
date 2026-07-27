# NEXT.md — Iron Shadows continuity document

Primary short-term continuity doc for autonomous/resumed work sessions. Read this before
`plan.md`/`plan/plan_NN-*.md`. Update it whenever project state changes materially — do not wait
until the end of a session to reconstruct it from memory.

## Where things stand (as of this entry)

Repo: `/rv/data/development/github.com/openeggbert/iron-shadows`, branch `develop` (not `master` —
someone switched branches outside this session at some point; both exist, `develop` is ahead).
**No remote configured, nothing pushed** — commit locally only until explicitly told otherwise.

Gates M0-M4 (see `plan.md` milestone order) are done:

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
  and `VehicleController` are now actually driven by physics (not just standalone-prototyped) —
  see "What changed most recently" below.

Plan size: 2,148 tasks across 41 group files under `plan/`, down from an original 6,380-task
AAA/open-world-scoped draft — see `plan.md`'s "Locked scope decisions" section for the ten
scope decisions that drove that cut (Mafia-1 (2002) content scope, Mafia-1-era system fidelity,
no in-house editor suite beyond Mesh Craft, manual MC3 authoring, baked lighting target, etc.).

## What changed most recently (this session)

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
most likely things to need a quick numeric tweak once someone can see them.

**If continuing headless/autonomous work, the next milestone per `plan.md`'s own order is M5 — a
second district**, which forces the discrete district-loading design (`plan/plan_13-world-partitioning-and-streaming.md`):
loading screen, unload/reload world+physics static bodies, preserve player/vehicle/mission/save
state across the transition. This is a genuinely large task (new content authoring + a real
loading-screen state machine); scope it down to the smallest slice that proves the transition
mechanism (e.g. two trivial hand-boxed districts before investing in real MC3 content for a
second real place) rather than trying to fully art/content the second district in one pass.

Other open items worth picking up opportunistically (not blocking, not sequenced):

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
