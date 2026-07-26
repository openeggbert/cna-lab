# 39. Vertical-slice gates

[Back to master plan](../plan.md)


Define non-negotiable integrated milestones that prevent endless disconnected engine work.

- [ ] **IS-39-001 P0** — Gate M0: repository configures only after dependency preflight reports all required repositories/submodules present.
- [ ] **IS-39-002 P0** — Gate M1: the current procedural prototype builds, launches, completes its mission, saves, loads, and passes CTest.
- [ ] **IS-39-003 P0** — Gate M2: one MC3 building converts to GLB/CNJ, loads at runtime, collides correctly, and replaces debug geometry.
- [ ] **IS-39-004 P0** — Gate M3: one CNJ vehicle renders with separate wheels and preserves the current drivable mission.
- [ ] **IS-39-005 P0** — Gate M4: selected production physics library supports character, trigger, raycast, and vehicle prototypes behind abstractions.
- [ ] **IS-39-006 P0** — Gate M5: two sectors stream during high-speed driving without leaks, crashes, stale activation, or unacceptable hitches.
- [ ] **IS-39-007 P0** — Gate M6: one skinned character plays blended locomotion, dialogue pose, and vehicle entry/exit animations.
- [ ] **IS-39-008 P0** — Gate M7: one data-driven mission controls dialogue, objective UI, vehicle entry, destination trigger, checkpoint, and completion.
- [ ] **IS-39-009 P0** — Gate M8: one in-engine cutscene can play, skip, save-safe finalize, and hand control back correctly.
- [ ] **IS-39-010 P0** — Gate M9: a small pedestrian and traffic population survives ten minutes of walking/driving interaction.
- [ ] **IS-39-011 P0** — Gate M10: vertical-slice environment uses production-path assets, collision, lighting, audio, and UI.
- [ ] **IS-39-012 P0** — Gate M11: the complete vertical-slice mission passes happy-path, failure, retry, save/load, and cutscene-skip automation.
- [ ] **IS-39-013 P0** — Gate M12: target frame-time, memory, streaming, and content budgets pass on the primary target hardware/backend.
- [ ] **IS-39-014 P0** — Gate M13: all shipping assets have approved provenance and generated third-party notices.
- [ ] **IS-39-015 P0** — Gate M14: an external clean workspace can build, install, launch, and complete the demo using documented steps.
- [ ] **IS-39-016 P0** — M1 procedural executable: configure recursive dependencies.
- [ ] **IS-39-017 P0** — M1 procedural executable: build game and tests.
- [ ] **IS-39-018 P0** — M1 procedural executable: launch a visible window.
- [ ] **IS-39-019 P0** — M1 procedural executable: verify on-foot controls.
- [ ] **IS-39-020 P0** — M1 procedural executable: verify vehicle controls.
- [ ] **IS-39-021 P0** — M1 procedural executable: complete dialogue and mission.
- [ ] **IS-39-022 P0** — M1 procedural executable: save and load.
- [ ] **IS-39-023 P0** — M1 procedural executable: reset and quit.
- [ ] **IS-39-024 P0** — M1 procedural executable: capture logs and gameplay reference.
- [ ] **IS-39-025 P0** — M2 first production asset: validate MC3.
- [ ] **IS-39-026 P0** — M2 first production asset: convert MC3 to GLB.
- [ ] **IS-39-027 P0** — M2 first production asset: validate GLB.
- [ ] **IS-39-028 P0** — M2 first production asset: convert GLB to CNJ.
- [ ] **IS-39-029 P0** — M2 first production asset: load CNJ.
- [ ] **IS-39-030 P0** — M2 first production asset: assign material.
- [ ] **IS-39-031 P0** — M2 first production asset: create collision.
- [ ] **IS-39-032 P0** — M2 first production asset: replace procedural mesh.
- [ ] **IS-39-033 P0** — M2 first production asset: package provenance.
- [ ] **IS-39-034 P0** — M2 first production asset: test missing/corrupt asset fallback.
- [ ] **IS-39-035 P0** — M5 two-sector streaming: partition assets.
- [ ] **IS-39-036 P0** — M5 two-sector streaming: load asynchronously.
- [ ] **IS-39-037 P0** — M5 two-sector streaming: upload safely.
- [ ] **IS-39-038 P0** — M5 two-sector streaming: activate deterministically.
- [ ] **IS-39-039 P0** — M5 two-sector streaming: prefetch while driving.
- [ ] **IS-39-040 P0** — M5 two-sector streaming: cancel stale work.
- [ ] **IS-39-041 P0** — M5 two-sector streaming: evict under budget.
- [ ] **IS-39-042 P0** — M5 two-sector streaming: restore persistent entities.
- [ ] **IS-39-043 P0** — M5 two-sector streaming: connect collision/navigation.
- [ ] **IS-39-044 P0** — M5 two-sector streaming: soak high-speed traversal.
- [ ] **IS-39-045 P0** — M7 data-driven mission: parse graph.
- [ ] **IS-39-046 P0** — M7 data-driven mission: validate references.
- [ ] **IS-39-047 P0** — M7 data-driven mission: start dialogue.
- [ ] **IS-39-048 P0** — M7 data-driven mission: advance objective.
- [ ] **IS-39-049 P0** — M7 data-driven mission: track vehicle interaction.
- [ ] **IS-39-050 P0** — M7 data-driven mission: track destination trigger.
- [ ] **IS-39-051 P0** — M7 data-driven mission: checkpoint.
- [ ] **IS-39-052 P0** — M7 data-driven mission: fail/retry.
- [ ] **IS-39-053 P0** — M7 data-driven mission: save/load.
- [ ] **IS-39-054 P0** — M7 data-driven mission: trace/debug.
- [ ] **IS-39-055 P0** — M11 vertical slice: fresh-start playthrough.
- [ ] **IS-39-056 P0** — M11 vertical slice: save/load playthrough.
- [ ] **IS-39-057 P0** — M11 vertical slice: cutscene-skip playthrough.
- [ ] **IS-39-058 P0** — M11 vertical slice: mission-failure retry.
- [ ] **IS-39-059 P0** — M11 vertical slice: vehicle-loss recovery.
- [ ] **IS-39-060 P0** — M11 vertical slice: missing optional asset behavior.
- [ ] **IS-39-061 P0** — M11 vertical slice: ten-minute soak.
- [ ] **IS-39-062 P0** — M11 vertical slice: performance capture.
- [ ] **IS-39-063 P0** — M11 vertical slice: license audit.
- [ ] **IS-39-064 P0** — M11 vertical slice: clean package install.

