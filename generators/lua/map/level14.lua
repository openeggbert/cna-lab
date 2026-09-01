-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M299 (MAP18, 2026-07-10) — Lua child-tile generator for level 14
-- ("**Town / borough**" in map.md's own level table, content: "block
-- layout", ~1.4 km, directly below City=12). Registered as
-- "lua.map.child.level14.default" so MapPipeline's level-aware Lua lookup
-- only tries this for level-14 tiles.
--
-- Mirrors every other child script's generic bilinear-from-parent
-- elevation + fade-to-zero fBm detail technique, PLUS two things specific
-- to this level:
--
-- 1. Propagate an inherited "city" zone down from its parent (which, one
--    level up, is always city.lua's own level-12 output -- the direct
--    urban footprint city.lua's markUrbanCells() wrote). Without this,
--    BiomeClassifier's purely climate-driven classify() (no "city" output
--    at all) would silently overwrite it with a natural biome one level
--    down -- the exact bug class MAP24's M354 fixed for ChildGenerator.
--    cpp's generic C++ path and neighborhood.lua; any NEW dedicated script
--    for a level below city.lua bypasses that C++ path entirely and must
--    reimplement the same read itself (see plan.md's MAP18 intro note,
--    and neighborhood.lua's own M354 section for the pattern this copies).
-- 2. "Block layout": a coarse Map::ZoneCandidate grid (M156's own enum,
--    include/Map/ZoneCandidate.hpp), gated on the inherited city zone --
--    only cells the parent already said were urban get a block assignment,
--    matching city.lua's own "zoning is an urban-footprint concept" rule.
--    A coarser BLOCK_SPACING_CELLS than city.lua's own STREET_SPACING_CELLS
--    (8): this level's tile is 4x smaller than city.lua's (1.4 km vs
--    5.5 km), so the same cell-count spacing here means a genuinely coarser
--    real-world block size, appropriate for "block layout" sitting one
--    level below "street grid, zoning" (city.lua's own level-12 job) and
--    one level above "individual streets, named" (neighborhood.lua's own
--    level-15 job, 1 further down). No park exclusion or weighted-roll
--    variety here (that's still city.lua's own scope) -- every urban block
--    just gets ZONE_SMALL_HOUSE_BLOCK, the most common category city.lua's
--    own weighting already favors; a real V1, not a claim of full parity.
--
-- KNOWN LIMITATIONS (same as every other child script):
-- * ctx does not expose WorldConfig to Lua generators yet — hardcodes the
--   same defaults the other scripts do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides rely on this script's own bilinear math matching what
--   the sibling computes, same as the others.

local M = {}
M.id       = "lua.map.child.level14.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE       = 64
local SEA_LEVEL_M     = 0.0
local EQUATOR_TEMP_C  = 30.0
local POLE_TEMP_C     = -20.0
local DETAIL_OFFSET   = 2000.0
local MOISTURE_OFFSET = 6000.0
-- M354 (MAP24): must match ZoneType::city's ordinal (include/ZoneType.hpp)
-- -- city is that enum's first value, ordinal 0.
local CITY_ORDINAL    = 0
-- M156 Map::ZoneCandidate ordinals (include/Map/ZoneCandidate.hpp) -- must
-- match that enum's order exactly, same as city.lua's own copy.
local ZONE_NONE              = 0
local ZONE_SMALL_HOUSE_BLOCK = 1
local BLOCK_SPACING_CELLS    = 8

local function bilinear(grid, fx, fy)
    fx = math.max(0.0, math.min(grid.w - 1, fx))
    fy = math.max(0.0, math.min(grid.h - 1, fy))
    local x0 = math.floor(fx)
    local y0 = math.floor(fy)
    local x1 = math.min(x0 + 1, grid.w - 1)
    local y1 = math.min(y0 + 1, grid.h - 1)
    local tx = fx - x0
    local ty = fy - y0
    local function at(gx, gy) return grid.data[gy * grid.w + gx + 1] end
    local v00, v10 = at(x0, y0), at(x1, y0)
    local v01, v11 = at(x0, y1), at(x1, y1)
    local a = v00 + (v10 - v00) * tx
    local b = v01 + (v11 - v01) * tx
    return a + (b - a) * ty
end

function M.generate(ctx, map)
    local parent = ctx.parent
    if not parent then return end

    local W, H = GRID_SIZE, GRID_SIZE
    local cx = ctx.tile_x % 2
    local cy = ctx.tile_y % 2

    local planet_size_m = ctx.tile_size_m * (2 ^ ctx.level)
    local child_size_m  = ctx.tile_size_m
    local child_x0      = ctx.tile_x * child_size_m
    local child_y0      = ctx.tile_y * child_size_m

    local terrain_scale = math.max(child_size_m / 4.0, 1.0)
    local detail_amp    = math.min(child_size_m / 100.0, 500.0)

    local elevation, temperature, moisture = {}, {}, {}
    -- M354: nearest-neighbor sample of the parent's own classified biome
    -- per cell -- see CITY_ORDINAL/markUrbanCells() use below for why.
    local parent_city      = {}
    local any_parent_city  = false
    local parent_biome_w, parent_biome_h = parent.biome.w, parent.biome.h

    for gy = 0, H - 1 do
        for gx = 0, W - 1 do
            local pgx = cx * 32.0 + gx * (32.0 / (W - 1))
            local pgy = cy * 32.0 + gy * (32.0 / (H - 1))
            local base_elev = bilinear(parent.elevation, pgx, pgy)

            local pbx = math.max(0, math.min(parent_biome_w - 1, math.floor(pgx + 0.5)))
            local pby = math.max(0, math.min(parent_biome_h - 1, math.floor(pgy + 0.5)))
            local is_city = parent.biome.data[pby * parent_biome_w + pbx + 1] == CITY_ORDINAL
            -- markUrbanCells() round-trips through table_to_float_vec()/
            -- obj_float(), which only handles Lua numbers -- a raw boolean
            -- silently becomes 0 (obj_float()'s default), not an error, so
            -- this MUST be 1/0, not true/false (see neighborhood.lua's own
            -- identical comment).
            parent_city[gy * W + gx + 1] = is_city and 1 or 0
            if is_city then any_parent_city = true end

            local fx_fade = math.sin(math.pi * gx / (W - 1))
            local fy_fade = math.sin(math.pi * gy / (H - 1))
            local fade    = fx_fade * fx_fade * fy_fade * fy_fade

            local wx = child_x0 + (gx + 0.5) * child_size_m / W
            local wy = child_y0 + (gy + 0.5) * child_size_m / H

            local detail_fbm = ctx.noise(wx / terrain_scale + DETAIL_OFFSET, wy / terrain_scale)
            local elev = base_elev + fade * (detail_fbm - 0.5) * detail_amp

            local lat_factor = math.abs(wy / planet_size_m - 0.5) * 2.0
            local base_temp  = EQUATOR_TEMP_C + (POLE_TEMP_C - EQUATOR_TEMP_C) * lat_factor
            local elev_above = math.max(elev - SEA_LEVEL_M, 0.0)
            local temp       = base_temp - 6.5 * elev_above / 1000.0

            local moisture_fbm = ctx.noise(wx / terrain_scale + MOISTURE_OFFSET, wy / terrain_scale)
            local moist = math.max(0.0, math.min(1.0, moisture_fbm))

            local idx = gy * W + gx + 1
            elevation[idx]   = elev
            temperature[idx] = temp
            moisture[idx]    = moist
        end
    end

    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- M354: re-apply the inherited city override, otherwise this level's
    -- own natural classify() would silently erase it.
    if any_parent_city then
        map:markUrbanCells(parent_city)
    end

    -- "Block layout" (map.md's own words for this level): a coarse
    -- ZoneCandidate grid, gated on the inherited city zone -- see header
    -- note #2. Block boundaries are index-space only, same "no persisted
    -- block geometry" decoupling city.lua's own M156 note documents.
    if any_parent_city then
        local zone_candidates = {}
        for by_block = 0, math.ceil(H / BLOCK_SPACING_CELLS) - 1 do
            local gy0 = by_block * BLOCK_SPACING_CELLS
            local gy1 = math.min(gy0 + BLOCK_SPACING_CELLS, H) - 1
            for bx_block = 0, math.ceil(W / BLOCK_SPACING_CELLS) - 1 do
                local gx0 = bx_block * BLOCK_SPACING_CELLS
                local gx1 = math.min(gx0 + BLOCK_SPACING_CELLS, W) - 1

                local block_urban = false
                for gy = gy0, gy1 do
                    for gx = gx0, gx1 do
                        if parent_city[gy * W + gx + 1] == 1 then block_urban = true end
                    end
                end
                local candidate = block_urban and ZONE_SMALL_HOUSE_BLOCK or ZONE_NONE
                for gy = gy0, gy1 do
                    for gx = gx0, gx1 do
                        zone_candidates[gy * W + gx + 1] = candidate
                    end
                end
            end
        end
        map:setZoneCandidates(zone_candidates)
    end

    map:setMetadata(M.id, parent.culture)
end

return M
