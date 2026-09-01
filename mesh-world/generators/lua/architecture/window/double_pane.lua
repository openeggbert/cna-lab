-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: double-pane window, with an optional sill and
-- optional shutters (procedural-model-generator-roadmap, architecture tier)

local M = {}
M.id       = "lua.architecture.window.double_pane"
M.version  = "0.1.0"
M.category = "architecture"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.10
    local h    = p.height or 1.30
    local ft   = 0.07   -- frame thickness
    local fmat = p.frame_material   or "wood_window_frame"
    local gmat = p.glass_material   or "glass_clear"
    local smat = p.sill_material    or "stone_light"
    local shmat= p.shutter_material or "wood_natural"

    -- Outer frame
    scene:addBox("frame_top", {position={0, h - ft/2, 0}, size={w, ft, ft}, material=fmat})
    scene:addBox("frame_bot", {position={0, ft/2,     0}, size={w, ft, ft}, material=fmat})
    scene:addBox("frame_l",   {position={-(w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})
    scene:addBox("frame_r",   {position={ (w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})

    -- Centre mullion (splits into 2 panes, "double pane")
    scene:addBox("mullion", {position={0, h/2, 0}, size={ft*0.7, h - ft*2, ft*0.5}, material=fmat})

    -- Glass panes
    local pw = (w - ft*3) / 2
    local ph = h - ft*2
    scene:addPlane("pane_l", {position={-(pw/2 + ft*0.35), ft + ph/2, 0}, size={pw, ph}, material=gmat})
    scene:addPlane("pane_r", {position={ (pw/2 + ft*0.35), ft + ph/2, 0}, size={pw, ph}, material=gmat})

    -- Sill (protrudes below the frame)
    scene:addBox("sill", {position={0, -0.04, 0.05}, size={w + 0.18, 0.06, ft + 0.10}, material=smat})

    -- Shutters: deterministic per-call default via ctx.random() (seeded
    -- from ctx.variation/ctx.seed -- same house seed always gets the same
    -- shutter presence, but different houses/chunks vary).
    local has_shutters = p.shutters
    if has_shutters == nil then
        has_shutters = (ctx.random ~= nil) and (ctx.random() < 0.5)
    end
    if has_shutters then
        local sh_w = w * 0.42
        scene:addBox("shutter_l", {
            position = {-(w/2 + sh_w/2 + 0.02), h/2, ft/2 + 0.02},
            size     = {sh_w, h, 0.03},
            material = shmat
        })
        scene:addBox("shutter_r", {
            position = { (w/2 + sh_w/2 + 0.02), h/2, ft/2 + 0.02},
            size     = {sh_w, h, 0.03},
            material = shmat
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="window", style="double_pane", parts={panes=2, hasShutters=has_shutters}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
