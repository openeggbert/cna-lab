# 15. Physics integration

[Back to master plan](../plan.md)

A real physics library sits behind Iron Gang-owned interfaces so vehicle/character/mission code never depends on the library's own types. This is genuinely needed regardless of scope, but stays scoped to what one hand-authored district actually requires: rigid bodies, a character controller, vehicle contacts, and collision queries — not ragdolls, buoyancy, or destruction as first-class systems.

## Library selection and core world

- [x] **IG-15-001 P0** — Prototype Jolt Physics behind Iron Gang-owned interfaces (`PhysicsWorld`, `RigidBodyHandle`, `ShapeDesc`, `CharacterHandle`, `VehicleHandle`, `RaycastHit`, `TriggerEvent` in `include/IronGang/Physics/`; Jolt types never leave `src/Physics/PhysicsWorld.cpp`, a PIMPL boundary).
- [x] **IG-15-002 P0** — Compared against Bullet on concrete vertical-slice needs rather than building both: Jolt's built-in `CharacterVirtual` and `VehicleConstraint`/`WheeledVehicleController` map directly onto this project's character-controller and raycast-vehicle needs with far less integration code than Bullet's older, less actively maintained vehicle/character API surface; Jolt's modern C++, job-based multithreading, and native trigger/sensor contact events were the deciding factors. Recorded here rather than in a discarded second prototype.
- [x] **IG-15-003 P0** — Selected Jolt Physics; pinned to tag `v5.6.0` (commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, MIT) in `THIRD_PARTY.md`, shared checkout at `~/deps/jolt` per CLAUDE.md's dependency-reuse convention.
- [x] **IG-15-004 P0** — Created `PhysicsWorld::Step()` with fixed 1/60s sub-stepping, accumulator-based and decoupled from the caller's frame delta, capped at 4 catch-up steps per call to avoid a spiral of death after a stall.
- [x] **IG-15-005 P1** — Create stable physics handles that never leak library pointers into game/mission code (opaque `{value: uint32_t}` wrappers around Jolt's own `BodyID`/index handles).
- [ ] **IG-15-006 P1** — Create safe deferred body creation/destruction (bodies created/destroyed outside the physics step).
- [ ] **IG-15-007 P1** — Create sleeping and activation policies for bodies at rest (currently Jolt's untuned defaults).
- [x] **IG-15-008 P1** — Add focused unit tests for `PhysicsWorld` lifecycle (`tests/PhysicsTests.cpp`: create/step/destroy exercised by all 5 tests; teardown exercised by `PhysicsWorld`'s destructor running cleanly at the end of every test).

## MC3 collision integration

- [ ] **IG-15-009 P0** — Map MC3 static, dynamic, kinematic, trigger, and none collision roles onto physics bodies.
- [ ] **IG-15-010 P0** — Create collision layers and masks for world, player, vehicles, characters, and triggers.
- [ ] **IG-15-011 P1** — Load a district's collision proxies (from plan 14's collision-proxy pipeline) into the physics world at district-load time.
- [ ] **IG-15-012 P1** — Define limits and a safe fallback for malformed or missing collision data instead of crashing.
- [ ] **IG-15-013 P1** — Add an integration test that loads one hand-authored district and asserts every building has a working collision body.

## Shapes, materials, and queries

- [ ] **IG-15-014 P1** — Create primitive, convex, mesh, and compound shape paths.
- [ ] **IG-15-015 P2** — Create a heightfield shape path for countryside terrain.
- [ ] **IG-15-016 P1** — Create physics material definitions for asphalt, dirt, grass, wood, metal, and glass.
- [ ] **IG-15-017 P2** — Add convex-decomposition tooling, limited to explicitly approved building/prop assets rather than run automatically on everything.
- [ ] **IG-15-018 P0** — Implement raycast, sweep, overlap, and trigger queries. Raycast and trigger queries are implemented and tested (`PhysicsWorld::Raycast`, `CreateTrigger`/`ConsumeTriggerEvents`); sweep and overlap queries are still open.
- [x] **IG-15-019 P1** — Implement a contact-event and trigger-event queue consumable by gameplay/mission code (`TriggerContactListener` + `PhysicsWorld::ConsumeTriggerEvents()`, drains a mutex-guarded queue populated by Jolt's job-thread contact callbacks).
- [ ] **IG-15-020 P2** — Add focused unit tests for shape/material selection and query correctness.

## Character controller adapter

- [x] **IG-15-021 P0** — Create a `CharacterBody` adapter (capsule or similar) driven by `PlayerController`. `PlayerController` now owns a `Physics::CharacterHandle`; `Update()` computes desired velocity from input exactly as before (yaw/turn/sprint math unchanged) and hands it to `PhysicsWorld::MoveCharacter`, then reads position/grounded back from physics. `TestPlayerMotion` in `tests/CoreTests.cpp` walks the character into the hotel's static collider and asserts it does not tunnel through.
- [ ] **IG-15-022 P1** — Handle steps, slopes, and simple stairs within the character controller (untuned Jolt defaults so far).
- [x] **IG-15-023 P1** — Detect grounded/airborne state for movement and animation state (`PhysicsWorld::IsCharacterGrounded`, backed by `CharacterVirtual::IsSupported()`, tested; exposed as `PlayerController::IsGrounded()`, not yet consumed by animation since there is no animated character model yet).
- [ ] **IG-15-024 P1** — Add an integration test that walks the character controller through one hand-authored district without falling through geometry (the hotel-collider tunneling test above covers one building; a full-district soak test is still open).

## Vehicle physics adapter

- [x] **IG-15-025 P0** — Create a `VehiclePhysics` adapter exposing a chassis rigid body and per-wheel raycast contact points (tuning specifics live in plan 17). `VehicleController` now owns a `Physics::VehicleHandle` (chassis half-extents 1.05x0.325x2.1, matching `PrototypeRenderer`'s body box; wheel radius 0.33) and reads position/yaw/speed back from `PhysicsWorld` every frame instead of simulating them itself; `VehicleController::Update()` mirrors Jolt's own `VehicleConstraintTest` sample for forward/brake/reverse input handling.
- [ ] **IG-15-026 P1** — Expose suspension spring/damper hook points for `VehicleController` to drive (currently Jolt's default spring/damper values).
- [x] **IG-15-027 P1** — Integrate collision response between the vehicle adapter and world/character/vehicle collision layers (`PrototypeWorld::BuildPhysicsStaticBodies` gives the vehicle a real world to drive on and collide with; found and fixed a real bug along the way -- the ground plane was `collidable=false` in `colliders_` on purpose, for the unrelated XZ-only `CanOccupy` check, so it was silently never getting a physics body at all until this task added a dedicated `groundCollider_`).
- [ ] **IG-15-028 P1** — Add an integration test that drives one vehicle through one hand-authored district without tunneling through geometry (`TestVehicleMotion` in `tests/CoreTests.cpp` covers straight-line acceleration on the real district's ground plane; a dedicated obstacle-collision test is still open).

## Debug tooling

- [ ] **IG-15-029 P1** — Add debug collision wireframe rendering, toggled by a dev command.
- [ ] **IG-15-030 P2** — Add body/contact/query counters for profiling.

## Save and checkpoint integration

- [ ] **IG-15-031 P1** — Define deterministic-enough restoration rules so a loaded save reproduces equivalent physics state (position/velocity), not bit-exact replay.
- [ ] **IG-15-032 P1** — Tear down and recreate physics bodies cleanly across a district transition (ties to plan 13's district load/unload flow).
- [ ] **IG-15-033 P1** — Add a save/load round-trip test that verifies physics-relevant entity state (position, velocity, awake/asleep) survives.

## Deferred / limited-scope later work

- [ ] **IG-15-034 P3** — Add ragdoll support only after character animation priorities (plan 18) are met.
- [ ] **IG-15-035 P3** — Add water buoyancy only if a specific mission or district (e.g. a harbor) requires it.
- [ ] **IG-15-036 P3** — Add breakable-object constraints only for explicitly selected mission content, not as a general destruction system.
- [ ] **IG-15-037 P3** — Add physics replay/trace capture only if a specific hard-to-reproduce bug needs it.

## Profiling

- [ ] **IG-15-038 P2** — Profile a dense interior/worst-case pileup scenario against the CPU/memory budget in `docs/performance-targets.md`.
- [ ] **IG-15-039 P2** — Add a soak test with many concurrently active bodies to check for leaks or instability.
- [ ] **IG-15-040 P2** — Document the physics system's scope, invariants, and explicit non-goals (no ragdoll/buoyancy/destruction by default) for contributors.
