-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: crossroad zone chunk (64×64 m)

local M = {}
M.id       = "lua.zone.crossroad"
M.version  = "0.1.0"
M.category = "zone"

function M.generate(ctx, scene)
    local S    = ctx.chunk_size_m
    local half = S / 2
    local road_w = 12
    local var  = ctx.variation or 0

    -- Sidewalk base
    scene:addGround("sidewalk")

    -- R134/R138 -- the Lua-first compatibility path must obey the same
    -- canonical chunk-edge graph as the C++ fallback.  Full-tile N-S/E-W
    -- strips used to paint arms through every absent neighbour.
    local arm = half - road_w/2
    scene:addPlane("road_center", {
        position = {half - road_w/2, 0.02, half - road_w/2},
        size     = {road_w, road_w}, material = "asphalt"
    })
    if ctx.exits.north_road then scene:addPlane("road_n", {
        position = {half - road_w/2, 0.02, 0}, size = {road_w, arm}, material = "asphalt"
    }) end
    if ctx.exits.south_road then scene:addPlane("road_s", {
        position = {half - road_w/2, 0.02, half + road_w/2}, size = {road_w, arm}, material = "asphalt"
    }) end
    if ctx.exits.east_road then scene:addPlane("road_e", {
        position = {half + road_w/2, 0.02, half - road_w/2}, size = {arm, road_w}, material = "asphalt"
    }) end
    if ctx.exits.west_road then scene:addPlane("road_w", {
        position = {0, 0.02, half - road_w/2}, size = {arm, road_w}, material = "asphalt"
    }) end

    -- Crosswalk stripes (N, S, E, W approaches, 4 m wide)
    local stripe_w  = road_w
    local stripe_z0 = half + road_w/2          -- south edge of junction
    local stripe_z1 = half - road_w/2 - 4      -- north approach end
    local cw_mat    = "road_line_white"
    local cw_stripe = 0.5
    local cw_gap    = 0.5

    -- Crosswalks must follow the same canonical arms as the asphalt; a
    -- stripe into a missing approach is just another visual road stub.
    local cx, cz, si
    if ctx.exits.south_road then
        cx = half - stripe_w/2
        cz = stripe_z0 + 0.5
        si = 0
        while cx < half + stripe_w/2 do
            si = si + 1
            scene:addPlane("cw_s_"..si, {position={cx, 0.03, cz}, size={cw_stripe, 3.5}, material=cw_mat})
            cx = cx + cw_stripe + cw_gap
        end
    end

    if ctx.exits.north_road then
        cx = half - stripe_w/2
        cz = stripe_z1 - 0.5
        si = 0
        while cx < half + stripe_w/2 do
            si = si + 1
            scene:addPlane("cw_n_"..si, {position={cx, 0.03, cz - 3.5}, size={cw_stripe, 3.5}, material=cw_mat})
            cx = cx + cw_stripe + cw_gap
        end
    end

    -- R138 -- one deterministic state per approach.  The renderer does not
    -- advance MC3 object states/actions yet, so a stable snapshot is clearer
    -- and more truthful than all red/amber/green lenses being on together.
    local corners = {
        {half - road_w/2 - 1, half - road_w/2 - 1},
        {half + road_w/2 + 1, half - road_w/2 - 1},
        {half - road_w/2 - 1, half + road_w/2 + 1},
        {half + road_w/2 + 1, half + road_w/2 + 1},
    }
    local phase = var % 3
    local function active_signal(north_south)
        if phase == 0 then return north_south and "green" or "red" end
        if phase == 1 then return north_south and "amber" or "red" end
        return north_south and "red" or "green"
    end
    local function lens_material(active, color)
        if active == color then return "light_" .. color end
        return "light_" .. color .. "_dim"
    end
    for i, c in ipairs(corners) do
        local px, pz = c[1], c[2]
        local active = active_signal(i == 1 or i == 4)
        scene:addCylinder("tl_pole_"..i, {position={px, 0,   pz}, radius=0.06, height=4.5, material="metal_dark"})
        scene:addBox     ("tl_head_"..i, {position={px, 4.35, pz}, size={0.34, 0.90, 0.28}, material="metal_dark"})
        scene:addSphere("tl_red_"..i,   {position={px, 4.95, pz}, radius=0.12, material=lens_material(active, "red")})
        scene:addSphere("tl_amber_"..i, {position={px, 4.65, pz}, radius=0.12, material=lens_material(active, "amber")})
        scene:addSphere("tl_green_"..i, {position={px, 4.35, pz}, radius=0.12, material=lens_material(active, "green")})
    end

    -- R129 (zone-metadata bug fix) -- ctx.authored_zone, not ctx.zone: the
    -- latter may have been overridden by ChunkPipeline's own M157 map-layer
    -- biome sampling, which is unrelated to this world's own flat config.
    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        chunk      = {x=ctx.chunk_x, y=ctx.chunk_y, size_m=S},
        generation = {variationInput=var, zone=ctx.authored_zone, region=ctx.region}
    })
end

return M
