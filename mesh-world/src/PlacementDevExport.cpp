// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "PlacementDevExport.hpp"

#include "MC3Writer.hpp"

namespace MeshWorld {

std::string export_chunk_placements_to_mc3(const ChunkContext& ctx,
                                            const std::vector<ModelPlacement>& placements) {
    MC3Writer w(ctx);

    const int    chunk_size_i = static_cast<int>(ctx.chunk_size_m);
    const double origin_x = ctx.coord.world_x(chunk_size_i);
    const double origin_z = ctx.coord.world_z(chunk_size_i);

    for (std::size_t i = 0; i < placements.size(); ++i) {
        const ModelPlacement& p = placements[i];
        const float local_x = static_cast<float>(p.pos_x - origin_x);
        const float local_z = static_cast<float>(p.pos_z - origin_z);
        w.instance("placement_" + std::to_string(i), p.definition_id, local_x, local_z, p.rot_y,
                   static_cast<float>(p.pos_y), p.scale);
    }

    return w.build();
}

} // namespace MeshWorld
