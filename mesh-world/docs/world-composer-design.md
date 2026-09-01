# MeshWorld — C++ World Composer Design (R113)

**Status:** Design only, not implemented. Written 2026-07-13 at explicit
user request, following the same "scope it, don't implement yet" pattern
`docs/procedural-model-generator-roadmap.md`'s own "G10 scoping" section
established for the interior-room pipeline — this is a design for a
FUTURE session (or several) to implement, not a promise about current
behavior.

**Why this exists now:** the user paused ongoing G-series work (which adds
MORE Lua generator content — `fence/wood_picket.lua`, `gate/simple.lua`,
`building/garage.lua`) to point out a real, documented conflict:
`mesh_world_revival.md` §1/§4.6/§26 and `plan.md`'s own R100 entry already
say the long-term runtime path favors a **C++ world composer** selecting
and placing validated MC3 assets, with Lua demoted to a migration/offline/
compatibility layer — not deleted, but no longer the primary path for new
content. Adding new Lua generators works against that agreed direction.
R113 (`mesh_world_revival.md` workstream R5, `plan.md`'s own R113 entry) is
the task that actually builds this composer. This document scopes it.

---

## 1. What "the composer" actually replaces, precisely

**It does not replace world generation.** The macro-world/terrain/hydrology/
settlement layers (`Map::` namespace, MAP-series) stay exactly as they are
— `mesh_world_revival.md` §10.1/§4.7 are explicit that these are not MC3
prefabs and should not become composer output. The composer only replaces
**how the CONTENT of an already-decided parcel/chunk gets filled in** —
today that's either a Lua script (`scene:addBox`/`callGenerator`) or a C++
class hand-rolling raw primitives with `std::mt19937_64` (e.g.
`SmallHouseBlockGenerator.cpp`). The composer's job is to select and place
*real, reusable, metadata-tagged MC3 definitions* instead.

## 2. What already exists — the composer is NOT starting from zero

Checked directly against the current codebase, not assumed:

- **`MeshCraft::Mc3::Mc3AssetMetadata`** (R111, `mesh-craft/mc3/include/
  MeshCraft/Mc3/Mc3AssetMetadata.hpp`) already has almost every field §6
  of the revival doc asks for: `category`/`subcategory`, `semanticTags`/
  `styleTags`/`regionTags`/`periodTags`, `nominalSize`, `boundsMin/Max`,
  `facing`, `sockets`, `materialSlots`, `collisionProxy`, `clearanceVolume`,
  `lods`, `instancingEligible`, `shadowPolicy`, `maxVisibilityDistanceM`,
  `selectionWeight`, `license`/`provenance`, `sourceGeneratorOrHash`,
  `semanticVersion`. **This is the composer's own asset-query schema,
  already built.** It is attached to `Mc3Object::assetMetadata` but
  nothing populates it on any REAL definition yet, and nothing queries it.
- **`Mc3ImportResolver`** (R101/R102, `mesh-craft/mc3/include/MeshCraft/
  Mc3/Mc3ImportResolver.hpp`) resolves `mc3lib://name@version` into a
  `std::map<std::string, std::shared_ptr<Mc3Object>>` of definitions. This
  is the composer's raw *source* of candidate assets once R112 content
  exists — the composer's `AssetRegistry` (§4 below) is a thin index ON
  TOP of resolved libraries, not a competing loader.
- **`ObjectDefinitionLibrary`** (`include/ObjectDefinitionLibrary.hpp`)
  already holds ~250+ hand-built native C++ `Mc3Object` definitions
  (benches, trees, furniture, vehicles...) with NO `assetMetadata`
  attached. This is real, working, already-instanced content — a cheap
  first population source for the composer's registry (see v1, §7) without
  waiting for R112's real `mc3lib` content batch.
- **`StyleRegistry`/`Style`** (G11, `include/StyleRegistry.hpp`/
  `include/Style.hpp`) is a flat `id/name/palette` (dotted-key → material
  id) map, already wired into `ctx.style` for Lua and read directly by
  `SmallHouseBlockGenerator.cpp`'s own `mat()` lambda. It does **not** yet
  have the region/period/wealth/facade-family/window-family/roof-family
  structure §11's JSON example shows — this is a real gap the composer
  needs (§5 below), not something to invent from scratch (extend `Style`,
  don't replace it — `ctx.style`'s existing Lua consumers keep working).
- **`ContainmentRuleRegistry`** (G13, `include/ContainmentRuleRegistry.hpp`)
  already does real count/probability/lod-gated child selection
  (`data/taxonomy/containment.json`). This is a plausible mechanism for
  "how many windows/props does this building get", reusable rather than
  inventing a second rule system — see §6.
- **`SmallHouseBlockGenerator.cpp`** already has a real, working "parcel"
  concept: 4 fixed `Plot{cx, cz, street_z}` structs per chunk, each
  independently sized/oriented/styled via `std::mt19937_64`. This is
  exactly the shape `Parcel` (§4) generalizes — not a from-scratch
  invention, a promotion of an existing pattern to a shared type.
- **`ChunkPipeline::get()`** (`src/ChunkPipeline.cpp`) already has a
  documented, tested fallback chain: Lua first, then
  `get_generator(zone, region)`'s C++ class. The composer becomes a NEW
  first link in that chain (§8) — additive, not a replacement of the
  existing chain's own safety net.

## 3. Non-goals for R113 itself

R113 is workstream R5 only. These are explicitly OTHER workstreams and
out of scope here even though they're related:

- **R112** (urban `mc3lib` content batch — real windows/doors/roofs/etc.
  as importable libraries): R113 depends on SOME content existing to
  select from, but the composer's *architecture* can and should be
  designed against `AssetRegistry`'s interface, not against R112's actual
  content, which doesn't exist yet. v1 (§7) works around this by tagging
  `ObjectDefinitionLibrary`'s existing definitions instead of waiting.
- **R115** (AI content factory): a separate, offline, non-runtime tool
  that would eventually PRODUCE more assets for R112's libraries. The
  composer only ever consumes already-validated assets; it has no AI
  dependency at all, matching the project's offline-first rule.
- **R119** (`.mc3pack` packaging, streaming, cache invalidation): the
  composer reads from whatever `AssetRegistry` is populated with in-
  process; how that registry gets persisted/packaged/streamed is R119's
  job, not R113's.
- **R120** (PBR/shadows/AO/backend tiers): rendering concerns, entirely
  downstream of what the composer places.

## 4. Core new types

```cpp
// include/AssetRegistry.hpp (NEW)
namespace MeshWorld {

// One indexed, queryable entry: a real Mc3Object definition plus its
// (possibly synthesized, see v1 §7) Mc3AssetMetadata. Deliberately does
// NOT own the Mc3Object -- AssetRegistry entries reference definitions
// that live in ObjectDefinitionLibrary or a resolved Mc3ImportResolver
// library map, exactly like those already do today.
struct AssetEntry {
    std::string id;                                  // e.g. "window.classic.residential_03"
    std::shared_ptr<MeshCraft::Mc3::Mc3Object> def;   // the actual geometry
    MeshCraft::Mc3::Mc3AssetMetadata meta;            // R111 schema
};

// Read-only after population (same "populate once at startup, safe to
// read from any thread after" contract ObjectDefinitionLibrary/
// MaterialRegistry/LuaGeneratorRegistry already use).
class AssetRegistry {
public:
    static AssetRegistry& instance();

    void register_asset(AssetEntry entry);

    // Query surface -- deliberately narrow for v1: exact category match
    // + all-of styleTags/regionTags + within a size tolerance. A richer
    // scoring/ranking query (weighted tag overlap, etc.) is a v2+
    // refinement once there's enough real content for "no exact match"
    // to be the common case rather than the only case.
    std::vector<const AssetEntry*> query(const std::string& category,
                                          const std::vector<std::string>& required_style_tags,
                                          std::optional<std::array<float,3>> max_size = std::nullopt) const;

    const AssetEntry* get(const std::string& id) const;   // nullptr if absent
    bool has_category(const std::string& category) const; // cheap "is this worth trying" check

private:
    std::unordered_map<std::string, AssetEntry> by_id_;
    std::unordered_multimap<std::string, std::string> by_category_; // category -> ids
};

} // namespace MeshWorld
```

```cpp
// include/StyleProfile.hpp (NEW -- extends, does not replace, Style/StyleRegistry)
namespace MeshWorld {

// mesh_world_revival.md §11's own JSON example, promoted to a real type.
// A Style (G11, palette-only) MAY be referenced from a StyleProfile's own
// materialStyleId field for backward compatibility with existing Lua
// ctx.style consumers -- StyleProfile is additive, not a breaking change
// to Style/StyleRegistry.
struct StyleProfile {
    std::string id;               // e.g. "central_europe_1900_middle"
    std::string region;           // "central_europe"
    std::string period;           // "1890_1930"
    std::string wealth;           // "poor" | "middle" | "wealthy"
    std::string facadeFamily;     // style tag matched against AssetEntry.meta.styleTags
    std::string windowFamily;
    std::string roofFamily;
    std::string materialStyleId;  // optional: an existing StyleRegistry Style id, for palette fallback
};

class StyleProfileRegistry {
public:
    static StyleProfileRegistry& instance();
    void register_profile(StyleProfile p);
    const StyleProfile* get(const std::string& id) const;
    // Picks a deterministic profile for a given seed -- mirrors
    // ContentPackLoader's own "load once, query many times" registries.
    const StyleProfile* pick_for(std::uint64_t seed) const;
private:
    std::unordered_map<std::string, StyleProfile> profiles_;
};

} // namespace MeshWorld
```

```cpp
// include/Parcel.hpp (NEW -- promotes SmallHouseBlockGenerator.cpp's own
// local `Plot` struct to a shared type other composers/generators can use)
namespace MeshWorld {

struct Parcel {
    float center_x{0}, center_z{0};   // chunk-local, meters
    float width{0}, depth{0};         // footprint, meters
    float rotation_y{0};              // degrees, orients the building's front
    float street_edge_z{0};           // which side faces the street (v1: N/S only, matching SmallHouseBlockGenerator's own existing simplification)
    Map::ZoneCandidate kind{Map::ZoneCandidate::none}; // reuses the M156 enum, not a new taxonomy
};

// v1: derives parcels the same way SmallHouseBlockGenerator.cpp's own
// fixed 4-plot layout does (see §7) -- a real street-first block/parcel
// subdivision algorithm (§11's own "road network -> intersections ->
// sidewalks/curbs -> blocks -> parcels" pipeline) is v2+ scope, not
// invented wholesale here.
std::vector<Parcel> derive_parcels(const ChunkContext& ctx);

} // namespace MeshWorld
```

```cpp
// include/BuildingComposer.hpp (NEW -- the actual R113 "world composer",
// deliberately named BuildingComposer for v1: it only composes buildings
// onto parcels, not the full "roads/street-furniture/vehicles" scope
// mesh_world_revival.md §11 eventually wants -- see §7/§10 for why)
namespace MeshWorld {

class BuildingComposer {
public:
    // Returns std::nullopt if it has nothing usable to offer for this
    // parcel/style (see §9 fallback behavior) -- caller falls through to
    // the existing Lua/C++ chain exactly as today, never a hard failure.
    std::optional<Mc3SceneBuilder> compose(const Parcel& parcel,
                                            const StyleProfile& style,
                                            std::uint64_t seed) const;
};

} // namespace MeshWorld
```

## 5. `StyleProfile` vs `Style`/`StyleRegistry`: why both

`Style` (G11) is a flat material palette — it answers "what material for
`park.lamp`". `StyleProfile` answers a different question — "what FAMILY
of window/door/roof is coherent together for this building" — a selection
concern, not a material concern. Collapsing them into one type would
force every Lua `ctx.style` consumer (already-shipped, already-tested) to
change shape for a feature only the composer needs. `StyleProfile.
materialStyleId` is the bridge: a composed building can still resolve its
raw-primitive materials (foundation slab, ground) through the SAME
`StyleRegistry` palette an equivalent Lua-authored building would use,
while its DEFINITION selection goes through the new `styleTags` matching
in `AssetRegistry::query()`.

## 6. Should the composer reuse `ContainmentRuleRegistry` (G13)?

**Yes, for "how many of this prop category", no for "which building
shape".** G13's `ContainmentRuleRegistry::children_of("zone.<x>")` already
answers "how many trees/benches/lamps does a park get" with real
probability/count-range/lod data — the composer's own street-furniture
placement (v2+, §10) should query the SAME registry rather than inventing
a parallel probability system, exactly the same reuse `park.lua` (G13)
already demonstrated for a Lua caller. `AssetRegistry::query()` then picks
WHICH concrete definition satisfies a `ContainmentRule.child` taxonomy id
(`"object.lamp_post"` -> some `AssetEntry` tagged `category="lamp_post"`)
— the two systems compose (rule says "how many", registry says "which
one"), they don't overlap in responsibility.

## 7. Incremental path: v1 → v2 → v3

**v1 — prove the mechanism on ONE parcel type, reusing existing content.**
Deliberately does not wait for R112:
1. Add `Mc3AssetMetadata` to a small, real slice of `ObjectDefinitionLibrary`'s
   existing definitions (start with `category="house"` — currently there
   are none; the closest existing content is Lua's `building/
   simple_house.lua`/`house/detached.lua`/`garage.lua`, which are NOT in
   `ObjectDefinitionLibrary` at all today — so v1's real first step is
   actually porting 2-3 of the simplest EXISTING house shapes into native
   C++ `Mc3Object` definitions with real metadata, matching how
   `ObjectDefinitionLibrary.cpp`'s own `make_bench()`/`make_lamp_post()`
   already "mirror" their Lua counterparts per that file's own comments).
2. Build `AssetRegistry`/`StyleProfile`/`Parcel`/`BuildingComposer` per §4,
   with `derive_parcels()` literally reusing `SmallHouseBlockGenerator.
   cpp`'s own 4-plot layout (same `{cx, cz, street_z}` values) so v1 is a
   drop-in ALTERNATIVE content source for an already-proven parcel layout,
   not a new layout algorithm too — isolates "does asset selection/
   placement work" from "does block subdivision work".
3. Wire into `ChunkPipeline::get()` (§8) for `RegionType::small_house_block`
   only, gated behind a single `WorldConfig` flag (e.g. `use_world_composer`,
   default `false`) so this is fully opt-in and risk-free to land — existing
   worlds/tests are byte-for-byte unaffected until explicitly enabled.
4. New tests mirror `ChunkPipelineDiagnosticsTests.cpp`'s own style: given
   a real registered house asset, the composer's output actually differs
   from `SmallHouseBlockGenerator`'s own random-primitive output (proves
   real asset selection happened, not a silent no-op); given an EMPTY
   registry, the composer returns `std::nullopt` and `ChunkPipeline` falls
   through to the existing chain unchanged (proves the fallback is real).

**v2 — real content + street-first generation.** Once R112 lands real
`mc3lib` urban libraries: `AssetRegistry` gets populated from resolved
imports (via `Mc3ImportResolver`), not just `ObjectDefinitionLibrary`.
`derive_parcels()` is replaced with a real block/parcel subdivision driven
by the actual road network (§11's street-first order) instead of
`SmallHouseBlockGenerator`'s fixed 4-plot layout. Extend to
`apartment_block`/`shop_street`. `StyleProfileRegistry` gets real,
authored profiles (not just one hardcoded default).

**v3 — cross-chunk continuity + full coherence.** Parcels/buildings that
span chunk boundaries (a real gap even v2 doesn't solve — see §11). Full
style-profile-driven material/definition coherence across an entire
district, not just per-building. Containment-rule-driven street furniture
density (§6) replacing v1/v2's fixed counts.

## 8. Where this plugs into `ChunkPipeline`

```text
ChunkPipeline::get(x, y)
  1. [NEW, v1+] if cfg.use_world_composer && a composer exists for
     (ctx.zone, ctx.region): try it. Some content -> validate, cache,
     return (source = ChunkDiagnostics::Source::Composer, a NEW enum
     value, alongside the existing Lua/CppFallback/Cache).
     Nothing (nullopt) -> fall through, exactly like a disabled flag.
  2. [EXISTING, unchanged] Lua: "lua.zone." + region, via
     LuaGeneratorRegistry -- unchanged behavior, unchanged tests.
  3. [EXISTING, unchanged] C++: get_generator(zone, region) -- unchanged
     behavior, unchanged tests, still the final safety net.
```

This ordering matters: the composer goes FIRST (not last), because it is
meant to eventually become primary — but starts fully inert (`nullopt`
for everything) until v1's opt-in flag and real registered assets exist,
so landing this scaffolding is zero-risk to every existing test and every
existing generated world.

## 9. Fallback behavior (explicitly part of R113's own scope)

- No `AssetEntry` matches the parcel's required category/style at all →
  `BuildingComposer::compose()` returns `std::nullopt` → `ChunkPipeline`
  falls through to Lua, then C++, exactly as it does today for every zone/
  region the composer doesn't touch yet. **Never** a hard error, **never**
  a placeholder box — those failure modes already exist and are handled
  by the pre-existing chain.
- A partial match (e.g. a house asset exists but no roof family tagged
  compatible with the chosen `StyleProfile`) — v1 does NOT attempt partial
  composition (mixing a selected prefab with Lua-sourced decorative
  pieces); that's real complexity worth deferring to v2+ once there's
  enough real content for partial matches to be the common case rather
  than a hypothetical.

## 10. Cross-chunk continuity (explicitly part of R113's own scope, NOT solved by v1/v2)

A building or parcel that spans a chunk boundary is a genuinely unsolved
problem in the EXISTING codebase too — `SmallHouseBlockGenerator.cpp`'s
own 4-plot layout is already fully contained within one 64m chunk, same
constraint `derive_parcels()` inherits in v1/v2. Real cross-chunk parcels
would need something like `MapBuilder::applyParentEdgeConstraints()`'s own
sibling-boundary-agreement technique (R124 fixed a real bug in exactly
this mechanism this session) generalized from terrain elevation to parcel/
building placement — a real, nontrivial v3+ design question, explicitly
NOT attempted in v1/v2. Documented here as a known limitation rather than
silently ignored, matching this project's own "honest scope-limitation"
convention (`docs/procedural-model-generator-roadmap.md`'s G10 section,
`docs/migration-stages.md`'s archival-readiness tables).

## 11. Open questions for whoever implements v1 (not decided here)

- Exact `WorldConfig` flag name/shape for `use_world_composer` gating.
- Whether `ChunkDiagnostics::Source` gets a new `Composer` enumerator now
  (v1) or whether v1 reuses `CppFallback` with a distinguishing
  `generator_id` prefix (e.g. `"composer.house.*"` vs `"cpp.chunk.*"`) —
  the latter avoids touching every existing `switch` over `Source` in
  tests/diagnostics code for a v1 that's disabled by default anyway.
- Whether the first 2-3 native house `Mc3Object` definitions (§7 step 1)
  should be genuinely NEW C++ code, or a mechanical port of
  `simple_house.lua`'s own geometry (same numbers, translated from Lua
  calls to `Mc3DocumentBuilder` calls directly) — the latter is faster and
  lower-risk (proven geometry) but produces a definition that's a
  duplicate of existing Lua content rather than new content; the former
  is slower but avoids the duplication. Recommend the mechanical port for
  v1 (de-risks the FIRST real usage of `Mc3AssetMetadata`+`AssetRegistry`
  together), revisit for v2's real content batch.
