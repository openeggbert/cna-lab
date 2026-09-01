-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: park zone chunk (64x64 m)
--
-- G13 (2026-07-13): migrated to consume real ctx.containment.childrenOf()
-- data (docs/taxonomy-and-containment.md's own long-illustrated, never-
-- implemented example) instead of hand-hardcoded counts for every object
-- category. data/taxonomy/containment.json's real "zone.park" rules (tree/
-- bench/lamp_post/flower_bed/fountain, each with its own probability/count
-- range/lod_max) now genuinely drive how many of each get placed -- no
-- longer always exactly 16 trees/4 benches/6 lamps/4 flower beds/1
-- fountain. tree/bench/lamp_post/fountain delegate to the REAL registered
-- Lua object generators via scene:callGenerator() (a quality upgrade too --
-- e.g. lua.object.tree.deciduous's real icosphere canopy vs. this file's
-- old flattened-box approximation). object.flower_bed has NO matching Lua
-- generator anywhere in generators/lua/object/ (checked, not assumed) --
-- kept as this file's own inline geometry, still driven by the same
-- containment rule's count/probability roll, not silently unmigrated.
--
-- Position/orientation algorithms below are simple, clearly-scoped
-- generalizations of the OLD fixed-count layout (quadrant tree scatter,
-- a bench ring around the plaza, lamps along the path cross, flower beds
-- near the plaza corners) parameterized by count -- not a from-scratch
-- landscape-design pass, and not visually verified (no GPU in this
-- environment, same limitation every Lua content change this session has).

local M = {}
M.id       = "lua.zone.park"
M.version  = "0.2.0"
M.category = "zone"

-- Place a flower bed (flat box + colourful top). No generators/lua/object/
-- flower_bed*.lua exists yet, so this stays inline rather than a
-- scene:callGenerator() delegation -- an honest, documented gap, not
-- pretending full migration.
local FLOWER_COLORS = { "flowers_red", "flowers_yellow", "flowers_white", "flowers_purple" }
local function place_flower_bed(scene, id, x, z, w, d, color_i)
    local fc = FLOWER_COLORS[(color_i % #FLOWER_COLORS) + 1]
    scene:addBox(id .. "_soil",    { position={x, 0.05, z}, size={w, 0.10, d}, material="soil" })
    scene:addBox(id .. "_flowers", { position={x, 0.18, z}, size={w, 0.16, d}, material=fc    })
end

-- N trees scattered across the 4 quadrants (round-robin), each quadrant's
-- own safe area kept clear of the central plaza/path cross.
local function place_trees(scene, ctx, count, S, half, var)
    local margin = 8
    for i = 1, count do
        local quadrant = (i - 1) % 4
        local qx = (quadrant % 2 == 0) and 1 or -1   -- +1 = east half, -1 = west half
        local qz = (quadrant < 2) and 1 or -1        -- +1 = south half, -1 = north half
        local jx = margin + ctx.random() * (half - margin - 6)
        local jz = margin + ctx.random() * (half - margin - 6)
        local x  = half + qx * jx
        local z  = half + qz * jz
        local scale = 0.8 + ctx.random() * 0.5
        scene:callGenerator("lua.object.tree.deciduous",
            { variation = var + i, parameters = { scale = scale } },
            { position = { x, 0, z }, id = "tree_" .. i })
    end
end

-- N benches evenly spaced in a ring around the central plaza.
local function place_benches(scene, ctx, count, half)
    local radius = 6
    for i = 1, count do
        local angle = (i - 1) / count * 360
        local rad   = math.rad(angle)
        local x = half + radius * math.sin(rad)
        local z = half - radius * math.cos(rad)
        scene:callGenerator("lua.object.bench.simple",
            { variation = i, parameters = {} },
            { position = { x, 0, z }, rotation_y = angle, id = "bench_" .. i })
    end
end

-- N lamp posts distributed along the N-S and E-W paths.
local function place_lamps(scene, ctx, count, S, half)
    local n_vertical   = math.ceil(count / 2)
    local n_horizontal = count - n_vertical
    local margin = 8
    for i = 1, n_vertical do
        local t = n_vertical == 1 and 0.5 or (i - 1) / (n_vertical - 1)
        local z = margin + t * (S - margin * 2)
        scene:callGenerator("lua.object.lamp.post",
            { variation = i, parameters = {} },
            { position = { half, 0, z }, id = "lamp_v_" .. i })
    end
    for i = 1, n_horizontal do
        local t = n_horizontal == 1 and 0.5 or (i - 1) / (n_horizontal - 1)
        local x = margin + t * (S - margin * 2)
        scene:callGenerator("lua.object.lamp.post",
            { variation = i, parameters = {} },
            { position = { x, 0, half }, id = "lamp_h_" .. i })
    end
end

-- N flower beds arranged around the plaza corners (cycling through 4
-- corner anchors if count > 4).
local function place_flower_beds(scene, count, half, var)
    local corners = {
        { half - 5, half - 5 }, { half + 5, half - 5 },
        { half - 5, half + 5 }, { half + 5, half + 5 },
    }
    for i = 1, count do
        local c = corners[((i - 1) % #corners) + 1]
        place_flower_bed(scene, "bed_" .. i, c[1], c[2], 3, 3, var + i)
    end
end

-- 0 or 1 fountain at the plaza centre (its own containment rule has
-- min_count=0/max_count=1 -- a real park doesn't always get one).
local function place_fountain(scene, count, half)
    if count < 1 then return end
    scene:callGenerator("lua.object.fountain.classic",
        { variation = 0, parameters = {} },
        { position = { half, 0, half }, id = "fountain_" })
end

function M.generate(ctx, scene)
    local S    = ctx.chunk_size_m   -- 64
    local half = S / 2
    local var  = ctx.variation or 0

    -- Ground
    scene:addGround("grass")

    -- Central paved plaza (8x8 m) -- structural, always present regardless
    -- of whether a fountain ends up placed inside it.
    scene:addPlane("plaza", {
        position = { half - 4, 0.01, half - 4 },
        size     = { 8, 8 },
        material = "stone_pavement"
    })

    -- Diagonal paths (N-S and E-W, 2 m wide) -- structural, always present.
    scene:addPlane("path_ns", { position={half-1, 0.01, 0},      size={2, S},    material="stone_path" })
    scene:addPlane("path_ew", { position={0,      0.01, half-1}, size={S, 2},    material="stone_path" })

    -- Real containment data drives every object category below --
    -- data/taxonomy/containment.json's own "zone.park" rules.
    local rules = ctx.containment.childrenOf("zone.park")
    for _, rule in ipairs(rules) do
        if rule.probability > ctx.random() and ctx.lod <= rule.lod_max then
            local count = ctx.randomInt(rule.min_count, rule.max_count)
            if rule.child == "object.tree" then
                place_trees(scene, ctx, count, S, half, var)
            elseif rule.child == "object.bench" then
                place_benches(scene, ctx, count, half)
            elseif rule.child == "object.lamp_post" then
                place_lamps(scene, ctx, count, S, half)
            elseif rule.child == "object.fountain" then
                place_fountain(scene, count, half)
            elseif rule.child == "object.flower_bed" then
                place_flower_beds(scene, count, half, var)
            end
            -- An unrecognized rule.child (a future containment.json
            -- addition with no matching branch below) is silently skipped,
            -- not an error -- matches the doc's own "data-driven" framing:
            -- a new rule shouldn't crash generation just because no
            -- handler exists for it yet.
        end
    end

    scene:setMetadata({
        generator  = { id=M.id, version=M.version, category=M.category, language="lua" },
        chunk      = { x=ctx.chunk_x, y=ctx.chunk_y, size_m=S },
        -- R129 (zone-metadata bug fix) -- ctx.authored_zone, not ctx.zone:
        -- the latter may have been overridden by ChunkPipeline's own M157
        -- map-layer biome sampling, unrelated to this world's own flat config.
        generation = { variationInput=var, zone=ctx.authored_zone, region=ctx.region }
    })
end

return M
