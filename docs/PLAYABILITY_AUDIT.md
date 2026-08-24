# Human Playability Audit

Black Pine's automated scenario proves that all required items, flags, rules,
hazards and endings can be reached. It does **not** by itself prove that a new
player can read the artwork, discover the intended route and finish the game
without knowing internal target IDs. This audit tracks that second standard.

## Definition of done for a region

A region is human-playable only when all of the following are true:

- its routes match `GAME_DESIGN.md`, including branches and return paths;
- every door, path, ladder, hatch and dangerous route has a visible affordance;
- collectible items are visibly placed where the story says they are;
- important mechanisms have recognisable code-drawn artwork, not only a generic
  hotspot mark;
- F1 describes the next physical action in terms visible on screen;
- death resumes from a safe checkpoint without discarding story progress;
- the physical scenario test traverses the same route a player uses;
- representative before/after states have been rendered and inspected.

## Current regional status

| Region | Screens | Status | Remaining work |
|---|---:|---|---|
| Storm gate and caretaker hub | 1–12 | In progress | Screens 1–11 have a safe opening route, visible phone and pickups, an enterable cabin, distinct cabin/radio/shed/cellar interiors and authored hub portals. Screen 12 still needs its three-way service-road fork and forestry barrier. |
| Relay yard and local power | 13–24 | Scripted, not audited | Replace the linear route with the yard/trench/generator branches; draw each repair target and verify the full fuse/cable/fuel/battery/transformer order manually. |
| North forest | 25–38 | Scripted, not audited | Author the rescue and cache branches, visible clues, harmless bear solution and lookout route. |
| Quarry and ravine | 39–50 | Scripted, not audited | Author vertical rope/hoist topology, water route, guard positions and crusher-safe path. |
| Logging railway | 51–63 | Scripted, not audited | Build the sawmill/camp/engine hub, visibly persistent five-part engine repair and trestle sequence. |
| Reservoir and dam | 64–75 | Scripted, not audited | Build gatehouse/turbine/pump branches, water-state artwork and safe return paths. |
| Mine and underground power | 76–90 | Scripted, not audited | Build mine branches, readable gas/electrical warnings, lift and switchgear topology. |
| Observatory | 91–102 | Scripted, not audited | Build courtyard diversion routes, archive/dome branches and room-specific interiors. |
| Nightjar bunker | 103–115 | Scripted, not audited | Build access-lock sequence, patrol containment, laboratory branches and Kline rescue route. |
| Summit and transmitter | 116–124 | Scripted, not audited | Build the branching climb, readable lightning shelters, persistent grounding repairs and three ending presentations. |

## Completed systemic fixes

- Directly collectible objects use visible 16-colour code-drawn object classes.
- The emergency phone remains visible before and after the opening call.
- The live-feeder route cannot trap a new player in a death/new-game loop.
- Death resumes at the latest Explore2D travel-anchor checkpoint.
- The caretaker cabin is entered through visible doors with ENTER.
- Screens 6–11 now form the authored caretaker hub instead of a false linear
  sequence: porch, cabin, radio nook, cellar, tool shed and weather mast.

The next implementation pass starts with screen 12 and the relay-yard hub,
because that is the first remaining place where the documented geography and
the playable geography diverge.
