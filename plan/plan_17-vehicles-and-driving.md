# 17. Vehicles and driving

[Back to master plan](../plan.md)

Progress from kinematic movement to one believable, tunable historical sedan (plus a small campaign vehicle roster later — see group 32). Wheel-damage granularity, full telemetry graphing, and garage/persistence systems are cut down to the minimum needed, or later.

- [ ] **IG-17-001 P0** — Preserve the current kinematic controller as a deterministic fallback/test vehicle. *(**Not done, and the window for it closed:** the kinematic controller was replaced outright by the Jolt raycast vehicle during gate M4 (plan_15 IG-15-025), not kept beside it. Anything wanting a deterministic vehicle for tests today has to drive the physics one. Reinstating a kinematic fallback would mean writing it again against the current `VehicleController` interface -- worth doing only if physics determinism actually becomes a testing problem.)*
- [x] **IG-17-002 P0** — Create a raycast-vehicle prototype in the selected physics library. *(Done during gate M4 and recorded under plan_15 IG-15-025 rather than here: `Physics::PhysicsWorld::CreateFourWheelVehicle` builds a Jolt `VehicleConstraint` with four wheels, and `VehicleController` drives it, reading position/yaw/speed back from physics each frame instead of simulating movement itself. Recorded here so this file stops claiming nothing exists.)*
- [x] **IG-17-003 P0** — Define a versioned vehicle configuration schema. *(`assets/vehicles/sedan.vehicle.json` (schema version 1) plus `VehicleConfig`/`LoadVehicleConfig` (`include/IronGang/Gameplay/VehicleConfig.hpp`, `src/Gameplay/VehicleConfig.cpp`): chassis mass and half extents, wheel radius/width/positions, and forward/reverse speed limits -- exactly the constants `VehicleController.cpp` used to hard-code. Same failure contract as the other loaders: a missing file, an unknown key (named **with its section**), a wrong type, an out-of-range number, or a wheel list that is not exactly four entries are warnings that keep defaults; only malformed JSON, a non-object root, or an unsupported version fails, and a failure leaves the caller's configuration untouched. A **zero mass or zero-radius wheel can never reach the physics body**, and one malformed wheel entry defaults all four rather than producing a wheelbase nobody designed. `VehicleController::Configure` warns if it is called after the body exists, since mass and geometry are baked in at creation -- a real trap, found and fixed while wiring this up: the loader was initially called after `vehicle_.Reset()` and would have applied nothing. Covered by `TestVehicleConfigLoadsValidatesAndFallsBack`, which asserts the committed file loads with zero warnings **and still carries the previously hard-coded values**. Documented in `docs/vehicles.md`.)*
- [ ] **IG-17-004 P0** — Implement chassis mass, center of mass, wheels, suspension, engine, gears, brakes, and steering. *(Partial: chassis mass and dimensions, four wheels with radius/width/positions, steering, braking, and a forward/reverse speed limit exist and are now data (IG-17-003). Suspension, steering response, and braking use **Jolt's own defaults** rather than tuned values, which is why they are deliberately not in the schema yet -- a key the game does not read would be worse than no key. Centre of mass, an engine torque curve, and gears do not exist at all (IG-17-009).)*
- [ ] **IG-17-005 P0** — Tune one vertical-slice sedan for stable keyboard and gamepad control.
- [ ] **IG-17-006 P0** — Attach a converted CNJ vehicle body and wheel meshes.
- [ ] **IG-17-007 P1** — Implement wheel visual pose from physics contacts.
- [ ] **IG-17-008 P1** — Implement speed-sensitive steering and assists.
- [ ] **IG-17-009 P1** — Implement forward, reverse, neutral, and gear-shift behavior.
- [ ] **IG-17-010 P1** — Implement handbrake and controlled skids.
- [ ] **IG-17-011 P1** — Implement asphalt, dirt, grass, wet, and damaged-surface grip.
- [ ] **IG-17-012 P1** — Implement engine start/stop and stalled states if useful.
- [ ] **IG-17-013 P1** — Implement doors, seats, occupants, and entry points.
- [ ] **IG-17-014 P1** — Implement headlights, brake lights, indicators, horn, and dashboard states.
- [ ] **IG-17-015 P1** — Implement impact damage and disabled states, including simple wheel damage as part of the same system (no separate wheel-damage-state machine).
- [ ] **IG-17-016 P1** — Implement vehicle-to-mission stable IDs.
- [ ] **IG-17-017 P1** — Implement AI and player control through the same command interface.
- [ ] **IG-17-018 P1** — Implement vehicle spawn, despawn, pooling, and district-load restoration.
- [ ] **IG-17-019 P1** — Implement safe reset for overturned or stuck mission vehicles.
- [ ] **IG-17-020 P1** — Implement vehicle camera presets and transitions.
- [ ] **IG-17-021 P1** — Implement deterministic vehicle test tracks.
- [ ] **IG-17-022 P2** — Implement manual clutch and gearbox mode.
- [ ] **IG-17-023 P2** — Implement differential and drivetrain variants.
- [ ] **IG-17-024 P2** — Implement per-vehicle parking/ownership persistence only if the campaign needs it (no full garage system).
- [ ] **IG-17-025 P2** — Implement trailers only if campaign content requires them.
- [ ] **IG-17-026 P2** — Implement motorcycles only as a separate post-slice project.
- [ ] **IG-17-027 P2** — Implement deformable cosmetic damage only after gameplay damage works.
- [ ] **IG-17-028 P3** — Evaluate high-fidelity tire models only if current handling cannot meet the game target.

## Vehicle configuration

- [ ] **IG-17-029 P1** — Define the scope, public API, and versioned schema for vehicle configuration.
- [ ] **IG-17-030 P1** — Implement the smallest deterministic reference path for loading a vehicle configuration.
- [ ] **IG-17-031 P1** — Add input validation and actionable failure reporting to vehicle configuration.
- [ ] **IG-17-032 P1** — Add focused unit tests for vehicle configuration.
- [ ] **IG-17-033 P1** — Add an integration scenario that exercises vehicle configuration in a running game flow.
- [ ] **IG-17-034 P2** — Document usage examples and common failure modes for vehicle configuration.

## Raycast vehicle physics (chassis, wheels, steering, brakes)

- [ ] **IG-17-035 P0** — Define the scope, public API, and versioned config for raycast vehicle physics.
- [ ] **IG-17-036 P0** — Implement the smallest deterministic reference path for raycast vehicle physics (one sedan, four wheels).
- [ ] **IG-17-037 P1** — Add input validation and actionable failure reporting to raycast vehicle physics.
- [ ] **IG-17-038 P1** — Add focused unit tests for raycast vehicle physics.
- [ ] **IG-17-039 P1** — Add an integration scenario that exercises raycast vehicle physics in a running game flow.
- [ ] **IG-17-040 P1** — Define save/checkpoint serialization and restoration for vehicle physics state.
- [ ] **IG-17-041 P2** — Profile raycast vehicle physics under a representative worst-case scene.
- [ ] **IG-17-042 P2** — Document usage examples and common failure modes for raycast vehicle physics.

## Engine simulation

- [ ] **IG-17-043 P1** — Define the scope and public API for a simple torque-curve engine model.
- [ ] **IG-17-044 P1** — Implement the smallest deterministic reference path for the engine model.
- [ ] **IG-17-045 P1** — Add focused unit tests for the engine model.
- [ ] **IG-17-046 P1** — Add an integration scenario driving the sedan through its full RPM range.
- [ ] **IG-17-047 P2** — Profile the engine model under sustained full-throttle driving.
- [ ] **IG-17-048 P2** — Document usage examples and common failure modes for the engine model.

## Gearbox simulation

- [ ] **IG-17-049 P1** — Define the scope and public API for a simple gearbox/final-drive model.
- [ ] **IG-17-050 P1** — Implement the smallest deterministic reference path for the gearbox model.
- [ ] **IG-17-051 P1** — Add focused unit tests for the gearbox model, including shift timing.
- [ ] **IG-17-052 P1** — Add an integration scenario exercising gear changes during driving.
- [ ] **IG-17-053 P2** — Profile the gearbox model during rapid shifting.
- [ ] **IG-17-054 P2** — Document usage examples and common failure modes for the gearbox model.

## Tire model

- [ ] **IG-17-055 P1** — Define the scope and public API for a simple longitudinal/lateral tire-grip model.
- [ ] **IG-17-056 P1** — Implement the smallest deterministic reference path for the tire model.
- [ ] **IG-17-057 P1** — Add focused unit tests for the tire model, including low-grip surfaces.
- [ ] **IG-17-058 P1** — Add an integration scenario exercising cornering and braking grip.
- [ ] **IG-17-059 P2** — Profile the tire model under a representative worst-case scene.
- [ ] **IG-17-060 P2** — Document usage examples and common failure modes for the tire model.

## Suspension model

- [ ] **IG-17-061 P1** — Define the scope and public API for a simple spring/damper suspension model.
- [ ] **IG-17-062 P1** — Implement the smallest deterministic reference path for the suspension model.
- [ ] **IG-17-063 P1** — Add focused unit tests for the suspension model.
- [ ] **IG-17-064 P1** — Add an integration scenario exercising suspension travel over uneven ground.
- [ ] **IG-17-065 P2** — Profile the suspension model under a representative worst-case scene.
- [ ] **IG-17-066 P2** — Document usage examples and common failure modes for the suspension model.

## Vehicle command interface

- [ ] **IG-17-067 P1** — Define the scope, public API, and versioned config for the vehicle command interface shared by player and AI.
- [ ] **IG-17-068 P1** — Implement the smallest deterministic reference path for the command interface.
- [ ] **IG-17-069 P1** — Add input validation and actionable failure reporting to the command interface.
- [ ] **IG-17-070 P1** — Add focused unit tests for the command interface.
- [ ] **IG-17-071 P1** — Add an integration scenario driving the same command interface from player input and from a scripted AI route.
- [ ] **IG-17-072 P2** — Document usage examples and common failure modes for the command interface.

## Wheel visual controller

- [ ] **IG-17-073 P1** — Define the scope and public API for wheel visual pose from physics contacts.
- [ ] **IG-17-074 P1** — Implement the smallest deterministic reference path for wheel visual pose.
- [ ] **IG-17-075 P1** — Add focused unit tests for wheel visual pose.
- [ ] **IG-17-076 P1** — Add an integration scenario exercising wheel visuals during driving.
- [ ] **IG-17-077 P2** — Document usage examples and common failure modes for wheel visual pose.

## Vehicle damage

- [ ] **IG-17-078 P1** — Define the scope, public API, and versioned config for vehicle damage (impact + simple wheel damage).
- [ ] **IG-17-079 P1** — Implement the smallest deterministic reference path for vehicle damage and disabled states.
- [ ] **IG-17-080 P1** — Add input validation and actionable failure reporting to vehicle damage.
- [ ] **IG-17-081 P1** — Add focused unit tests for vehicle damage.
- [ ] **IG-17-082 P1** — Add an integration scenario that exercises vehicle damage in a running game flow.
- [ ] **IG-17-083 P1** — Define save/checkpoint serialization and restoration for vehicle damage state.
- [ ] **IG-17-084 P2** — Document usage examples and common failure modes for vehicle damage.

## Vehicle entry points

- [ ] **IG-17-085 P1** — Define the scope and public API for vehicle entry-point definitions (doors, seats).
- [ ] **IG-17-086 P1** — Implement the smallest deterministic reference path for vehicle entry points.
- [ ] **IG-17-087 P1** — Add focused unit tests for vehicle entry points.
- [ ] **IG-17-088 P1** — Add an integration scenario exercising entry/exit for the sedan.
- [ ] **IG-17-089 P2** — Document usage examples and common failure modes for vehicle entry points.

## Vehicle light controller

- [ ] **IG-17-090 P2** — Define the scope and public API for headlight/brake-light/indicator/horn states.
- [ ] **IG-17-091 P2** — Implement the smallest deterministic reference path for the vehicle light controller.
- [ ] **IG-17-092 P2** — Add focused unit tests for the vehicle light controller.
- [ ] **IG-17-093 P2** — Add an integration scenario exercising lights during a night driving segment.
- [ ] **IG-17-094 P2** — Document usage examples and common failure modes for the vehicle light controller.

## Vehicle debug counters (no full telemetry-graph tooling)

- [ ] **IG-17-095 P2** — Add basic debug counters for RPM, gear, speed, and slip (text/overlay, not a graphing UI).
- [ ] **IG-17-096 P2** — Add focused unit tests confirming debug counters match physics state.
- [ ] **IG-17-097 P2** — Document how to read the vehicle debug counters.
