-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: microwave oven (T198)

local M = {}
M.id       = "lua.object.microwave.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.48
    local h    = p.height or 0.30
    local d    = p.depth  or 0.40
    local mat  = p.material or "appliance_white"
    local hmat = p.handle_material or "plastic_black"

    -- Body
    scene:addBox("body", {
        position = {0, h/2, 0},
        size     = {w, h, d},
        material = mat
    })

    -- Door handle (small vertical box on the front-right edge)
    scene:addBox("handle", {
        position = {w/2 - 0.03, h/2, d/2 + 0.015},
        size     = {0.02, h * 0.7, 0.03},
        material = hmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="microwave", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
