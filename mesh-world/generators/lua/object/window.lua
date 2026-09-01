-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: window (frame + glass plane)

local M = {}
M.id       = "lua.object.window.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.20
    local h    = p.height or 1.40
    local ft   = 0.07
    local fmat = p.frame_material or "wood_window_frame"
    local gmat = p.glass_material or "glass_clear"

    -- Outer frame
    scene:addBox("frame_top", {position={0, h - ft/2, 0}, size={w, ft, ft}, material=fmat})
    scene:addBox("frame_bot", {position={0, ft/2,     0}, size={w, ft, ft}, material=fmat})
    scene:addBox("frame_l",   {position={-(w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})
    scene:addBox("frame_r",   {position={ (w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})

    -- Centre cross-bar (divides window into 4 panes)
    scene:addBox("bar_h", {position={0, h/2,     0}, size={w - ft*2, ft*0.6, ft*0.5}, material=fmat})
    scene:addBox("bar_v", {position={0, h/2,     0}, size={ft*0.6, h - ft*2, ft*0.5}, material=fmat})

    -- Glass (4 panes as planes)
    local pw = (w - ft*3) / 2
    local ph = (h - ft*3) / 2
    scene:addPlane("pane_tl", {position={-(pw/2 + ft*0.3), ft + ph + ft*0.6, 0}, size={pw, ph}, material=gmat})
    scene:addPlane("pane_tr", {position={ (pw/2 + ft*0.3), ft + ph + ft*0.6, 0}, size={pw, ph}, material=gmat})
    scene:addPlane("pane_bl", {position={-(pw/2 + ft*0.3), ft + ph*0.5,      0}, size={pw, ph}, material=gmat})
    scene:addPlane("pane_br", {position={ (pw/2 + ft*0.3), ft + ph*0.5,      0}, size={pw, ph}, material=gmat})

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="window", style="simple", panes=4},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
