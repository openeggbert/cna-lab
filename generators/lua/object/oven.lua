-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: kitchen oven (T199)

local M = {}
M.id       = "lua.object.oven.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.60
    local h    = p.height or 0.85
    local d    = p.depth  or 0.60
    local mat  = p.material      or "appliance_white"
    local dmat = p.door_material or "metal_dark"

    -- Body
    scene:addBox("body", {
        position = {0, h/2, 0},
        size     = {w, h, d},
        material = mat
    })

    -- Door (inset panel covering the lower two-thirds of the front face)
    local door_h = h * 0.55
    scene:addBox("door", {
        position = {0, door_h/2 + 0.03, d/2 + 0.005},
        size     = {w - 0.04, door_h, 0.01},
        material = dmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="oven", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
