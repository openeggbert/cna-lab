-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: sofa (T196)

local M = {}
M.id       = "lua.object.sofa.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.8
    local d    = p.depth  or 0.85
    local sh   = p.seat_height or 0.42
    local bh   = p.back_height or 0.55
    local aw   = 0.16   -- arm width
    local mat  = p.material or "fabric_sofa"

    -- Seat cushion block
    scene:addBox("seat", {
        position = {0, sh/2, 0.05},
        size     = {w - aw * 2, sh, d - 0.05},
        material = mat
    })

    -- Backrest
    scene:addBox("back", {
        position = {0, sh + bh/2, -(d/2 - 0.08)},
        size     = {w - aw * 2, bh, 0.16},
        material = mat
    })

    -- Arms (both sides, full seat+back height)
    local arm_h = sh + bh
    scene:addBox("arm_l", {
        position = {-(w/2 - aw/2), arm_h/2, 0},
        size     = {aw, arm_h, d},
        material = mat
    })
    scene:addBox("arm_r", {
        position = { (w/2 - aw/2), arm_h/2, 0},
        size     = {aw, arm_h, d},
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="sofa", style="simple", parts={arms=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
