// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "LuaSandbox.hpp"
#include "LuaRuntime.hpp"
#include "Mc3SceneBuilder.hpp"
#include "MapBuilder.hpp"
#include "ChunkGenerator.hpp"
#include "GenerationMetadata.hpp"

namespace MeshWorld {

std::string LuaSandbox::execute(const std::string& source,
                                 const ChunkContext& ctx,
                                 std::string*        error_out) {
    // Build the scene for this chunk.
    Mc3SceneBuilder scene(
        "chunk_" + ctx.coord.to_string(),
        ctx.chunk_size_m,
        ctx.coord.x,
        ctx.coord.y
    );

    // Run the generator in a fresh sandbox state.
    LuaRuntime runtime(scene, ctx);
    std::string err = runtime.run(source);

    if (!err.empty()) {
        if (error_out) *error_out = err;
        return "";
    }

    return scene.buildToString();
}

Map::MapTilePayload LuaSandbox::executeMap(const std::string&  source,
                                            const MapGenContext& mapctx,
                                            std::string*         error_out) {
    MapBuilder builder(mapctx.tile, mapctx.entropy, mapctx.sea_level_m, mapctx.parent);

    // Run the generator in a fresh sandbox state.
    LuaRuntime runtime(builder, mapctx.parent);
    const std::string err = runtime.run(source);

    if (!err.empty()) {
        if (error_out) *error_out = err;
        return Map::MapTilePayload{};
    }

    // M162 — derive TileEdge::crossings from every Street/Road feature the
    // script just added, now that generate() has fully finished (not
    // script-visible; see MapBuilder::deriveEdgeCrossings()'s own doc
    // comment for why this must happen here rather than inside the script).
    builder.deriveEdgeCrossings();

    return builder.payload();
}

} // namespace MeshWorld
