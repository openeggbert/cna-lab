-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M147 (MAP9) — Lua child-tile generator for level 9 ("metro/county" scale
-- detail within a parent tile, ~44 km per map.md's level table).
-- Registered as "lua.map.child.level9.default" so MapPipeline's level-aware
-- Lua lookup (M115) only tries this for level-9 tiles.
--
-- Mirrors continent.lua's (M115)/region.lua's (M116)/country.lua's (M146)
-- generic child-refinement technique (bilinear-from-parent elevation +
-- fade-to-zero fBm detail; temperature and moisture recomputed from world
-- position) for the same reasons those scripts already document (elevation
-- stays continuous with the parent, M112 sibling coherence).
--
-- map.md describes this level as "city footprint, suburbs, lakes." MAP19,
-- M317 wires in REAL rivers/lakes via the M313 Map::Hydrology Lua binding
-- layer (NEXT.md §9): Hydrology::trace()'s own basin-filling (M123,
-- fill_basin()) now runs directly over this tile's own elevation grid, and
-- every resulting HydrologyNetwork::lakes entry is exported as a named
-- MapFeature::Lake by appendHydrologyFeatures() -- this REPLACES the prior
-- "crude near-sea-level threshold" v1 lake proxy entirely (no more
-- LAKE_ELEV_MARGIN_M point-cloud collection below). City-footprint/suburb
-- placement still has no bound C++ algorithm (Settlements::place()'s real
-- suitability model needs shared, multi-tile data a per-tile script doesn't
-- have) -- see country.lua's own header note for the same reasoning.
--
-- V1 SIMPLIFICATIONS (read before extending):
-- * **City footprint = one center point, not a footprint polygon/mask.**
--   Real building-mass/footprint geometry is level 12's job per map.md's
--   own level table ("City -- street grid, zoning, parks, water",
--   `city.lua`, not built yet) -- attempting real footprint geometry here
--   would duplicate that future level's work at the wrong scale. The metro
--   center is emitted as an ordinary map:addCity(name, x, z, "") (default
--   -> FeatureType::City), same tier addCity gives any other city (no
--   FeatureType::Capital or FeatureType::Metro exists either -- same
--   documented limitation country.lua's own header note already makes).
-- * **Suburbs are satellite Town-tier sites sampled NEAR the metro
--   center** (within SUBURB_RADIUS_FRACTION of the tile size), not
--   region.lua's fully-independent random sampling across the whole tile
--   -- this is the one meaningful difference from region.lua's own town
--   placement: "suburb" implies proximity to a city, so this script's
--   site search is centered on the metro center instead of uniform over
--   the whole grid. Still the same non-ocean/non-mountain suitability
--   check region.lua/country.lua both already use, not a new model.
-- * If no metro-center site can be found anywhere on this tile (e.g. an
--   all-ocean tile), no city/suburbs are emitted at all (mirrors country.
--   lua's own "no capital site -> no country" rule) -- lakes are still
--   checked independently, since a tile can have low-lying wet terrain
--   without having a viable city site.
--
-- KNOWN LIMITATIONS (same as the other 3 child scripts, repeated here since
-- this script doesn't require reading those first):
-- * ctx does not expose WorldConfig (sea level, equator/pole temperature) to
--   Lua generators yet — hardcodes the same defaults the other 3 scripts do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides (shared with a sibling) rely on this script's own
--   bilinear math matching what the sibling computes, same as the others.

local M = {}
M.id       = "lua.map.child.level9.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE              = 64
local SEA_LEVEL_M            = 0.0    -- WorldConfig default; see header note
local EQUATOR_TEMP_C         = 30.0
local POLE_TEMP_C            = -20.0
local MOUNTAIN_ELEV_M        = 2500.0 -- above sea level; mirrors BiomeClassifier
local DETAIL_OFFSET          = 2000.0 -- decorrelates detail noise (ctx.noise has one fixed seed)
local MOISTURE_OFFSET        = 6000.0 -- decorrelates moisture noise from detail noise
local SUBURBS_MIN            = 1
local SUBURBS_MAX            = 4
local MAX_SITE_ATTEMPTS      = 8      -- per site; gives up on that one site if none found
local SUBURB_RADIUS_FRACTION = 0.35   -- suburbs sampled within this fraction of the tile size of the metro center

-- MAP19, M317: metro-area rivers/lakes via the M313 binding layer. Offset
-- (+600) sits clear of this file's own city/suburb-naming draws
-- (ctx.variation, ctx.variation+1..+n).
local RIVER_NAME_OFFSET      = 600

-- Bilinear interpolation of a {w=,h=,data={...}} field-grid table at
-- fractional position (fx, fy). Mirrors ChildGenerator.cpp's private helper
-- of the same name/shape (also used by continent.lua/region.lua/country.lua).
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

-- One suitable (non-ocean, non-mountain) random site anywhere on the tile,
-- or nil if none found within MAX_SITE_ATTEMPTS tries. Same suitability
-- check region.lua's/country.lua's own site placement already uses.
local function find_site(ctx, elevation, W, H, child_x0, child_y0, child_size_m)
    for _attempt = 1, MAX_SITE_ATTEMPTS do
        local gx = ctx.randomInt(0, W - 1)
        local gy = ctx.randomInt(0, H - 1)
        local elev_above_sea = elevation[gy * W + gx + 1] - SEA_LEVEL_M
        if elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M then
            local wx = child_x0 + (gx + 0.5) * child_size_m / W
            local wy = child_y0 + (gy + 0.5) * child_size_m / H
            return wx, wy
        end
    end
    return nil, nil
end

-- One suitable site within `radius_m` of (center_wx, center_wz), clamped to
-- this tile's own bounds -- the "suburb near a city" search this script
-- adds on top of find_site()'s plain whole-tile search.
local function find_site_near(ctx, elevation, W, H, child_x0, child_y0, child_size_m,
                               center_wx, center_wz, radius_m)
    for _attempt = 1, MAX_SITE_ATTEMPTS do
        local angle = ctx.random() * 2.0 * math.pi
        local dist  = ctx.random() * radius_m
        local wx = math.max(child_x0, math.min(child_x0 + child_size_m - 1e-6,
                                                 center_wx + math.cos(angle) * dist))
        local wz = math.max(child_y0, math.min(child_y0 + child_size_m - 1e-6,
                                                 center_wz + math.sin(angle) * dist))
        local gx = math.max(0, math.min(W - 1, math.floor((wx - child_x0) / child_size_m * W)))
        local gy = math.max(0, math.min(H - 1, math.floor((wz - child_y0) / child_size_m * H)))
        local elev_above_sea = elevation[gy * W + gx + 1] - SEA_LEVEL_M
        if elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M then
            return wx, wz
        end
    end
    return nil, nil
end

function M.generate(ctx, map)
    local parent = ctx.parent
    if not parent then return end  -- level 9 always has a parent; defensive only

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

    -- MAP19, M317: metro-area rivers/lakes, traced/carved before
    -- setBiomeField() so classification sees the final, carved elevation.
    local rivers = map:traceRivers(W, H, elevation)
    map:carveRivers(W, H, elevation, rivers)

    -- setBiomeField() default-derives all 4 edges from this tile's own FINAL
    -- elevation (M115, MapBuilder.cpp) — see continent.lua's header note for
    -- why this script (like the others) must not call setEdge() itself.
    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- MAP19, M317/M316: names the rivers/lakes just traced above -- this is
    -- the ONLY lake source now (see header note on why the prior
    -- LAKE_ELEV_MARGIN_M point-cloud proxy was removed).
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

    -- City footprint (center point) + suburbs sampled near it. See header
    -- note: no site anywhere on the tile means no city here at all.
    local city_x, city_z = find_site(ctx, elevation, W, H, child_x0, child_y0, child_size_m)
    if city_x ~= nil then
        local city_name = names.city(parent.culture, ctx.variation)
        map:addCity(city_name, city_x, city_z, "")

        local suburb_radius = child_size_m * SUBURB_RADIUS_FRACTION
        local n = ctx.randomInt(SUBURBS_MIN, SUBURBS_MAX)
        for i = 1, n do
            local sub_x, sub_z = find_site_near(ctx, elevation, W, H, child_x0, child_y0, child_size_m,
                                                 city_x, city_z, suburb_radius)
            if sub_x ~= nil then
                local suburb_name = names.city(parent.culture, ctx.variation + i)
                map:addCity(suburb_name, sub_x, sub_z, "town")
            end
        end
    end

    map:setMetadata(M.id, parent.culture)
end

return M
