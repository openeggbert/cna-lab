-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: mailbox (T205)

local M = {}
M.id       = "lua.object.mailbox.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local h    = p.post_height or 0.95
    local mat  = p.material      or "mailbox_body"
    local pmat = p.post_material or (ctx.style and ctx.style.wood_material) or "wood_natural"

    -- Post
    scene:addCylinder("post", {
        position = {0, 0, 0},
        radius   = 0.045,
        height   = h,
        material = pmat
    })

    -- Box body (on top of the post)
    scene:addBox("body", {
        position = {0, h + 0.11, 0.02},
        size     = {0.24, 0.22, 0.36},
        material = mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="mailbox", style="simple"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
