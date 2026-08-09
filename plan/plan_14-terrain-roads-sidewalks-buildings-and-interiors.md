# 14. Terrain, roads, sidewalks, buildings, and interiors

[Back to master plan](../plan.md)

Iron City's districts and countryside chapters are hand-authored in Mesh Craft/MC3, not procedurally generated. This group defines the runtime data/loading systems that consume that hand-authored content (road graph, sidewalk graph, collision proxies, portals, terrain) plus the authoring conventions content creators follow so every district is consistent and drivable/walkable end to end.

## Road graph (consumed by traffic AI and vehicle physics)

- [ ] **IG-14-001 P0** — Define a road-graph data schema (nodes, directed segments, lanes, turn links, speed limits, stop lines) independent of the visual road mesh.
- [ ] **IG-14-002 P0** — Load a district's road graph at district-load time and expose it to traffic AI and navigation.
- [ ] **IG-14-003 P1** — Validate a loaded road graph for disconnected segments, dangling turn links, and missing speed limits.
- [ ] **IG-14-004 P1** — Add focused unit tests for road-graph loading and validation using a small hand-authored sample.
- [ ] **IG-14-005 P2** — Add a debug overlay that draws the loaded road graph over the 3D scene.
- [ ] **IG-14-006 P2** — Document the road-graph authoring convention (node/segment/lane naming, required metadata) for MC3 content authors.

## Sidewalk and pedestrian path graph

- [ ] **IG-14-007 P0** — Define a sidewalk/pedestrian-path data schema, separate from the road graph, with crossings and building-entrance connections.
- [ ] **IG-14-008 P0** — Load a district's sidewalk graph at district-load time and expose it to pedestrian navigation.
- [ ] **IG-14-009 P1** — Validate a loaded sidewalk graph for disconnected paths and crossings that don't align with road-graph crossings.
- [ ] **IG-14-010 P2** — Document the sidewalk-graph authoring convention for MC3 content authors.

## Buildings and collision proxies

- [ ] **IG-14-011 P0** — Define the convention for pairing a building's detailed render mesh with a separate simplified collision-proxy mesh in MC3.
- [ ] **IG-14-012 P0** — Load collision proxies at district-load time and register them with the physics world, independent of render LOD.
- [ ] **IG-14-013 P1** — Validate that every building placed in a district has a matching collision proxy; fail district validation if one is missing.
- [ ] **IG-14-014 P1** — Add focused unit tests for collision-proxy loading against a small hand-authored sample building.
- [ ] **IG-14-015 P2** — Document facade-module and prefab reuse conventions (windows, doors, roofs, stairs, elevators, fire escapes) so buildings share authored pieces instead of every building being bespoke.
- [ ] **IG-14-016 P2** — Document a building-to-navigation export convention (which surfaces block pedestrian navigation).

## Interiors and portals

- [ ] **IG-14-017 P0** — Define an interior/exterior visibility-portal schema linking an interior scene to its exterior entrance.
- [ ] **IG-14-018 P0** — Load and connect interior portals at district-load time so entering a doorway transitions into the interior scene.
- [ ] **IG-14-019 P1** — Define the "full interior" vs. "fake interior" (facade-only, no walkable inside) classification and how MC3 content is tagged for each.
- [ ] **IG-14-020 P1** — Author collision and simple baked lighting conventions for interior scenes.
- [ ] **IG-14-021 P1** — Add an integration test that enters and exits one hand-authored interior through its portal.
- [ ] **IG-14-022 P2** — Document the minimum interior requirement per mission district (which buildings need a full interior vs. a fake one).

## Terrain and countryside

- [ ] **IG-14-023 P1** — Define a terrain data representation (heightmap or hand-modeled mesh) for countryside districts.
- [ ] **IG-14-024 P1** — Load terrain height/material/collision data at district-load time for non-urban districts.
- [ ] **IG-14-025 P2** — Document countryside authoring conventions: road/fence/vegetation prop sets distinct from the urban prop set.
- [ ] **IG-14-026 P3** — Define water/shoreline/dock authoring and collision rules, only if the story requires a harbor district.
- [ ] **IG-14-027 P3** — Add simple wetness/puddle decal guidance, only if weather is later added; otherwise skip.

## Props, street furniture, and instancing

- [ ] **IG-14-028 P1** — Define a shared street-furniture prop set (lamps, benches, hydrants, signs, mailboxes) authored once and instanced at runtime via CNA's existing instancing support.
- [ ] **IG-14-029 P1** — Document the convention for marking repeated props as instancing-eligible in MC3.
- [ ] **IG-14-030 P2** — Define a district-specific vegetation/prop palette so districts read as visually distinct without bespoke assets per building.
- [ ] **IG-14-031 P2** — Define the scope limit for destructible props (which props can break vs. are always static) to avoid an unbounded destruction system.
- [ ] **IG-14-032 P2** — Document day/night emissive-window tagging convention for building facades, for later use by the lighting system.
- [ ] **IG-14-033 P3** — Document a lightweight distant-silhouette convention for a district as seen from a neighboring district, only if a mission needs a visible skyline.

## District content validation and budgets

- [ ] **IG-14-034 P0** — Extend `scripts/validate-mc3.sh` to check a district's road graph, sidewalk graph, collision proxies, and portals for consistency, not just XSD schema validity.
- [ ] **IG-14-035 P1** — Define a per-district triangle/texture-memory/collision-body budget aligned with `docs/performance-targets.md` and fail validation if exceeded.
- [ ] **IG-14-036 P1** — Add a validation check for unreachable rooms and disconnected streets within one district.
- [ ] **IG-14-037 P2** — Add a per-district completeness checklist (minimum road coverage, at least one accessible interior, sidewalks connected to every building entrance) enforced before a district is considered mission-ready.
- [ ] **IG-14-038 P2** — Record district content validation results in the asset registry alongside license/provenance data.

## Tests and documentation

- [ ] **IG-14-039 P0** — Add an integration test that loads one full hand-authored test district and asserts the player can walk every sidewalk and drive every road without a collision gap.
- [ ] **IG-14-040 P1** — Add a test that intentionally breaks a sample MC3 district (disconnected road, missing collision proxy) and asserts validation catches it.
- [ ] **IG-14-041 P2** — Add a soak test that walks/drives the full extent of one district without a crash or leak.
- [ ] **IG-14-042 P1** — Write a content-authoring guide covering all conventions above for anyone building a new district in MC3.
