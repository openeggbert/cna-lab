# 15. Physics integration

[Back to master plan](../plan.md)

A real physics library sits behind Iron Shadows-owned interfaces so vehicle/character/mission code never depends on the library's own types. This is genuinely needed regardless of scope, but stays scoped to what one hand-authored district actually requires: rigid bodies, a character controller, vehicle contacts, and collision queries — not ragdolls, buoyancy, or destruction as first-class systems.

## Library selection and core world

- [x] **IS-15-001 P0** — Prototype Jolt Physics behind Iron Shadows-owned interfaces (`PhysicsWorld`, `RigidBodyHandle`, `ShapeDesc`, `CharacterHandle`, `VehicleHandle`, `RaycastHit`, `TriggerEvent` in `include/IronShadows/Physics/`; Jolt types never leave `src/Physics/PhysicsWorld.cpp`, a PIMPL boundary).
- [x] **IS-15-002 P0** — Compared against Bullet on concrete vertical-slice needs rather than building both: Jolt's built-in `CharacterVirtual` and `VehicleConstraint`/`WheeledVehicleController` map directly onto this project's character-controller and raycast-vehicle needs with far less integration code than Bullet's older, less actively maintained vehicle/character API surface; Jolt's modern C++, job-based multithreading, and native trigger/sensor contact events were the deciding factors. Recorded here rather than in a discarded second prototype.
- [x] **IS-15-003 P0** — Selected Jolt Physics; pinned to tag `v5.6.0` (commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, MIT) in `THIRD_PARTY.md`, shared checkout at `~/deps/jolt` per CLAUDE.md's dependency-reuse convention.
- [x] **IS-15-004 P0** — Created `PhysicsWorld::Step()` with fixed 1/60s sub-stepping, accumulator-based and decoupled from the caller's frame delta, capped at 4 catch-up steps per call to avoid a spiral of death after a stall.
- [x] **IS-15-005 P1** — Create stable physics handles that never leak library pointers into game/mission code (opaque `{value: uint32_t}` wrappers around Jolt's own `BodyID`/index handles).
- [ ] **IS-15-006 P1** — Create safe deferred body creation/destruction (bodies created/destroyed outside the physics step).
- [ ] **IS-15-007 P1** — Create sleeping and activation policies for bodies at rest (currently Jolt's untuned defaults).
- [x] **IS-15-008 P1** — Add focused unit tests for `PhysicsWorld` lifecycle (`tests/PhysicsTests.cpp`: create/step/destroy exercised by all 5 tests; teardown exercised by `PhysicsWorld`'s destructor running cleanly at the end of every test).

## MC3 collision integration

- [ ] **IS-15-009 P0** — Map MC3 static, dynamic, kinematic, trigger, and none collision roles onto physics bodies.
- [ ] **IS-15-010 P0** — Create collision layers and masks for world, player, vehicles, characters, and triggers.
- [ ] **IS-15-011 P1** — Load a district's collision proxies (from plan 14's collision-proxy pipeline) into the physics world at district-load time.
- [ ] **IS-15-012 P1** — Define limits and a safe fallback for malformed or missing collision data instead of crashing.
- [ ] **IS-15-013 P1** — Add an integration test that loads one hand-authored district and asserts every building has a working collision body.

## Shapes, materials, and queries

- [ ] **IS-15-014 P1** — Create primitive, convex, mesh, and compound shape paths.
- [ ] **IS-15-015 P2** — Create a heightfield shape path for countryside terrain.
- [ ] **IS-15-016 P1** — Create physics material definitions for asphalt, dirt, grass, wood, metal, and glass.
- [ ] **IS-15-017 P2** — Add convex-decomposition tooling, limited to explicitly approved building/prop assets rather than run automatically on everything.
- [ ] **IS-15-018 P0** — Implement raycast, sweep, overlap, and trigger queries. Raycast and trigger queries are implemented and tested (`PhysicsWorld::Raycast`, `CreateTrigger`/`ConsumeTriggerEvents`); sweep and overlap queries are still open.
- [x] **IS-15-019 P1** — Implement a contact-event and trigger-event queue consumable by gameplay/mission code (`TriggerContactListener` + `PhysicsWorld::ConsumeTriggerEvents()`, drains a mutex-guarded queue populated by Jolt's job-thread contact callbacks).
- [ ] **IS-15-020 P2** — Add focused unit tests for shape/material selection and query correctness.

## Character controller adapter

- [ ] **IS-15-021 P0** — Create a `CharacterBody` adapter (capsule or similar) driven by `PlayerController`. Adapter itself is prototyped and tested (`PhysicsWorld::CreateCharacter`/`MoveCharacter`, a `JPH::CharacterVirtual` capsule proven to be blocked by a wall in `tests/PhysicsTests.cpp`); wiring it to actually drive `PlayerController` instead of the existing simple AABB movement is separate, larger follow-up work.
- [ ] **IS-15-022 P1** — Handle steps, slopes, and simple stairs within the character controller.
- [x] **IS-15-023 P1** — Detect grounded/airborne state for movement and animation state (`PhysicsWorld::IsCharacterGrounded`, backed by `CharacterVirtual::IsSupported()`, tested).
- [ ] **IS-15-024 P1** — Add an integration test that walks the character controller through one hand-authored district without falling through geometry.

## Vehicle physics adapter

- [x] **IS-15-025 P0** — Create a `VehiclePhysics` adapter exposing a chassis rigid body and per-wheel raycast contact points (tuning specifics live in plan 17). Prototyped as `PhysicsWorld::CreateFourWheelVehicle`/`GetVehicleWheelStates` (Jolt `VehicleConstraint` + `WheeledVehicleController` + `VehicleCollisionTesterRay`), proven to drive forward under throttle input in `tests/PhysicsTests.cpp`. Wiring it to replace `VehicleController`'s existing kinematic model is separate, larger follow-up work (plan 17).
- [ ] **IS-15-026 P1** — Expose suspension spring/damper hook points for `VehicleController` to drive.
- [ ] **IS-15-027 P1** — Integrate collision response between the vehicle adapter and world/character/vehicle collision layers.
- [ ] **IS-15-028 P1** — Add an integration test that drives one vehicle through one hand-authored district without tunneling through geometry.

## Debug tooling

- [ ] **IS-15-029 P1** — Add debug collision wireframe rendering, toggled by a dev command.
- [ ] **IS-15-030 P2** — Add body/contact/query counters for profiling.

## Save and checkpoint integration

- [ ] **IS-15-031 P1** — Define deterministic-enough restoration rules so a loaded save reproduces equivalent physics state (position/velocity), not bit-exact replay.
- [ ] **IS-15-032 P1** — Tear down and recreate physics bodies cleanly across a district transition (ties to plan 13's district load/unload flow).
- [ ] **IS-15-033 P1** — Add a save/load round-trip test that verifies physics-relevant entity state (position, velocity, awake/asleep) survives.

## Deferred / limited-scope later work

- [ ] **IS-15-034 P3** — Add ragdoll support only after character animation priorities (plan 18) are met.
- [ ] **IS-15-035 P3** — Add water buoyancy only if a specific mission or district (e.g. a harbor) requires it.
- [ ] **IS-15-036 P3** — Add breakable-object constraints only for explicitly selected mission content, not as a general destruction system.
- [ ] **IS-15-037 P3** — Add physics replay/trace capture only if a specific hard-to-reproduce bug needs it.

## Profiling

- [ ] **IS-15-038 P2** — Profile a dense interior/worst-case pileup scenario against the CPU/memory budget in `docs/performance-targets.md`.
- [ ] **IS-15-039 P2** — Add a soak test with many concurrently active bodies to check for leaks or instability.
- [ ] **IS-15-040 P2** — Document the physics system's scope, invariants, and explicit non-goals (no ragdoll/buoyancy/destruction by default) for contributors.
