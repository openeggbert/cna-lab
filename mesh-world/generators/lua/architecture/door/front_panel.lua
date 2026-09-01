-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: front door -- frame, panel, handle, optional
-- glass insert and mail slot (procedural-model-generator-roadmap, architecture tier)

local M = {}
M.id       = "lua.architecture.door.front_panel"
M.version  = "0.1.0"
M.category = "architecture"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.00
    local h    = p.height or 2.10
    local ft   = 0.09
    local pt   = 0.05
    local fmat = p.frame_material or "wood_door_frame"
    local pmat = p.panel_material or "wood_door_panel"
    local gmat = p.glass_material or "glass_clear"

    -- Frame: top, left, right beams
    scene:addBox("frame_top", {position={0, h - ft/2, 0}, size={w, ft, ft}, material=fmat})
    scene:addBox("frame_l",   {position={-(w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})
    scene:addBox("frame_r",   {position={ (w/2 - ft/2), h/2, 0}, size={ft, h, ft}, material=fmat})

    -- Panel
    local pw = w - ft * 2 - 0.02
    local ph = h - ft     - 0.02
    scene:addBox("panel", {position={0, ph/2, 0}, size={pw, ph, pt}, material=pmat})

    -- Handle
    scene:addCylinder("handle", {
        position = {pw/2 - 0.08, h * 0.45, pt/2 + 0.04},
        radius   = 0.02,
        height   = 0.12,
        material = "metal_chrome"
    })

    -- Optional small glass insert near the top of the panel
    local has_glass = p.glass_insert
    if has_glass == nil then
        has_glass = (ctx.random ~= nil) and (ctx.random() < 0.35)
    end
    if has_glass then
        scene:addPlane("glass_insert", {
            position = {0, ph * 0.78, pt/2 + 0.005},
            size     = {pw * 0.55, ph * 0.18},
            material = gmat
        })
    end

    -- Optional mail slot (a thin horizontal box near the bottom of the panel)
    local has_mail_slot = p.mail_slot
    if has_mail_slot == nil then
        has_mail_slot = (ctx.random ~= nil) and (ctx.random() < 0.35)
    end
    if has_mail_slot then
        scene:addBox("mail_slot", {
            position = {0, ph * 0.22, pt/2 + 0.008},
            size     = {pw * 0.35, 0.04, 0.01},
            material = "metal_dark"
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="door", style="front_panel",
                       parts={hasGlassInsert=has_glass, hasMailSlot=has_mail_slot}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
