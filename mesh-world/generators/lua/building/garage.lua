-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: detached single-car garage -- floor slab + 3
-- solid walls + a front wall with a large roller-door opening (pillars +
-- lintel, since MC3 has no boolean wall-cutting) + a flat roof.
-- Composed like building/house/detached.lua: the roller door itself is
-- inline (no matching generator exists for it), but the optional side
-- pedestrian door reuses the real architecture/door/front_panel.lua
-- generator via scene:callGenerator() rather than duplicating door
-- geometry (procedural-model-generator-roadmap, G14).

local M = {}
M.id       = "lua.building.garage.detached"
M.version  = "0.1.0"
M.category = "building"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width       or 6.0    -- X footprint
    local d    = p.depth       or 6.0    -- Z footprint
    local wh   = p.wall_height or 2.6
    local wt   = 0.25                     -- wall thickness
    local wmat = p.wall_material  or "plaster_white"
    local rmat = p.roof_material  or "concrete_panel"
    local gmat = p.ground_material or "concrete"
    local dmat = p.door_material or "metal_dark"

    local base_variation = (ctx.variation or 0) + 1

    -- Floor slab
    scene:addBox("floor", {position={0, -0.10, 0}, size={w + wt*2, 0.20, d + wt*2}, material=gmat})

    -- Back and side walls, solid
    scene:addBox("wall_back",  {position={0, wh/2, -(d/2+wt/2)}, size={w + wt*2, wh, wt}, material=wmat})
    scene:addBox("wall_left",  {position={-(w/2+wt/2), wh/2, 0}, size={wt, wh, d},        material=wmat})
    scene:addBox("wall_right", {position={ (w/2+wt/2), wh/2, 0}, size={wt, wh, d},        material=wmat})

    -- Front wall: two pillars flanking the roller-door opening, plus a
    -- lintel above it (no CSG wall-cutting in MC3 -- same technique
    -- simple_house.lua's own door/window cut-outs use, generalized to a
    -- much wider opening).
    local door_w = p.door_width  or (w * 0.72)
    local door_h = p.door_height or 2.2
    local pillar_w = (w + wt*2 - door_w) / 2
    scene:addBox("wall_front_l", {
        position = {-(door_w/2 + pillar_w/2), wh/2, d/2 + wt/2}, size = {pillar_w, wh, wt}, material = wmat
    })
    scene:addBox("wall_front_r", {
        position = { (door_w/2 + pillar_w/2), wh/2, d/2 + wt/2}, size = {pillar_w, wh, wt}, material = wmat
    })
    scene:addBox("lintel", {
        position = {0, door_h + (wh - door_h)/2, d/2 + wt/2}, size = {door_w, wh - door_h, wt}, material = wmat
    })

    -- Roller door panel filling the opening, plus horizontal groove lines
    -- suggesting a real roller shutter (not a from-scratch mechanism --
    -- same "flat panel + thin accent boxes" technique architecture/gate/
    -- simple.lua's own hinge/latch hardware uses).
    scene:addBox("garage_door", {
        position = {0, door_h/2, d/2 + wt/2 + 0.02}, size = {door_w - 0.05, door_h, 0.04}, material = dmat
    })
    local grooves = 5
    for i = 1, grooves - 1 do
        local gy = door_h * i / grooves
        scene:addBox("door_groove_" .. i, {
            position = {0, gy, d/2 + wt/2 + 0.045}, size = {door_w - 0.10, 0.015, 0.006}, material = "metal_lamp"
        })
    end

    -- Optional side pedestrian door (right wall), composed from the real
    -- registered generator -- same rotation_y=-90 side-placement
    -- convention building/house/detached.lua's own side window uses.
    if p.side_door ~= false then
        scene:callGenerator("lua.architecture.door.front_panel", {
            variation  = base_variation,
            style      = ctx.style,
            lod        = ctx.lod,
            parameters = { width = 0.85, height = 1.95 }
        }, {
            position   = { (w/2 + wt/2 + 0.01), 0, d/4 },
            rotation_y = -90,
            id         = "side_door"
        })
    end

    -- Flat roof
    scene:addBox("roof", {
        position = {0, wh + 0.075, 0}, size = {w + wt*2 + 0.2, 0.15, d + wt*2 + 0.2}, material = rmat
    })

    scene:setMetadata({
        generator  = { id=M.id, version=M.version, category=M.category, language="lua" },
        object     = { type="garage", style="detached_flat_roof", dims={w=w, d=d, wall_height=wh} },
        generation = { variationInput=ctx.variation or 0 }
    })
end

return M
