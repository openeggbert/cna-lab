# 03. Architecture and module boundaries

[Back to master plan](../plan.md)

Create stable dependency directions before systems grow into a monolith.

- [ ] **IS-03-001 P0** — Write a dependency-direction diagram (sharp-runtime -> CNA -> cna-extended -> Iron Shadows core -> game content) and enforce it in review.
- [ ] **IS-03-002 P0** — Keep game-specific systems out of CNA, sharp-runtime, and cna-extended.
- [ ] **IS-03-003 P0** — Define a composition root that constructs long-lived game services (world, mission, dialogue, save, input) in one explicit place.
- [ ] **IS-03-004 P0** — Define ownership and shutdown order for graphics, audio, physics, world state, and save/load.
- [ ] **IS-03-005 P0** — Ban unrestricted global service locators from gameplay code.
- [ ] **IS-03-006 P0** — Define public/private headers per module.
- [x] **IS-03-007 P0** — Adopt cna-extended's ECS/`World`/`Entity`/`Transform3` as the scene-object model instead of evaluating alternatives from scratch.
- [ ] **IS-03-008 P1** — Define module-level CMake targets after boundaries stabilize.
- [ ] **IS-03-009 P1** — Define error-handling policy: exceptions, expected/result values, assertions, and fatal errors.
- [ ] **IS-03-010 P1** — Define coordinate-system, units, angle, handedness, and time conventions.
- [ ] **IS-03-011 P1** — Define stable ID types (entity, asset, mission, dialogue, district) rather than passing raw strings everywhere.
- [ ] **IS-03-012 P1** — Define data ownership across a loaded district (not streamed sectors — see plan_13 for the district-loading model).
- [ ] **IS-03-013 P1** — Define gameplay event semantics and avoid hidden synchronous recursion.
- [ ] **IS-03-014 P1** — Define subsystem initialization order and shutdown contracts.
- [ ] **IS-03-015 P1** — Define policies for `std` versus sharp-runtime collections/types.
- [ ] **IS-03-016 P1** — Keep a short running decision log (one paragraph per decision) instead of a formal ADR process.
- [ ] **IS-03-017 P1** — Create a forbidden-dependency check for low-level modules (e.g. `iron_shadows_core` must not depend on the executable target).
- [ ] **IS-03-018 P2** — Define plugin/extension boundaries for the one small debug tool from plan_30, not core gameplay.
- [ ] **IS-03-019 P2** — Create a code ownership map by subsystem.
- [ ] **IS-03-020 P0** — Implement the composition root: construct and wire graphics, audio, world, mission, dialogue, and save services in `IronShadowsGame`.
- [ ] **IS-03-021 P1** — Add a focused unit test that the composition root constructs and tears down cleanly with no leaked resources.
- [ ] **IS-03-022 P1** — Implement stable ID types (entity/asset/mission/dialogue/district) as small value types with unit tests.
- [ ] **IS-03-023 P1** — Implement the forbidden-dependency check as a CI script over `#include` directions.
- [ ] **IS-03-024 P2** — Implement a minimal gameplay event dispatch used by mission/dialogue/AI integration (a plain callback list is enough; no generic event-bus framework).
- [ ] **IS-03-025 P2** — Document the module map (directories under `include/IronShadows/` and `src/`) and what may depend on what.
