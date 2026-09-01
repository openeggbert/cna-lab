-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: picnic table with 2 attached benches (T212)

local M = {}
M.id       = "lua.object.picnic_table.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.75
    local len  = p.length or 1.80
    local th   = p.table_height or 0.72
    local mat  = p.material or (ctx.style and ctx.style.wood_material) or "wood_natural"

    -- Tabletop
    scene:addBox("top", {
        position = {0, th, 0},
        size     = {w, 0.05, len},
        material = mat
    })

    -- Table legs (4, trestle-style uprights)
    local lt = 0.06
    local leg_offsets = { {-1,-1}, {1,-1}, {-1,1}, {1,1} }
    for i, off in ipairs(leg_offsets) do
        scene:addBox("leg_" .. i, {
            position = {off[1] * (w/2 - lt/2), th/2, off[2] * (len/2 - 0.15)},
            size     = {lt, th, lt},
            material = mat
        })
    end

    -- Attached benches (one each side, lower and wider-spaced than the table)
    local bench_h = th * 0.6
    local bench_w = 0.22
    local bench_offset = w/2 + 0.30
    for i, side in ipairs({-1, 1}) do
        local suffix = (side < 0) and "l" or "r"
        scene:addBox("bench_" .. suffix, {
            position = {side * bench_offset, bench_h, 0},
            size     = {bench_w, 0.04, len},
            material = mat
        })
        -- 2 support legs per bench
        for j, z in ipairs({-(len/2 - 0.15), (len/2 - 0.15)}) do
            scene:addBox("bench_" .. suffix .. "_leg_" .. j, {
                position = {side * bench_offset, bench_h/2, z},
                size     = {lt, bench_h, lt},
                material = mat
            })
        end
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="picnic_table", style="simple", parts={benches=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
