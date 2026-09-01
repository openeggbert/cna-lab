-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: brick chimney with a cap and optional pots
-- (procedural-model-generator-roadmap, architecture tier)

local M = {}
M.id       = "lua.architecture.chimney.brick"
M.version  = "0.1.0"
M.category = "architecture"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width  or 0.55
    local d    = p.depth  or 0.45
    local h    = p.height or 1.60
    local mat  = p.material      or "brick_red"
    local cmat = p.cap_material  or "stone_light"

    -- Body
    scene:addBox("body", {position={0, h/2, 0}, size={w, h, d}, material=mat})

    -- Cap (slightly wider, sits on top)
    scene:addBox("cap", {position={0, h + 0.05, 0}, size={w + 0.10, 0.10, d + 0.10}, material=cmat})

    -- Optional chimney pots (2, central-European terracotta-pot look)
    local has_pots = p.pots
    if has_pots == nil then
        has_pots = (ctx.random ~= nil) and (ctx.random() < 0.6)
    end
    if has_pots then
        scene:addCylinder("pot_l", {
            position = {-w * 0.22, h + 0.10, 0}, radius = 0.09, height = 0.22, material = "brick_dark"
        })
        scene:addCylinder("pot_r", {
            position = { w * 0.22, h + 0.10, 0}, radius = 0.09, height = 0.22, material = "brick_dark"
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="chimney", style="brick", parts={hasPots=has_pots}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
