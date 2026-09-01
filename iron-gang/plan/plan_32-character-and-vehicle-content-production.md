# 32. Character and vehicle content production

[Back to master plan](../plan.md)

Create a coherent, modestly sized cast and vehicle fleet for a 15-20 mission campaign, hand-authored with shared standards and budgets. Each named character and each vehicle model gets three production tasks instead of the previous eight-step template.

## Shared foundations

- [ ] **IG-32-001 P0** — Produce one playable character on the standard skeleton.
- [ ] **IG-32-002 P0** — Produce one named mission NPC and three ambient civilian variants.
- [ ] **IG-32-003 P0** — Produce one vertical-slice sedan with separate wheels, doors, lights, seats, and collision.
- [ ] **IG-32-004 P0** — Produce required idle, walk, run, talk, enter, exit, sit, and drive animations.
- [ ] **IG-32-005 P1** — Define body, face, hair, clothing, accessory, and material variation rules.
- [ ] **IG-32-006 P1** — Define demographic variety appropriate to the fictional setting without tokenization.
- [ ] **IG-32-007 P1** — Define character LOD, texture, bone, material, and animation budgets.
- [ ] **IG-32-008 P1** — Define vehicle scale, wheel, seat, door, light, collider, and damage conventions.
- [ ] **IG-32-009 P1** — Create fictional manufacturer, model, year, class, and performance data for the vehicle roster.
- [ ] **IG-32-010 P1** — Create civilian, commercial, taxi, police, and mission vehicle categories.
- [ ] **IG-32-011 P1** — Create shared animation retarget and validation pipeline for every character on the standard skeleton.
- [ ] **IG-32-012 P1** — Create facial/jaw fallback for characters without full facial rigs.
- [ ] **IG-32-013 P1** — Create crowd outfit palette and combination validation for ambient civilians.
- [ ] **IG-32-014 P1** — Create vehicle color/material variants through material instances.
- [ ] **IG-32-015 P1** — Create voice and character-ID binding shared across all named characters.
- [ ] **IG-32-016 P1** — Create named-character persistence metadata (for save games and mission state).
- [ ] **IG-32-017 P1** — Create seated pose validation for every vehicle interior class.
- [ ] **IG-32-018 P1** — Create collision and occlusion proxy assets for characters and vehicles.
- [ ] **IG-32-019 P2** — Create damaged clothing and vehicle variants only where a specific mission requires them.
- [ ] **IG-32-020 P2** — Create emergency/service vehicle sets (ambulance, fire) only if a mission needs one, after traffic and police are stable.

## Named characters

Roster for the 15-20 mission campaign: protagonist, named ally, named antagonist, bartender, mechanic, warehouse worker, civilian man, civilian woman, civilian elder, police officer, driver, pedestrian child substitute (or an explicit decision to omit children), mission guard, radio announcer.

- [ ] **IG-32-021 P0** — Author (brief, source model, retarget, skin weights) the playable protagonist.
- [ ] **IG-32-022 P0** — Assign LOD/texture variants, animation set, and behavior archetype for the protagonist.
- [ ] **IG-32-023 P1** — Record voice/provenance and capture a review reference for the protagonist.
- [ ] **IG-32-024 P1** — Author (brief, source model, retarget, skin weights) the named ally.
- [ ] **IG-32-025 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the named ally.
- [ ] **IG-32-026 P2** — Record voice/provenance and capture a review reference for the named ally.
- [ ] **IG-32-027 P1** — Author (brief, source model, retarget, skin weights) the named antagonist.
- [ ] **IG-32-028 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the named antagonist.
- [ ] **IG-32-029 P2** — Record voice/provenance and capture a review reference for the named antagonist.
- [ ] **IG-32-030 P1** — Author (brief, source model, retarget, skin weights) the bartender.
- [ ] **IG-32-031 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the bartender.
- [ ] **IG-32-032 P2** — Record voice/provenance and capture a review reference for the bartender.
- [ ] **IG-32-033 P1** — Author (brief, source model, retarget, skin weights) the mechanic.
- [ ] **IG-32-034 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the mechanic.
- [ ] **IG-32-035 P2** — Record voice/provenance and capture a review reference for the mechanic.
- [ ] **IG-32-036 P1** — Author (brief, source model, retarget, skin weights) the warehouse worker.
- [ ] **IG-32-037 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the warehouse worker.
- [ ] **IG-32-038 P2** — Record voice/provenance and capture a review reference for the warehouse worker.
- [ ] **IG-32-039 P1** — Author (brief, source model, retarget, skin weights) the civilian man variant.
- [ ] **IG-32-040 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian man variant.
- [ ] **IG-32-041 P2** — Record voice/provenance and capture a review reference for the civilian man variant.
- [ ] **IG-32-042 P1** — Author (brief, source model, retarget, skin weights) the civilian woman variant.
- [ ] **IG-32-043 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian woman variant.
- [ ] **IG-32-044 P2** — Record voice/provenance and capture a review reference for the civilian woman variant.
- [ ] **IG-32-045 P1** — Author (brief, source model, retarget, skin weights) the civilian elder variant.
- [ ] **IG-32-046 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the civilian elder variant.
- [ ] **IG-32-047 P2** — Record voice/provenance and capture a review reference for the civilian elder variant.
- [ ] **IG-32-048 P1** — Author (brief, source model, retarget, skin weights) the police officer.
- [ ] **IG-32-049 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the police officer.
- [ ] **IG-32-050 P2** — Record voice/provenance and capture a review reference for the police officer.
- [ ] **IG-32-051 P1** — Author (brief, source model, retarget, skin weights) the driver.
- [ ] **IG-32-052 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the driver.
- [ ] **IG-32-053 P2** — Record voice/provenance and capture a review reference for the driver.
- [ ] **IG-32-054 P1** — Decide whether to include a pedestrian child substitute or omit children entirely, then author (or document the omission) accordingly.
- [ ] **IG-32-055 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the pedestrian child substitute, if included.
- [ ] **IG-32-056 P2** — Record voice/provenance and capture a review reference for the pedestrian child substitute, if included.
- [ ] **IG-32-057 P1** — Author (brief, source model, retarget, skin weights) the mission guard.
- [ ] **IG-32-058 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the mission guard.
- [ ] **IG-32-059 P2** — Record voice/provenance and capture a review reference for the mission guard.
- [ ] **IG-32-060 P1** — Author (brief, source model, retarget, skin weights) the radio announcer.
- [ ] **IG-32-061 P1** — Assign LOD/texture variants, animation set, and behavior archetype for the radio announcer.
- [ ] **IG-32-062 P2** — Record voice/provenance and capture a review reference for the radio announcer.

## Vehicle roster

Roster for the campaign: vertical-slice sedan, compact sedan, large sedan, coupe, delivery van, flatbed truck, taxi, police sedan, luxury car, utility truck, parked wreck, mission getaway car.

- [ ] **IG-32-063 P0** — Author (identity/handling target, source model, separated parts) the vertical-slice sedan.
- [ ] **IG-32-064 P0** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the vertical-slice sedan.
- [ ] **IG-32-065 P1** — Record provenance and run handling/seat/lighting/damage validation for the vertical-slice sedan.
- [ ] **IG-32-066 P1** — Author (identity/handling target, source model, separated parts) the compact sedan.
- [ ] **IG-32-067 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the compact sedan.
- [ ] **IG-32-068 P2** — Record provenance and run handling/seat/lighting/damage validation for the compact sedan.
- [ ] **IG-32-069 P1** — Author (identity/handling target, source model, separated parts) the large sedan.
- [ ] **IG-32-070 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the large sedan.
- [ ] **IG-32-071 P2** — Record provenance and run handling/seat/lighting/damage validation for the large sedan.
- [ ] **IG-32-072 P1** — Author (identity/handling target, source model, separated parts) the coupe.
- [ ] **IG-32-073 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the coupe.
- [ ] **IG-32-074 P2** — Record provenance and run handling/seat/lighting/damage validation for the coupe.
- [ ] **IG-32-075 P1** — Author (identity/handling target, source model, separated parts) the delivery van.
- [ ] **IG-32-076 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the delivery van.
- [ ] **IG-32-077 P2** — Record provenance and run handling/seat/lighting/damage validation for the delivery van.
- [ ] **IG-32-078 P1** — Author (identity/handling target, source model, separated parts) the flatbed truck.
- [ ] **IG-32-079 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the flatbed truck.
- [ ] **IG-32-080 P2** — Record provenance and run handling/seat/lighting/damage validation for the flatbed truck.
- [ ] **IG-32-081 P1** — Author (identity/handling target, source model, separated parts) the taxi.
- [ ] **IG-32-082 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the taxi.
- [ ] **IG-32-083 P2** — Record provenance and run handling/seat/lighting/damage validation for the taxi.
- [ ] **IG-32-084 P1** — Author (identity/handling target, source model, separated parts) the police sedan.
- [ ] **IG-32-085 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the police sedan.
- [ ] **IG-32-086 P2** — Record provenance and run handling/seat/lighting/damage validation for the police sedan.
- [ ] **IG-32-087 P2** — Author (identity/handling target, source model, separated parts) the luxury car.
- [ ] **IG-32-088 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the luxury car.
- [ ] **IG-32-089 P2** — Record provenance and run handling/seat/lighting/damage validation for the luxury car.
- [ ] **IG-32-090 P2** — Author (identity/handling target, source model, separated parts) the utility truck.
- [ ] **IG-32-091 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the utility truck.
- [ ] **IG-32-092 P2** — Record provenance and run handling/seat/lighting/damage validation for the utility truck.
- [ ] **IG-32-093 P2** — Author (identity/handling target, source model, separated parts) the parked wreck.
- [ ] **IG-32-094 P2** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the parked wreck.
- [ ] **IG-32-095 P2** — Record provenance and run handling/seat/lighting/damage validation for the parked wreck.
- [ ] **IG-32-096 P1** — Author (identity/handling target, source model, separated parts) the mission getaway car.
- [ ] **IG-32-097 P1** — Create collision/COM/entry data, LOD/material variants, and configuration/sound for the mission getaway car.
- [ ] **IG-32-098 P2** — Record provenance and run handling/seat/lighting/damage validation for the mission getaway car.
