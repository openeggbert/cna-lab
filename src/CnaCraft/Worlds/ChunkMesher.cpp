#include "ChunkMesher.hpp"

#include <cmath>

#include "BlockType.hpp"
#include "Chunk.hpp"
#include "NoiseGenerator.hpp"
#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {

struct FaceDef {
    float nx, ny, nz;
    float corners[4][3];
    int dx, dy, dz; // neighbor offset to test for occlusion
};

// Corners are listed so that (0,1,2) + (0,2,3) wind counter-clockwise when
// viewed from outside the cube, looking along -normal. NOTE (not yet
// empirically verified against CNA's default RasterizerState — see plan.md
// "known constraints" / M1): CNA/XNA is left-handed with
// CullMode::CullCounterClockwiseFace as the default, which culls
// counter-clockwise-wound faces. If chunks render with faces missing/inside
// out at M1 bring-up, either flip this winding or set
// RasterizerState::CullNone on the chunk draw call temporarily.
constexpr FaceDef kFaces[6] = {
    // +X
    {1, 0, 0, {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}}, 1, 0, 0},
    // -X
    {-1, 0, 0, {{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}}, -1, 0, 0},
    // +Y (top)
    {0, 1, 0, {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}}, 0, 1, 0},
    // -Y (bottom)
    {0, -1, 0, {{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}}, 0, -1, 0},
    // +Z
    {0, 0, 1, {{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}}, 0, 0, 1},
    // -Z
    {0, 0, -1, {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, 0, 0, -1},
};

constexpr float kUv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

int TileIndexForFace(const BlockDef& def, const FaceDef& face) {
    if (face.ny > 0.5f) return def.topTile;
    if (face.ny < -0.5f) return def.bottomTile;
    return def.sideTile;
}

// Cross-quad "X" billboard for plant blocks (CRAFT_PARITY.md §3.7), ported
// from Craft's make_plant (src/cube.c): two full-block-diagonal planes,
// each emitted twice (once per winding) so both are visible from either
// side without backface culling hiding half the cross — 4 quads total,
// matching Craft's own 4-quad plant geometry exactly. Unlike cube faces,
// plants are never culled against neighbors (Craft draws all 4 quads
// unconditionally regardless of what's next to the block).
struct PlantQuad {
    float nx, ny, nz;
    float corners[4][3];
};
constexpr float kDiagInvSqrt2 = 0.70710678118654752440f; // normalized (±1, 0, ∓1)
constexpr PlantQuad kPlantQuads[4] = {
    // Diagonal plane A: (0,0,0)-(1,0,1) to (0,1,0)-(1,1,1), front winding.
    {kDiagInvSqrt2, 0, -kDiagInvSqrt2, {{0, 0, 0}, {1, 0, 1}, {1, 1, 1}, {0, 1, 0}}},
    // Same plane, reverse winding (visible from the other side).
    {-kDiagInvSqrt2, 0, kDiagInvSqrt2, {{1, 0, 1}, {0, 0, 0}, {0, 1, 0}, {1, 1, 1}}},
    // Diagonal plane B: (1,0,0)-(0,0,1) to (1,1,0)-(0,1,1), front winding.
    {-kDiagInvSqrt2, 0, -kDiagInvSqrt2, {{1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 0}}},
    // Same plane, reverse winding.
    {kDiagInvSqrt2, 0, kDiagInvSqrt2, {{0, 0, 1}, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}}},
};

// Per-instance random Y-axis rotation (CRAFT_PARITY.md §3.7, added
// 2026-07-10 per user decision): ports Craft's own `rotation =
// simplex2(ex, ez, 4, 0.5, 2) * 360` (main.c) exactly, applied via
// `mat_rotate(mb, 0, 1, 0, RADIANS(rotation))` before translating into
// place -- a deterministic pseudo-random rotation purely a function of the
// block's world (x,z), so a field of grass/flowers reads as organic
// instead of every blade sharing the exact same orientation. Uses a fixed
// seed (not the world's own terrain seed) since Craft's real simplex2 here
// draws from its single global, non-per-world-seeded permutation table --
// this rotation is cosmetic per-position variation, not part of the
// seed-varying terrain shape.
constexpr std::uint32_t kPlantRotationSeed = 0;

void EmitPlant(MeshData& mesh, const BlockDef& def, float lx, float ly, float lz, int wx, int wz) {
    const float rotationDegrees =
        NoiseGenerator::Simplex2(kPlantRotationSeed, static_cast<float>(wx), static_cast<float>(wz), 4, 0.5f, 2.0f) *
        360.0f;
    const float rotationRadians = rotationDegrees * (3.14159265358979323846f / 180.0f);
    const float cosR = std::cos(rotationRadians);
    const float sinR = std::sin(rotationRadians);

    for (const PlantQuad& quad : kPlantQuads) {
        const auto baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());
        for (int c = 0; c < 4; ++c) {
            // Rotate around the block's own vertical center axis (0.5, *, 0.5)
            // -- corners are defined in [0,1] local space, not centered at the
            // origin, so recenter before rotating and shift back after,
            // matching Craft's own rotate-then-translate order (cube.c).
            const float dx = quad.corners[c][0] - 0.5f;
            const float dz = quad.corners[c][2] - 0.5f;
            const float rx = dx * cosR - dz * sinR;
            const float rz = dx * sinR + dz * cosR;

            MeshVertex v;
            v.px = lx + rx + 0.5f;
            v.py = ly + quad.corners[c][1];
            v.pz = lz + rz + 0.5f;
            v.nx = quad.nx * cosR - quad.nz * sinR;
            v.ny = quad.ny;
            v.nz = quad.nx * sinR + quad.nz * cosR;
            v.u = kUv[c][0];
            v.v = kUv[c][1];
            v.tileIndex = def.topTile;
            mesh.vertices.push_back(v);
        }
        mesh.indices.push_back(baseIndex + 0);
        mesh.indices.push_back(baseIndex + 1);
        mesh.indices.push_back(baseIndex + 2);
        mesh.indices.push_back(baseIndex + 0);
        mesh.indices.push_back(baseIndex + 2);
        mesh.indices.push_back(baseIndex + 3);
    }
}

}

ChunkMeshData ChunkMesher::Build(const World& world, int originX, int originY, int originZ) {
    ChunkMeshData result;

    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                const int wx = originX + lx;
                const int wy = originY + ly;
                const int wz = originZ + lz;

                const BlockType block = world.GetBlock(wx, wy, wz);
                const BlockDef def = GetBlockDef(block);
                if (!def.solid) continue;

                MeshData& mesh = def.transparent ? result.transparent : result.opaque;

                if (def.plant) {
                    EmitPlant(mesh, def, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz), wx,
                              wz);
                    continue;
                }

                // Light-toggle glow pass (plan.md §12.1 item 17 follow-up):
                // a light-source cube block's exposed faces get emitted a
                // SECOND time into `glow`, additive to the normal opaque/
                // transparent emission below -- see MeshData.hpp's
                // ChunkMeshData doc comment for why this is a separate
                // unlit mesh rather than baked into the lit-textured one.
                // Plants are deliberately excluded (Craft's own
                // is_destructable guard on toggle-eligibility doesn't
                // discriminate, but a cross-billboard has no "exposed cube
                // faces" concept to emit a glow pass for).
                const bool isLightSource = world.IsLightSource(wx, wy, wz);

                for (const FaceDef& face : kFaces) {
                    if (world.IsOpaque(wx + face.dx, wy + face.dy, wz + face.dz)) continue;

                    const int tileIndex = TileIndexForFace(def, face);
                    const auto baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());

                    for (int c = 0; c < 4; ++c) {
                        MeshVertex v;
                        v.px = static_cast<float>(lx) + face.corners[c][0];
                        v.py = static_cast<float>(ly) + face.corners[c][1];
                        v.pz = static_cast<float>(lz) + face.corners[c][2];
                        v.nx = face.nx;
                        v.ny = face.ny;
                        v.nz = face.nz;
                        v.u = kUv[c][0];
                        v.v = kUv[c][1];
                        v.tileIndex = tileIndex;
                        mesh.vertices.push_back(v);
                    }

                    mesh.indices.push_back(baseIndex + 0);
                    mesh.indices.push_back(baseIndex + 1);
                    mesh.indices.push_back(baseIndex + 2);
                    mesh.indices.push_back(baseIndex + 0);
                    mesh.indices.push_back(baseIndex + 2);
                    mesh.indices.push_back(baseIndex + 3);

                    if (isLightSource) {
                        const auto glowBaseIndex = static_cast<std::uint32_t>(result.glow.vertices.size());
                        for (int c = 0; c < 4; ++c) {
                            GlowVertex gv;
                            gv.px = static_cast<float>(lx) + face.corners[c][0];
                            gv.py = static_cast<float>(ly) + face.corners[c][1];
                            gv.pz = static_cast<float>(lz) + face.corners[c][2];
                            gv.u = kUv[c][0];
                            gv.v = kUv[c][1];
                            gv.tileIndex = tileIndex;
                            result.glow.vertices.push_back(gv);
                        }
                        result.glow.indices.push_back(glowBaseIndex + 0);
                        result.glow.indices.push_back(glowBaseIndex + 1);
                        result.glow.indices.push_back(glowBaseIndex + 2);
                        result.glow.indices.push_back(glowBaseIndex + 0);
                        result.glow.indices.push_back(glowBaseIndex + 2);
                        result.glow.indices.push_back(glowBaseIndex + 3);
                    }
                }
            }
        }
    }

    return result;
}

}
