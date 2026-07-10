#include "WorldEditor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace WorldEditor {

void PaintBlock(World& world, int x, int y, int z, BlockType type) {
    // Craft's own `y<=0 || y>=256` guard, scaled to this project's real
    // world height -- protects the Bedrock floor (y=0) and the topmost
    // layer, matching Craft's literal bound relationship exactly.
    if (y <= 0 || y >= WORLD_SIZE_Y - 1) return;
    if (world.IsBreakable(x, y, z)) world.SetBlockAndRecordEdit(x, y, z, BlockType::Air);
    if (type != BlockType::Air) world.SetBlockAndRecordEdit(x, y, z, type);
}

void FillCuboid(World& world, int x1, int y1, int z1, int x2, int y2, int z2, BlockType type, bool fill) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (z1 > z2) std::swap(z1, z2);
    const int degenerateAxes = (x1 == x2 ? 1 : 0) + (y1 == y2 ? 1 : 0) + (z1 == z2 ? 1 : 0);
    for (int x = x1; x <= x2; ++x) {
        for (int y = y1; y <= y2; ++y) {
            for (int z = z1; z <= z2; ++z) {
                if (!fill) {
                    const int boundaryAxes = ((x == x1 || x == x2) ? 1 : 0) + ((y == y1 || y == y2) ? 1 : 0) +
                                              ((z == z1 || z == z2) ? 1 : 0);
                    if (boundaryAxes <= degenerateAxes) continue;
                }
                PaintBlock(world, x, y, z, type);
            }
        }
    }
}

void FillSphere(World& world, int cx, int cy, int cz, int radius, BlockType type, bool fill, bool flattenX,
                 bool flattenY, bool flattenZ) {
    // The 8 unit-cube corner offsets Craft's own sphere() samples per cell
    // to decide "fully inside," "fully outside," or "straddling" the
    // radius -- a cell straddling the boundary is the hollow-shell case.
    static constexpr float kCornerOffsets[8][3] = {
        {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, -0.5f},  {0.5f, 0.5f, 0.5f},
    };
    for (int x = cx - radius; x <= cx + radius; ++x) {
        if (flattenX && x != cx) continue;
        for (int y = cy - radius; y <= cy + radius; ++y) {
            if (flattenY && y != cy) continue;
            for (int z = cz - radius; z <= cz + radius; ++z) {
                if (flattenZ && z != cz) continue;
                bool inside = false;
                bool outside = fill;
                for (const auto& offset : kCornerOffsets) {
                    const float dx = static_cast<float>(x) + offset[0] - static_cast<float>(cx);
                    const float dy = static_cast<float>(y) + offset[1] - static_cast<float>(cy);
                    const float dz = static_cast<float>(z) + offset[2] - static_cast<float>(cz);
                    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (d < static_cast<float>(radius)) {
                        inside = true;
                    } else {
                        outside = true;
                    }
                }
                if (inside && outside) PaintBlock(world, x, y, z, type);
            }
        }
    }
}

void FillCylinder(World& world, int x1, int y1, int z1, int x2, int y2, int z2, int radius, BlockType type,
                   bool fill) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (z1 > z2) std::swap(z1, z2);
    const bool varyX = x1 != x2;
    const bool varyY = y1 != y2;
    const bool varyZ = z1 != z2;
    // Craft's own guard: the two corners must differ along exactly one
    // axis -- that axis becomes the cylinder's length, the other two are
    // the flattened-circle cross-section.
    if ((varyX ? 1 : 0) + (varyY ? 1 : 0) + (varyZ ? 1 : 0) != 1) return;
    if (varyX) {
        for (int x = x1; x <= x2; ++x) FillSphere(world, x, y1, z1, radius, type, fill, true, false, false);
    } else if (varyY) {
        for (int y = y1; y <= y2; ++y) FillSphere(world, x1, y, z1, radius, type, fill, false, true, false);
    } else {
        for (int z = z1; z <= z2; ++z) FillSphere(world, x1, y1, z, radius, type, fill, false, false, true);
    }
}

void FillArray(World& world, int x1, int y1, int z1, int x2, int y2, int z2, BlockType type, int xc, int yc,
               int zc) {
    const int dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    xc = dx ? xc : 1;
    yc = dy ? yc : 1;
    zc = dz ? zc : 1;
    for (int i = 0; i < xc; ++i) {
        const int x = x1 + dx * i;
        for (int j = 0; j < yc; ++j) {
            const int y = y1 + dy * j;
            for (int k = 0; k < zc; ++k) {
                const int z = z1 + dz * k;
                PaintBlock(world, x, y, z, type);
            }
        }
    }
}

void GrowTree(World& world, int x, int y, int z) {
    // Craft's own tree(): a 7-tall Wood trunk plus a small Leaves blob
    // (distance-squared < 11 from the blob's center) -- distinct from, and
    // simpler than, this project's own world-gen tree shape.
    for (int cy = y + 3; cy < y + 8; ++cy) {
        for (int dx = -3; dx <= 3; ++dx) {
            for (int dz = -3; dz <= 3; ++dz) {
                const int dy = cy - (y + 4);
                const int distSq = dx * dx + dy * dy + dz * dz;
                if (distSq < 11) PaintBlock(world, x + dx, cy, z + dz, BlockType::Leaves);
            }
        }
    }
    for (int cy = y; cy < y + 7; ++cy) {
        PaintBlock(world, x, cy, z, BlockType::Wood);
    }
}

void PasteRegion(World& world, const ClipboardRegion& source, int destX1, int destY1, int destZ1, int destX2,
                  int destY2, int destZ2) {
    // Craft's own paste(): scx/scz/spx/spz are each +1/-1/0 step directions
    // (matches the source's own corner order to the destination's), oy is
    // the vertical offset between the two regions' first corners. Reads
    // *live* world state at (source.x1 + x*scx, y, source.z1 + z*scz) for
    // each cell -- not a snapshot captured at /copy time (see
    // WorldEditor.hpp's PasteRegion doc comment).
    //
    // Faithfully reproduces a real Craft quirk (same loop shape as Craft's
    // own paste()): this reads and writes the *same* live World in one
    // ascending-y sweep, so if the destination Y range overlaps a
    // not-yet-read source row (e.g. pasting a region a few blocks above
    // its own source), an earlier iteration can clobber source data before
    // a later iteration reads it. Not a bug to "fix" by buffering the
    // source first -- Craft's real paste() has this exact same
    // characteristic, and it's harmless for the common case (pasting a
    // copied structure somewhere the source and destination don't
    // Y-overlap at all).
    const int scx = (source.x2 > source.x1) - (source.x2 < source.x1);
    const int scz = (source.z2 > source.z1) - (source.z2 < source.z1);
    const int spx = (destX2 > destX1) - (destX2 < destX1);
    const int spz = (destZ2 > destZ1) - (destZ2 < destZ1);
    const int oy = destY1 - source.y1;
    const int dx = std::abs(source.x2 - source.x1);
    const int dz = std::abs(source.z2 - source.z1);
    for (int y = 0; y < WORLD_SIZE_Y; ++y) {
        for (int x = 0; x <= dx; ++x) {
            for (int z = 0; z <= dz; ++z) {
                const BlockType w = world.GetBlock(source.x1 + x * scx, y, source.z1 + z * scz);
                PaintBlock(world, destX1 + x * spx, y + oy, destZ1 + z * spz, w);
            }
        }
    }
}

}

namespace {

// std::sscanf works the same against a C++ std::string's c_str() as it does
// against Craft's own C buffers -- a direct, low-risk port of
// parse_command's real dispatch chain, same library, same format strings.

bool MatchesExact(const std::string& command, const char* literal) { return command == literal; }

}

std::string ExecuteCommand(World& world, const std::string& command, const BlockMark& mark0, const BlockMark& mark1,
                            ClipboardRegion& clipboard, CommandRadii& radii) {
    int radius = 0, count = 0, xc = 0, yc = 0, zc = 0;

    if (std::sscanf(command.c_str(), "/view %d", &radius) == 1) {
        if (radius >= 1 && radius <= 24) {
            radii.createRadius = radius;
            // This project's own hysteresis margin (kDeleteRadius =
            // kCreateRadius + 3), not Craft's literal +4 -- consistent
            // with already not copying Craft's literal 10/10/14 radii
            // during the chunk-streaming redesign.
            radii.deleteRadius = radius + 3;
            return "Viewing distance set to " + std::to_string(radius) + ".";
        }
        return "Viewing distance must be between 1 and 24.";
    }
    if (MatchesExact(command, "/copy")) {
        clipboard = ClipboardRegion{mark1.x, mark1.y, mark1.z, mark0.x, mark0.y, mark0.z};
        return "Copied.";
    }
    if (MatchesExact(command, "/paste")) {
        WorldEditor::PasteRegion(world, clipboard, mark1.x, mark1.y, mark1.z, mark0.x, mark0.y, mark0.z);
        return "Pasted.";
    }
    if (MatchesExact(command, "/tree")) {
        WorldEditor::GrowTree(world, mark0.x, mark0.y, mark0.z);
        return "Tree planted.";
    }
    if (std::sscanf(command.c_str(), "/array %d %d %d", &xc, &yc, &zc) == 3) {
        if (mark0.type != mark1.type) return "Marked blocks must be the same type.";
        WorldEditor::FillArray(world, mark1.x, mark1.y, mark1.z, mark0.x, mark0.y, mark0.z, mark0.type, xc, yc, zc);
        return "Array created.";
    }
    if (std::sscanf(command.c_str(), "/array %d", &count) == 1) {
        if (mark0.type != mark1.type) return "Marked blocks must be the same type.";
        WorldEditor::FillArray(world, mark1.x, mark1.y, mark1.z, mark0.x, mark0.y, mark0.z, mark0.type, count, count,
                                count);
        return "Array created.";
    }
    if (MatchesExact(command, "/fcube") || MatchesExact(command, "/cube")) {
        if (mark0.type != mark1.type) return "Marked blocks must be the same type.";
        WorldEditor::FillCuboid(world, mark0.x, mark0.y, mark0.z, mark1.x, mark1.y, mark1.z, mark0.type,
                                 MatchesExact(command, "/fcube"));
        return "Cube created.";
    }
    if (std::sscanf(command.c_str(), "/fsphere %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, true, false, false, false);
        return "Sphere created.";
    }
    if (std::sscanf(command.c_str(), "/sphere %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, false, false, false, false);
        return "Sphere created.";
    }
    if (std::sscanf(command.c_str(), "/fcirclex %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, true, true, false, false);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/circlex %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, false, true, false, false);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/fcircley %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, true, false, true, false);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/circley %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, false, false, true, false);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/fcirclez %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, true, false, false, true);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/circlez %d", &radius) == 1) {
        WorldEditor::FillSphere(world, mark0.x, mark0.y, mark0.z, radius, mark0.type, false, false, false, true);
        return "Circle created.";
    }
    if (std::sscanf(command.c_str(), "/fcylinder %d", &radius) == 1) {
        if (mark0.type != mark1.type) return "Marked blocks must be the same type.";
        WorldEditor::FillCylinder(world, mark0.x, mark0.y, mark0.z, mark1.x, mark1.y, mark1.z, radius, mark0.type,
                                   true);
        return "Cylinder created.";
    }
    if (std::sscanf(command.c_str(), "/cylinder %d", &radius) == 1) {
        if (mark0.type != mark1.type) return "Marked blocks must be the same type.";
        WorldEditor::FillCylinder(world, mark0.x, mark0.y, mark0.z, mark1.x, mark1.y, mark1.z, radius, mark0.type,
                                   false);
        return "Cylinder created.";
    }

    return "Unknown command: " + command;
}

}
