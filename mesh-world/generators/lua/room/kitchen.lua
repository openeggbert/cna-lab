-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: kitchen interior

local M = {}
M.id       = "lua.room.kitchen.standard"
M.version  = "0.1.0"
M.category = "room"

function M.generate(ctx, scene)
    local p     = ctx.parameters or {}
    local w     = p.width  or 4.0   -- X
    local d     = p.depth  or 3.5   -- Z
    local h     = p.height or 2.6   -- ceiling
    local fmat  = p.floor_material  or "tile_kitchen"
    local wmat  = p.wall_material   or "plaster_white"
    local cmat  = p.counter_material or "wood_counter"

    -- Floor
    scene:addPlane("floor", {position={0, 0, 0}, size={w, d}, material=fmat})

    -- Counter along back wall (Z-)
    local counter_h = 0.90
    local counter_d = 0.65
    scene:addBox("counter", {
        position = {0, counter_h/2, -(d/2 - counter_d/2)},
        size     = {w - 0.10, counter_h, counter_d},
        material = cmat
    })
    scene:addBox("counter_top", {
        position = {0, counter_h + 0.025, -(d/2 - counter_d/2)},
        size     = {w - 0.08, 0.05, counter_d + 0.02},
        material = "stone_countertop"
    })

    -- Wall cabinets above counter
    scene:addBox("cabinets", {
        position = {0, counter_h + 0.80, -(d/2 - 0.20)},
        size     = {w - 0.20, 0.70, 0.38},
        material = cmat
    })

    -- Fridge (right side against back wall)
    scene:addBox("fridge_body", {
        position = {w/2 - 0.38, 0.90, -(d/2 - 0.35)},
        size     = {0.70, 1.80, 0.68},
        material = "appliance_white"
    })
    scene:addBox("fridge_handle", {
        position = {w/2 - 0.06, 1.35, -(d/2 - 0.70)},
        size     = {0.03, 0.40, 0.03},
        material = "metal_chrome"
    })

    -- Sink (left half of counter)
    scene:addBox("sink_basin", {
        position = {-w/4, counter_h + 0.04, -(d/2 - counter_d/2)},
        size     = {0.60, 0.12, 0.50},
        material = "metal_chrome"
    })
    scene:addCylinder("tap", {
        position = {-w/4, counter_h + 0.05, -(d/2 - counter_d/2 + 0.18)},
        radius   = 0.025,
        height   = 0.30,
        material = "metal_chrome"
    })

    -- Kitchen table (centre of room)
    local tw, td, th2 = 1.20, 0.80, 0.75
    scene:addBox("table_top", {position={0, th2, 0}, size={tw, 0.05, td}, material="wood_natural"})
    local legs = {{tw/2-0.05, -td/2+0.05},{tw/2-0.05,td/2-0.05},{-tw/2+0.05,-td/2+0.05},{-tw/2+0.05,td/2-0.05}}
    for i, lg in ipairs(legs) do
        scene:addBox("tleg_"..i, {position={lg[1], th2/2, lg[2]}, size={0.06, th2, 0.06}, material="wood_natural"})
    end

    -- Two chairs
    local ch_sh = 0.44
    local function chair(pfx, cx, cz)
        scene:addBox(pfx.."_seat", {position={cx, ch_sh+0.02, cz}, size={0.45,0.04,0.45}, material="wood_natural"})
        scene:addBox(pfx.."_back", {position={cx, ch_sh+0.25, cz-0.21}, size={0.41,0.45,0.04}, material="wood_natural"})
        local offs = {{0.18,0.18},{-0.18,0.18},{0.18,-0.18},{-0.18,-0.18}}
        for i2, o in ipairs(offs) do
            scene:addCylinder(pfx.."_leg"..i2, {position={cx+o[1], 0, cz+o[2]}, radius=0.02, height=ch_sh, material="wood_natural"})
        end
    end
    chair("chair_n", 0, -td/2 - 0.50)
    chair("chair_s", 0,  td/2 + 0.50)

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        room       = {type="kitchen", dims={w=w, d=d, h=h}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
