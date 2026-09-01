-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M116 (MAP7) — Lua child-tile generator for level 7 ("region/province"
-- scale detail within a parent tile, ~176 km per map.md's level table).
-- Registered as "lua.map.child.level7.default" so MapPipeline's level-aware
-- Lua lookup (M115) only tries this for level-7 tiles.
--
-- Mirrors continent.lua's (M115) generic child-refinement technique
-- (bilinear-from-parent elevation + fade-to-zero fBm detail; temperature and
-- moisture recomputed from world position) — see that script's header for
-- why this keeps elevation continuous with the parent and coherent between
-- siblings (M112).
--
-- map.md describes this level as "secondary rivers, town placement, terrain
-- detail." MAP19, M317 wires in real secondary rivers via the M313 Map::
-- Hydrology Lua binding layer (NEXT.md §9) -- traced/carved into this tile's
-- own elevation before setBiomeField(), same as continent.lua/country.lua.
-- No mountain ranges here: region tiles (~176 km) are small enough that any
-- real range influence should already be visible in the inherited parent
-- elevation (bilinear-from-parent, below); re-seeding brand new ranges at
-- this scale would look disconnected from whatever a parent already placed.
-- Town placement has no such dependency: MapBuilder::addCity(..., "town")
-- (previously unused by any real generator, same situation addMountainRange
-- was in before continent.lua) at a small number of randomly-sampled,
-- non-ocean, non-mountainous candidate sites.
--
-- KNOWN LIMITATIONS (same as continent.lua, repeated here since this script
-- doesn't require reading that one first):
-- * ctx does not expose WorldConfig (sea level, equator/pole temperature) to
--   Lua generators yet — hardcodes the same defaults planet.lua/continent.lua do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides (shared with a sibling) rely on this script's own
--   bilinear math matching what the sibling computes, same as continent.lua.
-- * Town site selection is a fixed number of random single-cell samples
--   rejecting ocean/mountain cells, not a real settlement-suitability model
--   (proximity to water, flat terrain radius, etc.) — a first, simple v1,
--   same spirit as continent.lua's mountain-range point cloud.

local M = {}
M.id       = "lua.map.child.level7.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE         = 64
local SEA_LEVEL_M       = 0.0    -- WorldConfig default; see header note
local EQUATOR_TEMP_C    = 30.0
local POLE_TEMP_C       = -20.0
local MOUNTAIN_ELEV_M   = 2500.0 -- above sea level; mirrors BiomeClassifier
local DETAIL_OFFSET     = 2000.0 -- decorrelates detail noise (ctx.noise has one fixed seed)
local MOISTURE_OFFSET   = 6000.0 -- decorrelates moisture noise from detail noise
local TOWNS_MIN         = 0
local TOWNS_MAX         = 2
local MAX_SITE_ATTEMPTS = 8       -- per town; gives up (places fewer) if no site found

-- MAP19, M317: secondary rivers via the M313 binding layer. Offset (+600)
-- sits clear of this file's own town-naming draws (ctx.variation+1..+n).
local RIVER_NAME_OFFSET = 600

-- Bilinear interpolation of a {w=,h=,data={...}} field-grid table at
-- fractional position (fx, fy). Mirrors ChildGenerator.cpp's private helper
-- of the same name/shape (also used by continent.lua).
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
    if not parent then return end  -- level 7 always has a parent; defensive only

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

    for gy = 0, H - 1 do
        for gx = 0, W - 1 do
            local pgx = cx * 32.0 + gx * (32.0 / (W - 1))
            local pgy = cy * 32.0 + gy * (32.0 / (H - 1))
            local base_elev = bilinear(parent.elevation, pgx, pgy)

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

    -- MAP19, M317: secondary rivers, traced/carved before setBiomeField()
    -- so classification sees the final, carved elevation.
    local rivers = map:traceRivers(W, H, elevation)
    map:carveRivers(W, H, elevation, rivers)

    -- setBiomeField() default-derives all 4 edges from this tile's own FINAL
    -- elevation (M115, MapBuilder.cpp) — see continent.lua's header note for
    -- why this script (like that one) must not call setEdge() itself.
    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- MAP19, M317/M316: names the rivers/lakes just traced above.
    map:appendHydrologyFeatures(rivers, parent.culture, ctx.variation + RIVER_NAME_OFFSET)

    -- MAP19, M315: coastal beach + swamp-flatness refinement.
    map:applyCoastalBeach()
    map:applySwampFlatnessCheck()

    -- M259/M274/M275 (2026-07-11): canyon carving + coastal relief
    -- refinement (tidal_flat/sea_cliff).
    map:applyCanyonCarving()
    map:applyCoastalReliefRefinement()

    -- M247 (2026-07-11): riparian_forest, needs the traced river network.
    map:applyRiparianForest(rivers)

    -- Town placement: a handful of random candidate sites, rejecting ocean
    -- and mountainous cells. Deterministic given ctx's seeded random draws.
    local n = ctx.randomInt(TOWNS_MIN, TOWNS_MAX)
    for i = 1, n do
        for _attempt = 1, MAX_SITE_ATTEMPTS do
            local gx  = ctx.randomInt(0, W - 1)
            local gy  = ctx.randomInt(0, H - 1)
            local elev = elevation[gy * W + gx + 1]
            local elev_above_sea = elev - SEA_LEVEL_M
            if elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M then
                local town_x = child_x0 + (gx + 0.5) * child_size_m / W
                local town_y = child_y0 + (gy + 0.5) * child_size_m / H
                local name = names.city(parent.culture, ctx.variation + i)
                map:addCity(name, town_x, town_y, "town")
                break
            end
        end
    end

    map:setMetadata(M.id, parent.culture)
end

return M
