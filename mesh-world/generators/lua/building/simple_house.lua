-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: simple detached house

local M = {}
M.id       = "lua.building.simple_house.standard"
M.version  = "0.1.0"
M.category = "building"

function M.generate(ctx, scene)
    local p     = ctx.parameters or {}
    local w     = p.width       or 10.0   -- X
    local d     = p.depth       or 8.0    -- Z
    local wh    = p.wall_height or 3.2    -- wall top
    local rt    = p.roof_type   or "gable"
    local wmat  = p.wall_material  or "plaster_white"
    local rmat  = p.roof_material  or "roof_tile_red"
    local gmat  = p.ground_material or "concrete"
    local wt    = 0.30  -- wall thickness

    -- Floor slab
    scene:addBox("floor", {position={0, -0.10, 0}, size={w + wt*2, 0.20, d + wt*2}, material=gmat})

    -- Walls: front (Z+), back (Z-), left (X-), right (X+)
    --
    -- FIXED (2026-07-13, found while implementing R113): addBox's own `y`
    -- parameter is a BASE elevation, NOT a center -- Mc3DocumentBuilder::
    -- box() always stores position.y = y + sy/2 internally (confirmed
    -- empirically, see tests/LuaCompositionTests.cpp's own
    -- CallGeneratorAppliesPositionOffset comment). Passing `wh/2` here
    -- (as if it were the wall's center) made the ACTUAL stored center
    -- wh/2 + wh/2 = wh -- every wall floated a full wh/2 above the floor,
    -- with its top poking wh/2 above the roofline. `floor` just above
    -- this comment was already written correctly as a real base value
    -- (-0.10); walls/door/windows/gable below were not -- same class of
    -- API-semantics mistake G12 already found+fixed for `ry` vs `rz`, a
    -- different bug in the same file. Now passes `0` (the real floor-
    -- level base), so a wall's real span is [0, wh] as intended.
    scene:addBox("wall_front", {position={0,      0, d/2 + wt/2}, size={w + wt*2, wh, wt}, material=wmat})
    scene:addBox("wall_back",  {position={0,      0, -(d/2+wt/2)}, size={w + wt*2, wh, wt}, material=wmat})
    scene:addBox("wall_left",  {position={-(w/2+wt/2), 0, 0},     size={wt, wh, d},         material=wmat})
    scene:addBox("wall_right", {position={ (w/2+wt/2), 0, 0},     size={wt, wh, d},         material=wmat})

    -- Door opening cut-out approximated by thin box (darker tone) --
    -- same base-not-center fix: real span [0, dh], door resting on the floor.
    local dw, dh = 1.00, 2.10
    scene:addBox("door_hole", {
        position = {0, 0, d/2 + wt/2 + 0.001},
        size     = {dw, dh, wt + 0.002},
        material = "wood_door_panel"
    })

    -- Two front windows -- same fix: `win_y` is the real sill height
    -- (base), not `win_y + win_h/2`; real span [win_y, win_y+win_h].
    local window_mat = "glass_clear"
    local win_w, win_h = 1.20, 1.00
    local win_y = 1.60
    scene:addBox("win_front_l", {
        position = {-w/4, win_y, d/2 + wt/2 + 0.001},
        size     = {win_w, win_h, wt + 0.002},
        material = window_mat
    })
    scene:addBox("win_front_r", {
        position = { w/4, win_y, d/2 + wt/2 + 0.001},
        size     = {win_w, win_h, wt + 0.002},
        material = window_mat
    })

    -- Gable roof (two sloped planes)
    --
    -- FIXED (2026-07-13, G12): previously used `ry` here (Mc3SceneBuilder's
    -- Y-axis / yaw rotation, which only spins a box flat in the horizontal
    -- plane) even though `angle`/`slope_w` below were already computing a
    -- real slope -- so roof_l/roof_r rendered as flat horizontal planks
    -- rotated in-plane, not sloped roof faces (found 2026-07-11, left
    -- unfixed until rx/rz rotation binding existed). Now uses `rz` (the
    -- ridge runs along Z, so pitching the panel from flat to sloped is a
    -- rotation around Z), which now exists (Mc3SceneBuilder/MC3Writer/
    -- Mc3DocumentBuilder all gained rx/rz support, see their own doc
    -- comments).
    if rt == "gable" then
        local rh = p.roof_height or 2.5
        -- Approximate with two angled boxes
        local slope_w = math.sqrt((w/2)^2 + rh^2) + 0.4
        local angle   = math.deg(math.atan(rh / (w/2)))
        scene:addBox("roof_l", {
            position = {-w/4, wh + rh/2, 0},
            size     = {slope_w, 0.25, d + wt*2 + 0.2},
            material = rmat,
            rz       = -angle
        })
        scene:addBox("roof_r", {
            position = { w/4, wh + rh/2, 0},
            size     = {slope_w, 0.25, d + wt*2 + 0.2},
            material = rmat,
            rz       = angle
        })
        -- Ridge cap and the two sloped roof_l/roof_r panels above are
        -- deliberately NOT touched by the base-vs-center fix: roof_l/
        -- roof_r are rotated (rz), so their `position` doubles as the
        -- rotation pivot -- what a "correct base" even means for a tilted
        -- thin panel is genuinely ambiguous, and the resulting error is
        -- small (half of the panel's own 0.25 thickness = 0.125m) versus
        -- the walls' full wh/2 (1.6m default) -- left as a known, minor,
        -- explicitly-scoped-out imprecision rather than risking a new,
        -- unverifiable-without-rendering mistake under rotation.
        scene:addBox("ridge", {
            position = {0, wh + rh, 0},
            size     = {0.30, 0.20, d + wt*2 + 0.4},
            material = rmat
        })
        -- Gable triangles -- same base-not-center fix as the walls above:
        -- real span [wh, wh+rh], filling the wall-top-to-ridge gap.
        scene:addBox("gable_front", {
            position = {0, wh, d/2 + wt/2},
            size     = {w + wt*2, rh, wt},
            material = wmat
        })
        scene:addBox("gable_back", {
            position = {0, wh, -(d/2 + wt/2)},
            size     = {w + wt*2, rh, wt},
            material = wmat
        })
    else
        -- Flat roof fallback -- same fix: real span [wh, wh+0.20],
        -- resting directly on the wall top instead of floating 0.1m above it.
        scene:addBox("roof_flat", {
            position = {0, wh, 0},
            size     = {w + wt*2 + 0.2, 0.20, d + wt*2 + 0.2},
            material = rmat
        })
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="simple_house", roof_type=rt,
                      dims={w=w, d=d, wall_height=wh}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
