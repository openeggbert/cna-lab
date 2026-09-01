-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bathtub (T202)

local M = {}
M.id       = "lua.object.bathtub.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.75
    local h    = p.height or 0.55
    local len  = p.length or 1.70
    local mat  = p.material or "ceramic_white"

    -- Outer body
    scene:addBox("body", {
        position = {0, h/2, 0},
        size     = {w, h, len},
        material = mat
    })

    -- Recessed basin (an inset box near the top, reads as the hollow interior)
    scene:addBox("basin", {
        position = {0, h - 0.06, 0},
        size     = {w - 0.10, 0.10, len - 0.10},
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="bathtub", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
