-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: forest zone chunk (64×64 m)

local M = {}
M.id       = "lua.zone.forest"
M.version  = "0.1.0"
M.category = "zone"

local SPECIES = {
    { tr=0.20, th=6.0, cr=3.5, tm="wood_bark_dark",  cm="foliage_oak"      },
    { tr=0.14, th=5.5, cr=2.5, tm="wood_bark_light", cm="foliage_linden"   },
    { tr=0.09, th=7.0, cr=1.8, tm="wood_birch",      cm="foliage_birch"    },
    { tr=0.22, th=5.0, cr=4.0, tm="wood_bark_dark",  cm="foliage_chestnut" },
    { tr=0.16, th=9.0, cr=2.0, tm="wood_pine",       cm="foliage_pine"     },
}

local function hash(seed, i, j)
    local v = seed * 1664525 + i * 22695477 + j * 6364136 + 1013904223
    return (v % (2^32)) / (2^32)
end

function M.generate(ctx, scene)
    local S   = ctx.chunk_size_m
    local var = ctx.variation or 0

    scene:addGround("forest_floor")

    -- Sparse undergrowth plane
    scene:addPlane("floor_detail", {
        position = {0, 0.02, 0},
        size     = {S, S},
        material = "leaf_litter"
    })

    -- Grid of trees with jitter and density variation
    local step   = 8      -- base grid step
    local margin = 4
    local idx    = 0
    local x = margin
    while x < S - margin do
        local z = margin
        while z < S - margin do
            local h1 = hash(var, math.floor(x), math.floor(z))
            -- Skip ~20 % of slots randomly for natural gaps
            if h1 > 0.20 then
                local jx   = (hash(var + 1, math.floor(x), math.floor(z)) - 0.5) * step * 0.6
                local jz   = (hash(var + 2, math.floor(x), math.floor(z)) - 0.5) * step * 0.6
                local px   = x + jx
                local pz   = z + jz
                local si   = math.floor(hash(var + 3, math.floor(x), math.floor(z)) * #SPECIES) + 1
                local sp   = SPECIES[si] or SPECIES[1]
                local scale = 0.7 + hash(var + 4, math.floor(x), math.floor(z)) * 0.7
                local tr = sp.tr * scale
                local th = sp.th * scale
                local cr = sp.cr * scale
                local cy = th * 0.72
                idx = idx + 1
                local pfx = "t" .. idx
                scene:addCylinder(pfx.."_trunk",  {position={px, 0,       pz}, radius=tr,    height=th,          material=sp.tm})
                scene:addBox     (pfx.."_canopy", {position={px, cy,      pz}, size={cr*2, cr*1.3, cr*2},        material=sp.cm})
                scene:addBox     (pfx.."_top",    {position={px, cy+cr*0.5, pz}, size={cr*1.2, cr*0.6, cr*1.2}, material=sp.cm})
            end
            z = z + step
        end
        x = x + step
    end

    -- A few boulders for variety
    local boulders = {
        {12, 20, 1.2}, {40, 15, 0.9}, {55, 45, 1.5}, {20, 52, 1.0}
    }
    for i, b in ipairs(boulders) do
        if hash(var + 10, i, 0) > 0.4 then
            scene:addBox("boulder_"..i, {
                position = {b[1], b[3]*0.4, b[2]},
                size     = {b[3]*2, b[3], b[3]*1.8},
                material = "rock"
            })
        end
    end

    scene:setMetadata({
        generator  = {id=M.id, version=M.version, category=M.category, language="lua"},
        chunk      = {x=ctx.chunk_x, y=ctx.chunk_y, size_m=S},
        -- R129 (zone-metadata bug fix) -- ctx.authored_zone, not ctx.zone:
        -- the latter may have been overridden by ChunkPipeline's own M157
        -- map-layer biome sampling, unrelated to this world's own flat config.
        generation = {variationInput=var, zone=ctx.authored_zone, region=ctx.region}
    })
end

return M
