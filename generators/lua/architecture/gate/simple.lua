-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: simple swinging gate -- 2 posts, 1 hinged panel
-- (frame + pickets, matching architecture/fence/wood_picket.lua's own
-- style) sitting closed in the opening (procedural-model-generator-
-- roadmap, architecture tier, G14)

local M = {}
M.id       = "lua.architecture.gate.simple"
M.version  = "0.1.0"
M.category = "architecture"

-- Same local coordinate convention as fence/wood_picket.lua: the opening
-- runs along X, centered at the local origin, z=0.
function M.generate(ctx, scene)
    local p        = ctx.parameters or {}
    local width    = p.width  or 1.20   -- clear opening width
    local h        = p.height or 1.0
    local mat      = p.material or "wood_fence"
    local hardware_mat = p.hardware_material or "metal_dark"

    -- Posts (matching fence/wood_picket.lua's own post proportions)
    local post_h = h + 0.10
    local post_w = 0.10
    scene:addBox("post_l", {
        position = { -(width/2 + post_w/2), 0, 0 }, size = { post_w, post_h, post_w }, material = mat
    })
    scene:addBox("post_r", {
        position = {  (width/2 + post_w/2), 0, 0 }, size = { post_w, post_h, post_w }, material = mat
    })

    -- Gate panel: a light frame + 4 evenly-spaced pickets, shown closed
    -- (filling the opening) -- an open/swung-out state would need the
    -- panel rotated about the hinge post, which (same limitation noted in
    -- fence/wood_picket.lua) isn't a composition Mc3SceneBuilder's rx/rz
    -- guarantees correctly through a nested transform yet.
    local frame_t = 0.05
    scene:addBox("frame_top", {position={0, h - frame_t/2, 0}, size={width, frame_t, 0.03}, material=mat})
    scene:addBox("frame_bottom", {position={0, frame_t/2, 0}, size={width, frame_t, 0.03}, material=mat})

    local picket_w = 0.07
    local count = 4
    local pitch = width / count
    for i = 0, count - 1 do
        local x = -width/2 + pitch * (i + 0.5)
        scene:addBox("picket_" .. (i + 1), {
            position = { x, h / 2, 0 }, size = { picket_w, h, 0.025 }, material = mat
        })
    end

    -- Hinges (hinge side = left post) + a latch (right side)
    scene:addBox("hinge_top", {
        position = { -(width/2 - 0.03), h * 0.85, 0.02 }, size = { 0.05, 0.05, 0.04 }, material = hardware_mat
    })
    scene:addBox("hinge_bottom", {
        position = { -(width/2 - 0.03), h * 0.15, 0.02 }, size = { 0.05, 0.05, 0.04 }, material = hardware_mat
    })
    scene:addBox("latch", {
        position = { (width/2 - 0.04), h * 0.55, 0.03 }, size = { 0.10, 0.04, 0.03 }, material = hardware_mat
    })

    scene:setMetadata({
        generator  = { id=M.id, version=M.version, category=M.category, language="lua" },
        object     = { type="gate", style="simple", parts={pickets=count} },
        generation = { variationInput=ctx.variation or 0 }
    })
end

return M
