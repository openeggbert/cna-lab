// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Map/TileCoord.hpp"

namespace MeshWorld {

// M187-189 (MAP12) — viewport state for the zoomable planetary map UI:
// which tile is centered, at which zoom level, and which tiles are
// currently visible. Pure state/logic, no rendering — ImGui draws
// (M190-198) live in apps/mesh-world-app, consuming this class's output,
// mirroring the WorldStreamer (headless)/WorldRenderer (rendering wrapper)
// split MAP11 already established for chunk streaming.
class MapView {
public:
    // Starts centered at the planet root (level 0, the quadtree's only tile).
    MapView() = default;

    int                   level()  const { return level_; }
    const Map::TileCoord& center() const { return center_; }

    // Jumps directly to `tile`, at its own level, discarding the current
    // level/center — for opening the map already centered on some known
    // position (e.g. the player's current tile at a sensible default zoom)
    // rather than always starting at the planet root and zooming in by hand.
    void set_view(const Map::TileCoord& tile) {
        level_  = tile.level;
        center_ = tile;
    }

    // Zoom in (deeper level, finer tiles) / out (shallower, coarser),
    // clamped to [0, Map::MAX_LEVEL]; a no-op past either end. The center
    // tile is re-derived via TileCoord::child(0,0)/parent(), so the
    // viewport stays over approximately the same world position across the
    // zoom change — child(0,0) always picks one specific sub-tile, not
    // "whichever child contains the viewport's exact current focus point",
    // so this is an approximation, not a precise center-preserving zoom.
    // M197's click-to-travel (not built yet) is the natural place to refine
    // this with an explicit focus point; this class doesn't have one.
    void zoom_in();
    void zoom_out();

    // Moves the center tile by (dx, dy) tiles at the current level, clamped
    // to the valid [0, 2^level) index range on each axis — no wraparound,
    // matching map.md's flat, bounded-square planet model (panning off one
    // edge does not wrap to the opposite edge).
    void pan(std::int64_t dx_tiles, std::int64_t dy_tiles);

    // Every tile visible in a `width` x `height`-tile viewport centered on
    // the current center tile (center tile included; extra space distributed
    // one more column/row after the center on an even width/height, matching
    // integer-division rounding), clamped to the valid grid — a viewport
    // near a planet edge returns fewer than width*height tiles rather than
    // wrapping or fabricating off-grid entries. Returns an empty vector if
    // width or height is <= 0.
    std::vector<Map::TileCoord> visible_tiles(int width, int height) const;

    // M195/M202 — whether a Map::PlaceLabel of the given `kind` should be
    // drawn at the given zoom `level`, so shallow zooms aren't cluttered
    // with every village name (map.md's own "no street names at continent
    // zoom" principle, applied here to PlaceLabel::kind — the only kinds
    // any real generator currently produces are Settlements::
    // appendLabels()'s tier_kind() values: "capital"/"city"/"town"/
    // "village", see Settlements.cpp; "continent"/"country" are handled
    // too since existing test fixtures already use them and a future
    // generator may start emitting them). Pure function, no MapView state
    // needed — a static method purely for discoverability alongside the
    // rest of this class's zoom-level logic.
    static bool should_show_label(const std::string& kind, int level);

    // M199 — the "1:N" display scale denominator for a viewport showing
    // `tiles_across` tiles at the given zoom `level`, matching map.md
    // §5.4's own formula (N = ground width shown / screen width shown) and
    // its own "~0.2 m-wide map viewport" reference screen width — a
    // deliberately approximate real-world assumption, not a pixel-perfect
    // DPI calibration (map.md's own words: "compute the 1:N label for the
    // player on the fly"). With tiles_across=1 this reproduces map.md
    // §5.4's own worked examples exactly (z0 ≈ 1:113,000,000, z3 ≈
    // 1:14,000,000, z5 ≈ 1:3,500,000). Returns 0.0 for a non-positive
    // `tiles_across` or `screen_width_m`.
    static double scale_denominator(int level, int tiles_across, double screen_width_m = 0.2);

private:
    int             level_{0};
    Map::TileCoord  center_{0, 0, 0};
};

} // namespace MeshWorld
