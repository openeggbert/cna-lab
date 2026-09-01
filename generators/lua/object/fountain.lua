-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: park fountain

local M = {}
M.id       = "lua.object.fountain.classic"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local r    = p.radius or 2.0   -- basin outer radius
    local mat  = p.material or "stone_wall"
    local wmat = p.water_material or "water_fountain"

    -- Lower basin
    scene:addCylinder("basin_outer", {position={0, 0,    0}, radius=r,       height=0.50, material=mat  })
    scene:addCylinder("basin_inner", {position={0, 0.05, 0}, radius=r-0.15,  height=0.42, material=wmat })

    -- Centre column
    scene:addCylinder("column", {position={0, 0.5, 0}, radius=0.20, height=1.20, material=mat})

    -- Upper bowl
    scene:addCylinder("bowl_outer", {position={0, 1.7, 0}, radius=r*0.55,      height=0.35, material=mat })
    scene:addCylinder("bowl_water", {position={0, 1.75, 0}, radius=r*0.55-0.12, height=0.27, material=wmat})

    -- Top nozzle
    scene:addCylinder("nozzle", {position={0, 2.05, 0}, radius=0.06, height=0.30, material=mat})

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="fountain", style="classic"},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
