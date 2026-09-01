-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: gable roof
--
-- STYLE CHOICE (not a hard limitation anymore): this approximates the gable
-- silhouette with stepped, progressively narrower boxes -- a common
-- low-poly technique, and matches the "central European small town,
-- low-poly" style default. G12 (2026-07-13) added real rx/rz rotation
-- binding to Mc3SceneBuilder/Mc3DocumentBuilder/MC3Writer (see their own
-- doc comments), so a literally-sloped single-plane gable face IS now
-- buildable (building/simple_house.lua's own gable roof uses exactly that,
-- rz-rotated boxes) -- rewriting this generator to match is a separate,
-- future content task, not attempted here (this file's own stepped
-- silhouette is a deliberate style, not something broken).

local M = {}
M.id       = "lua.architecture.roof.gable"
M.version  = "0.1.0"
M.category = "architecture"

function M.generate(ctx, scene)
    local p      = ctx.parameters or {}
    local w      = p.width  or 6.0   -- footprint width (matches house body width)
    local len    = p.length or 8.0   -- footprint length
    local height = p.height or 2.2   -- ridge height above the eave line
    local steps  = p.steps  or 4     -- stepped approximation resolution
    local mat    = p.material or "roof_tile_red"
    local ridge_mat = p.ridge_material or mat

    local step_h = height / steps
    local half_w = w / 2

    for i = 0, steps - 1 do
        -- Each step is narrower than the last as it rises toward the ridge,
        -- on both sides (a symmetric stepped pyramid-ridge silhouette).
        local frac_lo = i / steps
        local frac_hi = (i + 1) / steps
        local width_lo = half_w * (1.0 - frac_lo)
        local y = step_h * i

        scene:addBox("step_l_" .. (i + 1), {
            position = {-(width_lo * 0.5), y + step_h/2, 0},
            size     = {width_lo, step_h, len},
            material = mat
        })
        scene:addBox("step_r_" .. (i + 1), {
            position = { (width_lo * 0.5), y + step_h/2, 0},
            size     = {width_lo, step_h, len},
            material = mat
        })
    end

    -- Ridge cap
    scene:addBox("ridge", {
        position = {0, height + 0.05, 0},
        size     = {0.20, 0.10, len + 0.10},
        material = ridge_mat
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="roof", style="gable_stepped", parts={steps=steps}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
