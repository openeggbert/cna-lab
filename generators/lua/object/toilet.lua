-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: toilet (T201)

local M = {}
M.id       = "lua.object.toilet.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local mat  = p.material or "ceramic_white"
    local bh   = p.base_height or 0.40
    local bw   = p.base_width  or 0.38
    local bd   = p.base_depth  or 0.55

    -- Base (bowl + pedestal, approximated as one box)
    scene:addBox("base", {
        position = {0, bh/2, 0},
        size     = {bw, bh, bd},
        material = mat
    })

    -- Tank (sits on top, toward the back)
    local th = 0.35
    scene:addBox("tank", {
        position = {0, bh + th/2, -(bd/2 - 0.08)},
        size     = {bw - 0.02, th, 0.18},
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="toilet", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
