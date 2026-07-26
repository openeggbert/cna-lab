# 06. CNA and cna-extended integration roadmap

[Back to master plan](../plan.md)

Integrate CNA's existing rendering capabilities and cna-extended's scene/ECS/collision facilities into Iron Shadows instead of designing a new engine layer. Raw CNA already ships `PbrEffect`, `SkinnedPbrEffect`, `AnimationPlayer`, working shadow-mapping examples, GPU instancing across four backends, and a post-processing example. `cna-extended` (sibling dependency, already linked in `CMakeLists.txt`) supplies an ECS, a `Transform3` scene hierarchy, 3D collision/octree broadphase, and single-clip skinned-model playback. Nothing here should reimplement what already exists upstream.

- [ ] **IS-06-001 P0** — Inventory CNA's `PbrEffect`/`SkinnedPbrEffect` capabilities and confirm they cover Iron Shadows' material needs before writing any new material code.
- [ ] **IS-06-002 P0** — Inventory CNA's shadow-mapping example and adapt it for a single dynamic sun and a bounded set of shadow casters.
- [ ] **IS-06-003 P0** — Inventory CNA's GPU instancing examples and select the instancing path for the project's chosen backend (EasyGL first).
- [ ] **IS-06-004 P1** — Inventory CNA's post-processing example and select only the minimal effects needed (fog, color grading, basic bloom).
- [ ] **IS-06-005 P0** — Wire `PbrEffect` into Iron Shadows' renderer for city materials (stone, brick, plaster, metal, glass, wood, road).
- [ ] **IS-06-006 P0** — Wire the shadow-mapping example into Iron Shadows for the single dynamic sun and limited shadow casters.
- [ ] **IS-06-007 P1** — Wire GPU instancing into Iron Shadows for repeated props (lamps, windows, street furniture, vegetation).
- [ ] **IS-06-008 P1** — Wire `SkinnedPbrEffect` and `AnimationPlayer` into Iron Shadows for the player and NPC skeletons.
- [ ] **IS-06-009 P0** — Adopt cna-extended's ECS (`World`/`Entity`/`ComponentManager`) as Iron Shadows' entity/component model.
- [ ] **IS-06-010 P0** — Adopt cna-extended's `Transform3` hierarchy as Iron Shadows' scene-graph and parenting primitive.
- [ ] **IS-06-011 P0** — Adopt cna-extended's 3D collision shapes and octree broadphase for world, vehicle, and pedestrian collision queries.
- [ ] **IS-06-012 P1** — Adopt cna-extended's skinned-model playback component/system for single-clip character and vehicle-part animation.
- [ ] **IS-06-013 P0** — Document the boundary between CNA, cna-extended, and Iron Shadows-only game logic so future contributors do not duplicate upstream work.
- [ ] **IS-06-014 P1** — Add a regression test exercising cna-extended's 3D collision/octree at Iron Shadows' expected object counts, since `World3DEXT` is newer and less battle-tested than cna-extended's 2D port.
- [ ] **IS-06-015 P1** — Add a fallback path or explicit failure message for any targeted CNA backend that lacks one of the rendering examples this project depends on.
- [ ] **IS-06-016 P1** — Track upstream CNA/cna-extended defects found during integration as minimal reproductions, rather than forking either repository.
- [ ] **IS-06-017 P1** — Add unit tests for Iron Shadows' wrappers around the `PbrEffect`/shadow-mapping/instancing integration points.
- [ ] **IS-06-018 P1** — Add an integration scenario that renders one full district block with materials, the shadow path, and instanced props together.
- [ ] **IS-06-019 P1** — Add logging/counters for GPU resource usage (draw calls, instanced batches, shadow casters) to check against the `docs/performance-targets.md` budget.
- [ ] **IS-06-020 P1** — Document the specific CNA/cna-extended APIs used (file and class references) so later contributors do not reinvent them.
- [ ] **IS-06-021 P2** — Profile the first playable district against the 2-4GB RAM / 512MB-1GB VRAM target from `docs/performance-targets.md` and record results.
- [ ] **IS-06-022 P2** — Evaluate a minimal two-clip animation cross-fade only if cna-extended's single-clip playback proves visually insufficient for locomotion.
- [ ] **IS-06-023 P2** — Evaluate basic inverse kinematics (foot or hand placement) only after animation blending is stable and a real mission need appears.
- [ ] **IS-06-024 P3** — Evaluate reflection probes or environment-lighting helpers only if baked ambient lighting proves insufficient.
- [ ] **IS-06-025 P3** — Evaluate GPU-driven culling or bindless resource indexing only after CPU culling is measured and found insufficient.
- [ ] **IS-06-026 P3** — Evaluate virtual texturing only if real city texture memory pressure is measured and exceeds budget.
- [ ] **IS-06-027 P3** — Evaluate clustered/forward+ lighting only if a finished district's local-light count exceeds the simple forward-lit budget.
