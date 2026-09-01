# Risk Register & Definition-of-Success Checklist (R-series)

This is a **living document** for the R-series revival initiative (see
`plan.md` → "Revival architecture tasks (R-series)" and
`mesh_world_revival.md`). It turns the prose-only risks in
`mesh_world_revival.md` §23 ("Major Risks") and the prose-only success
criteria in §25-26 ("Definition of Success" / "Final Direction") into
checkable, owned, trackable items.

**Review cadence:** re-visit every row at each R107-R121 milestone
completion (i.e. whenever a workstream R0-R13 task from `plan.md` is
closed), and whenever a risk's trigger condition fires.

---

## 1. Risk register

Seeded from `mesh_world_revival.md` §23.1-23.7.

| ID | Risk (§23) | Mitigation | Owner | Trigger to revisit | Status |
|----|------------|------------|-------|---------------------|--------|
| RR1 | Too many assets without good placement rules (§23.1) — a large library will not automatically create a believable world | Semantic metadata, style profiles, placement rules, bounded kits, deterministic testing (R111, R113) | unassigned | First `mc3lib` content batch (R112) lands with >20 assets and no placement rules yet | open |
| RR2 | Visual variety becoming visual chaos (§23.2) — randomly mixing unrelated windows/roofs/materials looks worse than repetition | Style families, compatibility tags, building-level palette selection, district-level rules (R113, `StyleRegistry`/G11) | unassigned | City showcase (R114) shows >2 visually incompatible variant combinations in one screenshot review | open |
| RR3 | Performance collapse (§23.3) — dense assets create excessive draw calls / memory use | Definitions/instances, LOD, batching, GPU instancing, streaming, budgets, compiled MCB assets (R116, R119, R120) | unassigned | City showcase (R114) or nature libraries (R116) drop below the frame-budget/triangle/draw-call target captured in R107's baseline | open |
| RR4 | Premature deletion of Lua (§23.4) — removing Lua before migration destroys useful capability | Demote first, migrate gradually, preserve offline tooling, remove only after coverage exists (R105, R121) | unassigned | Any PR proposes deleting a Lua generator before its R107-tracked C++/MC3 equivalent has landed and been validated | open |
| RR5 | Two incompatible MC3 implementations (§23.5) — independent XML/JSON semantics diverge over time | One `Mc3Document` model, shared validators, shared compiler, round-trip tests (R109) | unassigned | `mc3.json` (R109) round-trip tests fail, or XML-only/JSON-only fields are added without updating both writers/readers | open |
| RR6 | Renderer work hiding content problems (§23.6) — PBR/post-processing cannot fix an empty scene | Build a strong vertical slice first; improve content/composition before relying on advanced rendering (sequence R114 before R120) | unassigned | Rendering work (R120) is scheduled/started before the city showcase (R114) reaches its Definition-of-Success bar (§2 below) | open |
| RR7 | Content work hiding renderer problems (§23.7) — dense geometry still looks flat without shadows/AO/materials | Evolve content and renderer in coordinated stages (alternate R11x content milestones with R120 rendering milestones) | unassigned | City showcase (R114) has dense, validated content but still looks visually flat in review (no shadows/AO/materials progress in >1 milestone) | open |

Status values: `open` (not yet mitigated / no owner assigned), `mitigating`
(owner assigned, mitigation in progress), `mitigated` (mitigation shipped,
monitoring only), `accepted` (explicitly accepted, will not be mitigated
further — requires a note explaining why).

**Note on RR1 (added 2026-07-18):** R125 (curating a bounded slice of
`object.md`'s `object.sqlite3` brainstorm database into
`data/taxonomy/taxonomy.json`/`data/taxonomy/containment.json`) is a
concrete, additional mitigation source for RR1 — real containment/
placement-rule data, not just more raw assets, directly addressing "a
large library will not automatically create a believable world." Status
stays `open` until R125 actually lands and R126/R127 consume it.

---

## 2. Definition-of-success checklist

Seeded from `mesh_world_revival.md` §25 ("Definition of Success") and §26
("Final Direction"). Each bullet becomes a checkable condition instead of
prose guidance.

- [ ] DS1 Mesh World no longer renders sparse test scenes as its normal
      city output (checked against R107's "before" baseline).
- [ ] DS2 A deterministic city showcase (R114) looks visibly rich and
      coherent in screenshot/video review. **2026-07-24 update:**
      `examples/city_showcase.json` is now a 7×7/49-chunk mixed city:
      small-house blocks, apartment blocks, two shop streets, and a square
      are all inside the app's radius-3 initial streaming area around its
      central crossroad, alongside the clocktower landmark. It validates at
      0 errors/0 warnings and `MeshWorldApp` has an explicit **Explore City
      Showcase** menu action that preserves this authored region layout.
      **2026-07-25 visual baseline:** an Xvfb/Mesa GLES screenshot of the
      live app now shows the road, building, vehicle, and street-furniture
      scene, and caught/fixed the persistent-map/planet-placement overlays
      that previously hid it. Human GPU/display review of richness and
      coherence remains `needs_human`, so this stays `[ ]`.
- [ ] DS3 Buildings reference reusable global definitions (R101/R102/R110)
      rather than embedding one-off geometry.
- [ ] DS4 Multiple style-controlled variants (R111/R113) are used across
      the showcase without visual chaos (cross-check against RR2).
      **2026-07-24 update:** the same 49-chunk scene now exercises the
      house/apartment/shop/square selection paths together, including
      lamps, mailboxes, vehicles, and yard/corner trees. The combined scene
      has been structurally validated but not visually reviewed for
      coherence/chaos; still `[ ]`.
- [ ] DS5 `mc3.json` and `mc3.xml` round-trip through one shared data model
      (R109) with passing round-trip tests.
- [ ] DS6 Libraries (`mc3lib`, R110) resolve imports deterministically —
      same input always yields same resolved dependency graph.
- [x] DS7 Standalone-compiled static assets include only their required
      dependencies (R132 dependency pruning), not the whole library.
      `MeshWorldPruneMc3Lib` retains the selected definition's transitive
      instance/variant/LOD graph plus document-owned material/texture
      dependencies and can write MC3 or MCB. Runtime-script assets are
      deliberately rejected until the separately scoped R104 path can expand
      their dynamic placements, rather than being mislabeled standalone.
- [ ] DS8 AI-generated content (R115) passes schema and semantic
      validation before being accepted into a library.
- [ ] DS9 Forests are built from instanced biome assets (R116), not
      hardcoded per-biome generators.
- [ ] DS10 Mountains combine procedural terrain with modular detail (R117),
      with no visible seam/slope repetition artifacts (cross-check RR2).
- [ ] DS11 Caves are assembled from validated modules (R118) with passing
      connection-socket/traversability/clearance validation.
- [ ] DS12 Runtime Lua is no longer required for the primary world path
      (R105/R113/R121) — Lua remains only as migration/offline tooling.
- [ ] DS13 LOD and instancing (R116/R117/R120) keep dense scenes within the
      performance budget from R107's baseline (cross-check RR3).
- [ ] DS14 CNA/NOXNA (R120) progressively improves lighting, materials,
      atmosphere, and GPU utilization without regressing DS2/DS13.
- [ ] DS15 No Claude/AI API calls happen at Mesh World runtime — AI content
      generation (R115) remains strictly an offline/dev-time tool
      (consistent with `plan.md`'s existing BYOK-only rule).
- [ ] DS16 Existing worlds/saves remain loadable throughout the Lua→C++
      migration (R105/R121) — no save-breaking change lands without an
      explicit migration/compat note (cross-check RR4).

---

## 3. How to use this document

1. When starting work on an R10x/R11x/R12x task from `plan.md`, skim the
   risk register for rows whose trigger condition the task touches.
2. When closing an R107-R121 milestone, update the relevant `Status`
   column(s) in §1 and tick any now-satisfied checklist items in §2.
3. If a new risk or success criterion emerges that isn't traceable to
   `mesh_world_revival.md` §23/§25/§26, add it here with a note on why it
   was added (don't silently expand scope — see RR1).
4. This document does not replace per-task acceptance criteria already
   described in each `plan.md` R-task; it tracks the cross-cutting,
   whole-initiative risks and goals that no single task fully owns.
