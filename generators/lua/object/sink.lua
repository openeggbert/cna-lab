-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: bathroom sink (basin + tap + pedestal)
--
-- generators/lua/room/kitchen.lua already has its OWN inline "sink_basin" +
-- "tap" boxes (T200, superseded in the old T-series triage) -- that stays
-- untouched. This is a separate, standalone, callable object generator for
-- bathroom.lua (T218) to compose via scene:callGenerator, closer to T200's
-- own original literal spec ("box basin + cylinder tap") than the inline
-- kitchen version ever was.

local M = {}
M.id       = "lua.object.sink.simple"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local h    = p.height or 0.85   -- basin top height
    local mat  = p.material      or "ceramic_white"
    local tmat = p.tap_material  or "metal_chrome"

    -- Pedestal
    scene:addCylinder("pedestal", {
        position = {0, 0, 0}, radius = 0.09, height = h - 0.10, material = mat
    })

    -- Basin
    scene:addBox("basin", {
        position = {0, h - 0.06, 0}, size = {0.55, 0.12, 0.42}, material = mat
    })

    -- Tap
    scene:addCylinder("tap", {
        position = {0, h + 0.02, -0.12}, radius = 0.02, height = 0.18, material = tmat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="sink", style="pedestal"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
