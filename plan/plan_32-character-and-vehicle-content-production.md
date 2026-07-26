# 32. Character and vehicle content production

[Back to master plan](../plan.md)

Create a coherent, modestly sized cast and vehicle fleet for a 15-20 mission campaign, hand-authored with shared standards and budgets. Each named character and each vehicle model gets three production tasks instead of the previous eight-step template.

## Shared foundations

- [ ] **IS-32-001 P0** — Produce one playable character on the standard skeleton.
- [ ] **IS-32-002 P0** — Produce one named mission NPC and three ambient civilian variants.
- [ ] **IS-32-003 P0** — Produce one vertical-slice sedan with separate wheels, doors, lights, seats, and collision.
- [ ] **IS-32-004 P0** — Produce required idle, walk, run, talk, enter, exit, sit, and drive animations.
- [ ] **IS-32-005 P1** — Define body, face, hair, clothing, accessory, and material variation rules.
- [ ] **IS-32-006 P1** — Define demographic variety appropriate to the fictional setting without tokenization.
- [ ] **IS-32-007 P1** — Define character LOD, texture, bone, material, and animation budgets.
- [ ] **IS-32-008 P1** — Define vehicle scale, wheel, seat, door, light, collider, and damage conventions.
- [ ] **IS-32-009 P1** — Create fictional manufacturer, model, year, class, and performance data for the vehicle roster.
- [ ] **IS-32-010 P1** — Create civilian, commercial, taxi, police, and mission vehicle categories.
- [ ] **IS-32-011 P1** — Create shared animation retarget and validation pipeline for every character on the standard skeleton.
- [ ] **IS-32-012 P1** — Create facial/jaw fallback for characters without full facial rigs.
- [ ] **IS-32-013 P1** — Create crowd outfit palette and combination validation for ambient civilians.
- [ ] **IS-32-014 P1** — Create vehicle color/material variants through material instances.
- [ ] **IS-32-015 P1** — Create voice and character-ID binding shared across all named characters.
- [ ] **IS-32-016 P1** — Create named-character persistence metadata (for save games and mission state).
- [ ] **IS-32-017 P1** — Create seated pose validation for every vehicle interior class.
- [ ] **IS-32-018 P1** — Create collision and occlusion proxy assets for characters and vehicles.
- [ ] **IS-32-019 P2** — Create damaged clothing and vehicle variants only where a specific mission requires them.
- [ ] **IS-32-020 P2** — Create emergency/service vehicle sets (ambulance, fire) only if a mission needs one, after traffic and police are stable.

## Named characters

Roster for the 15-20 mission campaign: protagonist, named ally, named antagonist, bartender, mechanic, warehouse worker, civilian man, civilian woman, civilian elder, police officer, driver, pedestrian child substitute (or an explicit decision to omit children), mission guard, radio announcer.

- [ ] **IS-32-021 P0** — Author (brief, source model, retarget, skin weights) the playable protagonist.
- [ ] **IS-32-022 P0** — Assign LOD/texture variants, animation set, and behavior archetype for the protagonist.
- [ ] **IS-32-023 P1** — Record voice/provenance and capture a review reference for the protagonist.
- [ ] **IS-32-024 P1** — Author (brief, source model, retarget, skin weights) the named ally.
- [ ] **IS-32-025 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the named ally.
- [ ] **IS-32-026 P2** — Record voice/provenance and capture a review reference for the named ally.
- [ ] **IS-32-027 P1** — Author (brief, source model, retarget, skin weights) the named antagonist.
- [ ] **IS-32-028 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the named antagonist.
- [ ] **IS-32-029 P2** — Record voice/provenance and capture a review reference for the named antagonist.
- [ ] **IS-32-030 P1** — Author (brief, source model, retarget, skin weights) the bartender.
- [ ] **IS-32-031 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the bartender.
- [ ] **IS-32-032 P2** — Record voice/provenance and capture a review reference for the bartender.
- [ ] **IS-32-033 P1** — Author (brief, source model, retarget, skin weights) the mechanic.
- [ ] **IS-32-034 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the mechanic.
- [ ] **IS-32-035 P2** — Record voice/provenance and capture a review reference for the mechanic.
- [ ] **IS-32-036 P1** — Author (brief, source model, retarget, skin weights) the warehouse worker.
- [ ] **IS-32-037 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the warehouse worker.
- [ ] **IS-32-038 P2** — Record voice/provenance and capture a review reference for the warehouse worker.
- [ ] **IS-32-039 P1** — Author (brief, source model, retarget, skin weights) the civilian man variant.
- [ ] **IS-32-040 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian man variant.
- [ ] **IS-32-041 P2** — Record voice/provenance and capture a review reference for the civilian man variant.
- [ ] **IS-32-042 P1** — Author (brief, source model, retarget, skin weights) the civilian woman variant.
- [ ] **IS-32-043 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian woman variant.
- [ ] **IS-32-044 P2** — Record voice/provenance and capture a review reference for the civilian woman variant.
- [ ] **IS-32-045 P1** — Author (brief, source model, retarget, skin weights) the civilian elder variant.
- [ ] **IS-32-046 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian elder variant.
- [ ] **IS-32-047 P2** — Record voice/provenance and capture a review reference for the civilian elder variant.
- [ ] **IS-32-048 P1** — Author (brief, source model, retarget, skin weights) the police officer.
- [ ] **IS-32-049 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the police officer.
- [ ] **IS-32-050 P2** — Record voice/provenance and capture a review reference for the police officer.
- [ ] **IS-32-051 P1** — Author (brief, source model, retarget, skin weights) the driver.
- [ ] **IS-32-052 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the driver.
- [ ] **IS-32-053 P2** — Record voice/provenance and capture a review reference for the driver.
- [ ] **IS-32-054 P1** — Decide whether to include a pedestrian child substitute or omit children entirely, then author (or document the omission) accordingly.
- [ ] **IS-32-055 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the pedestrian child substitute, if included.
- [ ] **IS-32-056 P2** — Record voice/provenance and capture a review reference for the pedestrian child substitute, if included.
- [ ] **IS-32-057 P1** — Author (brief, source model, retarget, skin weights) the mission guard.
- [ ] **IS-32-058 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the mission guard.
- [ ] **IS-32-059 P2** — Record voice/provenance and capture a review reference for the mission guard.
- [ ] **IS-32-060 P1** — Author (brief, source model, retarget, skin weights) the radio announcer.
- [ ] **IS-32-061 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the radio announcer.
- [ ] **IS-32-062 P2** — Record voice/provenance and capture a review reference for the radio announcer.

## Vehicle roster

Roster for the campaign: vertical-slice sedan, compact sedan, large sedan, coupe, delivery van, flatbed truck, taxi, police sedan, luxury car, utility truck, parked wreck, mission getaway car.

- [ ] **IS-32-063 P0** — Author (identity/handling target, source model, separated parts) the vertical-slice sedan.
- [ ] **IS-32-064 P0** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the vertical-slice sedan.
- [ ] **IS-32-065 P1** — Record provenance and run handling/seat/lighting/damage validation for the vertical-slice sedan.
- [ ] **IS-32-066 P1** — Author (identity/handling target, source model, separated parts) the compact sedan.
- [ ] **IS-32-067 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the compact sedan.
- [ ] **IS-32-068 P2** — Record provenance and run handling/seat/lighting/damage validation for the compact sedan.
- [ ] **IS-32-069 P1** — Author (identity/handling target, source model, separated parts) the large sedan.
- [ ] **IS-32-070 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the large sedan.
- [ ] **IS-32-071 P2** — Record provenance and run handling/seat/lighting/damage validation for the large sedan.
- [ ] **IS-32-072 P1** — Author (identity/handling target, source model, separated parts) the coupe.
- [ ] **IS-32-073 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the coupe.
- [ ] **IS-32-074 P2** — Record provenance and run handling/seat/lighting/damage validation for the coupe.
- [ ] **IS-32-075 P1** — Author (identity/handling target, source model, separated parts) the delivery van.
- [ ] **IS-32-076 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the delivery van.
- [ ] **IS-32-077 P2** — Record provenance and run handling/seat/lighting/damage validation for the delivery van.
- [ ] **IS-32-078 P1** — Author (identity/handling target, source model, separated parts) the flatbed truck.
- [ ] **IS-32-079 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the flatbed truck.
- [ ] **IS-32-080 P2** — Record provenance and run handling/seat/lighting/damage validation for the flatbed truck.
- [ ] **IS-32-081 P1** — Author (identity/handling target, source model, separated parts) the taxi.
- [ ] **IS-32-082 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the taxi.
- [ ] **IS-32-083 P2** — Record provenance and run handling/seat/lighting/damage validation for the taxi.
- [ ] **IS-32-084 P1** — Author (identity/handling target, source model, separated parts) the police sedan.
- [ ] **IS-32-085 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the police sedan.
- [ ] **IS-32-086 P2** — Record provenance and run handling/seat/lighting/damage validation for the police sedan.
- [ ] **IS-32-087 P2** — Author (identity/handling target, source model, separated parts) the luxury car.
- [ ] **IS-32-088 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the luxury car.
- [ ] **IS-32-089 P2** — Record provenance and run handling/seat/lighting/damage validation for the luxury car.
- [ ] **IS-32-090 P2** — Author (identity/handling target, source model, separated parts) the utility truck.
- [ ] **IS-32-091 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the utility truck.
- [ ] **IS-32-092 P2** — Record provenance and run handling/seat/lighting/damage validation for the utility truck.
- [ ] **IS-32-093 P2** — Author (identity/handling target, source model, separated parts) the parked wreck.
- [ ] **IS-32-094 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the parked wreck.
- [ ] **IS-32-095 P2** — Record provenance and run handling/seat/lighting/damage validation for the parked wreck.
- [ ] **IS-32-096 P1** — Author (identity/handling target, source model, separated parts) the mission getaway car.
- [ ] **IS-32-097 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the mission getaway car.
- [ ] **IS-32-098 P2** — Record provenance and run handling/seat/lighting/damage validation for the mission getaway car.
