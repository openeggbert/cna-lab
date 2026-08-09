# 06. CNA and cna-extended integration roadmap

[Back to master plan](../plan.md)

Integrate CNA's existing rendering capabilities and cna-extended's scene/ECS/collision facilities into Iron Gang instead of designing a new engine layer. Raw CNA already ships `PbrEffect`, `SkinnedPbrEffect`, `AnimationPlayer`, working shadow-mapping examples, GPU instancing across four backends, and a post-processing example. `cna-extended` (sibling dependency, already linked in `CMakeLists.txt`) supplies an ECS, a `Transform3` scene hierarchy, 3D collision/octree broadphase, and single-clip skinned-model playback. Nothing here should reimplement what already exists upstream.

- [ ] **IG-06-001 P0** — Inventory CNA's `PbrEffect`/`SkinnedPbrEffect` capabilities and confirm they cover Iron Gang's material needs before writing any new material code.
- [ ] **IG-06-002 P0** — Inventory CNA's shadow-mapping example and adapt it for a single dynamic sun and a bounded set of shadow casters.
- [ ] **IG-06-003 P0** — Inventory CNA's GPU instancing examples and select the instancing path for the project's chosen backend (EasyGL first).
- [ ] **IG-06-004 P1** — Inventory CNA's post-processing example and select only the minimal effects needed (fog, color grading, basic bloom).
- [ ] **IG-06-005 P0** — Wire `PbrEffect` into Iron Gang's renderer for city materials (stone, brick, plaster, metal, glass, wood, road).
- [ ] **IG-06-006 P0** — Wire the shadow-mapping example into Iron Gang for the single dynamic sun and limited shadow casters.
- [ ] **IG-06-007 P1** — Wire GPU instancing into Iron Gang for repeated props (lamps, windows, street furniture, vegetation).
- [ ] **IG-06-008 P1** — Wire `SkinnedPbrEffect` and `AnimationPlayer` into Iron Gang for the player and NPC skeletons.
- [ ] **IG-06-009 P0** — Adopt cna-extended's ECS (`World`/`Entity`/`ComponentManager`) as Iron Gang's entity/component model.
- [ ] **IG-06-010 P0** — Adopt cna-extended's `Transform3` hierarchy as Iron Gang's scene-graph and parenting primitive.
- [ ] **IG-06-011 P0** — Adopt cna-extended's 3D collision shapes and octree broadphase for world, vehicle, and pedestrian collision queries.
- [ ] **IG-06-012 P1** — Adopt cna-extended's skinned-model playback component/system for single-clip character and vehicle-part animation.
- [ ] **IG-06-013 P0** — Document the boundary between CNA, cna-extended, and Iron Gang-only game logic so future contributors do not duplicate upstream work.
- [ ] **IG-06-014 P1** — Add a regression test exercising cna-extended's 3D collision/octree at Iron Gang's expected object counts, since `World3DEXT` is newer and less battle-tested than cna-extended's 2D port.
- [ ] **IG-06-015 P1** — Add a fallback path or explicit failure message for any targeted CNA backend that lacks one of the rendering examples this project depends on.
- [ ] **IG-06-016 P1** — Track upstream CNA/cna-extended defects found during integration as minimal reproductions, rather than forking either repository.
- [ ] **IG-06-017 P1** — Add unit tests for Iron Gang's wrappers around the `PbrEffect`/shadow-mapping/instancing integration points.
- [ ] **IG-06-018 P1** — Add an integration scenario that renders one full district block with materials, the shadow path, and instanced props together.
- [ ] **IG-06-019 P1** — Add logging/counters for GPU resource usage (draw calls, instanced batches, shadow casters) to check against the `docs/performance-targets.md` budget.
- [ ] **IG-06-020 P1** — Document the specific CNA/cna-extended APIs used (file and class references) so later contributors do not reinvent them.
- [ ] **IG-06-021 P2** — Profile the first playable district against the 2-4GB RAM / 512MB-1GB VRAM target from `docs/performance-targets.md` and record results.
- [ ] **IG-06-022 P2** — Evaluate a minimal two-clip animation cross-fade only if cna-extended's single-clip playback proves visually insufficient for locomotion.
- [ ] **IG-06-023 P2** — Evaluate basic inverse kinematics (foot or hand placement) only after animation blending is stable and a real mission need appears.
- [ ] **IG-06-024 P3** — Evaluate reflection probes or environment-lighting helpers only if baked ambient lighting proves insufficient.
- [ ] **IG-06-025 P3** — Evaluate GPU-driven culling or bindless resource indexing only after CPU culling is measured and found insufficient.
- [ ] **IG-06-026 P3** — Evaluate virtual texturing only if real city texture memory pressure is measured and exceeds budget.
- [ ] **IG-06-027 P3** — Evaluate clustered/forward+ lighting only if a finished district's local-light count exceeds the simple forward-lit budget.
