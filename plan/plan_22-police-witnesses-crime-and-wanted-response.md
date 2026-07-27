# 22. Police, witnesses, crime, and wanted response

[Back to master plan](../plan.md)

Create fair, observable law-enforcement gameplay at Mafia-1 (2002) fidelity: a witnessed offense or crime triggers a chase with one escalation step, then resolves. No multi-tier GTA-style wanted stars, no search-area decay simulation, no roadblocks, and no persistent detective/investigation system — those belong to a much larger open-world simulation, not this game.

### Gate M9 status (first pass, vertical slice)

`PoliceSystem` (`include/IronShadows/Gameplay/PoliceSystem.hpp`) implements exactly `Clear -> Dispatched -> Chasing -> (one escalation) -> Clear`, deterministically unit-tested end to end in `tests/CoreTests.cpp`'s `TestPoliceSystemFullCycle`. Offenses are speeding (> 70 km/h) or being within 2.5 units of a witness while driving; witnesses are every `TrafficVehicle`/`Pedestrian` position within a fixed 15-unit radius — a simplified proximity check, **not** a real vision cone/line-of-sight test (documented simplification, see IS-22-002 below). Dispatch has a fixed 2-second delay; patrol cars drive straight toward the player's current position (ignoring roads) at a fixed speed rather than following the traffic lane graph; escalation adds one second patrol car after 20 seconds of chasing; resolution requires the closest patrol car to stay beyond 40 units of the player for 3 sustained seconds. No traffic-stop/ticket flow, no surrender/arrest, no route planning over the lane graph, no mission overrides, no save/checkpoint restoration of wanted state (deliberately never saved — see `IronShadowsGame::LoadPrototype()`'s own comment), and no debug view — those remain unstarted.

- [x] **IS-22-001 P0** — Define which actions are offenses (speeding, running a light, violence) and how they are detected. *(Speeding above a fixed km/h threshold, or close proximity to a witness while driving — no "running a light" (no signals exist yet, see plan 21) or violence detection.)*
- [ ] **IS-22-002 P0** — Create witness perception so an officer or civilian must actually see the act, not global instant knowledge. *(Simplified to a fixed-radius proximity check instead — a documented simplification, not a real vision cone/line-of-sight test.)*
- [x] **IS-22-003 P0** — Create a short dispatch delay between a witnessed offense and police responding. *(Fixed 2-second `Dispatched` state before a patrol car starts moving.)*
- [x] **IS-22-004 P1** — Create a single wanted state machine: clear -> chased -> resolved (arrested/fined, or escaped by breaking line of sight). *(`PoliceState::Clear/Dispatched/Chasing`; resolves back to `Clear` by sustained distance rather than a literal line-of-sight break.)*
- [ ] **IS-22-005 P1** — Create traffic-stop behavior for minor offenses (pull over, ticket/warning).
- [ ] **IS-22-006 P1** — Create pursuit behavior for vehicles and on-foot suspects.
- [ ] **IS-22-007 P1** — Create surrender, arrest, escape, and mission-failure states.
- [ ] **IS-22-008 P1** — Create police spawn rules that avoid visible pop-in.
- [ ] **IS-22-009 P1** — Create police route planning that reuses the traffic lane graph.
- [x] **IS-22-010 P1** — Create one backup-escalation step (a second officer/car joins an active pursuit) with no further tiers. *(A second patrol car spawns after 20 seconds of chasing; `activePatrolCount_` is capped at 2 with no further escalation.)*
- [ ] **IS-22-011 P1** — Create false-positive prevention and clear player feedback for why they are being chased.
- [ ] **IS-22-012 P1** — Create mission overrides for scripted police states.
- [ ] **IS-22-013 P1** — Create checkpoint-safe wanted-state restoration.
- [ ] **IS-22-014 P1** — Create a police debug view for witness knowledge, dispatch, and pursuit state.
- [ ] **IS-22-015 P1** — Create test scenarios for unseen crime, lost sight, vehicle swap, interior hiding, and district escape.
- [ ] **IS-22-016 P2** — Create radio call audio and subtitle events.
- [ ] **IS-22-017 P2** — Create foot patrols and parked patrol vehicles as passive set dressing.
- [ ] **IS-22-018 P2** — Create non-lethal arrest behavior before advanced combat is implemented.
- [ ] **IS-22-019 P2** — Create civilian witness variability (some report faster/slower, or not at all).
- [ ] **IS-22-020 P3** — Non-goal: detective/investigation persistence across sessions is a distant post-slice idea (see group 40), not part of v1.
- [ ] **IS-22-021 P1** — Define the scope, public API, and explicit non-goals of the crime/offense detector.
- [ ] **IS-22-022 P1** — Define versioned configuration/data for the crime/offense detector.
- [ ] **IS-22-023 P1** — Implement the smallest deterministic reference path for the crime/offense detector.
- [ ] **IS-22-024 P1** — Add focused unit tests and one integration scenario for the crime/offense detector.
- [ ] **IS-22-025 P1** — Define save/checkpoint serialization and restoration for the crime/offense detector.
- [ ] **IS-22-026 P2** — Add logging, counters, and a debug view for the crime/offense detector.
- [ ] **IS-22-027 P2** — Define CPU/memory/latency budgets for the crime/offense detector and profile a worst-case scene.
- [ ] **IS-22-028 P2** — Document usage examples and common failure modes for the crime/offense detector.
- [ ] **IS-22-029 P1** — Define the scope, public API, and explicit non-goals of the witness system.
- [ ] **IS-22-030 P1** — Define versioned configuration/data for the witness system.
- [ ] **IS-22-031 P1** — Implement the smallest deterministic reference path for the witness system.
- [ ] **IS-22-032 P1** — Add focused unit tests and one integration scenario for the witness system.
- [ ] **IS-22-033 P1** — Define save/checkpoint serialization and restoration for the witness system.
- [ ] **IS-22-034 P2** — Add logging, counters, and a debug view for the witness system.
- [ ] **IS-22-035 P2** — Define CPU/memory/latency budgets for the witness system and profile a worst-case scene.
- [ ] **IS-22-036 P2** — Document usage examples and common failure modes for the witness system.
- [ ] **IS-22-037 P1** — Define the scope, public API, and explicit non-goals of the police dispatcher.
- [ ] **IS-22-038 P1** — Define versioned configuration/data for the police dispatcher.
- [ ] **IS-22-039 P1** — Implement the smallest deterministic reference path for the police dispatcher.
- [ ] **IS-22-040 P1** — Add focused unit tests and one integration scenario for the police dispatcher.
- [ ] **IS-22-041 P1** — Define save/checkpoint serialization and restoration for the police dispatcher.
- [ ] **IS-22-042 P2** — Add logging, counters, and a debug view for the police dispatcher.
- [ ] **IS-22-043 P2** — Define CPU/memory/latency budgets for the police dispatcher and profile a worst-case scene.
- [ ] **IS-22-044 P2** — Document usage examples and common failure modes for the police dispatcher.
- [ ] **IS-22-045 P1** — Define the scope, public API, and explicit non-goals of the pursuit coordinator.
- [ ] **IS-22-046 P1** — Define versioned configuration/data for the pursuit coordinator.
- [ ] **IS-22-047 P1** — Implement the smallest deterministic reference path for the pursuit coordinator.
- [ ] **IS-22-048 P1** — Add focused unit tests and one integration scenario for the pursuit coordinator.
- [ ] **IS-22-049 P1** — Define save/checkpoint serialization and restoration for the pursuit coordinator.
- [ ] **IS-22-050 P2** — Add logging, counters, and a debug view for the pursuit coordinator.
- [ ] **IS-22-051 P2** — Define CPU/memory/latency budgets for the pursuit coordinator and profile a worst-case scene.
- [ ] **IS-22-052 P2** — Document usage examples and common failure modes for the pursuit coordinator.
- [ ] **IS-22-053 P1** — Define the scope, public API, and explicit non-goals of the arrest and resolution flow.
- [ ] **IS-22-054 P1** — Define versioned configuration/data for the arrest and resolution flow.
- [ ] **IS-22-055 P1** — Implement the smallest deterministic reference path for the arrest and resolution flow.
- [ ] **IS-22-056 P1** — Add focused unit tests and one integration scenario for the arrest and resolution flow.
- [ ] **IS-22-057 P1** — Define save/checkpoint serialization and restoration for the arrest and resolution flow.
- [ ] **IS-22-058 P2** — Add logging, counters, and a debug view for the arrest and resolution flow.
- [ ] **IS-22-059 P2** — Define CPU/memory/latency budgets for the arrest and resolution flow and profile a worst-case scene.
- [ ] **IS-22-060 P2** — Document usage examples and common failure modes for the arrest and resolution flow.
