-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: living room (T216) -- sofa + coffee table + tv +
-- bookshelves, composed via scene:callGenerator.
--
-- IMPORTANT SCOPE NOTE (same honesty room/kitchen.lua's own history already
-- established, and the T216-T218 triage note in plan.md/NEXT.md repeats):
-- this produces real, independently-testable room CONTENT, but there is
-- still no interior-room PLACEMENT PIPELINE anywhere in this codebase --
-- no building generator (SmallHouseBlockGenerator.cpp, ApartmentBlockGenerator.cpp,
-- lua.building.house.detached, ...) currently calls this. Building that
-- pipeline (deciding WHERE inside a building's footprint a room goes, at
-- what floor, with real wall openings) is a separate, larger, cross-cutting
-- feature -- not attempted here. This satisfies T216's literal ask (the
-- Lua generator itself) with room to be wired in properly later.

local M = {}
M.id       = "lua.room.living_room.basic"
M.version  = "0.1.0"
M.category = "room"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 4.5
    local d    = p.depth  or 4.0
    local fmat = p.floor_material or "wood_natural"

    scene:addPlane("floor", {position={0, 0, 0}, size={w, d}, material=fmat})

    local base_variation = (ctx.variation or 0) + 1

    -- Sofa along the back wall
    scene:callGenerator("lua.object.sofa.simple", {
        variation = base_variation, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {0, 0, -(d/2 - 0.55)}, id = "sofa"
    })

    -- Coffee table in front of the sofa (table.lua sized down)
    scene:callGenerator("lua.object.table.simple", {
        variation = base_variation + 1, style = ctx.style, lod = ctx.lod,
        parameters = {width = 0.90, depth = 0.55, height = 0.42}
    }, {
        position = {0, 0, -(d/2 - 1.5)}, id = "coffee_table"
    })

    -- TV against the front wall, facing the sofa
    scene:callGenerator("lua.object.tv.flatscreen", {
        variation = base_variation + 2, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {0, 0, d/2 - 0.15}, rotation_y = 180, id = "tv"
    })

    -- Two bookshelves along a side wall
    scene:callGenerator("lua.object.bookshelf.simple", {
        variation = base_variation + 3, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {-(w/2 - 0.20), 0, d/2 - 0.7}, rotation_y = 90, id = "bookshelf_a"
    })
    scene:callGenerator("lua.object.bookshelf.simple", {
        variation = base_variation + 4, style = ctx.style, lod = ctx.lod, parameters = {}
    }, {
        position = {-(w/2 - 0.20), 0, d/2 - 1.9}, rotation_y = 90, id = "bookshelf_b"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        room       = {type="living_room", dims={w=w, d=d}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
