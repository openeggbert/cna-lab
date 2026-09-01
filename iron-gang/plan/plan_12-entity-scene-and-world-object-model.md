# 12. Entity, scene, and world-object model

[Back to master plan](../plan.md)

Give district-loaded objects stable identity, ownership, components, and lifecycle semantics, built on top of `cna-extended`'s ECS (`World`/`Entity`/`ComponentManager`) and `Transform3` hierarchy rather than a parallel entity system designed from scratch. There is no in-house editor, so editor-only concerns (undo/redo transactions, full component-reflection-for-GUI-editing) are cut.

- [ ] **IG-12-001 P0** — Adopt cna-extended's ECS and Transform3 hierarchy as the entity/scene model instead of designing a new one; confirm it covers Iron Gang's needs through a small measured prototype.
- [ ] **IG-12-002 P0** — Assign stable IDs that survive save/load and district unload/reload.
- [ ] **IG-12-003 P0** — Separate logical entities from render and physics handles.
- [ ] **IG-12-004 P0** — Define entity ownership by persistent world, mission, district, or temporary effect.
- [ ] **IG-12-005 P1** — Define component registration and serialization metadata on top of cna-extended's component types.
- [ ] **IG-12-006 P1** — Define entity activation, deactivation, destruction, and rehydration states across district transitions.
- [ ] **IG-12-007 P1** — Define cross-district entity references (e.g. a mission character who appears in more than one district).
- [ ] **IG-12-008 P1** — Define prefab instantiation and override rules for MC3-authored content.
- [ ] **IG-12-009 P1** — Define runtime-spawned versus authored entity identity.
- [ ] **IG-12-010 P1** — Define pooled entity behavior without ID reuse bugs.
- [ ] **IG-12-011 P1** — Create query APIs that avoid unrestricted world scans.
- [ ] **IG-12-012 P1** — Create deterministic update ordering where gameplay depends on it.
- [ ] **IG-12-013 P1** — Create component change notification without recursive chaos.
- [ ] **IG-12-014 P1** — Create entity debug names separate from stable IDs.
- [ ] **IG-12-015 P1** — Create a text-based entity inspection/dump helper (not a GUI editor).
- [ ] **IG-12-016 P1** — Create orphan-reference validation.
- [ ] **IG-12-017 P1** — Create prefab circular-dependency validation.
- [ ] **IG-12-018 P2** — Create data-oriented storage for measured hot components, only after profiling justifies it.
- [ ] **IG-12-019 P2** — Create runtime entity diff capture for bug reports.

## cna-extended ECS integration

- [ ] **IG-12-020 P0** — Wire cna-extended's `World`/`Entity`/`ComponentManager` into Iron Gang's core library and confirm it links and runs (already validated end to end via the `compile-software` preset).
- [ ] **IG-12-021 P0** — Define Iron Gang-specific component types (mission-relevant, gameplay-relevant) on top of cna-extended's component storage.
- [ ] **IG-12-022 P1** — Confirm cna-extended's `Transform3` hierarchy satisfies parent/child transform ownership needs (vehicles, attached props, character-held items).
- [ ] **IG-12-023 P1** — Add focused unit tests for Iron Gang's component types layered on cna-extended's ECS.
- [ ] **IG-12-024 P1** — Add an integration scenario exercising entity creation/destruction through a full district load/unload cycle.
- [ ] **IG-12-025 P2** — Add logging and debug inspection for entity/component state.
- [ ] **IG-12-026 P2** — Document how Iron Gang's entity/component conventions build on cna-extended's ECS.

## Prefab instantiation and overrides

- [ ] **IG-12-027 P1** — Define the scope and public API of prefab instantiation for MC3-authored content.
- [ ] **IG-12-028 P1** — Implement the smallest deterministic reference path for instantiating one MC3 prefab as entities.
- [ ] **IG-12-029 P1** — Define per-instance override rules (position, material, tags) without duplicating the whole prefab.
- [ ] **IG-12-030 P1** — Add input validation and actionable failure reporting for prefab instantiation.
- [ ] **IG-12-031 P1** — Add focused unit tests for prefab instantiation and overrides.
- [ ] **IG-12-032 P1** — Add an integration scenario instantiating a full district's worth of prefabs.
- [ ] **IG-12-033 P2** — Document usage examples and common failure modes for prefab instantiation.

## Stable reference resolver

- [ ] **IG-12-034 P1** — Define the scope and public API of the stable-ID reference resolver used by missions and saves.
- [ ] **IG-12-035 P1** — Implement the smallest deterministic reference path resolving a stable ID to a live entity (or "not currently loaded").
- [ ] **IG-12-036 P1** — Add input validation and actionable failure reporting for unresolved/dangling references.
- [ ] **IG-12-037 P1** — Add focused unit tests for the reference resolver, including cross-district cases.
- [ ] **IG-12-038 P1** — Add an integration scenario where a mission references an entity across a district transition.
- [ ] **IG-12-039 P1** — Define save/checkpoint serialization and restoration for stable references.
- [ ] **IG-12-040 P2** — Document usage examples and common failure modes for the reference resolver.

## Entity lifecycle and district transitions

- [ ] **IG-12-041 P1** — Define the scope and public API of the entity lifecycle state machine (active/inactive/destroyed/rehydrating).
- [ ] **IG-12-042 P1** — Implement the smallest deterministic reference path for entity lifecycle across one district load/unload.
- [ ] **IG-12-043 P1** — Add input validation and actionable failure reporting for lifecycle transitions.
- [ ] **IG-12-044 P1** — Add focused unit tests for the lifecycle state machine.
- [ ] **IG-12-045 P1** — Add an integration scenario covering a full district transition with persistent-state entities.
- [ ] **IG-12-046 P1** — Define save/checkpoint serialization and restoration for entity lifecycle state.
- [ ] **IG-12-047 P2** — Document usage examples and common failure modes for the lifecycle state machine.

## Spatial queries

Reuse cna-extended's octree/collision broadphase where possible instead of building a second spatial index.

- [ ] **IG-12-048 P1** — Evaluate whether cna-extended's octree/collision broadphase can serve gameplay spatial queries directly.
- [ ] **IG-12-049 P1** — Define the public API for gameplay world queries (nearest entity, entities in radius, raycast-to-entity).
- [ ] **IG-12-050 P1** — Implement the smallest deterministic reference path for gameplay world queries.
- [ ] **IG-12-051 P1** — Add focused unit tests for gameplay world queries.
- [ ] **IG-12-052 P1** — Add an integration scenario using world queries from mission/AI code.
- [ ] **IG-12-053 P2** — Profile world queries under a representative worst-case district.
- [ ] **IG-12-054 P2** — Document usage examples and common failure modes for world queries.

## Entity command buffer

- [ ] **IG-12-055 P1** — Define the scope of a deferred entity command buffer (spawn/destroy/component-change requests applied at a safe point in the frame).
- [ ] **IG-12-056 P1** — Implement the smallest deterministic reference path for the command buffer.
- [ ] **IG-12-057 P1** — Add focused unit tests for the command buffer, including ordering guarantees.
- [ ] **IG-12-058 P2** — Document usage examples and common failure modes for the command buffer.
