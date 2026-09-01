-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: wardrobe (2-door)

local M = {}
M.id       = "lua.object.wardrobe.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.00
    local h    = p.height or 1.90
    local d    = p.depth  or 0.55
    local mat  = p.material  or (ctx.style and ctx.style.wood_material) or "wood_natural"
    local hmat = p.handle_material or "metal_chrome"

    -- Body
    scene:addBox("body", {position={0, h/2, 0}, size={w, h, d}, material=mat})

    -- Two doors (a thin seam between them, offset slightly forward of the body)
    local door_w = w/2 - 0.02
    scene:addBox("door_l", {position={-door_w/2 - 0.01, h/2, d/2 + 0.008}, size={door_w, h - 0.04, 0.015}, material=mat})
    scene:addBox("door_r", {position={ door_w/2 + 0.01, h/2, d/2 + 0.008}, size={door_w, h - 0.04, 0.015}, material=mat})

    -- Handles
    scene:addBox("handle_l", {position={-0.04, h/2, d/2 + 0.02}, size={0.02, 0.14, 0.02}, material=hmat})
    scene:addBox("handle_r", {position={ 0.04, h/2, d/2 + 0.02}, size={0.02, 0.14, 0.02}, material=hmat})

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="wardrobe", style="simple", parts={doors=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
