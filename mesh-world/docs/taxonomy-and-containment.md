# MeshWorld — Taxonomy and Containment

This document describes the data-driven system for defining what regions/zones/buildings/rooms/objects can contain what.

**Status as of 2026-07-11**: `TaxonomyRegistry`/`ContainmentRuleRegistry` and
real `data/taxonomy/{taxonomy,containment}.json` content have existed for a
while (`ContentPackLoader::load_from_disk()` already loads both at startup)
— but the `ctx.containment.childrenOf(...)` Lua binding this document has
long described was never actually wired up, so no Lua script could ever
reach this system despite it being fully loaded and populated. Fixed this
session: `ctx.containment.childrenOf(parent_id)` is now real (object/chunk-
mode generators only — see `LuaRuntime.cpp`'s `build_ctx_table()`), returning
a plain array of rule tables with fields `parent`/`child`/`probability`/
`min_count`/`max_count`/`lod_max` (the REAL `ContainmentRule.hpp` field
names — this doc's C++ snippet below used to say `parent_id`/`child_id`,
also fixed). No existing generator has been migrated to consume it yet
(a separate, larger follow-up — see the roadmap doc's own backlog).

## Why data-driven

Without explicit containment rules, generators become ad-hoc: ParkGenerator randomly decides to add 3 benches because someone hardcoded that number. Instead, containment rules say "a park zone can contain 2–8 benches with probability 0.9" — generators read these rules rather than hardcoding them.

## Taxonomy nodes

Every entity in MeshWorld has a taxonomy ID with a kind prefix:

| Kind | Examples |
|------|---------|
| `world` | `world` |
| `region` | `region.city`, `region.wilderness`, `region.underground` |
| `zone` | `zone.residential_estate`, `zone.park`, `zone.forest`, `zone.old_town` |
| `building` | `building.house`, `building.apartment`, `building.shop` |
| `room` | `room.kitchen`, `room.bedroom`, `room.bathroom` |
| `place` | `place.playground`, `place.parking_lot`, `place.garden` |
| `path` | `path.sidewalk`, `path.gravel_path`, `path.road` |
| `object` | `object.chair`, `object.tree`, `object.fridge`, `object.bench` |

## Containment rules

A containment rule says: `parent` can contain `child`, with optional count range, probability, and LOD threshold.

### JSON format

```json
// data/taxonomy/containment.json
[
  {
    "parent": "region.city",
    "child": "zone.residential_estate",
    "probability": 0.4,
    "comment": "40% of city area is residential"
  },
  {
    "parent": "region.city",
    "child": "zone.park",
    "probability": 0.15,
    "min_count": 1,
    "max_count": 5
  },
  {
    "parent": "zone.residential_estate",
    "child": "object.bench",
    "min_count": 2,
    "max_count": 8,
    "probability": 0.9,
    "lod_max": 2
  },
  {
    "parent": "zone.park",
    "child": "object.tree",
    "min_count": 8,
    "max_count": 40,
    "probability": 1.0
  },
  {
    "parent": "room.kitchen",
    "child": "object.fridge",
    "min_count": 1,
    "max_count": 1,
    "probability": 0.95,
    "lod_max": 3
  },
  {
    "parent": "object.fridge",
    "child": "object.milk_box",
    "min_count": 0,
    "max_count": 3,
    "probability": 0.7,
    "lod_max": 4
  }
]
```

### Rule fields

| Field | Type | Description |
|-------|------|-------------|
| `parent` | string | Taxonomy ID of the container |
| `child` | string | Taxonomy ID of the contained entity |
| `probability` | float 0–1 | Probability this rule fires at all |
| `min_count` | int | Minimum instances when rule fires |
| `max_count` | int | Maximum instances |
| `lod_max` | int 0–4 | Maximum LOD at which this rule is applied |
| `conditions_json` | string | Future: style/zone conditions |

## LOD limits (infinite recursion prevention)

`lod_max` prevents infinite detail chains:

```
LOD 0: exterior block shape only (city block, building outline)
LOD 1: facade, main visible parts (windows, doors, roofs)
LOD 2: nearby props and outdoor details (benches, lamps, trees)
LOD 3: interiors when explicitly requested (rooms, furniture)
LOD 4: tiny objects only when very close (milk in fridge, books on shelf)
```

Without `lod_max`, a city generator could try:
city → house → kitchen → fridge → milk → label → ink → molecules...

With `lod_max: 4` on the milk entry and `lod_max: 3` on the fridge entry, the chain stops at the right level.

## C++ registry API

```cpp
// include/TaxonomyRegistry.hpp
struct TaxonomyNode {
    std::string id;
    std::string kind;
    std::string name;
    std::string description;
};

class TaxonomyRegistry {
public:
    static TaxonomyRegistry& instance();
    bool load_from_file(const std::filesystem::path& json_path);
    const TaxonomyNode* get(const std::string& id) const;
    std::vector<TaxonomyNode> all() const;
    bool has(const std::string& id) const;
};

// include/ContainmentRule.hpp + include/ContainmentRuleRegistry.hpp (real field names)
struct ContainmentRule {
    std::string parent;
    std::string child;
    float       probability{1.0f};
    int         min_count{0};
    int         max_count{1};
    int         lod_max{0};
};

class ContainmentRuleRegistry {
public:
    static ContainmentRuleRegistry& instance();
    void load(const std::filesystem::path& json_path);
    std::vector<ContainmentRule> children_of(const std::string& parent_id) const;
    bool can_contain(const std::string& parent_id, const std::string& child_id) const;
    std::vector<ContainmentRule> children_at_lod(const std::string& parent_id, int lod) const;
};
```

## Lua access in generators

`ctx.containment.childrenOf(parent_id)` is real as of 2026-07-11 (object/
chunk-mode generators only, i.e. scripts bound under `scene`, not `map`).
It returns the rules AS-IS from `ContainmentRuleRegistry`; the script still
does its own probability roll and `lod_max` gate, same as this example
always showed — only the field names below changed to match the real
struct (`rule.child`, not `rule.child_id`):

```lua
-- generators/lua/zone/park.lua
function M.generate(ctx, scene)
    local rules = ctx.containment.childrenOf("zone.park")
    for _, rule in ipairs(rules) do
        if rule.probability > ctx.random() and ctx.lod <= rule.lod_max then
            local count = ctx.randomInt(rule.min_count, rule.max_count)
            for i = 1, count do
                scene:callGenerator(rule.child, {variation=ctx.variation+i, style=ctx.style, lod=ctx.lod},
                                     { position = randomPos(ctx) })
            end
        end
    end
end
```

Note `ctx.random()`/`ctx.randomInt(...)` (function call syntax), not
`ctx:random()`/`ctx:randomInt(...)` (method-call syntax) — `ctx` is a plain
table with function-valued fields, not a userdata with methods; the
original example's colon syntax would have raised a Lua error. This example
is illustrative, not literally implemented in `zone/park.lua` yet — no
existing generator has been migrated to consume `ctx.containment` (a
separate follow-up task, see the roadmap doc).

## Data files location

```
data/taxonomy/
    taxonomy.json      — taxonomy node definitions
    containment.json   — containment rules
```

These files are human-readable, Git-diffable, and can be edited by Claude Code. They can also be packed into `meshworld_content.sqlite` for release builds.
