-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: deciduous tree

local M = {}
M.id       = "lua.object.tree.deciduous"
M.version  = "0.1.0"
M.category = "object"

-- MAP20, M330 -- canopy shape now mirrors ObjectDefinitionLibrary.cpp's own
-- C++ tree definitions species-for-species, via scene:addIcoSphere() (M330,
-- LuaRuntime.cpp) instead of the flattened boxes this script used to
-- approximate a canopy with (no icosphere primitive was exposed to Lua
-- object generators before M330). Two canopy styles, matching the C++
-- shape helpers exactly:
--   "twin"   -- deciduous_tree()'s two-tier icosphere canopy (oak/lime/
--              chestnut in C++; oak/linden/chestnut here -- "linden" and
--              "lime" are the same species, just different common names).
--   "narrow" -- birch_tree()'s single tall-narrow (Y-scaled) icosphere
--              canopy -- birch's silhouette is genuinely different in the
--              C++ registry, not just a smaller/bigger twin canopy, so this
--              script now differentiates it too rather than reusing the
--              oak-style shape for every species.
--
-- Species table: trunk_r, trunk_h, canopy_r, trunk_mat, canopy_mat, style
local SPECIES = {
    oak       = { trunk_r=0.18, trunk_h=4.0, canopy_r=3.2, trunk_mat="wood_bark_dark",  canopy_mat="foliage_oak",      style="twin"   },
    linden    = { trunk_r=0.15, trunk_h=3.5, canopy_r=2.8, trunk_mat="wood_bark_light", canopy_mat="foliage_linden",   style="twin"   },
    birch     = { trunk_r=0.10, trunk_h=5.0, canopy_r=2.0, trunk_mat="wood_birch",      canopy_mat="foliage_birch",    style="narrow" },
    chestnut  = { trunk_r=0.20, trunk_h=4.5, canopy_r=3.5, trunk_mat="wood_bark_dark",  canopy_mat="foliage_chestnut", style="twin"   },
}
local SPECIES_LIST = { "oak", "linden", "birch", "chestnut" }

function M.generate(ctx, scene)
    local p      = ctx.parameters or {}
    local scale  = p.scale   or 1.0
    local var    = ctx.variation or 0

    -- Pick species from variation if not specified
    local sp_name = p.species or SPECIES_LIST[(var % #SPECIES_LIST) + 1]
    local sp      = SPECIES[sp_name] or SPECIES.oak

    local tr = sp.trunk_r  * scale
    local th = sp.trunk_h  * scale
    local cr = sp.canopy_r * scale

    scene:addCylinder("trunk", {
        position = { 0, 0, 0 },
        radius   = tr,
        height   = th,
        material = sp.trunk_mat
    })

    if sp.style == "narrow" then
        -- birch_tree() (ObjectDefinitionLibrary.cpp): one icosphere, tall
        -- and narrow (a Y-scale of 1.55, X/Z of 0.60), centered at
        -- cy + cr*1.3 where cy = trunk_h*0.65. addIcoSphere()'s own
        -- `position` is a BASE elevation (it adds `radius` internally, same
        -- convention as addSphere()/addCylinder()/addBox() already use) --
        -- so the base passed here is that desired center minus cr.
        local cy = th * 0.65
        scene:addIcoSphere("canopy", {
            position = { 0, cy + cr * 0.3, 0 },
            radius   = cr,
            material = sp.canopy_mat,
            scale    = { 0.60, 1.55, 0.60 }
        })
    else
        -- deciduous_tree() (ObjectDefinitionLibrary.cpp): two icospheres --
        -- a main canopy centered at cy+cr, and a smaller (0.6x radius) top
        -- canopy centered at cy+cr*1.9 -- where cy = trunk_h*0.70. Same
        -- base-elevation-vs-center-y translation as the narrow style above.
        local cy = th * 0.70
        scene:addIcoSphere("canopy_main", {
            position = { 0, cy, 0 },
            radius   = cr,
            material = sp.canopy_mat
        })
        scene:addIcoSphere("canopy_top", {
            position = { 0, cy + cr * 1.3, 0 },
            radius   = cr * 0.6,
            material = sp.canopy_mat
        })
    end

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = {
            type    = "tree",
            species = sp_name,
            scale   = scale
        },
        generation = { variationInput = var }
    })
end

return M
