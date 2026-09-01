-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: generic post sign (T208)

local M = {}
M.id       = "lua.object.sign.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local h    = p.post_height  or 2.0
    local pw   = p.panel_width  or 0.55
    local ph   = p.panel_height or 0.40
    local mat  = p.material      or "sign_panel"
    local pmat = p.post_material or "metal_dark"

    -- Post
    scene:addCylinder("post", {
        position = {0, 0, 0},
        radius   = 0.03,
        height   = h,
        material = pmat
    })

    -- Panel (mounted near the top)
    scene:addBox("panel", {
        position = {0, h - ph/2 - 0.05, 0},
        size     = {pw, ph, 0.03},
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="sign", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
