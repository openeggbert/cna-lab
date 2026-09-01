-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M115 (MAP7) — Lua child-tile generator for level 3 ("continent" scale
-- detail within a parent tile). Registered as "lua.map.child.level3.default"
-- so MapPipeline's level-aware Lua lookup (see M115 in NEXT.md) only tries
-- this for level-3 tiles, not every child level.
--
-- Mirrors src/Map/ChildGenerator.cpp's general refinement technique
-- (bilinear-from-parent elevation + fade-to-zero fBm detail; temperature and
-- moisture recomputed from world position, same treatment as the C++ path)
-- so elevation stays continuous with the parent, and two siblings computing
-- this same formula independently still agree on their shared boundary
-- (same argument as ChildGenerator's own sibling-coherence guarantee, M112).
--
-- MAP19, M317 — real Map::MountainRanges/Map::Hydrology via the M313/M314
-- Lua binding layer (NEXT.md §9). This USED TO emit a MapFeature::
-- MountainRange as a raw raster point-cloud of every cell above the
-- mountain elevation threshold (a "first, simple v1" per this file's own
-- prior header note) -- that has been replaced with the real algorithm:
-- MountainRanges::generate() seeds actual ridge polylines and
-- MountainRanges::apply() uplifts elevation along them (so mountains now
-- shape the terrain, not just describe cells terrain-noise already pushed
-- high), then Hydrology::trace()/carve() add continent-scale rivers.
-- Both happen BEFORE setBiomeField() (both mutate elevation in place, and
-- biome must be classified from the FINAL elevation).
--
-- KNOWN LIMITATIONS:
-- * ctx does not expose WorldConfig (sea level, equator/pole temperature) to
--   Lua generators yet — this script hardcodes the same defaults planet.lua
--   already hardcodes, for the same reason (see planet.lua's header note).
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the *parent's* boundary
--   regardless of what this script computes there; this script's own
--   bilinear math matters most for the OTHER 2 sides (shared with a
--   sibling), which M108 does not constrain for a Lua generator (see
--   NEXT.md known-limitation 3c).

local M = {}
M.id       = "lua.map.child.level3.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE       = 64
local SEA_LEVEL_M     = 0.0    -- WorldConfig default; see header note
local EQUATOR_TEMP_C  = 30.0
local POLE_TEMP_C     = -20.0
local MOUNTAIN_ELEV_M = 2500.0 -- above sea level; mirrors BiomeClassifier
local DETAIL_OFFSET   = 2000.0 -- decorrelates detail noise (ctx.noise has one fixed seed)
local MOISTURE_OFFSET = 6000.0 -- decorrelates moisture noise from detail noise

-- MAP19, M317: continent-scale mountain ranges/rivers. Naming offsets
-- (+500/+600) sit clear of the plain ctx.variation this file's own single
-- prior naming draw used, so a future per-tile draw here never collides.
local MOUNTAIN_RANGE_COUNT_MIN = 1
local MOUNTAIN_RANGE_COUNT_MAX = 3
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
-- of the same name/shape.
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
    if not parent then return end  -- level 3 always has a parent; defensive only

    local W, H = GRID_SIZE, GRID_SIZE
    local cx = ctx.tile_x % 2
    local cy = ctx.tile_y % 2

    -- Derived from ctx fields alone (tile_size_m = planet_size_m / 2^level),
    -- no new ctx exposure needed.
    local planet_size_m = ctx.tile_size_m * (2 ^ ctx.level)
    local child_size_m  = ctx.tile_size_m
    local child_x0       = ctx.tile_x * child_size_m
    local child_y0       = ctx.tile_y * child_size_m

    local terrain_scale = math.max(child_size_m / 4.0, 1.0)
    local detail_amp    = math.min(child_size_m / 100.0, 500.0)

    local elevation, temperature, moisture = {}, {}, {}

    for gy = 0, H - 1 do
        for gx = 0, W - 1 do
            -- Map this child cell to the quadrant of the parent it occupies
            -- (same 32/(W-1) split ChildGenerator.cpp uses).
            local pgx = cx * 32.0 + gx * (32.0 / (W - 1))
            local pgy = cy * 32.0 + gy * (32.0 / (H - 1))
            local base_elev = bilinear(parent.elevation, pgx, pgy)

            -- sin²-fade: 0 at boundaries, 1 at the interior centre — makes
            -- sibling shared boundaries identical (M112) since both siblings
            -- compute the same bilinear(parent) there regardless of fade.
            local fx_fade = math.sin(math.pi * gx / (W - 1))
            local fy_fade = math.sin(math.pi * gy / (H - 1))
            local fade    = fx_fade * fx_fade * fy_fade * fy_fade

            local wx = child_x0 + (gx + 0.5) * child_size_m / W
            local wy = child_y0 + (gy + 0.5) * child_size_m / H

            local detail_fbm = ctx.noise(wx / terrain_scale + DETAIL_OFFSET, wy / terrain_scale)
            local elev = base_elev + fade * (detail_fbm - 0.5) * detail_amp

            -- Temperature: same latitude-band + lapse-rate formula as
            -- PlanetGenerator/ChildGenerator/planet.lua.
            local lat_factor = math.abs(wy / planet_size_m - 0.5) * 2.0
            local base_temp  = EQUATOR_TEMP_C + (POLE_TEMP_C - EQUATOR_TEMP_C) * lat_factor
            local elev_above = math.max(elev - SEA_LEVEL_M, 0.0)
            local temp       = base_temp - 6.5 * elev_above / 1000.0

            -- Moisture: broad-scale fBm, decorrelated via a domain offset
            -- (same trick as planet.lua's MOISTURE_OFFSET).
            local moisture_fbm = ctx.noise(wx / terrain_scale + MOISTURE_OFFSET, wy / terrain_scale)
            local moist = math.max(0.0, math.min(1.0, moisture_fbm))

            local idx = gy * W + gx + 1
            elevation[idx]   = elev
            temperature[idx] = temp
            moisture[idx]    = moist
        end
    end

    -- MAP19, M317: mountain ranges uplift + river carve, BOTH before
    -- setBiomeField() -- elevation must be final before classification.
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

    -- setBiomeField() default-populates all 4 edge descriptors from this
    -- tile's own FINAL elevation (M115, MapBuilder.cpp) — including the 2
    -- sides M108's parent-edge constraint may have just overwritten above.
    -- Deriving edges from this script's own pre-constraint `elevation` table
    -- instead would export a side inconsistent with what actually got
    -- stored; let setBiomeField()'s default do it.
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

    map:setMetadata(M.id, parent.culture)
end

return M
