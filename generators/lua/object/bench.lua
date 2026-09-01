-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: park bench

local M = {}
M.id       = "lua.object.bench.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p   = ctx.parameters or {}
    local w   = p.width  or 1.6
    local sh  = p.seat_height or 0.44
    local mat = p.material or (ctx.style and ctx.style.bench_material) or "wood_bench"
    local leg = p.leg_material or (ctx.style and ctx.style.metal_material) or "metal_lamp"

    -- Two support legs (cast iron / metal ends)
    scene:addBox("leg_l", {
        position = { -(w/2 - 0.1), sh/2, 0 },
        size     = { 0.08, sh, 0.5 },
        material = leg
    })
    scene:addBox("leg_r", {
        position = {  (w/2 - 0.1), sh/2, 0 },
        size     = { 0.08, sh, 0.5 },
        material = leg
    })

    -- Seat planks
    scene:addBox("seat", {
        position = { 0, sh + 0.02, 0.08 },
        size     = { w - 0.05, 0.04, 0.35 },
        material = mat
    })

    -- Backrest
    if p.backrest ~= false then
        scene:addBox("backrest", {
            position = { 0, sh + 0.30, -(0.19) },
            size     = { w - 0.05, 0.35, 0.04 },
            material = mat
        })
    end

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = { type = "bench", style = "park_bench" },
        generation = { variationInput = ctx.variation or 0 }
    })
end

return M
