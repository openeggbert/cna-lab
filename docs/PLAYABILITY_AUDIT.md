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
| Storm gate and caretaker hub | 1–12 | Audited | Safe opening route, visible phone and pickups, enterable cabin, distinct cabin/radio/shed/cellar interiors, authored hub portals and a gated three-way service-road fork. |
| Relay yard and local power | 13–24 | Audited | The yard, trench, generator, battery, pump, transformer, workshop, hall and control-room branches use visible labelled portals. Repair targets have persistent before/after artwork and the physical scenario traverses the complete route. |
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
- Screen 12 is a real relay/forest fork. Its visible FOREST barrier stays
  closed until the direction trace is complete, then returns through screen 25.
- Screens 13–24 form the authored relay-yard hub. Every branch has a labelled
  entrance and return route, and F1 names those labels rather than internal
  screen numbers.
- Cable, fuse, battery, fuel, feeder, cabinet, Nightjar trunk, generator lever
  and trace-console states have dedicated 16-colour before/after drawings.
- The render-preview utility emits repaired-state frames for the relay region,
  and the scenario test now walks through the same portals as a player.

The next implementation pass starts with the North Forest (screens 25–38),
where the rescue/cache branches and Echo Grove route still differ from the
documented geography.
