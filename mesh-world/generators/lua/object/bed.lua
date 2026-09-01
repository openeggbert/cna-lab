-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bed (T195)

local M = {}
M.id       = "lua.object.bed.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 1.4   -- double bed
    local len  = p.length or 2.0
    local fh   = 0.25              -- frame height (base + headboard-relative)
    local mh   = 0.22              -- mattress height
    local fmat = p.frame_material    or (ctx.style and ctx.style.wood_material) or "wood_natural"
    local mmat = p.mattress_material or "fabric_bed"
    local pmat = p.pillow_material   or "fabric_pillow"

    -- Base frame
    scene:addBox("frame_base", {
        position = {0, fh/2, 0},
        size     = {w, fh, len},
        material = fmat
    })

    -- Headboard
    scene:addBox("headboard", {
        position = {0, fh + 0.35, -(len/2 - 0.04)},
        size     = {w, 0.70, 0.08},
        material = fmat
    })

    -- Mattress
    scene:addBox("mattress", {
        position = {0, fh + mh/2, 0},
        size     = {w - 0.06, mh, len - 0.06},
        material = mmat
    })

    -- Pillows (2, side by side near headboard)
    local pw = w / 2 - 0.08
    scene:addBox("pillow_l", {
        position = {-(pw/2 + 0.03), fh + mh + 0.06, -(len/2 - 0.30)},
        size     = {pw, 0.12, 0.40},
        material = pmat
    })
    scene:addBox("pillow_r", {
        position = { (pw/2 + 0.03), fh + mh + 0.06, -(len/2 - 0.30)},
        size     = {pw, 0.12, 0.40},
        material = pmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="bed", style="simple", parts={pillows=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
