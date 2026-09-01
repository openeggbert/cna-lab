-- SPDX-License-Identifier: MIT
-- MeshWorld Lua generator: street lamp post

local M = {}
M.id       = "lua.object.lamp.post"
M.version  = "0.1.0"
M.category = "object"

function M.generate(ctx, scene)
    local p      = ctx.parameters or {}
    local h      = p.height  or 4.5
    local ornate = p.ornate ~= false  -- default true
    local mat    = ornate and "metal_lamp_ornate" or "metal_lamp"

    -- Pole
    scene:addCylinder("pole", {
        position = { 0, 0, 0 },
        radius   = 0.055,
        height   = h,
        material = mat
    })

    -- Lamp head
    local hd = ornate and 0.35 or 0.25
    scene:addBox("head", {
        position = { 0, h, 0 },
        size     = { hd, 0.18, hd },
        material = mat
    })

    -- Base plate
    scene:addCylinder("base", {
        position = { 0, 0, 0 },
        radius   = 0.12,
        height   = 0.06,
        material = mat
    })

    scene:setMetadata({
        generator = { id = M.id, version = M.version, category = M.category, language = "lua" },
        object    = { type = "lamp_post", style = ornate and "ornate" or "simple" },
        generation = { variationInput = ctx.variation or 0 }
    })
end

return M
