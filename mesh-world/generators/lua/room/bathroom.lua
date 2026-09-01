-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bathroom (T218) -- toilet + sink + bathtub,
-- composed via scene:callGenerator. Same interior-placement-pipeline scope
-- note as generators/lua/room/living_room.lua -- content only, not wired
-- into any building generator yet.

local M = {}
M.id       = "lua.room.bathroom.basic"
M.version  = "0.1.0"
M.category = "room"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 2.4
    local d    = p.depth  or 2.2
    local fmat = p.floor_material or "tile_kitchen"

    scene:addPlane("floor", {position={0, 0, 0}, size={w, d}, material=fmat})

    local base_variation = (ctx.variation or 0) + 1

    -- Bathtub along the back wall
    scene:callGenerator("lua.object.bathtub.simple", {
        variation = base_variation, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {-(w/2 - 0.55), 0, -(d/2 - 0.90)}, rotation_y = 90, id = "bathtub"
    })

    -- Toilet in the corner
    scene:callGenerator("lua.object.toilet.simple", {
        variation = base_variation + 1, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {w/2 - 0.35, 0, -(d/2 - 0.35)}, id = "toilet"
    })

    -- Sink near the door
    scene:callGenerator("lua.object.sink.simple", {
        variation = base_variation + 2, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {w/2 - 0.35, 0, d/2 - 0.35}, id = "sink"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        room       = {type="bathroom", dims={w=w, d=d}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
