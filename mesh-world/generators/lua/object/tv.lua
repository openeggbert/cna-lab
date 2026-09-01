-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: flat-screen television

local M = {}
M.id       = "lua.object.tv.flatscreen"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local sw   = p.screen_width  or 1.20   -- screen width
    local sh   = p.screen_height or 0.70   -- screen height
    local sd   = 0.06                       -- screen depth (thin)
    local mat  = p.material or "tv_screen_off"
    local fmat = p.frame_material or "plastic_black"

    -- Frame (slightly larger than screen)
    scene:addBox("frame", {
        position = {0, sh/2 + 0.04, 0},
        size     = {sw + 0.06, sh + 0.06, sd + 0.01},
        material = fmat
    })

    -- Screen panel
    scene:addBox("screen", {
        position = {0, sh/2 + 0.04, sd/2 + 0.006},
        size     = {sw, sh, 0.005},
        material = mat
    })

    -- Stand pedestal
    local stand_h = 0.15
    scene:addBox("stand_neck", {
        position = {0, stand_h/2, 0},
        size     = {0.10, stand_h, 0.08},
        material = fmat
    })
    scene:addBox("stand_base", {
        position = {0, 0.02, 0},
        size     = {0.40, 0.04, 0.22},
        material = fmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="tv", style="flatscreen"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
