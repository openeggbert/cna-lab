-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M301 (MAP18, 2026-07-10) — Lua child-tile generator for level 17
-- ("street cluster" in map.md's own level table, content: "per-chunk
-- RegionType + EdgeExits assignment", ~172 m, directly below
-- Neighborhood=15's own child, level 16). Registered as
-- "lua.map.child.level17.default" so MapPipeline's level-aware Lua lookup
-- only tries this for level-17 tiles.
--
-- Mirrors every other child script's generic bilinear-from-parent
-- elevation + fade-to-zero fBm detail technique, PLUS two things specific
-- to this level, the one immediately above chunk hand-off (level 18):
--
-- 1. Propagate an inherited "city" zone down from its parent -- one level
--    up is level16.lua's own output, which itself re-applies whatever ITS
--    parent chain (ultimately neighborhood.lua at 15, then city.lua at 12)
--    already established. Without this, BiomeClassifier's purely climate-
--    driven classify() (no "city" output at all) would silently overwrite
--    it with a natural biome one level down -- the exact bug class MAP24's
--    M354 fixed for ChildGenerator.cpp's generic C++ path and neighborhood.
--    lua; any NEW dedicated script for a level below city.lua bypasses
--    that C++ path entirely and must reimplement the same read itself (see
--    plan.md's MAP18 intro note, and neighborhood.lua's own M354 section
--    for the pattern this copies).
-- 2. Finer streets: this level's own map.md content ("per-chunk RegionType
--    + EdgeExits assignment") is what the chunk hand-off (M157/M159) reads
--    at level 18 to decide each chunk's road connectivity -- the street
--    data feeding that decision should be at least as fine as
--    neighborhood.lua's own (level 15), and this level is a further 2
--    steps down (4x finer physically: 172 m vs neighborhood.lua's 688 m),
--    so its own street grid uses an even tighter spacing than
--    neighborhood.lua's, duplicated from its exact align_grid_line()
--    technique (see neighborhood.lua's own header for the full reasoning:
--    a cheap greedy contour-following walk, not a full Dijkstra).
--    Unconditional on buildability alone, same as neighborhood.lua and
--    city.lua both already do -- NOT gated on the inherited city zone,
--    since a real street network also reaches rural roads outside a city's
--    own urban footprint.
--
-- KNOWN LIMITATIONS (same as every other child script):
-- * ctx does not expose WorldConfig to Lua generators yet — hardcodes the
--   same defaults the other scripts do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides rely on this script's own bilinear math matching what
--   the sibling computes, same as the others.

local M = {}
M.id       = "lua.map.child.level17.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE             = 64
local SEA_LEVEL_M           = 0.0
local EQUATOR_TEMP_C        = 30.0
local POLE_TEMP_C           = -20.0
local MOUNTAIN_ELEV_M       = 2500.0
local DETAIL_OFFSET         = 2000.0
local MOISTURE_OFFSET       = 6000.0
-- Tighter than neighborhood.lua's own (4): this level is 4x finer
-- physically, so the same cell-count spacing here is a genuinely finer
-- real-world street grid.
local STREET_SPACING_CELLS  = 2
local SAMPLE_STEP_CELLS     = 2
local MAX_DRIFT_CELLS       = 1
-- M162 fix (§5 #18): tile bounds are half-open [min,max) (MapValidator
-- rejects a point at exactly max_x/max_z), but MapBuilder::
-- deriveEdgeCrossings() only matches within 0.01 m of the true edge.
local EDGE_SNAP_EPS_M       = 0.001
-- M354 (MAP24): must match ZoneType::city's ordinal (include/ZoneType.hpp)
-- -- city is that enum's first value, ordinal 0.
local CITY_ORDINAL          = 0

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

-- M155: bends a straight axis-aligned grid line to loosely follow terrain.
-- Exact duplicate of neighborhood.lua's/city.lua's own align_grid_line()
-- -- see city.lua's header note for the full reasoning.
local function align_grid_line(elevation, W, H, vertical, fixed_index, child_x0, child_y0, child_size_m)
    local along_len = vertical and H or W
    local cross_len = vertical and W or H
    local function elev_at(along, cross)
        local gx = vertical and cross or along
        local gy = vertical and along or cross
        return elevation[gy * W + gx + 1]
    end
    local function world_point(along, cross, along_boundary)
        local gx = vertical and cross or along
        local gy = vertical and along or cross
        local px = child_x0 + (gx + 0.5) * child_size_m / W
        local pz = child_y0 + (gy + 0.5) * child_size_m / H
        if along_boundary ~= nil then
            if vertical then pz = along_boundary else px = along_boundary end
        end
        return {px, pz}
    end
    local along_min = vertical and child_y0 or child_x0
    local along_max = along_min + child_size_m - EDGE_SNAP_EPS_M

    local lo, hi = math.max(0, fixed_index - MAX_DRIFT_CELLS), math.min(cross_len - 1, fixed_index + MAX_DRIFT_CELLS)
    local points = {}
    local prev_cross = fixed_index
    local prev_elev  = elev_at(0, fixed_index)
    local last_along = 0
    for along = 0, along_len - 1, SAMPLE_STEP_CELLS do
        local best_cross, best_diff = prev_cross, math.huge
        for d = -1, 1 do
            local cand = prev_cross + d
            if cand >= lo and cand <= hi then
                local diff = math.abs(elev_at(along, cand) - prev_elev)
                if diff < best_diff then
                    best_diff, best_cross = diff, cand
                end
            end
        end
        prev_cross = best_cross
        prev_elev  = elev_at(along, best_cross)
        last_along = along
        local boundary = nil
        if along == 0 then boundary = along_min end
        if along == along_len - 1 then boundary = along_max end
        points[#points + 1] = world_point(along, best_cross, boundary)
    end
    if last_along ~= along_len - 1 then
        points[#points + 1] = world_point(along_len - 1, prev_cross, along_max)
    end
    return points
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
    local is_buildable  = {}
    local any_buildable = false
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

            local elev_above_sea = elev - SEA_LEVEL_M
            local buildable = elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M
            is_buildable[idx] = buildable
            if buildable then any_buildable = true end
        end
    end

    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- M354: re-apply the inherited city override, otherwise this level's
    -- own natural classify() would silently erase it.
    if any_parent_city then
        map:markUrbanCells(parent_city)
    end

    if any_buildable then
        for gx = 0, W - 1, STREET_SPACING_CELLS do
            local street_name = names.street(parent.culture, ctx.variation + 200 + gx)
            local path = align_grid_line(elevation, W, H, true, gx, child_x0, child_y0, child_size_m)
            map:addStreet(street_name, path)
        end
        for gy = 0, H - 1, STREET_SPACING_CELLS do
            local street_name = names.street(parent.culture, ctx.variation + 300 + gy)
            local path = align_grid_line(elevation, W, H, false, gy, child_x0, child_y0, child_size_m)
            map:addStreet(street_name, path)
        end
    end

    map:setMetadata(M.id, parent.culture)
end

return M
