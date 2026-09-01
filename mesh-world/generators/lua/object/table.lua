-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: simple wooden table

local M = {}
M.id       = "lua.object.table.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p   = ctx.parameters or {}
    local w   = p.width  or 1.2
    local d   = p.depth  or 0.8
    local h   = p.height or 0.75  -- total table height
    local tt  = 0.05  -- tabletop thickness
    local lt  = 0.06  -- leg thickness
    local mat = p.material or (ctx.style and ctx.style.wood_material) or "wood_natural"

    local lh = h - tt

    -- 4 legs
    local offsets = { {-1,-1}, {1,-1}, {-1,1}, {1,1} }
    for i, off in ipairs(offsets) do
        scene:addBox("leg_" .. i, {
            position = { off[1] * (w/2 - lt/2), lh/2, off[2] * (d/2 - lt/2) },
            size     = { lt, lh, lt },
            material = mat
        })
    end

    -- tabletop
    scene:addBox("top", {
        position = { 0, h - tt/2, 0 },
        size     = { w, tt, d },
        material = mat
    })

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = {
            type   = "table",
            style  = "simple_wooden",
            parts  = { legs = 4 },
            params = { width = w, depth = d, height = h }
        },
        generation = { variationInput = ctx.variation or 0 }
    })
end

return M
