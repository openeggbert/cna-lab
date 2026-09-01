-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: road zone chunk (64×64 m)

local M = {}
M.id       = "lua.zone.road"
M.version  = "0.1.0"
M.category = "zone"

-- 2026-07-12 (R121 zone/chunk audit follow-up) -- this script used to
-- ALWAYS build a north-south (Z-axis) road, regardless of ctx.exits: a
-- real, live correctness bug (found by comparing against the C++
-- fallback, src/generators/RoadGenerator.cpp, which IS exit-aware) --
-- any chunk actually needing an east-west road would get one facing the
-- wrong way whenever this Lua script won (the default, per this
-- project's Lua-first policy). ctx.exits.{north,south,east,west}_road
-- have been real, Lua-visible fields since the G-series session
-- (2026-07-11) that added ctx.exits for chunk/zone-mode generators --
-- this script simply never used them. Fixed by mirroring
-- RoadGenerator.cpp's own ns/ew/dead-end orientation logic (same
-- variable names, same fallback-to-dead-end-stub shape for the
-- neither/both-ambiguous case), applied to this script's own simpler
-- geometry (no curbs/drains/guardrails -- this was never meant to be a
-- 1:1 port, just no longer facing the wrong way).
function M.generate(ctx, scene)
    local S    = ctx.chunk_size_m
    local half = S / 2
    local var  = ctx.variation or 0

    local n = ctx.exits.north_road
    local s = ctx.exits.south_road
    local e = ctx.exits.east_road
    local w = ctx.exits.west_road
    local ns = n or s
    local ew = e or w

    scene:addGround("sidewalk")

    local road_w = 12
    if n and s and not ew then
        -- ── North-South road (along Z) ──────────────────────────────
        scene:addPlane("road", {
            position = {half - road_w/2, 0.02, 0},
            size     = {road_w, S},
            material = "asphalt"
        })

        local dash_len, dash_gap, stripe_y = 3.0, 2.0, 0.03
        local z, dash_i = 1.0, 0
        while z < S - dash_len do
            dash_i = dash_i + 1
            scene:addPlane("dash_" .. dash_i, {
                position = {half - 0.1, stripe_y, z},
                size     = {0.2, dash_len},
                material = "road_line_white"
            })
            z = z + dash_len + dash_gap
        end

        scene:addPlane("line_l", { position={half - road_w/2, stripe_y, 0}, size={0.15, S}, material="road_line_white" })
        scene:addPlane("line_r", { position={half + road_w/2 - 0.15, stripe_y, 0}, size={0.15, S}, material="road_line_white" })

        local lamp_spacing, lamp_offset = 16, 7.5
        local lp, z2 = 0, lamp_spacing / 2
        while z2 < S do
            lp = lp + 1
            scene:addCylinder("lp_l_base_" .. lp, {position={half - lamp_offset, 0,   z2}, radius=0.12, height=0.06, material="metal_lamp"})
            scene:addCylinder("lp_l_pole_" .. lp, {position={half - lamp_offset, 0,   z2}, radius=0.06, height=5.5,  material="metal_lamp"})
            scene:addBox     ("lp_l_head_" .. lp, {position={half - lamp_offset, 5.5, z2}, size={0.35, 0.18, 0.35},  material="metal_lamp"})
            scene:addCylinder("lp_r_base_" .. lp, {position={half + lamp_offset, 0,   z2}, radius=0.12, height=0.06, material="metal_lamp"})
            scene:addCylinder("lp_r_pole_" .. lp, {position={half + lamp_offset, 0,   z2}, radius=0.06, height=5.5,  material="metal_lamp"})
            scene:addBox     ("lp_r_head_" .. lp, {position={half + lamp_offset, 5.5, z2}, size={0.35, 0.18, 0.35},  material="metal_lamp"})
            z2 = z2 + lamp_spacing
        end
    elseif e and w and not ns then
        -- ── East-West road (along X) -- axes swapped from the N-S case ──
        scene:addPlane("road", {
            position = {0, 0.02, half - road_w/2},
            size     = {S, road_w},
            material = "asphalt"
        })

        local dash_len, dash_gap, stripe_y = 3.0, 2.0, 0.03
        local x, dash_i = 1.0, 0
        while x < S - dash_len do
            dash_i = dash_i + 1
            scene:addPlane("dash_" .. dash_i, {
                position = {x, stripe_y, half - 0.1},
                size     = {dash_len, 0.2},
                material = "road_line_white"
            })
            x = x + dash_len + dash_gap
        end

        scene:addPlane("line_l", { position={0, stripe_y, half - road_w/2}, size={S, 0.15}, material="road_line_white" })
        scene:addPlane("line_r", { position={0, stripe_y, half + road_w/2 - 0.15}, size={S, 0.15}, material="road_line_white" })

        local lamp_spacing, lamp_offset = 16, 7.5
        local lp, x2 = 0, lamp_spacing / 2
        while x2 < S do
            lp = lp + 1
            scene:addCylinder("lp_l_base_" .. lp, {position={x2, 0,   half - lamp_offset}, radius=0.12, height=0.06, material="metal_lamp"})
            scene:addCylinder("lp_l_pole_" .. lp, {position={x2, 0,   half - lamp_offset}, radius=0.06, height=5.5,  material="metal_lamp"})
            scene:addBox     ("lp_l_head_" .. lp, {position={x2, 5.5, half - lamp_offset}, size={0.35, 0.18, 0.35},  material="metal_lamp"})
            scene:addCylinder("lp_r_base_" .. lp, {position={x2, 0,   half + lamp_offset}, radius=0.12, height=0.06, material="metal_lamp"})
            scene:addCylinder("lp_r_pole_" .. lp, {position={x2, 0,   half + lamp_offset}, radius=0.06, height=5.5,  material="metal_lamp"})
            scene:addBox     ("lp_r_head_" .. lp, {position={x2, 5.5, half + lamp_offset}, size={0.35, 0.18, 0.35},  material="metal_lamp"})
            x2 = x2 + lamp_spacing
        end
    else
        -- R134/R138 -- turns, intersections, and legal termini are a centre
        -- patch plus ONLY their canonical arms. The former fallback drew a
        -- full-tile road for a one-sided exit, which was the visible
        -- road-to-nowhere bug this Lua-first path had kept alive.
        local arm = half - road_w / 2
        scene:addPlane("road_center", {
            position = {half - road_w/2, 0.02, half - road_w/2},
            size     = {road_w, road_w}, material = "asphalt"
        })
        if n then scene:addPlane("road_n", {
            position = {half - road_w/2, 0.02, 0}, size = {road_w, arm}, material = "asphalt"
        }) end
        if s then scene:addPlane("road_s", {
            position = {half - road_w/2, 0.02, half + road_w/2}, size = {road_w, arm}, material = "asphalt"
        }) end
        if e then scene:addPlane("road_e", {
            position = {half + road_w/2, 0.02, half - road_w/2}, size = {arm, road_w}, material = "asphalt"
        }) end
        if w then scene:addPlane("road_w", {
            position = {0, 0.02, half - road_w/2}, size = {arm, road_w}, material = "asphalt"
        }) end
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        chunk      = {x=ctx.chunk_x, y=ctx.chunk_y, size_m=S},
        -- R129 (zone-metadata bug fix) -- ctx.authored_zone, not ctx.zone:
        -- the latter may have been overridden by ChunkPipeline's own M157
        -- map-layer biome sampling, unrelated to this world's own flat config.
        generation = {variationInput=var, zone=ctx.authored_zone, region=ctx.region}
    })
end

return M
