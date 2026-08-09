# 23. Combat, damage, and interaction

[Back to master plan](../plan.md)

Add only the combat and interaction systems required by the original campaign design, at Mafia-1 (2002) fidelity — this is a story-driven crime drama with occasional gunfights, not a cover-shooter arsenal.

- [ ] **IG-23-001 P1** — Define whether combat is central, limited, optional, or avoidable in the campaign.
- [ ] **IG-23-002 P1** — Define the weapon classes required by the vertical slice and the full campaign (expect a small handful, not a large catalog).
- [ ] **IG-23-003 P1** — Create cover interaction only if level design supports it.
- [ ] **IG-23-004 P1** — Create friendly/neutral/hostile faction rules.
- [ ] **IG-23-005 P1** — Create civilian and police reactions to visible weapons, integrated with the witness/wanted system in plan_22.
- [ ] **IG-23-006 P1** — Create mission rules for prohibited weapons or non-lethal goals.
- [ ] **IG-23-007 P1** — Create combat debug and damage logs.
- [ ] **IG-23-008 P1** — Create accessibility assists for aiming and camera motion.
- [ ] **IG-23-009 P2** — Create melee only if campaign content requires it.
- [ ] **IG-23-010 P2** — Create destructible cover only for selected authored objects.
- [ ] **IG-23-011 P2** — Create suppression behavior only if enemy AI needs it.
- [ ] **IG-23-012 P3** — Avoid advanced ballistic simulation unless it clearly improves the intended feel.
- [ ] **IG-23-013 P3** — Avoid adding a large weapon catalog before mission use cases exist.

- [ ] **IG-23-014 P1** — Implement weapon data, aiming, firing, reload, recoil, ammo state, and hitscan/projectile resolution for the vertical-slice weapon set.
- [ ] **IG-23-015 P1** — Add unit tests for weapon state transitions (fire, reload, jam/empty, switch).
- [ ] **IG-23-016 P1** — Add an integration test exercising a weapon through a scripted mission encounter.
- [ ] **IG-23-017 P1** — Define save/checkpoint serialization for weapon state.
- [ ] **IG-23-018 P2** — Add debug logging/inspection for weapon state.
- [ ] **IG-23-019 P2** — Document weapon authoring and tuning.

- [ ] **IG-23-020 P1** — Implement weapon inventory, pickup/drop, and ammo economy.
- [ ] **IG-23-021 P1** — Add unit tests for inventory transitions.
- [ ] **IG-23-022 P1** — Define save/checkpoint serialization for inventory state.
- [ ] **IG-23-023 P2** — Document inventory rules.

- [ ] **IG-23-024 P1** — Implement damage, health, armor, incapacitation, and death resolution.
- [ ] **IG-23-025 P1** — Add unit tests for damage-resolution edge cases.
- [ ] **IG-23-026 P1** — Add an integration test covering a full combat encounter through incapacitation/death.
- [ ] **IG-23-027 P1** — Define save/checkpoint serialization for health/damage state.
- [ ] **IG-23-028 P2** — Document damage rules and common failure modes.

- [ ] **IG-23-029 P1** — Implement combat perception and alert propagation, sharing the perception primitives used by pedestrian and police AI in plan_20/plan_22.
- [ ] **IG-23-030 P1** — Add unit tests for perception thresholds.
- [ ] **IG-23-031 P1** — Add an integration test for alert propagation across nearby NPCs.
- [ ] **IG-23-032 P2** — Define a CPU/latency budget for perception checks at the expected concurrent-NPC count.
- [ ] **IG-23-033 P2** — Document perception tuning.

- [ ] **IG-23-034 P1** — Implement combat animation layers, notifies, and hit-reaction poses on the shared production skeleton from plan_18.
- [ ] **IG-23-035 P1** — Add an integration test exercising combat animation in a running mission flow.
- [ ] **IG-23-036 P2** — Add debug visualization for animation notify timing.
- [ ] **IG-23-037 P2** — Document animation notify contracts.

- [ ] **IG-23-038 P1** — Implement impact effects, material response, and weapon audio, integrated with the audio bus graph in plan_27.
- [ ] **IG-23-039 P1** — Add an integration test for impact/audio feedback across a representative set of materials.
- [ ] **IG-23-040 P2** — Document impact/audio authoring conventions.

- [ ] **IG-23-041 P1** — Implement aim assist and camera/input transitions into and out of aiming.
- [ ] **IG-23-042 P1** — Add unit tests for aim-assist tuning and camera-transition behavior.
- [ ] **IG-23-043 P2** — Document accessibility tuning options for aim assist.
- [ ] **IG-23-044 P3** — Revisit weapon/combat scope after the first vertical-slice district ships, based on what the campaign's actual mission list needs.
