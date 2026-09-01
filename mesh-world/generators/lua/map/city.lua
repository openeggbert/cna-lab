-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Robert Vokac and contributors
--
-- M153 (MAP10) — Lua child-tile generator for level 12 ("City" scale
-- detail within a parent tile, ~5.5 km per map.md's level table).
-- Registered as "lua.map.child.level12.default" so MapPipeline's
-- level-aware Lua lookup (M115) only tries this for level-12 tiles.
--
-- Mirrors continent.lua's (M115)/region.lua's (M116)/country.lua's
-- (M146)/metro.lua's (M147) generic child-refinement technique
-- (bilinear-from-parent elevation + fade-to-zero fBm detail; temperature
-- and moisture recomputed from world position) for the same reasons those
-- scripts already document (elevation stays continuous with the parent,
-- M112 sibling coherence).
--
-- plan.md describes this level as "street grid, zoning (ZoneType), parks,
-- water." Read country.lua's/metro.lua's own header notes first if you
-- haven't -- the same architectural point applies here for the street
-- grid/zoning/water logic below: no C++ Settlements::place()-style
-- suitability model is called (MAP19, M317 DID wire in the real
-- BiomeRefinement coastal-beach/swamp passes, see below, but street
-- routing/zoning/water are still this script's own v1 logic). A level-12
-- tile also has no way to see whether an ancestor tile (e.g. metro.lua,
-- M147, level 9) actually flagged this specific area as urban -- ctx.parent
-- only exposes elevation/temperature/moisture/culture/variation, not the
-- parent's placed features (see LuaRuntime.cpp's make_parent_table()).
--
-- Because "City" is this LEVEL's scale identity (the same way "Continent"
-- is level 3's and "Region/province" is level 7's, regardless of whether a
-- specific tile happens to be mostly ocean), this script always attempts
-- to urbanize every buildable cell of the tile -- it does not roll a
-- single "is this tile a city" chance the way country.lua's capital or
-- metro.lua's city-footprint placement do. A tile with no buildable land
-- at all (e.g. open ocean) simply ends up with nothing urbanized, no
-- streets, and no parks -- there is nothing to zone.
--
-- V1 SIMPLIFICATIONS (read before extending):
-- * **Zoning is a raster override, not a new vector type.** plan.md's own
--   parenthetical "(ZoneType)" points at the existing `ZoneType` enum, not
--   a new concept -- but `BiomeClassifier::classify()` (run inside
--   `setBiomeField()`) only ever classifies natural terrain, it has no
--   "city" output. So this script calls `setBiomeField()` first (exactly
--   like every other child script), then overrides every buildable,
--   non-park cell to `ZoneType::city` via the new `map:markUrbanCells()`
--   (M153) -- the same "override on top of the default" shape `setEdge()`
--   already established for `TileEdge`.
-- * **Street grid follows terrain loosely (M155), still not block-aware.**
--   Each grid line is bent by `align_grid_line()` (below): a cheap greedy
--   walk that nudges the line's cross-axis cell at each sampled step
--   toward whichever neighbor's elevation is closest to the previous
--   sample's -- i.e. a street prefers staying at roughly constant
--   elevation (a contour) over cutting straight through a slope, bounded
--   to `MAX_DRIFT_CELLS` away from the original straight index so it
--   never wanders into a neighboring block. Not a full Dijkstra like
--   `Roads::build()`'s M144 terrain routing -- that solves a sparse
--   point-to-point graph; a dense per-cell street grid doesn't need that
--   machinery, so this is a deliberate, cheaper V1, the same "data/shape
--   first, refine later" split this codebase has used throughout (e.g.
--   M139 uniform-cost region growth -> M140 feature-aware cost).
-- * **Cross-tile street seams are pinned, not walked.** `align_grid_line()`'s
--   two tile-boundary samples (along=0 and along=along_len-1) always emit
--   the nominal, unbent `fixed_index` regardless of where the elevation
--   walk drifted to -- only the interior samples bend. A neighboring
--   sibling tile's own matching street (same `fixed_index`, since
--   `STREET_SPACING_CELLS` positions land on identical local indices
--   across every level-12 tile) independently computes that exact same
--   pinned value for its own edge, so the two segments always meet exactly
--   at the shared boundary without either tile knowing anything about the
--   other's terrain. Before this fix the walk's drift carried all the way
--   to the tile edge, so a street lined up with at most one of its two
--   neighbors by accident, never both -- visible as a kink at every tile
--   seam in a stitched `--png-region` render.
-- * **"Blocks between streets" (M155's other clause) is NOT modeled as a
--   new feature here.** map.md's own level table (§5.3) places "block
--   cluster" at level 16 -- one level below `neighborhood.lua` (15) --
--   and "per-chunk RegionType" at level 17/M157. Since no script exists
--   at those levels yet and `RegionType` is legacy-chunk-system-only
--   (never appears in `Map::`), it's still an open question whether a
--   block ever needs a persisted vector feature at THIS level at all, or
--   whether M156/M157 will derive it some other way once they actually
--   need to iterate blocks. Read map.md §5.3 again before assuming this
--   level owns that concept.
-- * **Parks are exclusion zones, not a new ZoneType.** `ZoneType` has no
--   "park" value, so a park is represented as: (a) a small neighborhood of
--   cells deliberately EXCLUDED from the urban-cell override (they keep
--   whatever natural biome `classify()` gave them -- green space within
--   the urban footprint), plus (b) an explicit `map:addPark()` (M153's
--   other new builder call) marking its center so it is a real, nameable,
--   locatable feature rather than just an absence of urbanization a
--   consumer couldn't otherwise detect. No dedicated `Naming::park()`
--   exists (and adding one for a single call site isn't justified yet) --
--   park names reuse `names.city()`'s output with a "Park" suffix appended
--   in this script, not a new C++ naming surface.
-- * **Water reuses metro.lua's (M147) exact near-sea-level point-cloud
--   technique** for `map:addLake()` -- the same crude "flat, low-lying,
--   plausibly wet" proxy, not real basin detection (see metro.lua's own
--   header for the full reasoning). Not duplicated here at length.
-- * **Zone candidates (M156) use a NEW Map::-native enum, not the legacy
--   RegionType.** `RegionType` (`include/RegionType.hpp`) is a legacy
--   chunk-system-only concept that never appears anywhere in `Map::` --
--   plan.md's own M157 describes the hand-off as converting `Map::`
--   output INTO legacy-system inputs, so `Map::` itself should not depend
--   on that legacy enum. `Map::ZoneCandidate` (new, `include/Map/
--   ZoneCandidate.hpp`) mirrors plan.md's M156 parenthetical (house_block/
--   apartment_block/shop_street/park/square) using RegionType's exact
--   spelling where the two differ ("small_house_block") so a future M157
--   translation is a direct lookup. `map:setZoneCandidates()` (M156, new
--   builder call) stores one ordinal per cell, uniform within each
--   BLOCK -- a block here is purely an index-space concept
--   (`floor(gx/STREET_SPACING_CELLS)`, `floor(gy/STREET_SPACING_CELLS)`),
--   deliberately decoupled from the actual bent street polylines M155
--   draws (no persisted block geometry exists, see that note above) --
--   a park-containing block becomes `park`; a non-urban block (no
--   buildable cell) becomes `none`; an urban block rolls a weighted
--   candidate among `small_house_block`/`apartment_block`/`shop_street`/
--   `square` (most common to rarest). This is city.lua's own scope only
--   (it already has the zoning infrastructure `markUrbanCells()`/
--   `park_sites` this needs) -- `neighborhood.lua` deliberately does not
--   zone at all (M154's own scope decision), so M156 doesn't touch it.
--
-- KNOWN LIMITATIONS (same as the other child scripts, repeated here since
-- this script doesn't require reading those first):
-- * ctx does not expose WorldConfig (sea level, equator/pole temperature) to
--   Lua generators yet — hardcodes the same defaults the other scripts do.
-- * MapBuilder::setBiomeField's M108 constraint overwrites this tile's own
--   boundary rows/columns on the 2 sides that touch the parent's boundary;
--   the OTHER 2 sides (shared with a sibling) rely on this script's own
--   bilinear math matching what the sibling computes, same as the others.
--   `markUrbanCells()` runs AFTER that constraint, so a boundary cell can
--   still be urbanized even though its elevation is fixed by the parent --
--   zoning and elevation are independent concerns here.

local M = {}
M.id       = "lua.map.child.level12.default"
M.version  = "0.1.0"
M.category = "map"

local GRID_SIZE             = 64
local SEA_LEVEL_M           = 0.0    -- WorldConfig default; see header note
local EQUATOR_TEMP_C        = 30.0
local POLE_TEMP_C           = -20.0
local MOUNTAIN_ELEV_M       = 2500.0 -- above sea level; mirrors BiomeClassifier
local DETAIL_OFFSET         = 2000.0 -- decorrelates detail noise (ctx.noise has one fixed seed)
local MOISTURE_OFFSET       = 6000.0 -- decorrelates moisture noise from detail noise
local STREET_SPACING_CELLS  = 8      -- one grid line every N cells, both axes
local SAMPLE_STEP_CELLS     = 8      -- M155: waypoint spacing along a bent street line
local MAX_DRIFT_CELLS       = 2      -- M155: max lateral bend from the straight index
-- M162 fix (§5 #18): tile bounds are half-open [min,max) (MapValidator rejects a point at
-- exactly max_x/max_z), but MapBuilder::deriveEdgeCrossings() only matches within 0.01 m
-- of the true edge -- this must be strictly less than max but within that tolerance.
local EDGE_SNAP_EPS_M       = 0.001
local PARKS_MIN             = 0
local PARKS_MAX             = 2
local PARK_RADIUS_CELLS     = 3      -- Chebyshev radius excluded from urbanization around each park
local MAX_SITE_ATTEMPTS     = 8
local LAKE_ELEV_MARGIN_M    = 50.0   -- cells in [SEA_LEVEL_M, SEA_LEVEL_M + this] count as "lake" (crude v1 proxy, see header)
-- MAP19, M317: at city scale (~5.5 km tile) a brand-new river/mountain
-- trace would be redundant with whatever a metro-level (level 9) ancestor
-- already carved into the inherited elevation -- see metro.lua's own M317
-- note. Only the cheap, scale-agnostic BiomeRefinement (M315) passes are
-- wired in here.

-- M156: Map::ZoneCandidate ordinals (include/Map/ZoneCandidate.hpp) --
-- must match that enum's order exactly.
local ZONE_NONE               = 0
local ZONE_SMALL_HOUSE_BLOCK  = 1
local ZONE_APARTMENT_BLOCK    = 2
local ZONE_SHOP_STREET        = 3
local ZONE_PARK               = 4
local ZONE_SQUARE             = 5
-- Weighted roll for an urban, non-park block (cumulative thresholds out
-- of 100, via ctx.randomInt(0, 99)): most blocks are ordinary housing,
-- with progressively rarer apartments/shops/squares -- a new, simple V1
-- weighting scheme (no existing script needed a >2-outcome weighted roll
-- to mirror).
local ZONE_SQUARE_MAX    = 5
local ZONE_SHOP_MAX      = 15
local ZONE_APARTMENT_MAX = 30

-- Bilinear interpolation of a {w=,h=,data={...}} field-grid table at
-- fractional position (fx, fy). Mirrors ChildGenerator.cpp's private helper
-- of the same name/shape (also used by continent.lua/region.lua/country.lua/metro.lua).
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
-- vertical=true bends an x=const line (cross axis is gx) as gy sweeps
-- 0..H-1; vertical=false bends a y=const line (cross axis is gy) as gx
-- sweeps 0..W-1. At each sampled step along the line's own axis, greedily
-- nudges the cross-axis cell by at most 1 toward whichever neighbor's
-- elevation is closest to the previous sample's (a street prefers a
-- constant-elevation contour over a straight cut through a slope),
-- clamped to MAX_DRIFT_CELLS from the original straight index. See the
-- header note above for why this is a greedy walk, not a full Dijkstra.
local function align_grid_line(elevation, W, H, vertical, fixed_index, child_x0, child_y0, child_size_m)
    local along_len = vertical and H or W
    local cross_len = vertical and W or H
    local function elev_at(along, cross)
        local gx = vertical and cross or along
        local gy = vertical and along or cross
        return elevation[gy * W + gx + 1]
    end
    -- along_boundary, when non-nil, overrides the along-axis world coordinate with the
    -- tile's true edge (child_{x,y}0 or +child_size_m) instead of the cell-center formula,
    -- so the first/last sampled point lands exactly on the boundary that
    -- MapBuilder::deriveEdgeCrossings() checks against (bent cross-axis untouched).
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
        -- Cross-tile seam fix: the EMITTED cross-index at a tile-boundary
        -- sample is pinned to the nominal, unbent fixed_index -- a
        -- neighboring tile computes this exact same deterministic value
        -- independently for its own matching edge (STREET_SPACING_CELLS
        -- positions line up 1:1 across siblings, see M156 note above), so
        -- both sides connect without any shared state, the same "pin the
        -- boundary, let the interior vary" shape MapBuilder::setBiomeField
        -- already uses for elevation. Before this fix only the WEST/NORTH
        -- end (along=0) happened to land near fixed_index (an accidental
        -- side effect of the walk's own self-comparison seed, not a real
        -- guarantee) while the EAST/SOUTH end kept whatever cross-index the
        -- walk had drifted to over the tile -- so a street reliably lined
        -- up with at most one of its two neighbors, never both.
        local emit_cross = best_cross
        if along == 0 then
            boundary   = along_min
            emit_cross = fixed_index
        end
        if along == along_len - 1 then
            boundary   = along_max
            emit_cross = fixed_index
        end
        points[#points + 1] = world_point(along, emit_cross, boundary)
    end
    if last_along ~= along_len - 1 then
        points[#points + 1] = world_point(along_len - 1, fixed_index, along_max)
    end
    return points
end

function M.generate(ctx, map)
    local parent = ctx.parent
    if not parent then return end  -- level 12 always has a parent; defensive only

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
    local is_buildable = {}  -- land, not too high -- candidate for urbanization
    local lake_points  = {}
    local any_buildable = false

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

            local elev_above_sea = elev - SEA_LEVEL_M
            local buildable = elev_above_sea > 0.0 and elev_above_sea < MOUNTAIN_ELEV_M
            is_buildable[idx] = buildable
            if buildable then any_buildable = true end

            if elev_above_sea >= 0.0 and elev_above_sea <= LAKE_ELEV_MARGIN_M then
                lake_points[#lake_points + 1] = {wx, wy}
            end
        end
    end

    -- setBiomeField() default-derives all 4 edges from this tile's own FINAL
    -- elevation (M115, MapBuilder.cpp) — see continent.lua's header note for
    -- why this script (like the others) must not call setEdge() itself.
    map:setBiomeField(W, H, elevation, temperature, moisture)

    -- MAP19, M315: coastal beach + swamp-flatness refinement, BEFORE
    -- markUrbanCells() below so urbanization always wins over natural
    -- refinement (same override precedence M354's parent-city inheritance
    -- already established for neighborhood.lua).
    map:applyCoastalBeach()
    map:applySwampFlatnessCheck()

    -- M259/M274/M275 (2026-07-11): canyon carving + coastal relief
    -- refinement (tidal_flat/sea_cliff), same "before markUrbanCells()"
    -- precedence as the two calls above.
    map:applyCanyonCarving()
    map:applyCoastalReliefRefinement()

    if any_buildable then
        -- Parks: a few sites on buildable land, excluded from urbanization.
        local park_sites = {}
        local n_parks = ctx.randomInt(PARKS_MIN, PARKS_MAX)
        for i = 1, n_parks do
            for _attempt = 1, MAX_SITE_ATTEMPTS do
                local gx, gy = ctx.randomInt(0, W - 1), ctx.randomInt(0, H - 1)
                if is_buildable[gy * W + gx + 1] then
                    park_sites[#park_sites + 1] = {gx = gx, gy = gy}
                    break
                end
            end
        end

        -- Zoning: every buildable cell becomes urban UNLESS within
        -- PARK_RADIUS_CELLS (Chebyshev) of a park site.
        local urban_mask = {}
        for gy = 0, H - 1 do
            for gx = 0, W - 1 do
                local idx = gy * W + gx + 1
                local urban = is_buildable[idx]
                if urban then
                    for _, p in ipairs(park_sites) do
                        if math.abs(gx - p.gx) <= PARK_RADIUS_CELLS and math.abs(gy - p.gy) <= PARK_RADIUS_CELLS then
                            urban = false
                            break
                        end
                    end
                end
                urban_mask[idx] = urban and 1 or 0
            end
        end
        map:markUrbanCells(urban_mask)

        for i, p in ipairs(park_sites) do
            local wx = child_x0 + (p.gx + 0.5) * child_size_m / W
            local wz = child_y0 + (p.gy + 0.5) * child_size_m / H
            local park_name = names.city(parent.culture, ctx.variation + 100 + i) .. " Park"
            map:addPark(park_name, wx, wz)
        end

        -- Street grid: each line bent to loosely follow terrain (M155) via
        -- align_grid_line() -- see header note.
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

        -- Zone candidates (M156): one Map::ZoneCandidate per BLOCK, where a
        -- block is purely an index-space rectangle between nominal
        -- (pre-bend) street grid lines -- STREET_SPACING_CELLS on both
        -- axes, deliberately decoupled from align_grid_line()'s actual bent
        -- polylines (see header note). Uniform within each block, not
        -- per-cell: a block containing a park becomes `park`; a block with
        -- no buildable cell at all becomes `none`; otherwise a weighted
        -- roll among the remaining candidates.
        local zone_candidates = {}
        for by_block = 0, math.ceil(H / STREET_SPACING_CELLS) - 1 do
            local gy0 = by_block * STREET_SPACING_CELLS
            local gy1 = math.min(gy0 + STREET_SPACING_CELLS, H) - 1
            for bx_block = 0, math.ceil(W / STREET_SPACING_CELLS) - 1 do
                local gx0 = bx_block * STREET_SPACING_CELLS
                local gx1 = math.min(gx0 + STREET_SPACING_CELLS, W) - 1

                local block_urban = false
                for gy = gy0, gy1 do
                    for gx = gx0, gx1 do
                        if urban_mask[gy * W + gx + 1] == 1 then block_urban = true end
                    end
                end
                local block_park = false
                for _, p in ipairs(park_sites) do
                    if p.gx >= gx0 and p.gx <= gx1 and p.gy >= gy0 and p.gy <= gy1 then
                        block_park = true
                    end
                end

                local candidate
                if block_park then
                    candidate = ZONE_PARK
                elseif not block_urban then
                    candidate = ZONE_NONE
                else
                    local roll = ctx.randomInt(0, 99)
                    if roll < ZONE_SQUARE_MAX then
                        candidate = ZONE_SQUARE
                    elseif roll < ZONE_SHOP_MAX then
                        candidate = ZONE_SHOP_STREET
                    elseif roll < ZONE_APARTMENT_MAX then
                        candidate = ZONE_APARTMENT_BLOCK
                    else
                        candidate = ZONE_SMALL_HOUSE_BLOCK
                    end
                end

                for gy = gy0, gy1 do
                    for gx = gx0, gx1 do
                        zone_candidates[gy * W + gx + 1] = candidate
                    end
                end
            end
        end
        map:setZoneCandidates(zone_candidates)
    end

    -- Water: independent of urbanization -- a tile can have low-lying wet
    -- terrain without any buildable land at all.
    if #lake_points > 0 then
        local lake_name = names.lake(parent.culture, ctx.variation)
        map:addLake(lake_name, lake_points)
    end

    map:setMetadata(M.id, parent.culture)
end

return M
