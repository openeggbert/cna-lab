# Dev notes

Practical, task-oriented checklists for common changes to this codebase — written
directly from how the change was actually made (not aspirational), so each step names
the exact file/function to touch. Update a checklist here whenever the pattern it
describes changes, the same way the rest of this codebase's own comments stay in sync
with the code they describe.

## How to add a new biome (`ZoneType` value)

Written from the MAP16 pattern (M235-M280, 2026-07-10 — 12 → 52 `ZoneType` values in
one pass) plus the individual generator-dispatch/rendering follow-ups MAP20/21 added
afterward. Six touch points; skipping one produces either a compile error (good — the
build catches it) or a silent fallback value (bad — the build succeeds, but the new
biome renders wrong until someone notices).

1. **Enum** — `include/ZoneType.hpp`'s `enum class ZoneType`. Add the new value
   **anywhere except last** — `empty` must stay the final ordinal, because
   `MapValidator`'s `max_valid` sentinel (`src/MapValidator.cpp`) is
   `static_cast<uint8_t>(ZoneType::empty)`; inserting after it silently breaks
   validation for every real biome that follows. The header's own comment on the enum
   already states this rule — read it before editing.

2. **String round-trip** — same file, `to_string(ZoneType)` and `zone_from_string(const
   std::string&)`, both plain `switch` statements. **Compile-enforced**: this project
   builds under `-Wall -Wextra -Werror`, so an unhandled enum value in a `switch`
   fails the build — you cannot forget this step and have the build silently succeed.

3. **`ZONE_NAMES` + colors** — `include/PlanetMapLogic.hpp`'s `ZONE_NAMES` array (must
   be resized: it's `std::array<const char*, N>`, a literal count, not inferred) and
   `src/PlanetMapLogic.cpp`'s `zone_rgb_color(int zone_ordinal)` (`kColors`, an array
   in the exact same order as the enum/`ZONE_NAMES` — the function's own comment says
   so explicitly). **NOT compile-enforced**: both are array-indexed with a safe
   out-of-range fallback (`zone_rgb_color` returns magenta `{255,0,255}` for an
   ordinal past the array's end) — a forgotten entry doesn't fail the build, it just
   renders every instance of the new biome as a jarring magenta in `--png`/`--legend`
   output. Pick an RGB color that's visually distinct from every biome already in the
   same climate family (MAP16's own approach: design the whole family's palette in one
   pass, not 8 independent picks across 8 later tasks).

4. **ASCII letter** — **two** separate places, both array/switch-indexed the same
   ordinal, and they must agree with each other (their own comments say so):
   - `src/PlanetMapLogic.cpp`'s `zone_ascii_char(int zone_ordinal)` (`kChars` array,
     `'?'` fallback for out-of-range — not compile-enforced) — feeds `--ascii` and
     `MeshWorldPlanet`'s planetary-map biome-grid rendering.
   - `src/tools/print_map.cpp`'s `zone_char(ZoneType z)` (a `switch`, **compile-
     enforced** under `-Werror`) — feeds the legacy flat-`WorldMap` `MeshWorldMap`
     visualizer. This one WILL fail the build if you forget it; `zone_ascii_char`
     above will not.
   Pick an unused letter — uppercase first, then lowercase once uppercase runs out
   (see `zone_ascii_char`'s own comment for exactly which letters were still free as
   of the MAP16 pass — mnemonics don't matter, only uniqueness does).

5. **`BiomeClassifier::classify()`** — `src/Map/BiomeClassifier.cpp`. A biome with no
   branch here is a real value (nameable, colorable, ASCII-able) that never actually
   appears in generated terrain — the exact "exists in name only" gap this session
   spent MAP21 (`ZoneType::cave`) and part of MAP16 itself (8 of the 40 new values,
   still open — see the file's own header comment) closing for other biomes. Slot the
   new threshold logic into the existing cascade (`elevation`, `temperature`,
   `moisture`, `sea_level_m` are the only 4 inputs `classify()` gets — a signal like
   slope, distance-to-coast, or distance-to-river genuinely cannot be expressed here;
   see the same header comment for which of the original 40 are blocked on exactly
   that and why). Where inserting a new branch would flip an *existing* test's
   expected result, that's not automatically wrong — check whether the existing
   test's own input values are actually a better example of the NEW biome than the
   old one (MAP16's own precedent: `elevation≈0, moisture=1.0` is mangrove's own
   definition, not just "jungle" — the reclassification there was intentional, not a
   regression).

6. **Chunk generator dispatch** (only if the biome needs one) — `src/ChunkGenerator.cpp`'s
   `get_generator()`. A new `ZoneType` with no `case` here silently falls through to
   `EmptyGenerator` (blank chunks) — not a build error either. `docs/map-generation.md`'s
   own "Zone → chunk generator dispatch audit" table tracks which of the 52 values
   have real dispatch today and records the most sensible existing generator as a
   stopgap for each still-unwired one — update BOTH the code and that table together,
   not just the code (a genuinely real drift this project found twice already, M280
   → M326 → M349's own dated correction pass).

### Tests to update alongside all of the above

- `tests/ZoneTypeTests.cpp` — `ExactlyFiftyTwoValues` (bump the count),
  `EmptyIsStillTheLastOrdinal`, `ToStringRoundTripsThroughZoneFromString`,
  `AllNamesAreUnique`, `ZoneNamesArrayMatchesToStringByOrdinal`,
  `EveryOrdinalHasAResolvableRgbColor`, `AllRgbColorsAreDistinct`,
  `EveryOrdinalHasAResolvableAsciiChar`, `AllAsciiCharsAreDistinct`,
  `WorldMapZoneColorHandlesEveryValueWithoutTheDeadDefault` — this file's own
  `kAllZones` list (an anonymous-namespace fixture, not the enum itself) needs the
  new value added too, or these tests simply won't see it.
- `tests/BiomeClassifierTests.cpp` — at minimum one test proving the new branch is
  actually reachable with real (elevation, temperature, moisture, sea_level_m) inputs,
  plus a check that it doesn't silently steal territory from an adjacent branch's own
  existing test inputs (or, if it deliberately does, a comment explaining why, per
  step 5 above).
- `tests/PlanetMapLogicTests.cpp` — if step 6 added a dispatch case, a test proving
  the new biome's chunk actually uses the intended generator (not `EmptyGenerator`).

### Docs to update alongside all of the above

- `map.md` §6 — the biome-family breakdown and the "N of 40 new values reachable"
  running count (see that section's own MAP16 update note for the pattern).
- `docs/map-generation.md`'s dispatch-audit table (step 6, above).
