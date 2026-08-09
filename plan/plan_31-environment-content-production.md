# 31. Environment content production

[Back to master plan](../plan.md)

Produce reusable, optimized, licensed environment content, hand-authored in Mesh Craft/MC3, for a multi-district-plus-countryside campaign built one district at a time. Naming, collision, and validation conventions live in group 09 — this file does not repeat them per asset class, only references them.

## Vertical slice and district foundations

- [ ] **IG-31-001 P0** — Choose the vertical-slice district and produce a blockout map.
- [ ] **IG-31-002 P0** — Produce one bar/meeting interior, one garage, one warehouse, and connecting streets for the vertical slice.
- [ ] **IG-31-003 P0** — Produce LOD and collision for every vertical-slice environment asset.
- [ ] **IG-31-004 P0** — Register and approve every vertical-slice asset license/provenance record.
- [ ] **IG-31-005 P1** — Create a district palette (architecture, road materials, signage, vegetation, props) for each district before authoring its content.
- [ ] **IG-31-006 P1** — Create reusable building facade kits shared across districts with district-specific palette swaps.
- [ ] **IG-31-007 P1** — Create reusable interior wall, floor, ceiling, door, stair, and trim kits.
- [ ] **IG-31-008 P1** — Create prop placement rules and density budgets per district.
- [ ] **IG-31-009 P1** — Create district signage and fictional business identities.
- [ ] **IG-31-010 P1** — Create material libraries with consistent texel density across all districts.
- [ ] **IG-31-011 P1** — Create damage/dirt variation rules applied through material variants, not unique per-instance assets.
- [ ] **IG-31-012 P1** — Review repeated silhouettes and visual monotony once a district's blockout is populated.
- [ ] **IG-31-013 P1** — Produce the countryside district(s) content plan (terrain tiles, rural roads, farm/industrial props) alongside the urban districts.
- [ ] **IG-31-014 P2** — Create waterfront/rail/industrial expansion kits only for the specific districts that need them.
- [ ] **IG-31-015 P2** — Create weather variants after base assets for a district are approved.
- [ ] **IG-31-016 P2** — Create night emissive variants and light-placement data for street lighting once the baked-lighting pass for a district is stable.
- [ ] **IG-31-017 P1** — Create fake-interior window cards/geometry for background buildings that are never entered.
- [ ] **IG-31-018 P2** — Create skyline and far-proxy assets once a district's near geometry is stable.

## Asset class production

Each category below gets three tasks instead of the previous nine-step template per class: author it, validate/license it, and (later) measure its runtime cost. Naming and metadata conventions come from group 09.

### Infrastructure (roads, crossings, bridges)

- [ ] **IG-31-019 P0** — Author render geometry, collision, and LOD for the road segment asset class.
- [ ] **IG-31-020 P0** — Validate and record provenance/license for the road segment asset class.
- [ ] **IG-31-021 P2** — Measure runtime cost of the road segment asset class in a representative repeated scene.
- [ ] **IG-31-022 P0** — Author render geometry, collision, and LOD for the intersection asset class.
- [ ] **IG-31-023 P0** — Validate and record provenance/license for the intersection asset class.
- [ ] **IG-31-024 P2** — Measure runtime cost of the intersection asset class in a representative repeated scene.
- [ ] **IG-31-025 P0** — Author render geometry, collision, and LOD for the sidewalk module asset class.
- [ ] **IG-31-026 P0** — Validate and record provenance/license for the sidewalk module asset class.
- [ ] **IG-31-027 P2** — Measure runtime cost of the sidewalk module asset class in a representative repeated scene.
- [ ] **IG-31-028 P1** — Author, validate, and license the road marking asset class.
- [ ] **IG-31-029 P2** — Author, validate, and license the bridge module asset class, only if a district's design needs one.
- [ ] **IG-31-030 P2** — Author, validate, and license the tunnel module asset class, only if a district's design needs one.
- [ ] **IG-31-031 P2** — Author, validate, and license the dock module asset class, only if a waterfront district needs one.

### Buildings

- [ ] **IG-31-032 P0** — Author render geometry, collision, and LOD for the building facade asset class.
- [ ] **IG-31-033 P0** — Validate and record provenance/license for the building facade asset class.
- [ ] **IG-31-034 P2** — Measure runtime cost of the building facade asset class in a representative repeated scene.
- [ ] **IG-31-035 P1** — Author, validate, and license the roof module asset class.
- [ ] **IG-31-036 P0** — Author render geometry, collision, and LOD for the window module asset class.
- [ ] **IG-31-037 P0** — Validate and record provenance/license for the window module asset class.
- [ ] **IG-31-038 P0** — Author render geometry, collision, and LOD for the door module asset class.
- [ ] **IG-31-039 P0** — Validate and record provenance/license for the door module asset class.
- [ ] **IG-31-040 P1** — Author, validate, and license the stair module asset class.
- [ ] **IG-31-041 P1** — Author, validate, and license the interior room shell asset class for mission-critical interiors.
- [ ] **IG-31-042 P2** — Measure runtime cost of the building-kit asset classes (roof/window/door/stair/interior shell) in a representative repeated scene.

### Interior and industrial props

- [ ] **IG-31-043 P1** — Author, validate, and license the warehouse prop asset class.
- [ ] **IG-31-044 P1** — Author, validate, and license the garage prop asset class.
- [ ] **IG-31-045 P1** — Author, validate, and license the bar furniture asset class.
- [ ] **IG-31-046 P2** — Author, validate, and license the industrial machine asset class, only for districts/missions that need one.
- [ ] **IG-31-047 P2** — Measure runtime cost of the interior/industrial prop classes in a representative repeated scene.

### Street furniture and utilities

- [ ] **IG-31-048 P0** — Author render geometry, collision, and LOD for the street lamp asset class as a definition/instance prefab.
- [ ] **IG-31-049 P0** — Validate and record provenance/license for the street lamp asset class.
- [ ] **IG-31-050 P0** — Author, validate, and license the traffic sign asset class as a definition/instance prefab.
- [ ] **IG-31-051 P0** — Author, validate, and license the bench asset class as a definition/instance prefab.
- [ ] **IG-31-052 P1** — Author, validate, and license the fence asset class as a definition/instance prefab.
- [ ] **IG-31-053 P1** — Author, validate, and license the remaining small street-furniture prop set (hydrant, mailbox, utility pole, overhead cable, drain) as shared definition/instance prefabs.
- [ ] **IG-31-054 P2** — Measure runtime cost of the street-furniture prop set in a representative repeated scene, and confirm instancing is actually used at runtime (see group 06/07 instancing integration).

### Vegetation and distant dressing

- [ ] **IG-31-055 P1** — Author, validate, and license a tree/shrub/grass-patch vegetation set as shared definition/instance prefabs, with district-appropriate variants (urban vs. countryside).
- [ ] **IG-31-056 P2** — Author, validate, and license the far skyline proxy asset class once a district's near geometry is stable.
- [ ] **IG-31-057 P2** — Measure runtime cost of the vegetation and skyline-proxy sets in a representative repeated scene.

## Content gate

- [ ] **IG-31-058 P0** — Run every environment asset in this file through the group 09 content validation gates before it is considered production-ready.
