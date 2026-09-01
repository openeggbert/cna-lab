-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: fire hydrant (T206)

local M = {}
M.id       = "lua.object.fire_hydrant.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local h    = p.height or 0.65
    local mat  = p.material or "hydrant_red"

    -- Body (main barrel)
    scene:addCylinder("body", {
        position = {0, 0, 0},
        radius   = 0.14,
        height   = h,
        material = mat
    })

    -- Cap (bonnet on top)
    scene:addBox("cap", {
        position = {0, h + 0.06, 0},
        size     = {0.16, 0.12, 0.16},
        material = mat
    })

    -- Side nozzles (two small stubs, left/right)
    scene:addCylinder("nozzle_l", {
        position = {-0.16, h * 0.55, 0},
        radius   = 0.045,
        height   = 0.10,
        material = mat
    })
    scene:addCylinder("nozzle_r", {
        position = {0.16, h * 0.55, 0},
        radius   = 0.045,
        height   = 0.10,
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="fire_hydrant", style="simple", parts={nozzles=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
