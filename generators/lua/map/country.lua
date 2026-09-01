-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M146 (MAP9) — Lua child-tile generator for level 5 ("country" scale
-- detail within a parent tile). Registered as "lua.map.child.level5.default"
-- so MapPipeline's level-aware Lua lookup (M115) only tries this for
-- level-5 tiles.
--
-- Mirrors continent.lua's (M115)/region.lua's (M116) generic child-
-- refinement technique (bilinear-from-parent elevation + fade-to-zero fBm
-- detail; temperature and moisture recomputed from world position) for the
-- same reasons those two scripts already document (elevation stays
-- continuous with the parent, M112 sibling coherence).
--
-- plan.md describes this level as "borders + capital + trunk roads." This
-- is a genuinely different shape of task than continent.lua/region.lua: the
-- real C++ MAP9 algorithms it echoes (Settlements::place(), Countries::
-- grow(), Roads::build()) all operate over ONE shared grid/settlement list
-- spanning many tiles' worth of area at once (multiple capitals partition
-- a whole continent; Roads::build() connects settlements that already all
-- exist together). A single M.generate(ctx, map) call only ever sees this
-- one tile plus its parent -- no sibling or cross-tile capital data exists
-- to flood-fill borders against, or a settlement list to run Kruskal's
-- algorithm over -- Settlements::place()/Countries::grow()/Roads::build()
-- stay un-bound v1 Lua logic for that reason. MAP19, M317 DID add real
-- bindings for the 4 algorithm families that operate purely within one
-- tile's own elevation grid (Map::MountainRanges/Map::Hydrology, see the
-- mountain/river calls below) -- those don't have Countries::grow()'s
-- "needs several tiles' worth of shared state" problem, so there was no
-- reason to keep reimplementing them independently once the binding layer
-- existed (NEXT.md §9).
--
-- V1 SIMPLIFICATIONS (read before extending):
-- * **One country per land tile.** Real Countries::grow() needs multiple
--   capitals visible at once to flood-fill contested borders between them;
--   a per-tile script has no such view. Instead: if a capital site can be
--   found on this tile at all, the ENTIRE tile becomes that one country's
--   territory, exported as a single MapFeature::Border whose polygon is
--   just the tile's own 4 corners. This means country size is coupled to
--   level-5 tile size, not to any real geographic/political logic -- an
--   accepted placeholder, not a claim of correctness, the same spirit as
--   continent.lua's "not a traced ridge polyline, a first simple v1" note.
-- * **No FeatureType::Capital.** MapFeature's FeatureType only distinguishes
--   City/Town (see Settlements.hpp's own M137 note making the same
--   observation for the C++ side) -- the capital is emitted via
--   map:addCity(name, x, z, "city"), the closest available tier, same as
--   any other city. There is currently no way to tell a Border's capital
--   apart from an ordinary City feature by type alone; a reader has to
--   assume the first City inside a given Border's polygon is its capital.
-- * **Trunk roads are hub-and-spoke from the capital, not a Kruskal MST.**
--   Roads::build() (M143) finds a minimum spanning network over settlements
--   that already all exist together; here, the capital is placed first and
--   every minor town this same script places afterward gets one straight
--   road back to the capital via map:addRoad(). With only ever one hub per
--   tile, a spanning-tree algorithm would degenerate to exactly this shape
--   anyway -- Kruskal's machinery isn't needed for a single-hub star graph.
-- * Capital/town site selection reuses region.lua's exact suitability
--   check (non-ocean, non-mountain, random single-cell sampling) --
--   deliberately not the real elevation-cap/coastal/flatness suitability
--   model `Settlements::place()` (M138) implements in C++, for the same
--   "first simple v1" reasoning region.lua itself already gives.
-- * If no capital site can be found anywhere on this tile (e.g. an
--   all-ocean tile), no country/border/capital/roads are emitted at all --
--   an oceanic level-5 tile simply has no country, which is the honest
--   outcome, not a bug to work around.
--
-- KNOWN LIMITATIONS (same as continent.lua/region.lua, repeated here since
-- this script doesn't require reading those first):
-- * ctx does not expose WorldConfig (sea level, equator/pole temperature) to
--   Lua generators yet — hardcodes the same defaults the other 3 scripts do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides (shared with a sibling) rely on this script's own
--   bilinear math matching what the sibling computes, same as the others.

local M = {}
M.id       = "lua.map.child.level5.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE         = 64
local SEA_LEVEL_M       = 0.0    -- WorldConfig default; see header note
local EQUATOR_TEMP_C    = 30.0
local POLE_TEMP_C       = -20.0
local MOUNTAIN_ELEV_M   = 2500.0 -- above sea level; mirrors BiomeClassifier
local DETAIL_OFFSET     = 2000.0 -- decorrelates detail noise (ctx.noise has one fixed seed)
local MOISTURE_OFFSET   = 6000.0 -- decorrelates moisture noise from detail noise
local TOWNS_MIN         = 1
local TOWNS_MAX         = 3
local MAX_SITE_ATTEMPTS = 8       -- per site (capital or town); gives up on that one site if none found

-- MAP19, M317: country-scale mountain ranges/rivers, via the M313/M314 Lua
-- binding layer (NEXT.md §9). Naming offsets (+500/+600) sit clear of this
-- file's own capital/town draws (ctx.variation, ctx.variation+1..+1+n).
local MOUNTAIN_RANGE_COUNT_MIN = 1
local MOUNTAIN_RANGE_COUNT_MAX = 2
local MOUNTAIN_MIN_PEAK_M      = 600.0
local MOUNTAIN_MAX_PEAK_M      = 2200.0
local MOUNTAIN_NAME_OFFSET     = 500
local RIVER_NAME_OFFSET        = 600

-- M265-268 (2026-07-11): volcanism, same reasoning as planet.lua's own
-- header note.
local VOLCANIC_HOTSPOT_PROBABILITY = 0.2
local VOLCANIC_MIN_PEAK_M          = 1500.0
local VOLCANIC_MAX_PEAK_M          = 4000.0
local VOLCANIC_ENTROPY_OFFSET      = 700

-- Bilinear interpolation of a {w=,h=,data={...}} field-grid table at
-- fractional position (fx, fy). Mirrors ChildGenerator.cpp's private helper
-- of the same name/shape (also used by continent.lua/region.lua).
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

-- Attempts to find one suitable (non-ocean, non-mountain) random site,
-- returning its (world_x, world_z) or nil if none found within
-- MAX_SITE_ATTEMPTS tries. Same suitability check region.lua's own town
-- placement uses -- see this file's header note on why a full
-- Settlements::place()-style suitability model isn't ported here.
local function find_site(ctx, elevation, W, H, child_x0, child_y0, child_size_m)
    for _attempt = 1, MAX_SITE_ATTEMPTS do
        local gx  = ctx.randomInt(0, W - 1)
        local gy  = ctx.randomInt(0, H - 1)
        local elev_above_sea = elevation[gy * W + gx + 1] - SEA_LEVEL_M
        if elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M then
            local wx = child_x0 + (gx + 0.5) * child_size_m / W
            local wy = child_y0 + (gy + 0.5) * child_size_m / H
            return wx, wy
        end
    end
    return nil, nil
end

function M.generate(ctx, map)
    local parent = ctx.parent
    if not parent then return end  -- level 5 always has a parent; defensive only

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

    -- MAP19, M317: mountain ranges uplift + river carve, BOTH before
    -- setBiomeField() -- elevation must be final before classification, and
    -- capital/town siting below (find_site()) benefits from seeing the
    -- final post-uplift/post-carve terrain too.
    local mountains = map:generateMountainRanges(ctx.variation + MOUNTAIN_NAME_OFFSET,
                                                   ctx.randomInt(MOUNTAIN_RANGE_COUNT_MIN, MOUNTAIN_RANGE_COUNT_MAX),
                                                   MOUNTAIN_MIN_PEAK_M, MOUNTAIN_MAX_PEAK_M)
    map:applyMountainRanges(W, H, elevation, mountains, child_size_m / 8.0)

    -- M265-268 (2026-07-11): volcanism, same "before setBiomeField()" slot
    -- as mountain ranges above.
    local volcanic_hotspot_count = (ctx.random() < VOLCANIC_HOTSPOT_PROBABILITY) and 1 or 0
    local volcanoes = map:generateVolcanicHotspots(ctx.variation + VOLCANIC_ENTROPY_OFFSET,
                                                     volcanic_hotspot_count,
                                                     VOLCANIC_MIN_PEAK_M, VOLCANIC_MAX_PEAK_M)
    map:applyVolcanism(W, H, elevation, volcanoes)

    local rivers = map:traceRivers(W, H, elevation)
    map:carveRivers(W, H, elevation, rivers)

    -- setBiomeField() default-derives all 4 edges from this tile's own FINAL
    -- elevation (M115, MapBuilder.cpp) — see continent.lua's header note for
    -- why this script (like the others) must not call setEdge() itself.
    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- MAP19, M317/M316: names the ranges/rivers/lakes just generated above.
    map:appendMountainRangeFeatures(W, H, elevation, mountains, parent.culture,
                                     ctx.variation + MOUNTAIN_NAME_OFFSET)
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

    -- M265-268 (2026-07-11): volcanic/geothermal/ash_plain/volcanic_island,
    -- runs last among the refinement passes.
    map:applyVolcanicBiomes(volcanoes)

    -- Capital first: if no suitable site exists anywhere on this tile,
    -- there is no country here (see header note) -- skip border/towns/roads
    -- entirely rather than emit a country with no capital.
    local capital_x, capital_z = find_site(ctx, elevation, W, H, child_x0, child_y0, child_size_m)
    if capital_x ~= nil then
        local country_name = names.country(parent.culture, ctx.variation)
        local capital_name  = names.city(parent.culture, ctx.variation + 1)

        -- Tile bounds are half-open [min,max) (MapValidator rejects a point at
        -- exactly max_x/max_z); nudge the two max-side corners 1 micrometer
        -- inside, same epsilon metro.lua's own find_site() clamp already uses.
        local BORDER_EPS_M = 1e-6
        map:addBorder(country_name, {
            {child_x0, child_y0},
            {child_x0 + child_size_m - BORDER_EPS_M, child_y0},
            {child_x0 + child_size_m - BORDER_EPS_M, child_y0 + child_size_m - BORDER_EPS_M},
            {child_x0, child_y0 + child_size_m - BORDER_EPS_M},
        })
        map:addCity(capital_name, capital_x, capital_z, "city")

        -- A handful of minor towns, each connected back to the capital by
        -- one straight trunk road (see header note on why this is a
        -- hub-and-spoke shape, not a ported Roads::build() MST).
        local n = ctx.randomInt(TOWNS_MIN, TOWNS_MAX)
        for i = 1, n do
            local town_x, town_z = find_site(ctx, elevation, W, H, child_x0, child_y0, child_size_m)
            if town_x ~= nil then
                local town_name = names.city(parent.culture, ctx.variation + 1 + i)
                local road_name = names.street(parent.culture, ctx.variation + 1 + i)
                map:addCity(town_name, town_x, town_z, "town")
                map:addRoad(road_name, {{capital_x, capital_z}, {town_x, town_z}})
            end
        end
    end

    map:setMetadata(M.id, parent.culture)
end

return M
