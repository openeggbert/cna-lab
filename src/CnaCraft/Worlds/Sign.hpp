#pragma once

#include <string>
#include <vector>

namespace CnaCraft::Worlds {

// A single placed sign (CRAFT_PARITY.md §4.3), ported from Craft's real
// `Sign{x,y,z,face,text[64]}` (src/sign.h) — a separate overlay, not a
// block type, exactly like Craft's own model.
//
// `face` uses a simplified 6-direction convention (0=+X, 1=-X, 2=+Y/top,
// 3=-Y/bottom, 4=+Z, 5=-Z — directly from the raycast hit's face normal),
// NOT a literal port of Craft's own `hit_test_face` (`src/main.c:666-696`):
// Craft's real scheme is asymmetric (only 4 side faces + a top face with 4
// player-facing-angle sub-rotations baked into the face index 4-7; bottom
// faces aren't supported for signs at all) and derives the face by
// comparing two separate raycasts' hit cells, needed because Craft's own
// hit-test doesn't return a face normal directly. This project's
// VoxelRaycast already returns an explicit face normal, so deriving a
// plain 6-way index directly from it is simpler, supports all 6 faces
// uniformly (including bottom, which Craft's own scheme cannot), and needs
// no player-angle-dependent sub-rotation. A deliberate simplification, not
// an oversight.
struct Sign {
    int x = 0, y = 0, z = 0;
    int face = 0;
    std::string text;
};

// In-memory sign list (CRAFT_PARITY.md §4.3) — kept engine-agnostic like
// the rest of Worlds/ so placement/lookup is unit-testable without CNA.
// Persistence (Persistence::WorldStore) and 3D billboard rendering
// (Render/SignBillboard) both live outside this class; SignStore only owns
// the data, matching Craft's own `SignList` (src/sign.c) being a plain
// data structure separate from `db_insert_sign`/`gen_sign_buffer`.
class SignStore {
public:
    // Places (or replaces, if one already exists at the same x,y,z,face —
    // matches Craft's own `sign_xyzface_idx` unique index / `insert or
    // replace`) a sign. Empty text removes any existing sign at that
    // position instead of storing a blank one (matches Craft's own
    // `sign_list_remove` path for empty-text signs, `src/main.c`
    // `set_sign`).
    void PlaceSign(int x, int y, int z, int face, const std::string& text);

    // Removes every sign at (x,y,z) regardless of face, matching Craft's
    // own `unset_sign` (src/main.c) — called when the underlying block is
    // broken (`_set_block`'s `w == 0` branch), since a sign can't outlive
    // the block face it was attached to. Returns true if anything was
    // actually removed (callers use this to skip a needless DB delete /
    // billboard rebuild).
    bool RemoveAllAt(int x, int y, int z);

    // Removes every sign whose (x,z) falls within chunk-column (cx,cz),
    // regardless of y or face — used when a column unloads (plan.md §12.1
    // item 19): a sign can't survive after the World data it was attached
    // to is gone (and would otherwise either accumulate unboundedly across
    // a long streamed-world session or desync from a re-generated column
    // that doesn't know about it until WorldStore reloads it). Returns
    // true if anything was actually removed.
    bool RemoveAllInColumn(int cx, int cz);

    [[nodiscard]] const std::vector<Sign>& Signs() const { return signs_; }

    // Used by Persistence::WorldStore when loading — replaces the whole
    // list at once (loading isn't a series of individual PlaceSign calls,
    // it's "here's everything that was saved").
    void ReplaceAll(std::vector<Sign> signs) { signs_ = std::move(signs); }

private:
    std::vector<Sign> signs_;
};

}
