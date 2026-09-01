-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: simple wooden chair

local M = {}
M.id       = "lua.object.chair.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p   = ctx.parameters or {}
    local w   = p.width    or 0.45
    local d   = p.depth    or 0.45
    local sh  = p.seat_height or 0.44  -- seat top height
    local lt  = 0.04   -- leg thickness
    local mat = p.material or (ctx.style and ctx.style.wood_material) or "wood_natural"

    -- 4 legs (cylinders at each corner)
    local offsets = { {-1,-1}, {1,-1}, {-1,1}, {1,1} }
    for i, off in ipairs(offsets) do
        scene:addCylinder("leg_" .. i, {
            position = { off[1] * (w/2 - lt), 0, off[2] * (d/2 - lt) },
            radius   = lt / 2,
            height   = sh,
            material = mat
        })
    end

    -- seat
    scene:addBox("seat", {
        position = { 0, sh, 0 },
        size     = { w, 0.04, d },
        material = mat
    })

    -- backrest (optional)
    if p.backrest ~= false then
        local bh = p.backrest_height or 0.45
        scene:addBox("backrest", {
            position = { 0, sh + bh/2, -(d/2 - 0.02) },
            size     = { w - 0.04, bh, 0.04 },
            material = mat
        })
    end

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = {
            type    = "chair",
            style   = "simple_wooden",
            parts   = { legs = 4, hasBackrest = (p.backrest ~= false) }
        },
        generation = {
            variationInput = ctx.variation or 0,
            notes = "Variation input is not a long-term compatibility guarantee."
        }
    })
end

return M
