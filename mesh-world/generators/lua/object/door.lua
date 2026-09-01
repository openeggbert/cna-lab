-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: door (frame + panel)

local M = {}
M.id       = "lua.object.door.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.95
    local h    = p.height or 2.10
    local ft   = 0.08          -- frame thickness
    local pt   = 0.05          -- panel thickness
    local fmat = p.frame_material or "wood_door_frame"
    local pmat = p.panel_material or "wood_door_panel"

    -- Frame: top, left, right beams
    scene:addBox("frame_top", {
        position = {0, h - ft/2, 0},
        size     = {w, ft, ft},
        material = fmat
    })
    scene:addBox("frame_l", {
        position = {-(w/2 - ft/2), h/2, 0},
        size     = {ft, h, ft},
        material = fmat
    })
    scene:addBox("frame_r", {
        position = { (w/2 - ft/2), h/2, 0},
        size     = {ft, h, ft},
        material = fmat
    })

    -- Door panel (slightly inset from frame)
    local pw = w - ft * 2 - 0.02
    local ph = h - ft     - 0.02
    scene:addBox("panel", {
        position = {0, ph/2, 0},
        size     = {pw, ph, pt},
        material = pmat
    })

    -- Handle
    scene:addCylinder("handle", {
        position = {pw/2 - 0.08, h * 0.45, pt/2 + 0.04},
        radius   = 0.02,
        height   = 0.12,
        material = "metal_chrome"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="door", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
