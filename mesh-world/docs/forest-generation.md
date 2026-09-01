# MeshWorld — Forest Generation

A good forest generator does not randomly scatter trees. It uses layered generation to produce a believable, varied result.

## ForestGenerator composition

```
ForestGenerator
  ├── PathGenerator           — winding dirt path through the forest
  ├── ClearingGenerator       — 1–3 open grass clearings
  ├── TreeClusterGenerator    — dense tree groups
  │     └── TreeGenerator     — individual tree (trunk + canopy, varied species)
  ├── BushGenerator           — shrubs at clearing edges and path sides
  ├── GrassPatchGenerator     — grass/meadow patches
  ├── FlowerPatchGenerator    — flower clusters (spring/summer only)
  ├── RockGenerator           — boulders and rock formations
  └── FallenLogGenerator      — fallen log props
```

## Layers

Forest generation proceeds in layers, from large-scale structure to small details:

1. **Density map** — hash-based density grid; dense in center, sparse at edges
2. **Path network** — winding path from N/S/E/W exits if applicable
3. **Clearings** — 0–3 open circular areas, distributed pseudo-randomly
4. **Tree clusters** — dense groups avoiding clearings and paths
5. **Individual trees** — placed within clusters; species varies by zone style
6. **Understory** — bushes, ferns at cluster edges and clearing borders
7. **Ground cover** — grass patches, flower patches in clearings
8. **Props** — rocks, fallen logs, sparse mushrooms

## Lua implementation sketch

```lua
-- generators/lua/zone/forest.lua
local M = {}
M.id = "lua.zone.forest.temperate"

function M.generate(ctx, scene)
    local cs = scene:chunkSize()

    -- Ground
    scene:addPlane("ground", {
        position = { cs/2, 0, cs/2 },
        size     = { cs, cs },
        material = ctx.style.forest_ground or "soil_leaf"
    })

    -- Generate exits (paths)
    local exits = ctx.exits
    if exits.north_road or exits.north_path then
        scene:callGenerator("lua.path.forest_trail", ctx, {
            from = { cs/2, 0, 0 },
            to   = { cs/2, 0, cs/2 }
        })
    end

    -- Density-based tree placement
    local density = ctx:random(0.6, 0.9)
    local tree_count = math.floor(cs * cs * density / 25)

    for i = 1, tree_count do
        local x = ctx:random(2, cs - 2)
        local z = ctx:random(2, cs - 2)
        local scale = ctx:random(0.8, 1.3)
        local species = ctx.style.tree_species or "deciduous"
        scene:callGenerator("lua.object.tree." .. species, ctx, {
            position  = { x, 0, z },
            scale     = scale,
            variation = ctx.variation + i * 1009
        })
    end

    -- Clearings
    local clearing_count = ctx:randomInt(0, 2)
    for i = 1, clearing_count do
        scene:callGenerator("lua.place.clearing.forest", ctx, {
            position = { ctx:random(10, cs-10), 0, ctx:random(10, cs-10) },
            radius   = ctx:random(5, 12)
        })
    end

    -- Rocks and logs (LOD 2+)
    if ctx.lod >= 2 then
        for i = 1, ctx:randomInt(1, 4) do
            scene:callGenerator("lua.object.rock.boulder", ctx, {
                position = { ctx:random(3, cs-3), 0, ctx:random(3, cs-3) }
            })
        end
    end

    scene:setMetadata({
        generator  = { id = M.id, version = "0.1.0", language = "lua" },
        zone       = { type = "forest", density = density, tree_count = tree_count },
        generation = { variationInput = ctx.variation }
    })
end

return M
```

## Tree species by zone style

| Style | Tree species |
|-------|-------------|
| `central_europe_small_city` | oak, linden, birch, chestnut, maple |
| `nordic_town` | birch, spruce, pine, rowan |
| `desert_outpost` | palm, acacia |
| `jungle_village` | palm, banana, ficus, bamboo |

## Neighbor chunk connections

Forest chunks connect to neighbors via the `ctx.exits` table. If a neighbor is also forest, paths and tree density extend across borders. If a neighbor is city, tree density reduces toward the city edge (zone border transition).

## LOD rules for forests

| LOD | What is generated |
|-----|------------------|
| 0 | Ground plane, rough tree cluster shapes only |
| 1 | Individual trees (simplified canopy) |
| 2 | Bushes, grass patches, rocks |
| 3 | Fallen logs, flower patches, detailed tree bark |
| 4 | Tiny props (mushrooms, pinecones) — not yet planned |
