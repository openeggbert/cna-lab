#pragma once

#include <string>

#include "BlockType.hpp"

namespace CnaCraft::Worlds {

class World;

// The last-edited-block "marks" a player-driven break/place leaves behind
// (Craft's own `g->block0`/`g->block1`, `main.c`) -- the anchor points every
// world-editing command below reads. Zero-initialized to match Craft's own
// zero-initialized global marks: a command run before ever breaking/placing
// anything operates on (0,0,0)/Air, the same (slightly odd but faithful)
// behavior Craft itself has, not a special "unset" case.
struct BlockMark {
    int x = 0, y = 0, z = 0;
    BlockType type = BlockType::Air;
};

// The `/copy` + `/paste` clipboard (Craft's `g->copy0`/`g->copy1`) -- just
// two corner *positions*, not a snapshot of their contents. `/copy` only
// remembers where to look; `/paste` reads whatever is actually there at
// paste time (see PasteRegion's doc comment for why this matters).
// Zero-initialized, same reasoning as BlockMark above.
struct ClipboardRegion {
    int x1 = 0, y1 = 0, z1 = 0;
    int x2 = 0, y2 = 0, z2 = 0;
};

// The view/create/delete streaming radii `/view` mutates at runtime
// (CnaCraftGame's own kCreateRadius/kDeleteRadius, previously
// compile-time-fixed constants -- plan.md §12.1 item 17).
struct CommandRadii {
    int createRadius = 0;
    int deleteRadius = 0;
};

// World-editing primitives (plan.md §12.1 item 17, CRAFT_PARITY.md §4.5) --
// direct, verified ports of Craft's real command-macro functions
// (`src/main.c`), each taking plain coordinates/types instead of Craft's
// global mutable `g->` state. Engine-agnostic (no CNA dependency), fully
// unit-testable against a plain World.
namespace WorldEditor {

// Craft's `builder_block`: the primitive every function below paints
// through. Skips the world's bottom (`y<=0`, protects the Bedrock floor)
// and top (`y>=WORLD_SIZE_Y-1`, Craft's own `y>=256` scaled to this
// project's real height) layers entirely. Otherwise deletes any existing
// breakable content first (`World::IsBreakable` is already the exact
// `solid && breakable` port of Craft's `is_destructable`), then places
// `type` if it isn't Air. Goes through `World::SetBlockAndRecordEdit` --
// the same persisted path as ordinary player-driven placement, matching
// Craft's own `builder_block` calling the same `set_block` every other
// edit does.
void PaintBlock(World& world, int x, int y, int z, BlockType type);

// `/cube` (fill=false, a hollow shell) and `/fcube` (fill=true, solid) --
// Craft's `cube()`: the axis-aligned box between two corners.
void FillCuboid(World& world, int x1, int y1, int z1, int x2, int y2, int z2, BlockType type, bool fill);

// `/sphere`+`/fsphere` (flattenX=flattenY=flattenZ=false) and
// `/circlex|y|z`+`/fcirclex|y|z` (exactly one flatten flag set) -- Craft's
// `sphere()`, which already covers both command families via these same
// flags (a sphere flattened along one axis *is* a circle).
void FillSphere(World& world, int cx, int cy, int cz, int radius, BlockType type, bool fill,
                 bool flattenX, bool flattenY, bool flattenZ);

// `/cylinder`+`/fcylinder` -- Craft's `cylinder()`: the two corners must
// differ along exactly one axis (a no-op otherwise, matching Craft exactly
// -- e.g. two marks with no axis difference, or differing along more than
// one axis, aren't a valid cylinder axis). Internally a stack of
// FillSphere calls flattened along that one axis, same as Craft's own
// implementation.
void FillCylinder(World& world, int x1, int y1, int z1, int x2, int y2, int z2, int radius, BlockType type,
                   bool fill);

// `/array` -- Craft's `array()`: repeats `type` `xc*yc*zc` times starting
// at (x1,y1,z1), stepping by (x2-x1, y2-y1, z2-z1) each time. An axis
// whose step is zero is forced to count 1 (Craft: `xc = dx ? xc : 1`), so
// e.g. two marks that only differ in X never repeat along Y/Z regardless
// of the requested yc/zc.
void FillArray(World& world, int x1, int y1, int z1, int x2, int y2, int z2, BlockType type, int xc, int yc,
               int zc);

// `/tree` -- Craft's `tree()`: a fixed Wood-trunk (7 tall) + Leaves-canopy
// (a small blob) shape rooted at (x,y,z), via PaintBlock. Deliberately
// distinct from World::GenerateTreesColumn's own world-gen tree shape --
// Craft itself has two different tree shapes (world-gen vs. this command),
// so this is a faithful port of the command's shape specifically, not a
// reuse of the other one.
void GrowTree(World& world, int x, int y, int z);

// `/paste` -- Craft's `paste()`: copies the live block contents of the
// `source` region (read at call time, in `[0, WORLD_SIZE_Y)`) into the
// destination region between (destX1,destY1,destZ1) and
// (destX2,destY2,destZ2), matching sizes along X/Z (Craft matches the
// larger region's extent, walking the smaller one's corner-relative
// direction). `/copy` itself is just `clipboard = {mark1.x,y,z,
// mark0.x,y,z}` -- trivial enough that it has no dedicated function, only
// this paste-time read. A real, faithfully-reproduced Craft quirk: since
// `/copy` only remembers *positions*, not contents, a `/paste` run after
// the source region has since changed reflects the *new* state, not a
// snapshot from `/copy` time.
void PasteRegion(World& world, const ClipboardRegion& source, int destX1, int destY1, int destZ1, int destX2,
                  int destY2, int destZ2);

}

// The `/`-command parser + dispatcher (Craft's `parse_command`, `main.c`) --
// every multiplayer-only branch (`/identity`, `/login`, `/logout`,
// `/online`, `/offline`) and the final plain-chat fallback (`client_talk`)
// are omitted entirely, per CRAFT_PARITY.md §4.5's own framing: those need
// networking this project deliberately doesn't have (§4.6). An unrecognized
// command returns an "Unknown command" message instead of being forwarded
// anywhere.
//
// Pure function of its inputs (World aside) -- no CNA/GPU dependency,
// directly unit-testable. `mark0`/`mark1` are read-only (the marks
// themselves are only ever updated by CnaCraftGame's own break/place
// handling, never by a command); `clipboard`/`radii` are mutated in place
// by `/copy` and `/view` respectively. Returns the feedback message to
// show (mirrors Craft's own `add_message` calls inside `parse_command`) --
// possibly empty for a command that succeeded with nothing to say.
[[nodiscard]] std::string ExecuteCommand(World& world, const std::string& command, const BlockMark& mark0,
                                          const BlockMark& mark1, ClipboardRegion& clipboard, CommandRadii& radii);

}
