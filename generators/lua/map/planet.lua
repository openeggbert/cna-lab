-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M095 (MAP6) — Lua port of the level-0 planet generator
-- (src/generators/map/PlanetGenerator.cpp). Structural parity, not
-- byte-identical output: same continent-count range, same field shapes,
-- same land/ocean-ratio and elevation/temperature invariants — see
-- tests/PlanetGeneratorTests.cpp for the invariants this mirrors, and
-- plan.md M101 for the parity test this script must pass.
--
-- KNOWN LIMITATION: continent-count range, sea level, and climate band are
-- WorldConfig fields (include/WorldConfig.hpp) — ctx does not yet expose
-- world config to Lua generators (only tile geometry + parent/edges +
-- noise/random, see M092/M093). This script hardcodes the same defaults
-- WorldConfig itself uses. If a world is ever configured with different
-- values, this generator silently diverges from the C++ one; wiring real
-- world config into ctx is a follow-up task, out of scope for MAP6.
--
-- Coast noise: the C++ version gives each continent its own noise seed
-- (entropy XORed with a per-continent constant). ctx.noise() has a single
-- fixed seed (the tile's entropy, see M093), so this script decorrelates
-- per-continent coast noise by sampling the same field at a large,
-- continent-index-dependent offset instead — a different mechanism, close
-- enough in effect for structural (not byte-identical) parity.
--
-- MAP19, M317 — real Map::MountainRanges/Map::Hydrology via the M313/M314
-- Lua binding layer (NEXT.md §9 explains why this overrides the earlier "no
-- speculative binding layer" rule): after the continent/terrain elevation
-- above is computed, a handful of planet-scale ranges are seeded and
-- uplifted into it, then rivers are traced and carved -- both BEFORE
-- setBiomeField() (both mutate elevation in place, and biome must be
-- classified from the FINAL elevation, same ordering reasoning
-- applyParentEdgeConstraints() already has over setBiomeField() itself).
-- Named MountainRange/River/Lake features are appended right after.
-- applyCoastalBeach()/applySwampFlatnessCheck() (M315) then refine the
-- freshly-classified biome grid -- cheap, per-cell/neighbor touch-ups, safe
-- to run at any tile resolution (see continent.lua's own header for why a
-- coarse cell is no different in kind from BiomeClassifier's own per-cell
-- approximation at any level).

local M = {}
M.id       = "lua.map.planet.default"
M.version  = "0.1.0"
M.category = "map"

local CONTINENTS_MIN  = 5
local CONTINENTS_MAX  = 12
local SEA_LEVEL_M     = 0.0
local EQUATOR_TEMP_C  = 30.0
local POLE_TEMP_C     = -20.0
local GRID_SIZE       = 64
local TARGET_COVERAGE = 0.35
local COAST_OFFSET    = 1000.0  -- per-continent noise-domain offset, see header note
local MOISTURE_OFFSET = 5000.0  -- decorrelates moisture noise from coast/terrain noise

-- MAP19, M317: planet-scale mountain ranges/rivers. Naming offsets (+500/
-- +600) sit clear of the +1..+n continent-naming draws below so no two
-- named features in this one tile ever collide on the same entropy+index.
local MOUNTAIN_RANGE_COUNT_MIN = 3
local MOUNTAIN_RANGE_COUNT_MAX = 8
local MOUNTAIN_MIN_PEAK_M      = 600.0
local MOUNTAIN_MAX_PEAK_M      = 2200.0
local MOUNTAIN_NAME_OFFSET     = 500
local RIVER_NAME_OFFSET        = 600

-- M265-268 (2026-07-11): volcanism -- far sparser than mountain ranges
-- (real volcanism is a much rarer, more localized phenomenon), so a
-- probability draw for a SINGLE hotspot rather than a count range.
-- +700 sits clear of the mountain/river offsets above.
local VOLCANIC_HOTSPOT_PROBABILITY = 0.2
local VOLCANIC_MIN_PEAK_M          = 1500.0
local VOLCANIC_MAX_PEAK_M          = 4000.0
local VOLCANIC_ENTROPY_OFFSET      = 700

function M.generate(ctx, map)
    local planet_size_m = ctx.tile_size_m  -- level 0 tile == the whole planet
    local culture = names.culture(ctx.variation)

    -- Continent centers + radii.
    local n = ctx.randomInt(CONTINENTS_MIN, CONTINENTS_MAX)
    local center_x, center_y, radius = {}, {}, {}
    local base_r = math.sqrt(TARGET_COVERAGE * planet_size_m * planet_size_m
                              / (math.pi * n))
    for i = 1, n do
        center_x[i] = ctx.random() * planet_size_m
        center_y[i] = ctx.random() * planet_size_m
        radius[i]   = base_r * (0.70 + 0.60 * ctx.random())
    end

    local coast_scale   = math.max(planet_size_m / 4.0, 1.0)
    local terrain_scale = math.max(planet_size_m / 8.0, 1.0)

    local elevation, temperature, moisture = {}, {}, {}
    for gy = 0, GRID_SIZE - 1 do
        for gx = 0, GRID_SIZE - 1 do
            local wx = (gx + 0.5) * planet_size_m / GRID_SIZE
            local wy = (gy + 0.5) * planet_size_m / GRID_SIZE

            -- Continent influence: max over all continents; > 0 means the
            -- cell lies inside that continent's noisy coastline.
            local max_inf = -1.0
            for i = 1, n do
                local dx = wx - center_x[i]
                local dy = wy - center_y[i]
                local dist = math.sqrt(dx * dx + dy * dy)
                local coast_fbm = ctx.noise(wx / coast_scale + i * COAST_OFFSET,
                                            wy / coast_scale)
                local eff_r = radius[i] * (1.0 + 0.4 * (coast_fbm - 0.5))
                local inf = 1.0 - dist / math.max(eff_r, 1.0)
                if inf > max_inf then max_inf = inf end
            end

            local is_land = max_inf > 0.0
            local terrain_fbm = ctx.noise(wx / terrain_scale, wy / terrain_scale)

            local elev
            if is_land then
                local uplift = math.min(max_inf, 1.0) * 1500.0
                elev = SEA_LEVEL_M + 1.0 + uplift + (terrain_fbm - 0.5) * 2000.0
                elev = math.max(elev, SEA_LEVEL_M + 1.0)
            else
                local depth = math.min(-max_inf, 1.0) * 4000.0
                elev = SEA_LEVEL_M - 1.0 - depth + (terrain_fbm - 0.5) * 200.0
                elev = math.min(elev, SEA_LEVEL_M - 1.0)
            end

            -- Latitude temperature bands + elevation lapse rate (6.5 C/1000 m).
            local lat_factor  = math.abs((gy + 0.5) / GRID_SIZE - 0.5) * 2.0
            local base_temp   = EQUATOR_TEMP_C + (POLE_TEMP_C - EQUATOR_TEMP_C) * lat_factor
            local elev_above  = math.max(elev - SEA_LEVEL_M, 0.0)
            local temp        = base_temp - 6.5 * elev_above / 1000.0

            -- M060 (C++ parity) — broad-scale fBm noise, decorrelated from the
            -- coast/terrain channels via a domain offset (ctx.noise has one
            -- fixed seed, see header note on COAST_OFFSET for the same trick).
            local moisture_fbm = ctx.noise(wx / coast_scale + MOISTURE_OFFSET, wy / coast_scale)
            local moist = math.max(0.0, math.min(1.0, moisture_fbm))

            local idx = gy * GRID_SIZE + gx + 1
            elevation[idx]   = elev
            temperature[idx] = temp
            moisture[idx]    = moist
        end
    end

    -- MAP19, M317: mountain ranges uplift + river carve, BOTH before
    -- setBiomeField() -- see header note on why (elevation must be final
    -- before classification).
    local mountains = map:generateMountainRanges(ctx.variation + MOUNTAIN_NAME_OFFSET,
                                                   ctx.randomInt(MOUNTAIN_RANGE_COUNT_MIN, MOUNTAIN_RANGE_COUNT_MAX),
                                                   MOUNTAIN_MIN_PEAK_M, MOUNTAIN_MAX_PEAK_M)
    map:applyMountainRanges(GRID_SIZE, GRID_SIZE, elevation, mountains, planet_size_m / 40.0)

    -- M265-268 (2026-07-11): volcanism, same "before setBiomeField()" slot
    -- as mountain ranges above.
    local volcanic_hotspot_count = (ctx.random() < VOLCANIC_HOTSPOT_PROBABILITY) and 1 or 0
    local volcanoes = map:generateVolcanicHotspots(ctx.variation + VOLCANIC_ENTROPY_OFFSET,
                                                     volcanic_hotspot_count,
                                                     VOLCANIC_MIN_PEAK_M, VOLCANIC_MAX_PEAK_M)
    map:applyVolcanism(GRID_SIZE, GRID_SIZE, elevation, volcanoes)

    local rivers = map:traceRivers(GRID_SIZE, GRID_SIZE, elevation)
    map:carveRivers(GRID_SIZE, GRID_SIZE, elevation, rivers)

    map:setBiomeField(GRID_SIZE, GRID_SIZE, elevation, temperature, moisture)

    -- Continent markers, named via the M091 naming stub.
    for i = 1, n do
        map:addContinent(names.continent(culture, ctx.variation + i), center_x[i], center_y[i])
    end

    -- MAP19, M317/M316: names the ranges/rivers/lakes just generated above.
    map:appendMountainRangeFeatures(GRID_SIZE, GRID_SIZE, elevation, mountains, culture,
                                     ctx.variation + MOUNTAIN_NAME_OFFSET)
    map:appendHydrologyFeatures(rivers, culture, ctx.variation + RIVER_NAME_OFFSET)

    -- MAP19, M315: coastal beach + swamp-flatness refinement over the
    -- freshly-classified biome grid.
    map:applyCoastalBeach()
    map:applySwampFlatnessCheck()

    -- M259/M274/M275 (2026-07-11): canyon carving + coastal relief
    -- refinement (tidal_flat/sea_cliff), same "runs after the earlier
    -- refinement passes" placement as ChildGenerator.cpp's C++ fallback.
    map:applyCanyonCarving()
    map:applyCoastalReliefRefinement()

    -- M247 (2026-07-11): riparian_forest -- needs the traced river network,
    -- so only the 5 scripts that call traceRivers() get this call (matches
    -- MAP19's own river scale-gating: planet/continent/country/region/metro,
    -- not city/neighborhood).
    map:applyRiparianForest(rivers)

    -- M265-268 (2026-07-11): volcanic/geothermal/ash_plain/volcanic_island
    -- -- needs the generated hotspot field; runs LAST among the refinement
    -- passes (see BiomeRefinement::applyVolcanicBiomes()'s own doc comment).
    map:applyVolcanicBiomes(volcanoes)

    -- Edge descriptors for child constraint propagation (map.md S7).
    -- N = top row (gy=0), E = right col (gx=GRID_SIZE-1),
    -- S = bottom row (gy=GRID_SIZE-1), W = left col (gx=0).
    local edge_n, edge_e, edge_s, edge_w = {}, {}, {}, {}
    for i = 1, GRID_SIZE do
        edge_n[i] = elevation[(0)              * GRID_SIZE + i]
        edge_s[i] = elevation[(GRID_SIZE - 1)  * GRID_SIZE + i]
        edge_e[i] = elevation[(i - 1)          * GRID_SIZE + GRID_SIZE]
        edge_w[i] = elevation[(i - 1)          * GRID_SIZE + 1]
    end
    map:setEdge("N", edge_n)
    map:setEdge("E", edge_e)
    map:setEdge("S", edge_s)
    map:setEdge("W", edge_w)

    map:setMetadata(M.id, culture)
end

return M
