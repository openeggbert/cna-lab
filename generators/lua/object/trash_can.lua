-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: street trash can

local M = {}
M.id       = "lua.object.trash_can.street"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local r    = p.radius or 0.22
    local h    = p.height or 0.90
    local mat  = p.material or "metal_dark"
    local lmat = p.lid_material or "plastic_black"

    -- Body
    scene:addCylinder("body", {
        position = {0, 0, 0},
        radius   = r,
        height   = h,
        material = mat
    })

    -- Lid (slightly wider, sits on top)
    scene:addCylinder("lid", {
        position = {0, h, 0},
        radius   = r + 0.02,
        height   = 0.08,
        material = lmat
    })

    -- Mounting post (wall-mounted variant has none; stand-alone has a short post)
    if not p.wall_mounted then
        scene:addCylinder("post", {
            position = {0, 0, -(r + 0.05)},
            radius   = 0.04,
            height   = h + 0.05,
            material = mat
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="trash_can", style="street"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
