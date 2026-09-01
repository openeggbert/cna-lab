// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/SmallHouseBlockGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "StyleRegistry.hpp"
#include <random>

namespace MeshWorld {

std::string SmallHouseBlockGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.small_house_block").to_json()
    );
    const float s = ctx.chunk_size_m;

    const Style* style_ptr = StyleRegistry::instance().has(ctx.style)
                             ? &StyleRegistry::instance().get(ctx.style)
                             : nullptr;
    auto mat = [&](const std::string& key, const char* fallback) -> std::string {
        if (style_ptr) return style_ptr->mat(key, fallback);
        return fallback;
    };

    w.ground(mat("block.ground", "grass_garden"));

    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> wall_h(4.0f, 6.5f);
    std::uniform_real_distribution<float> hwidth(8.0f, 14.0f);
    std::uniform_real_distribution<float> hdepth(7.0f, 11.0f);
    std::uniform_int_distribution<int>    mat_pick(0, 2);
    std::uniform_int_distribution<int>    roof_pick(0, 1);
    std::uniform_int_distribution<int>    shape_pick(0, 2);  // 0=rect, 1=L-wide, 2=L-deep
    std::uniform_real_distribution<float> toff(-2.5f, 2.5f);
    std::uniform_real_distribution<float> rot360(0.0f, 360.0f);

    const std::string facades[3]   = {mat("block.facade.0", "brick_red"),
                                       mat("block.facade.1", "plaster_cream"),
                                       mat("block.facade.2", "plaster_yellow")};
    const std::string roof_mats[2] = {mat("block.roof.0", "roof_tile_red"),
                                       mat("block.roof.1", "roof_tile_grey")};
    static const char* trees[3]     = {"tree_apple", "tree_cherry", "tree_pear"};
    std::uniform_int_distribution<int> tree_pick(0, 2);

    // Four house plots: NW, NE, SW, SE
    // Each plot is centred at these base positions (in chunk space)
    struct Plot { float cx, cz, street_z; };  // street_z = z-coordinate of street edge
    const Plot plots[4] = {
        {13.0f, 13.0f, 2.0f},   // NW — street to north
        {51.0f, 13.0f, 2.0f},   // NE — street to north
        {13.0f, 51.0f, s - 2.0f},  // SW — street to south
        {51.0f, 51.0f, s - 2.0f},  // SE — street to south
    };

    for (int i = 0; i < 4; ++i) {
        const float cx    = plots[i].cx;
        const float cz    = plots[i].cz;
        const float fh    = wall_h(rng);
        const float hw    = hwidth(rng);
        const float hd    = hdepth(rng);
        const int   shape = shape_pick(rng);
        const std::string& facade = facades[mat_pick(rng)];
        const std::string& roof   = roof_mats[roof_pick(rng)];
        std::string idx    = std::to_string(i);

        if (shape == 0) {
            // Simple rectangle
            w.box("house_" + idx, cx, cz, hw, fh, hd, facade);
            w.box("roof_"  + idx, cx, cz, hw + 0.5f, 1.3f, hd + 0.5f, roof, fh);

        } else if (shape == 1) {
            // L-shape: main body + narrower wing to one side
            float wing_w = hw * 0.55f;
            float wing_d = hd * 0.45f;
            float body_d = hd * 0.65f;
            w.box("house_" + idx + "a", cx - hw * 0.25f + wing_w * 0.5f, cz - hd * 0.5f + body_d * 0.5f,
                  hw, fh, body_d, facade);
            w.box("house_" + idx + "b", cx - hw * 0.5f + wing_w * 0.5f, cz + hd * 0.5f - wing_d * 0.5f,
                  wing_w, fh * 0.9f, wing_d, facade);
            w.box("roof_"  + idx, cx, cz, hw + 0.4f, 1.1f, hd + 0.4f, roof, fh * 0.95f);

        } else {
            // L-shape: main body + extension in depth
            float ext_w = hw * 0.45f;
            float ext_d = hd * 0.5f;
            w.box("house_" + idx + "a", cx, cz - hd * 0.25f, hw, fh, hd * 0.6f, facade);
            w.box("house_" + idx + "b", cx + hw * 0.5f - ext_w * 0.5f, cz + hd * 0.25f,
                  ext_w, fh * 0.85f, ext_d, facade);
            w.box("roof_"  + idx, cx, cz, hw + 0.4f, 1.1f, hd + 0.4f, roof, fh * 0.9f);
        }

        // Garden fence along street side
        const float fence_z = (plots[i].street_z < s * 0.5f)
                              ? cz - hd * 0.5f - 2.5f
                              : cz + hd * 0.5f + 2.5f;
        w.box("fence_" + idx, cx, fence_z, hw * 1.1f, 1.0f, 0.15f, mat("block.fence", "wood_fence"));

        // Gate posts (gap in fence at centre)
        const float gp = 1.2f;  // half gate gap
        const auto gate_post = mat("block.gate_post", "stone_post");
        w.cylinder("gate_l_" + idx, cx - gp - 0.2f, fence_z, 0.1f, 1.3f, gate_post);
        w.cylinder("gate_r_" + idx, cx + gp + 0.2f, fence_z, 0.1f, 1.3f, gate_post);

        // Garden path from gate to front door
        const float path_start_z = fence_z;
        const float path_end_z   = (fence_z < cz) ? cz - hd * 0.5f : cz + hd * 0.5f;
        const float path_len     = std::fabs(path_end_z - path_start_z);
        const float path_mid_z   = (path_start_z + path_end_z) * 0.5f;
        w.plane("garden_path_" + idx, cx - 0.8f, std::min(path_start_z, path_end_z),
                1.6f, path_len, mat("block.path", "path_stone"));

        // Garden tree (off-centre, seed-varied)
        float tx = cx + toff(rng);
        float tz = path_mid_z + toff(rng);
        // keep away from path
        if (std::fabs(tx - cx) < 1.5f) tx += (tx < cx ? -2.0f : 2.0f);
        w.instance("tree_" + idx, trees[tree_pick(rng)], tx, tz, rot360(rng));

        // Low shrubs near fence corners
        w.instance("shrub_l_" + idx, "shrub_round", cx - hw * 0.5f + 1.0f, fence_z, rot360(rng), 0.0f, 0.6f);
        w.instance("shrub_r_" + idx, "shrub_round", cx + hw * 0.5f - 1.0f, fence_z, rot360(rng), 0.0f, 0.6f);
    }

    // Shared interior paths connecting the four plots
    const auto block_path = mat("block.path", "path_stone");
    w.plane("path_h", 20.0f,           s * 0.5f - 1.5f, 24.0f, 3.0f, block_path);
    w.plane("path_v", s * 0.5f - 1.5f, 20.0f,           3.0f, 24.0f, block_path);

    // MAP17 -- a snow-guard marker on each plot's roofline at high
    // elevation (a mountain house needs snow retention hardware a lowland
    // one doesn't). Purely additive after every pre-MAP17 draw above,
    // reusing `plots[]`'s own base positions rather than the per-plot
    // shape/height locals scoped inside the loop above.
    if (ctx.map_context.available && ctx.map_context.elevation_m > 800.0f) {
        for (int i = 0; i < 4; ++i)
            w.box("snowguard_" + std::to_string(i), plots[i].cx, plots[i].cz,
                  2.0f, 0.15f, 0.3f, mat("block.fence", "wood_fence"), 6.0f);
    }

    return w.build();
}

} // namespace MeshWorld
