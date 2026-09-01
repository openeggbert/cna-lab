-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bookshelf (T197)

local M = {}
M.id       = "lua.object.bookshelf.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p     = ctx.parameters or {}
    local w     = p.width  or 0.90
    local h     = p.height or 1.80
    local d     = p.depth  or 0.30
    local shelves = p.shelf_count or 4
    local pt    = 0.03   -- panel thickness
    local mat   = p.material or (ctx.style and ctx.style.wood_material) or "wood_natural"

    -- Back panel
    scene:addBox("back", {
        position = {0, h/2, -(d/2 - pt/2)},
        size     = {w, h, pt},
        material = mat
    })

    -- Side panels
    scene:addBox("side_l", {
        position = {-(w/2 - pt/2), h/2, 0},
        size     = {pt, h, d},
        material = mat
    })
    scene:addBox("side_r", {
        position = { (w/2 - pt/2), h/2, 0},
        size     = {pt, h, d},
        material = mat
    })

    -- Shelves (evenly spaced, including top and bottom)
    local gaps = math.max(shelves - 1, 1)
    for i = 0, shelves - 1 do
        local y = (h - pt) * (i / gaps) + pt/2
        scene:addBox("shelf_" .. (i + 1), {
            position = {0, y, 0},
            size     = {w - pt * 2, pt, d - pt},
            material = mat
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="bookshelf", style="simple", parts={shelves=shelves}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
