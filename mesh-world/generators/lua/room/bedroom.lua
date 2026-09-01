-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bedroom (T217) -- bed + wardrobe + nightstand,
-- composed via scene:callGenerator. Same interior-placement-pipeline scope
-- note as generators/lua/room/living_room.lua -- content only, not wired
-- into any building generator yet.

local M = {}
M.id       = "lua.room.bedroom.basic"
M.version  = "0.1.0"
M.category = "room"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 3.6
    local d    = p.depth  or 3.4
    local fmat = p.floor_material or "wood_natural"

    scene:addPlane("floor", {position={0, 0, 0}, size={w, d}, material=fmat})

    local base_variation = (ctx.variation or 0) + 1

    -- Bed against the back wall, centred
    scene:callGenerator("lua.object.bed.simple", {
        variation = base_variation, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {0, 0, -(d/2 - 1.1)}, id = "bed"
    })

    -- Wardrobe along a side wall
    scene:callGenerator("lua.object.wardrobe.simple", {
        variation = base_variation + 1, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {-(w/2 - 0.30), 0, d/2 - 1.0}, rotation_y = 90, id = "wardrobe"
    })

    -- Nightstand beside the bed
    scene:callGenerator("lua.object.nightstand.simple", {
        variation = base_variation + 2, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {w/2 - 0.35, 0, -(d/2 - 1.6)}, id = "nightstand"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        room       = {type="bedroom", dims={w=w, d=d}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
