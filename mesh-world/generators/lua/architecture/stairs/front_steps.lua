-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: front steps leading up to a raised doorway
-- (procedural-model-generator-roadmap, architecture tier)

local M = {}
M.id       = "lua.architecture.stairs.front_steps"
M.version  = "0.1.0"
M.category = "architecture"

function M.generate(ctx, scene)
    local p          = ctx.parameters or {}
    local step_count = p.step_count or 3
    local step_h     = p.step_height or 0.17
    local step_d     = p.step_depth  or 0.30
    local w          = p.width or 1.40
    local mat        = p.material or "stone_light"

    -- Steps rise toward the door (z=0, the doorway edge); each step is a
    -- solid slab from the ground up to its own tread height, deepest/lowest
    -- step furthest from the door.
    for i = 1, step_count do
        local slab_h = step_h * i
        local z = -(step_count - i) * step_d - step_d/2
        scene:addBox("step_" .. i, {
            position = {0, slab_h/2, z},
            size     = {w, slab_h, step_d},
            material = mat
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="stairs", style="front_steps", parts={steps=step_count}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
