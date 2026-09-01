# 22. Police, witnesses, crime, and wanted response

[Back to master plan](../plan.md)

Create fair, observable law-enforcement gameplay at Mafia-1 (2002) fidelity: a witnessed offense or crime triggers a chase with one escalation step, then resolves. No multi-tier GTA-style wanted stars, no search-area decay simulation, no roadblocks, and no persistent detective/investigation system — those belong to a much larger open-world simulation, not this game.

### Gate M9 status (first pass, vertical slice)

`PoliceSystem` (`include/IronGang/Gameplay/PoliceSystem.hpp`) implements exactly `Clear -> Dispatched -> Chasing -> (one escalation) -> Clear`, deterministically unit-tested end to end in `tests/CoreTests.cpp`'s `TestPoliceSystemFullCycle`. Offenses are speeding (> 70 km/h), being within 2.5 units of a witness while driving, or crossing a stop line while its signal says stop (2026-08-26, plan_21's lights); witnesses are every `TrafficVehicle`/`Pedestrian` position within a fixed 15-unit radius **and** with an unobstructed line of sight to the player's vehicle (2026-08-26, IG-22-002); still not a vision cone, so a witness facing away sees just as well as one facing the act. Dispatch has a fixed 2-second delay; patrol cars drive straight toward the player's current position (ignoring roads) at a fixed speed rather than following the traffic lane graph; escalation adds one second patrol car after 20 seconds of chasing; resolution requires the closest patrol car to stay beyond 40 units of the player for 3 sustained seconds. Since 2026-08-25 the chase is readable by missions: `PoliceSystem::GetChaseSeconds()` plus the `police_alerted`/`police_chasing`/`police_chase_seconds` mission facts let a mission fail for staying wanted too long (the committed prologue does, see plan_24 IG-24-006/043), and `IronGangGame::RetryMission()` clears the response on a checkpoint retry. No traffic-stop/ticket flow, no surrender/arrest, no route planning over the lane graph, no mission overrides, no save/checkpoint restoration of wanted state (deliberately never saved — see `IronGangGame::LoadPrototype()`'s own comment), and no debug view — those remain unstarted.

- [x] **IG-22-001 P0** — Define which actions are offenses (speeding, running a light, violence) and how they are detected. *(Three offences, each detected where its knowledge lives: **speeding** above a fixed km/h threshold and **collision** (close proximity to a witness while driving) inside `PoliceSystem`, and **running a red light** computed by the game -- which owns both the signal and the vehicle -- and passed in through `PoliceObservation`. The red-light test is a **segment** crossing, not a position check: at 20 m/s a 60 Hz frame covers a third of a metre, so a car is behind the line one frame and past it the next, and a position test would miss exactly the fast crossings that matter. Reversing back over a line does not count, nor does crossing the same plane on the pavement beside the lane. When several offences happen at once the **worst** is the one recorded, so the reason the player is told is the worst thing they did rather than whichever check ran first. No violence detection -- there is no combat (plan_23).)*
- [x] **IG-22-002 P0** — Create witness perception so an officer or civilian must actually see the act, not global instant knowledge. *(A witness must now be both **close** and **able to see**: `HasLineOfSight` (`include/IronGang/Gameplay/Visibility.hpp`) traces the segment from the witness's eye height to the player's vehicle against the district's collidable boxes, using the slab method, and the game filters the witness list before `PoliceSystem` ever sees it -- the police system has no business knowing about geometry. Distance is checked first (`PoliceSystem::kWitnessRadius` is public for exactly that), so a ray is traced only for the handful of candidates the radius keeps. Non-collidable boxes -- road markings, trigger decals -- are ignored, since treating paint as an occluder would blind every witness standing near a crossing. **Measured in a real `mixed` run: witness checks per update dropped from 16 to 9**, so nearly half the "witnesses" had been reporting through something; `ai_cpu` p95 moved 0.068 -> 0.074 ms. Still a radius plus a ray, **not a vision cone**: a cone needs a facing direction and a peripheral-vision rule per witness, neither of which changes the outcome nearly as much as noticing a building is in the way.)*
- [x] **IG-22-003 P0** — Create a short dispatch delay between a witnessed offense and police responding. *(Fixed 2-second `Dispatched` state before a patrol car starts moving.)*
- [x] **IG-22-004 P1** — Create a single wanted state machine: clear -> chased -> resolved (arrested/fined, or escaped by breaking line of sight). *(`PoliceState::Clear/Dispatched/Chasing`; resolves back to `Clear` by sustained distance rather than a literal line-of-sight break.)*
- [ ] **IG-22-005 P1** — Create traffic-stop behavior for minor offenses (pull over, ticket/warning).
- [ ] **IG-22-006 P1** — Create pursuit behavior for vehicles and on-foot suspects.
- [ ] **IG-22-007 P1** — Create surrender, arrest, escape, and mission-failure states. *(Partial: **mission failure** exists -- a sustained chase fails the committed prologue through the `police_chase_seconds` fact and the mission's own failure branch (plan_24 IG-24-006/009/043), and escape already existed as the chase's own resolve rule (40 units for 3 sustained seconds). Still missing: surrender and arrest, which need a stopped-vehicle interaction the prototype has no input or animation for.)*
- [ ] **IG-22-008 P1** — Create police spawn rules that avoid visible pop-in.
- [ ] **IG-22-009 P1** — Create police route planning that reuses the traffic lane graph.
- [x] **IG-22-010 P1** — Create one backup-escalation step (a second officer/car joins an active pursuit) with no further tiers. *(A second patrol car spawns after 20 seconds of chasing; `activePatrolCount_` is capped at 2 with no further escalation.)*
- [ ] **IG-22-011 P1** — Create false-positive prevention and clear player feedback for why they are being chased. *(Partial: **feedback** is done, and false positives are substantially reduced since IG-22-002 gave witnesses line of sight -- a witness inside a building no longer reports a car outside it. `PoliceSystem::GetOffence()` records what started the response and the HUD shows `WANTED - speeding` / `ran a red light` / `hit someone` rather than a bare "WANTED" -- the information already existed at the moment of detection, and withholding it is the single most common complaint about systems like this. The record clears when the chase resolves. What remains is that a witness **facing away** still sees the act, since there is no vision cone.)*
- [ ] **IG-22-012 P1** — Create mission overrides for scripted police states.
- [ ] **IG-22-013 P1** — Create checkpoint-safe wanted-state restoration.
- [ ] **IG-22-014 P1** — Create a police debug view for witness knowledge, dispatch, and pursuit state.
- [ ] **IG-22-015 P1** — Create test scenarios for unseen crime, lost sight, vehicle swap, interior hiding, and district escape.
- [ ] **IG-22-016 P2** — Create radio call audio and subtitle events.
- [ ] **IG-22-017 P2** — Create foot patrols and parked patrol vehicles as passive set dressing.
- [ ] **IG-22-018 P2** — Create non-lethal arrest behavior before advanced combat is implemented.
- [ ] **IG-22-019 P2** — Create civilian witness variability (some report faster/slower, or not at all).
- [ ] **IG-22-020 P3** — Non-goal: detective/investigation persistence across sessions is a distant post-slice idea (see group 40), not part of v1.
- [ ] **IG-22-021 P1** — Define the scope, public API, and explicit non-goals of the crime/offense detector.
- [ ] **IG-22-022 P1** — Define versioned configuration/data for the crime/offense detector.
- [ ] **IG-22-023 P1** — Implement the smallest deterministic reference path for the crime/offense detector.
- [ ] **IG-22-024 P1** — Add focused unit tests and one integration scenario for the crime/offense detector.
- [ ] **IG-22-025 P1** — Define save/checkpoint serialization and restoration for the crime/offense detector.
- [ ] **IG-22-026 P2** — Add logging, counters, and a debug view for the crime/offense detector.
- [ ] **IG-22-027 P2** — Define CPU/memory/latency budgets for the crime/offense detector and profile a worst-case scene.
- [ ] **IG-22-028 P2** — Document usage examples and common failure modes for the crime/offense detector.
- [ ] **IG-22-029 P1** — Define the scope, public API, and explicit non-goals of the witness system.
- [ ] **IG-22-030 P1** — Define versioned configuration/data for the witness system.
- [ ] **IG-22-031 P1** — Implement the smallest deterministic reference path for the witness system.
- [ ] **IG-22-032 P1** — Add focused unit tests and one integration scenario for the witness system.
- [ ] **IG-22-033 P1** — Define save/checkpoint serialization and restoration for the witness system.
- [ ] **IG-22-034 P2** — Add logging, counters, and a debug view for the witness system.
- [ ] **IG-22-035 P2** — Define CPU/memory/latency budgets for the witness system and profile a worst-case scene.
- [ ] **IG-22-036 P2** — Document usage examples and common failure modes for the witness system.
- [ ] **IG-22-037 P1** — Define the scope, public API, and explicit non-goals of the police dispatcher.
- [ ] **IG-22-038 P1** — Define versioned configuration/data for the police dispatcher.
- [ ] **IG-22-039 P1** — Implement the smallest deterministic reference path for the police dispatcher.
- [ ] **IG-22-040 P1** — Add focused unit tests and one integration scenario for the police dispatcher.
- [ ] **IG-22-041 P1** — Define save/checkpoint serialization and restoration for the police dispatcher.
- [ ] **IG-22-042 P2** — Add logging, counters, and a debug view for the police dispatcher.
- [ ] **IG-22-043 P2** — Define CPU/memory/latency budgets for the police dispatcher and profile a worst-case scene.
- [ ] **IG-22-044 P2** — Document usage examples and common failure modes for the police dispatcher.
- [ ] **IG-22-045 P1** — Define the scope, public API, and explicit non-goals of the pursuit coordinator.
- [ ] **IG-22-046 P1** — Define versioned configuration/data for the pursuit coordinator.
- [ ] **IG-22-047 P1** — Implement the smallest deterministic reference path for the pursuit coordinator.
- [ ] **IG-22-048 P1** — Add focused unit tests and one integration scenario for the pursuit coordinator.
- [ ] **IG-22-049 P1** — Define save/checkpoint serialization and restoration for the pursuit coordinator.
- [ ] **IG-22-050 P2** — Add logging, counters, and a debug view for the pursuit coordinator.
- [ ] **IG-22-051 P2** — Define CPU/memory/latency budgets for the pursuit coordinator and profile a worst-case scene.
- [ ] **IG-22-052 P2** — Document usage examples and common failure modes for the pursuit coordinator.
- [ ] **IG-22-053 P1** — Define the scope, public API, and explicit non-goals of the arrest and resolution flow.
- [ ] **IG-22-054 P1** — Define versioned configuration/data for the arrest and resolution flow.
- [ ] **IG-22-055 P1** — Implement the smallest deterministic reference path for the arrest and resolution flow.
- [ ] **IG-22-056 P1** — Add focused unit tests and one integration scenario for the arrest and resolution flow.
- [ ] **IG-22-057 P1** — Define save/checkpoint serialization and restoration for the arrest and resolution flow.
- [ ] **IG-22-058 P2** — Add logging, counters, and a debug view for the arrest and resolution flow.
- [ ] **IG-22-059 P2** — Define CPU/memory/latency budgets for the arrest and resolution flow and profile a worst-case scene.
- [ ] **IG-22-060 P2** — Document usage examples and common failure modes for the arrest and resolution flow.
