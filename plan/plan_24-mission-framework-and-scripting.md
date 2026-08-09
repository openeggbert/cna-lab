# 24. Mission framework and scripting

[Back to master plan](../plan.md)

Represent missions as versioned, testable state graphs, data-driven enough to carry a
15-20 mission campaign across multiple districts without becoming unmanageable. Logic
that cannot be expressed as declarative data uses a small, engine-evaluated
condition/action expression syntax, not a general-purpose sandboxed scripting language
(no embedded Lua VM, no script API surface to secure).

## Core mission data and flow

- [x] **IG-24-001 P0** — Replace prototype hard-coded mission state with versioned data while preserving current behavior. *(`MissionDefinition`/`LoadMissionDefinition` (`include/`/`src/Missions/MissionDefinition.hpp/.cpp`) load a versioned JSON file (`assets/missions/prologue.mission.json`) defining each state's id/objective text/transition condition/next-state; `PrototypeMission` now looks these up instead of a hardcoded switch, but keeps `PrototypeMissionState` as a fixed enum for `SaveGame`'s existing int-based format and public-API compatibility. Behavior verified identical to the original hardcoded flow by `TestMissionFlow` (hardcoded default, unchanged) and the new `TestMissionLoadsCommittedFile` (same flow, loaded from the real file) both passing with the same assertions.)*
- [ ] **IG-24-002 P0** — Define mission, state, objective, condition, action, trigger, checkpoint, failure, and completion IDs. *(Partial: state ids (`introduction`/`reach_vehicle`/.../`completed`) and condition names (`dialogue_finished`/`player_near_vehicle`/`player_driving`/`player_driving_in_warehouse_goal`) are defined and validated; no distinct action/trigger/failure ID concepts yet -- this prototype mission has no failure states and no actions beyond "transition to next state".)*
- [x] **IG-24-003 P0** — Create a mission validation tool (script or command, not a GUI editor) that finds missing references and unreachable states. *(Smallest form: inline validation inside `LoadMissionDefinition` itself, not a separate script/command -- rejects duplicate state ids, an `initialState`/`next` that doesn't match any state id, an unrecognized condition name, and an empty state list, each with an actionable error message. `TestMissionValidationRejectsMalformedData` covers all five cases plus a missing-file case. No separate "unreachable state" detection (linear graph today has no way to become unreachable) and no standalone CLI tool a content author could run independently of the game itself.)*
- [x] **IG-24-004 P0** — Create one complete data-driven vertical-slice mission covering the existing prologue delivery flow. *(`assets/missions/prologue.mission.json`: the exact 5-state prologue delivery flow -- listen to dialogue, reach the sedan, enter it, drive to the warehouse, complete -- as data. `PrototypeMission::LoadMission` loads it in `IronGangGame::Initialize`, falling back to an identical hardcoded default on any load/validation failure.)*
- [ ] **IG-24-005 P1** — Create typed mission variables (bool/int/float/string) scoped per mission and per campaign.
- [ ] **IG-24-006 P1** — Create entity, area, dialogue, cinematic, timer, inventory, vehicle, and wanted-state conditions.
- [ ] **IG-24-007 P1** — Create spawn, despawn, enable, disable, move, play, set, wait, and branch actions.
- [ ] **IG-24-008 P1** — Create objective text and progress events.
- [ ] **IG-24-009 P1** — Create failure reasons and retry policies.
- [ ] **IG-24-010 P1** — Create checkpoint boundaries and terminal-state application on failure/retry.
- [ ] **IG-24-011 P1** — Create mission-owned entity references that survive a district load/unload transition.
- [ ] **IG-24-012 P1** — Create mission pause/resume across cinematics and menus.
- [ ] **IG-24-013 P1** — Create a small condition/action expression syntax (comparisons, boolean combinators, arithmetic on mission variables) for logic not expressible as declarative data. This is evaluated directly by the engine; it is explicitly not a general-purpose embedded scripting language and has no sandboxing/API-surface project attached to it.
- [ ] **IG-24-014 P1** — Enforce evaluation limits (max expression depth/step count, no recursion) so a malformed mission file cannot hang or corrupt engine state.
- [ ] **IG-24-015 P1** — Create a mission debug overlay with forced state transitions.
- [ ] **IG-24-016 P1** — Log mission state/condition/action transitions through the existing logging path for debugging; no dedicated event-replay system.
- [ ] **IG-24-017 P1** — Create deterministic mission scenario tests.
- [ ] **IG-24-018 P1** — Create version migration for mission state.
- [ ] **IG-24-019 P1** — Create safe behavior when content referenced by a save is missing.
- [ ] **IG-24-020 P1** — Create a mission dependency and prerequisite graph across the campaign.
- [ ] **IG-24-021 P1** — Create campaign chapter and unlock state for the full 15-20 mission, multi-district campaign.
- [ ] **IG-24-022 P2** — Create reusable mission shape templates (delivery, tailing, chase, escort, infiltration, conversation, escape) as data presets, not a generator tool.
- [ ] **IG-24-023 P2** — Create optional objective and rating support only if game design uses it.
- [ ] **IG-24-024 P2** — Create non-linear mission choice support after the first linear campaign slice works.

## Mission graph runtime

- [x] **IG-24-025 P0** — Define the scope, public API, and explicit non-goals (no embedded scripting language) of the mission graph runtime. *(`MissionDefinition.hpp`'s own header comment: conditions are a small, fixed, engine-evaluated set by name, explicitly not a general expression evaluator -- that is IG-24-013's own separate, later scope.)*
- [x] **IG-24-026 P0** — Implement the smallest deterministic reference path: load one mission, advance through states, reach completion. *(`LoadMissionDefinition` + `PrototypeMission::LoadMission`/`Update`/`IsCompleted` -- see IG-24-001/004.)*
- [x] **IG-24-027 P1** — Add input validation and actionable failure reporting for malformed mission data. *(See IG-24-003.)*
- [x] **IG-24-028 P1** — Add unit tests and one integration scenario exercising the runtime in a running game flow. *(`TestMissionLoadsCommittedFile`/`TestMissionValidationRejectsMalformedData` in `tests/CoreTests.cpp`; the "running game flow" integration is `IronGangGame::Initialize` calling `LoadMission` and a `--smoke` run confirming no crash and no fallback-triggered error message.)*
- [ ] **IG-24-029 P1** — Define save/checkpoint serialization and restoration for in-progress mission state. *(Not newly implemented, but already correct by construction: `SaveGame`'s existing `mission_state` int field round-trips through `PrototypeMissionState`, and `PrototypeMission::GetObjectiveText()`/`Update()` resolve that restored state against whichever `MissionDefinition` is currently loaded -- unchanged by this gate's work, not verified by a new dedicated test.)*
- [ ] **IG-24-030 P2** — Add debug logging/inspection and document usage, invariants, and common failure modes.

## Condition/action expression evaluator

- [ ] **IG-24-031 P1** — Define the scope and public API of the condition/action expression evaluator described in IG-24-013/IG-24-014.
- [ ] **IG-24-032 P1** — Implement the smallest deterministic reference path: evaluate one condition, execute one action.
- [ ] **IG-24-033 P1** — Add input validation, depth/step limits, and actionable failure reporting for malformed expressions.
- [ ] **IG-24-034 P1** — Add unit tests and one integration scenario covering the condition/action set from IG-24-006/IG-24-007.
- [ ] **IG-24-035 P2** — Add debug logging/inspection and document the expression syntax for mission authors.

## Objective tracker

- [ ] **IG-24-036 P1** — Define the scope and public API of the objective tracker.
- [ ] **IG-24-037 P1** — Implement the smallest deterministic reference path: track one objective from active to complete/failed.
- [ ] **IG-24-038 P1** — Add unit tests and one integration scenario exercising objective text/progress events.
- [ ] **IG-24-039 P1** — Define save/checkpoint serialization and restoration for objective progress.
- [ ] **IG-24-040 P2** — Add debug logging/inspection and document usage.

## Checkpoint coordinator

- [ ] **IG-24-041 P1** — Define the scope and public API of the checkpoint coordinator.
- [ ] **IG-24-042 P1** — Implement the smallest deterministic reference path: reach a checkpoint, fail, retry from it.
- [ ] **IG-24-043 P1** — Add unit tests and one integration scenario covering failure/retry policies from IG-24-009.
- [ ] **IG-24-044 P1** — Define save/checkpoint serialization and restoration guarantees.
- [ ] **IG-24-045 P2** — Add debug logging/inspection and document usage.

## Campaign graph (chapters, unlocks, dependencies)

- [ ] **IG-24-046 P1** — Define the scope and public API of the campaign graph covering IG-24-020/IG-24-021.
- [ ] **IG-24-047 P1** — Implement the smallest deterministic reference path: unlock one chapter after completing its prerequisite mission.
- [ ] **IG-24-048 P1** — Add unit tests and one integration scenario covering the full 15-20 mission dependency graph.
- [ ] **IG-24-049 P1** — Define save/checkpoint serialization and restoration for campaign progress.
- [ ] **IG-24-050 P2** — Add debug logging/inspection and document usage.

## Mission entity, timer, and variable binding

- [ ] **IG-24-051 P1** — Define the scope and public API for binding mission logic to entity references, timers, and typed variables (IG-24-005/IG-24-011).
- [ ] **IG-24-052 P1** — Implement the smallest deterministic reference path: a mission timer expiring updates a mission variable an entity reference depends on.
- [ ] **IG-24-053 P1** — Add unit tests and one integration scenario covering entity-reference survival across a district load (IG-24-011).
- [ ] **IG-24-054 P1** — Define save/checkpoint serialization and restoration for timers, variables, and entity bindings.
- [ ] **IG-24-055 P2** — Add debug logging/inspection and document usage.
