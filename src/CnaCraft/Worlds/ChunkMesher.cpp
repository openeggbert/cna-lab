#include "ChunkMesher.hpp"

#include <algorithm>
#include <array>
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

// Craft-style padded occupancy snapshot (compute_chunk's opaque[] scratch
// array, main.c:958-1006): one cell per block for the chunk plus a 1-cell
// border in X/Z and a -1..+(CHUNK_SIZE+8) range in Y -- the extra 8 above
// the chunk top covers the column-shade scan (a neighborhood cell at
// y=CHUNK_SIZE scans up to 7 further). Filled once per Build so every
// AO/culling neighbor read is a plain array lookup instead of a
// World::GetBlock hash walk -- the same reason Craft fills its own padded
// array instead of calling map_get per neighbor.
struct PaddedOccupancy {
    static constexpr int kXZ = CHUNK_SIZE + 2;  // px, pz in [-1, CHUNK_SIZE]
    static constexpr int kY = CHUNK_SIZE + 10;  // py in [-1, CHUNK_SIZE + 8]
    std::array<std::uint8_t, static_cast<std::size_t>(kXZ) * kY * kXZ> cells{};

    static constexpr std::size_t IndexOf(int px, int py, int pz) {
        return (static_cast<std::size_t>(px + 1) * kY + static_cast<std::size_t>(py + 1)) * kXZ +
               static_cast<std::size_t>(pz + 1);
    }
    std::uint8_t At(int px, int py, int pz) const { return cells[IndexOf(px, py, pz)]; }
    void Set(int px, int py, int pz, bool opaque) { cells[IndexOf(px, py, pz)] = opaque ? 1 : 0; }
};

// Craft's fixed diffuse light direction, block_vertex.glsl:
// `diffuse = max(0.0, dot(normal, normalize(vec3(-1.0, 1.0, -1.0))))`.
// Time-independent, which is exactly what makes baking it per-vertex sound.
float DiffuseFor(float nx, float ny, float nz) {
    constexpr float kInvSqrt3 = 0.57735026918962576451f;
    return std::max(0.0f, (-nx + ny - nz) * kInvSqrt3);
}

// The complete static per-vertex factor of Craft's block lighting
// (block_vertex.glsl + block_fragment.glsl with fragment_light == 0):
//   brightness = 0.3 + (1 - ao) * 0.7          (vertex shader's fragment_ao)
//   final      = texel * (daylight*0.3 + 0.2) * (1 + df) * brightness
// This returns the `(1 + df) * brightness` part, in [0.3, 2.0]. Cloud faces
// get Craft's contrast compression (block_fragment.glsl: df' = 1 - df*0.2,
// ao' = 1 - (1-ao_brightness)*0.2, so 1+df' = 2 - df*0.2) -- keyed on the
// block type at mesh time instead of Craft's pure-white-texel test in the
// fragment shader, which is the same predicate expressed sanely.
float BakedShade(float df, float ao, bool cloud) {
    const float aoBrightness = 0.3f + (1.0f - ao) * 0.7f;
    if (cloud) return (2.0f - df * 0.2f) * (1.0f - (1.0f - aoBrightness) * 0.2f);
    return (1.0f + df) * aoBrightness;
}

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

// `minAo` is Craft's scalar plant occlusion (main.c:1102-1116): the MINIMUM
// (least-occluded, i.e. brightest) of the block's 24 per-face-corner ao
// values, applied uniformly to every plant vertex -- a plant on open ground
// gets minAo = 0 (its unoccluded top-face corners win the min), so plants
// must never be darkened by the ground they stand on.
void EmitPlant(MeshData& mesh, const BlockDef& def, float lx, float ly, float lz, int wx, int wz, float minAo) {
    const float rotationDegrees =
        NoiseGenerator::Simplex2(kPlantRotationSeed, static_cast<float>(wx), static_cast<float>(wz), 4, 0.5f, 2.0f) *
        360.0f;
    const float rotationRadians = rotationDegrees * (3.14159265358979323846f / 180.0f);
    const float cosR = std::cos(rotationRadians);
    const float sinR = std::sin(rotationRadians);

    for (const PlantQuad& quad : kPlantQuads) {
        const auto baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());
        // Craft computes plant diffuse from the ROTATED normal (make_plant
        // rotates the whole vertex block, normals included, before the
        // vertex shader's dot(normal, light_direction)) -- same here.
        const float rotatedNx = quad.nx * cosR - quad.nz * sinR;
        const float rotatedNz = quad.nx * sinR + quad.nz * cosR;
        const float shade = BakedShade(DiffuseFor(rotatedNx, quad.ny, rotatedNz), minAo, /*cloud=*/false);
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
            v.nx = rotatedNx;
            v.ny = quad.ny;
            v.nz = rotatedNz;
            v.u = kUv[c][0];
            v.v = kUv[c][1];
            v.tileIndex = def.topTile;
            v.shade = shade;
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

void ChunkMesher::ComputeOcclusion(const std::uint8_t neighbors[27], const float shades[27], float ao[6][4]) {
    // Craft's occlusion() (main.c:879-921) with the light[] half dropped.
    // Craft hardcodes lookup3 (the {corner, side1, side2} AO cells) and
    // lookup4 (those three plus the face-center cell, whose shades are
    // averaged) as literal index tables in ITS face/corner order; here the
    // same cells are derived geometrically from kFaces so they can't drift
    // from this mesher's own conventions: all four cells live in the layer
    // one step out along the face normal, side1/side2 one step along each
    // in-plane tangent toward the corner, the corner cell along both.
    static constexpr float kCurve[4] = {0.0f, 0.25f, 0.5f, 0.75f};
    for (int f = 0; f < 6; ++f) {
        const FaceDef& face = kFaces[f];
        const int n[3] = {face.dx, face.dy, face.dz};
        int axisU = -1, axisV = -1;  // the two non-normal axes
        for (int a = 0; a < 3; ++a) {
            if (n[a] != 0) continue;
            (axisU < 0 ? axisU : axisV) = a;
        }
        for (int c = 0; c < 4; ++c) {
            const int stepU = face.corners[c][axisU] > 0.5f ? 1 : -1;
            const int stepV = face.corners[c][axisV] > 0.5f ? 1 : -1;
            int cell[3] = {n[0], n[1], n[2]};
            const int centerIdx = NeighborIndex(cell[0], cell[1], cell[2]);
            cell[axisU] += stepU;
            const int side1Idx = NeighborIndex(cell[0], cell[1], cell[2]);
            cell[axisV] += stepV;
            const int cornerIdx = NeighborIndex(cell[0], cell[1], cell[2]);
            cell[axisU] -= stepU;
            const int side2Idx = NeighborIndex(cell[0], cell[1], cell[2]);

            const int corner = neighbors[cornerIdx] ? 1 : 0;
            const int side1 = neighbors[side1Idx] ? 1 : 0;
            const int side2 = neighbors[side2Idx] ? 1 : 0;
            // The "0432" rule: a vertex flanked by both in-plane sides is
            // fully occluded regardless of the corner cell.
            const int value = (side1 && side2) ? 3 : corner + side1 + side2;
            const float shadeSum = shades[cornerIdx] + shades[side1Idx] + shades[side2Idx] + shades[centerIdx];
            ao[f][c] = std::min(kCurve[value] + shadeSum / 4.0f, 1.0f);
        }
    }
}

ChunkMeshData ChunkMesher::Build(const World& world, int originX, int originY, int originZ) {
    ChunkMeshData result;

    // Fill the padded snapshot in chunk-aligned slabs: one TryChunkAt per
    // overlapped chunk, then plain in-chunk array reads. (A per-cell
    // World::IsOpaque fill -- one hash walk per ~8.4K cells -- measurably
    // ~2x'd the worlds test suite; Craft fills its own padded array from
    // raw chunk maps for the same reason.) Origin is chunk-aligned per this
    // function's contract, so each chunk offset covers a fixed cell range.
    // Unloaded / out-of-Y-range chunks are skipped: their cells stay 0
    // (non-opaque), the same graceful degradation as World::GetBlock.
    PaddedOccupancy padded;
    {
        const auto floorDiv = [](int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); };
        const int ccx = floorDiv(originX, CHUNK_SIZE), ccy = floorDiv(originY, CHUNK_SIZE),
                  ccz = floorDiv(originZ, CHUNK_SIZE);
        struct CellRange {
            int lo, hi;
        };
        const auto rangeFor = [](int chunkOffset, int hiPad) -> CellRange {
            if (chunkOffset < 0) return {-1, -1};
            if (chunkOffset == 0) return {0, CHUNK_SIZE - 1};
            return {CHUNK_SIZE, hiPad};
        };
        for (int sx = -1; sx <= 1; ++sx) {
            for (int sy = -1; sy <= 1; ++sy) {
                for (int sz = -1; sz <= 1; ++sz) {
                    const Chunk* chunk = world.TryChunkAt(ccx + sx, ccy + sy, ccz + sz);
                    if (!chunk) continue;
                    const CellRange rx = rangeFor(sx, CHUNK_SIZE);
                    const CellRange ry = rangeFor(sy, CHUNK_SIZE + 8);
                    const CellRange rz = rangeFor(sz, CHUNK_SIZE);
                    for (int px = rx.lo; px <= rx.hi; ++px) {
                        for (int py = ry.lo; py <= ry.hi; ++py) {
                            for (int pz = rz.lo; pz <= rz.hi; ++pz) {
                                const BlockDef d = GetBlockDef(chunk->GetBlock(
                                    px - sx * CHUNK_SIZE, py - sy * CHUNK_SIZE, pz - sz * CHUNK_SIZE));
                                padded.Set(px, py, pz, d.solid && !d.transparent);  // == World::IsOpaque
                            }
                        }
                    }
                }
            }
        }
    }

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

                // Exposure via the padded snapshot -- identical semantics to
                // the previous per-face world.IsOpaque calls.
                bool exposed[6];
                bool anyExposed = false;
                for (int f = 0; f < 6; ++f) {
                    exposed[f] = padded.At(lx + kFaces[f].dx, ly + kFaces[f].dy, lz + kFaces[f].dz) == 0;
                    anyExposed = anyExposed || exposed[f];
                }
                if (!anyExposed && !def.plant) continue;  // fully buried, nothing to emit or shade

                // AO neighborhood (fill order must match NeighborIndex).
                // Column shade, per cell (Craft main.c:1085-1093): nearest
                // opaque cell within 8 starting AT the cell itself (oy=0 --
                // an opaque cell shades itself fully, which is what darkens
                // e.g. a block's bottom face via the cell directly below).
                // Craft's highest[] gate is deliberately skipped: it only
                // short-circuits scans that would find nothing anyway.
                std::uint8_t neighbors[27];
                float shades[27];
                int idx = 0;
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            neighbors[idx] = padded.At(lx + dx, ly + dy, lz + dz);
                            shades[idx] = 0.0f;
                            for (int oy = 0; oy < 8; ++oy) {
                                if (padded.At(lx + dx, ly + dy + oy, lz + dz)) {
                                    shades[idx] = 1.0f - static_cast<float>(oy) * 0.125f;
                                    break;
                                }
                            }
                            ++idx;
                        }
                    }
                }
                float ao[6][4];
                ComputeOcclusion(neighbors, shades, ao);

                if (def.plant) {
                    float minAo = 1.0f;
                    for (int f = 0; f < 6; ++f) {
                        for (int c = 0; c < 4; ++c) minAo = std::min(minAo, ao[f][c]);
                    }
                    EmitPlant(mesh, def, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz), wx,
                              wz, minAo);
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
                const bool isCloud = block == BlockType::Cloud;

                for (int f = 0; f < 6; ++f) {
                    if (!exposed[f]) continue;
                    const FaceDef& face = kFaces[f];

                    const int tileIndex = TileIndexForFace(def, face);
                    const auto baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());
                    const float df = DiffuseFor(face.nx, face.ny, face.nz);

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
                        v.shade = BakedShade(df, ao[f][c], isCloud);
                        mesh.vertices.push_back(v);
                    }

                    // Craft's anti-anisotropy diagonal flip (cube.c:65-67):
                    // split the quad along its LESS-occluded diagonal so
                    // interpolation doesn't smear a dark corner across the
                    // whole face. Craft compares ao[0]+ao[3] vs ao[1]+ao[2]
                    // because its corners are bit-pattern ordered (0-3 and
                    // 1-2 are its diagonals); this mesher's corners are
                    // fan-ordered, so the diagonals are 0-2 and 1-3 and the
                    // default 0,1,2 / 0,2,3 split runs along 0-2. Compare
                    // RAW ao (higher = darker), never baked shade (which
                    // inverts the ordering). Both patterns preserve winding.
                    if (ao[f][0] + ao[f][2] > ao[f][1] + ao[f][3]) {
                        mesh.indices.push_back(baseIndex + 0);
                        mesh.indices.push_back(baseIndex + 1);
                        mesh.indices.push_back(baseIndex + 3);
                        mesh.indices.push_back(baseIndex + 1);
                        mesh.indices.push_back(baseIndex + 2);
                        mesh.indices.push_back(baseIndex + 3);
                    } else {
                        mesh.indices.push_back(baseIndex + 0);
                        mesh.indices.push_back(baseIndex + 1);
                        mesh.indices.push_back(baseIndex + 2);
                        mesh.indices.push_back(baseIndex + 0);
                        mesh.indices.push_back(baseIndex + 2);
                        mesh.indices.push_back(baseIndex + 3);
                    }

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
