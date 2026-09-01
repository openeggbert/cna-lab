-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: parked car (T203)
--
-- "cylinder wheels" per the original T203 spec: the scene API's addCylinder
-- only builds a vertical-axis cylinder (no rx/rz rotation binding exists to
-- lay one on its side as a wheel). Wheels are instead built as flattened
-- addIcoSphere ellipsoids (thin along X, full radius on Y/Z) -- a disc lying
-- in the Y-Z plane with its axle along X, visually equivalent to a wheel.

local M = {}
M.id       = "lua.object.car.sedan"
M.version  = "0.1.0"
M.category = "object"

local PAINTS = { "car_paint_red", "car_paint_blue", "car_paint_white", "car_paint_black" }

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local len  = p.length or 4.3
    local w    = p.width  or 1.75
    local r    = p.wheel_radius or 0.32
    local mat  = p.material or PAINTS[(math.abs(ctx.variation or 0) % #PAINTS) + 1]
    local tmat = p.tire_material or "tire_rubber"

    -- Body
    local body_h = 0.55
    local body_y = r
    scene:addBox("body", {
        position = {0, body_y + body_h/2, 0},
        size     = {w, body_h, len * 0.90},
        material = mat
    })

    -- Roof (cabin, narrower and shorter than the body, offset toward centre)
    local roof_h = 0.42
    scene:addBox("roof", {
        position = {0, body_y + body_h + roof_h/2, -len * 0.03},
        size     = {w * 0.82, roof_h, len * 0.48},
        material = mat
    })

    -- 4 wheels (flattened ico-spheres, see header comment)
    local wx = w/2
    local wz = len/2 - r * 1.2
    local wheels = {
        {"wheel_fl",  wx,  wz},
        {"wheel_fr", -wx,  wz},
        {"wheel_rl",  wx, -wz},
        {"wheel_rr", -wx, -wz},
    }
    for _, wheel in ipairs(wheels) do
        scene:addIcoSphere(wheel[1], {
            position = {wheel[2], r, wheel[3]},
            radius   = r,
            material = tmat,
            scale    = {0.35, 1.0, 1.0}
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="car", style="sedan_parked", parts={wheels=4}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
