-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bicycle (T204)
--
-- Wheels use the same flattened-icosphere technique car.lua documents (no
-- rx/rz rotation binding exists to lay a vertical cylinder on its side).

local M = {}
M.id       = "lua.object.bicycle.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local r    = p.wheel_radius or 0.34
    local wb   = p.wheelbase or 1.05   -- distance between wheel hubs
    local fmat = p.frame_material or "bicycle_frame"
    local tmat = p.tire_material  or "tire_rubber"

    -- Wheels
    scene:addIcoSphere("wheel_front", {
        position = {0, r, wb/2},
        radius   = r,
        material = tmat,
        scale    = {0.12, 1.0, 1.0}
    })
    scene:addIcoSphere("wheel_rear", {
        position = {0, r, -wb/2},
        radius   = r,
        material = tmat,
        scale    = {0.12, 1.0, 1.0}
    })

    -- Main frame bar (hub to hub, low)
    scene:addBox("frame_main", {
        position = {0, r * 0.75, 0},
        size     = {0.05, 0.05, wb},
        material = fmat
    })

    -- Seat post
    scene:addBox("seat_post", {
        position = {0, r * 1.25, -wb * 0.15},
        size     = {0.04, r * 1.0, 0.04},
        material = fmat
    })

    -- Handlebar post
    scene:addBox("handlebar_post", {
        position = {0, r * 1.15, wb * 0.42},
        size     = {0.04, r * 0.8, 0.04},
        material = fmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="bicycle", style="simple", parts={wheels=2}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
