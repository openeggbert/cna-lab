#include "ChunkMesher.hpp"

#include "BlockType.hpp"
#include "Chunk.hpp"
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

void EmitPlant(MeshData& mesh, const BlockDef& def, float lx, float ly, float lz) {
    for (const PlantQuad& quad : kPlantQuads) {
        const auto baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());
        for (int c = 0; c < 4; ++c) {
            MeshVertex v;
            v.px = lx + quad.corners[c][0];
            v.py = ly + quad.corners[c][1];
            v.pz = lz + quad.corners[c][2];
            v.nx = quad.nx;
            v.ny = quad.ny;
            v.nz = quad.nz;
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
                    EmitPlant(mesh, def, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                    continue;
                }

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
                }
            }
        }
    }

    return result;
}

}
