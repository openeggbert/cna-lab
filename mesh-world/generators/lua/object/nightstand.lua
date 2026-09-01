-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bedside nightstand (body + drawer + handle)

local M = {}
M.id       = "lua.object.nightstand.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.45
    local h    = p.height or 0.55
    local d    = p.depth  or 0.40
    local mat  = p.material  or (ctx.style and ctx.style.wood_material) or "wood_natural"
    local hmat = p.handle_material or "metal_chrome"

    -- Body
    scene:addBox("body", {position={0, h/2, 0}, size={w, h, d}, material=mat})

    -- Drawer front
    scene:addBox("drawer", {
        position = {0, h * 0.68, d/2 + 0.008},
        size     = {w - 0.06, h * 0.28, 0.015},
        material = mat
    })

    -- Handle
    scene:addBox("handle", {
        position = {0, h * 0.68, d/2 + 0.02},
        size     = {0.10, 0.02, 0.02},
        material = hmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="nightstand", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
