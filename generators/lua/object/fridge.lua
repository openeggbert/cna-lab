-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: refrigerator

local M = {}
M.id       = "lua.object.fridge.standard"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p   = ctx.parameters or {}
    local w   = p.width  or 0.70
    local d   = p.depth  or 0.68
    local h   = p.height or 1.80
    local mat = p.material or "appliance_white"

    -- Body
    scene:addBox("body", {
        position = {0, h/2, 0},
        size     = {w, h, d},
        material = mat
    })

    -- Door seam line (thin box on front face)
    scene:addBox("seam", {
        position = {0, h*0.55, d/2 + 0.001},
        size     = {w - 0.02, 0.01, 0.005},
        material = "metal_dark"
    })

    -- Handle (small cylinder on front)
    scene:addCylinder("handle", {
        position = {w/2 - 0.06, h * 0.75, d/2 + 0.04},
        radius   = 0.015,
        height   = h * 0.35,
        material = "metal_chrome"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="fridge", style="standard"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
