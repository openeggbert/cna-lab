-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: detached house, composed from architecture/object
-- sub-generators via scene:callGenerator (procedural-model-generator-roadmap
-- -- the concrete end-to-end demonstration that composition works: this
-- generator's own geometry is just a floor slab + 4 walls; everything else
-- (door, windows, roof, chimney, steps, mailbox) is a separately-callable,
-- independently-testable sub-generator placed onto this shell).
--
-- Unlike generators/lua/building/simple_house.lua (kept as-is, unrelated ID,
-- still works standalone), this one does NOT inline door/window/roof
-- geometry -- it calls the real generators/lua/architecture/* /object/*
-- files instead, so a fix or upgrade to e.g. window/double_pane.lua
-- automatically improves every building that composes it.

local M = {}
M.id       = "lua.building.house.detached"
M.version  = "0.1.0"
M.category = "building"

function M.generate(ctx, scene)
    local p    = ctx.parameters or {}
    local w    = p.width       or 8.0    -- X footprint
    local d    = p.depth       or 7.0    -- Z footprint
    local wh   = p.wall_height or 2.8
    local fy   = p.floor_height or 0.35  -- raised entry above ground level
    local wt   = 0.30                     -- wall thickness
    local wmat = p.wall_material   or "plaster_white"
    local gmat = p.ground_material or "concrete"

    -- Shared sub-context every callGenerator below derives from: same
    -- variation family (offset by +1 so sub-generators don't reroll
    -- identically to this generator's own top-level rolls), same lod.
    local base_variation = (ctx.variation or 0) + 1

    -- Foundation / floor slab (ground level up to the raised entry height)
    --
    -- FIXED (2026-07-13, found while implementing R113): addBox's own `y`
    -- parameter is a BASE elevation, NOT a center -- Mc3DocumentBuilder::
    -- box() always stores position.y = y + sy/2 internally (confirmed
    -- empirically, see tests/LuaCompositionTests.cpp's own
    -- CallGeneratorAppliesPositionOffset comment). Passing `fy/2`/
    -- `fy + wh/2` here (as if they were centers) made the ACTUAL stored
    -- centers wh/2 too high in each case -- the foundation floated fy/2
    -- above true ground level and the walls floated wh/2 above the
    -- foundation's own top, same bug simple_house.lua had (also fixed
    -- this session). The door/window/roof/chimney/steps/mailbox
    -- sub-generators composed below via scene:callGenerator() are NOT
    -- affected -- their own `position` is a pure translation offset for
    -- the sub-generator's local origin, a different mechanism verified
    -- correct by CallGeneratorAppliesPositionOffset's own test.
    scene:addBox("foundation", {
        position = {0, 0, 0},
        size     = {w + wt*2, fy, d + wt*2},
        material = gmat
    })

    -- Walls -- real span [fy, fy+wh], resting on the foundation's own top.
    scene:addBox("wall_front", {position={0, fy, d/2 + wt/2},  size={w + wt*2, wh, wt}, material=wmat})
    scene:addBox("wall_back",  {position={0, fy, -(d/2+wt/2)}, size={w + wt*2, wh, wt}, material=wmat})
    scene:addBox("wall_left",  {position={-(w/2+wt/2), fy, 0}, size={wt, wh, d},         material=wmat})
    scene:addBox("wall_right", {position={ (w/2+wt/2), fy, 0}, size={wt, wh, d},         material=wmat})

    -- Front door
    scene:callGenerator("lua.architecture.door.front_panel", {
        variation  = base_variation,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = { width = 1.0, height = 2.1 }
    }, {
        position    = {0, fy, d/2 + wt/2 + 0.01},
        rotation_y  = 0,
        id          = "front_door"
    })

    -- Two front windows
    scene:callGenerator("lua.architecture.window.double_pane", {
        variation  = base_variation + 1,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = {}
    }, {
        position   = {-w/4, fy + 1.0, d/2 + wt/2 + 0.01},
        rotation_y = 0,
        id         = "win_front_l"
    })
    scene:callGenerator("lua.architecture.window.double_pane", {
        variation  = base_variation + 2,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = {}
    }, {
        position   = {w/4, fy + 1.0, d/2 + wt/2 + 0.01},
        rotation_y = 0,
        id         = "win_front_r"
    })

    -- One side window (left wall) -- also exercises rotation_y in the
    -- composition transform, not just translation.
    scene:callGenerator("lua.architecture.window.double_pane", {
        variation  = base_variation + 3,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = { width = 0.90, height = 1.10 }
    }, {
        position   = {-(w/2 + wt/2 + 0.01), fy + 1.0, 0},
        rotation_y = -90,
        id         = "win_side_l"
    })

    -- Roof
    local roof_h = p.roof_height or 2.0
    scene:callGenerator("lua.architecture.roof.gable", {
        variation  = base_variation + 4,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = { width = w + wt*2 + 0.4, length = d + wt*2 + 0.4, height = roof_h }
    }, {
        position   = {0, fy + wh, 0},
        id         = "roof"
    })

    -- Chimney, offset from centre, sitting on the roof slope
    scene:callGenerator("lua.architecture.chimney.brick", {
        variation  = base_variation + 5,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = {}
    }, {
        position   = {w * 0.22, fy + wh + roof_h * 0.55, d * 0.15},
        id         = "chimney"
    })

    -- Front steps, bridging ground level up to the raised entry
    scene:callGenerator("lua.architecture.stairs.front_steps", {
        variation  = base_variation + 6,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = { step_count = 2, step_height = fy / 2, width = 1.4 }
    }, {
        position   = {0, 0, d/2 + wt/2},
        id         = "steps"
    })

    -- Mailbox near the front steps -- proves composition reaches into the
    -- pre-existing object library too, not just this session's own new
    -- architecture files.
    scene:callGenerator("lua.object.mailbox.simple", {
        variation  = base_variation + 7,
        style      = ctx.style,
        lod        = ctx.lod,
        parameters = {}
    }, {
        position   = {w/2 + 1.2, 0, d/2 + 0.6},
        id         = "mailbox"
    })

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        object     = {type="house", style="detached_composed",
                      dims={w=w, d=d, wall_height=wh}},
        generation = {variationInput=ctx.variation or 0}
    })
end

return M
