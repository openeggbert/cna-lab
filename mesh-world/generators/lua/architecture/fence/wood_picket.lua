-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: wood picket fence segment -- 2 end posts, 2
-- horizontal rails, N evenly-spaced vertical pickets (procedural-model-
-- generator-roadmap, architecture tier, G14)

local M = {}
M.id       = "lua.architecture.fence.wood_picket"
M.version  = "0.1.0"
M.category = "architecture"

-- Local coordinate convention: the segment runs along X, from -length/2 to
-- +length/2, at z=0 -- the same "centered at local origin, caller positions
-- and rotates via scene:callGenerator()'s own placement" convention every
-- other architecture-tier generator here already uses.
function M.generate(ctx, scene)
    local p       = ctx.parameters or {}
    local length  = p.length        or 2.0    -- segment span along X
    local h       = p.height        or 1.0
    local picket_w = p.picket_width or 0.08
    local gap      = p.picket_gap   or 0.06
    local pmat     = p.picket_material or "wood_fence"
    local post_mat = p.post_material   or "wood_fence"

    -- End posts (slightly taller than the picket line, like a real fence)
    local post_h = h + 0.10
    local post_w = 0.10
    scene:addBox("post_l", {
        position = { -length/2, 0, 0 }, size = { post_w, post_h, post_w }, material = post_mat
    })
    scene:addBox("post_r", {
        position = {  length/2, 0, 0 }, size = { post_w, post_h, post_w }, material = post_mat
    })

    -- Horizontal rails (top and bottom), spanning between the posts
    local rail_span = length - post_w
    scene:addBox("rail_top", {
        position = { 0, h * 0.82, 0 }, size = { rail_span, 0.06, 0.04 }, material = post_mat
    })
    scene:addBox("rail_bottom", {
        position = { 0, h * 0.22, 0 }, size = { rail_span, 0.06, 0.04 }, material = post_mat
    })

    -- Vertical pickets, evenly spaced between the posts, with a pointed
    -- top (a second, shorter box rotated 45 degrees along the rail axis
    -- would need rx/rz composed with per-picket placement, which
    -- Mc3SceneBuilder's rx/rz (G12) don't guarantee through nested
    -- transforms yet -- kept as plain flat-topped pickets, a deliberate
    -- simplification, not an oversight).
    local pitch   = picket_w + gap
    local usable  = rail_span - picket_w
    local count   = math.max(1, math.floor(usable / pitch) + 1)
    local total_w = (count - 1) * pitch
    local start_x = -total_w / 2
    for i = 0, count - 1 do
        local x = start_x + i * pitch
        scene:addBox("picket_" .. (i + 1), {
            position = { x, h / 2, 0 }, size = { picket_w, h, 0.03 }, material = pmat
        })
    end

    scene:setMetadata({
        generator  = { id=M.id, version=M.version, category=M.category, language="lua" },
        object     = { type="fence", style="wood_picket", parts={pickets=count} },
        generation = { variationInput=ctx.variation or 0 }
    })
end

return M
