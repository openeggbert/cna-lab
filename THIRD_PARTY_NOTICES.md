# Third-party notices and attribution

## Craft (fogleman/Craft) — the work CNA Craft is derived from

**CNA Craft is a derivative work of [Craft](https://github.com/fogleman/Craft)** by Michael
Fogleman (MIT license, Copyright © 2013 Michael Fogleman) — a small Minecraft clone written in
C/OpenGL. It is a **port**, not merely a project "inspired by" Craft: the goal, stated in
`plan.md` and tracked feature-by-feature in [CRAFT_PARITY.md](CRAFT_PARITY.md), is a faithful
reimplementation of Craft's behavior on the CNA (`Microsoft::Xna::Framework`) API.

**No Craft source code is copied verbatim.** CNA Craft is written from scratch in C++23 against a
different engine API, and none of Craft's C files, headers, shaders, or asset files ship in this
repository. But the *substance* is Craft's, and it is reproduced deliberately and closely:

- **Algorithms ported essentially 1:1**, including their exact constants and lookup tables — for
  example ambient occlusion (`occlusion()` in Craft's `src/main.c`, with its `curve[]` table, the
  "both sides solid" rule, the 8-block column-shade term and the anti-anisotropy quad-diagonal
  flip from `src/cube.c`), the world-editing command dispatcher (`parse_command`) and every
  geometry primitive behind it (`cube`, `sphere`, `cylinder`, `array`, `tree`, `paste`), the
  day/night curve (`get_daylight`/`time_of_day`), remote-player interpolation
  (`interpolate_player`), the player cube (`make_player`), terrain generation, tree/plant/cloud
  placement, block-placement and light-toggle guards, and the physics/collision model.
- **Data formats copied by design.** The SQLite save schema (`block`/`sign`/`light`/`state`/`key`
  tables, including Craft's `p,q` chunk-address columns and their index shapes) matches Craft's
  `src/db.c` schema; the multiplayer wire protocol is a dialect of Craft's own ASCII line protocol
  (same opcodes, field order and semantics as `src/client.c`/`server.py`, with documented
  deliberate deviations listed in [MULTIPLAYER_PLAN.md](MULTIPLAYER_PLAN.md)).
- **Values sampled from Craft's assets.** CNA Craft generates all of its textures procedurally at
  runtime and ships no image files, but several of the colors those generators use — the 32 dye
  swatches and the sky gradient's dawn/dusk bands — were read off Craft's own `textures/texture.png`
  and `textures/sky.png` and embedded as numeric constants.
- **Architecture and behavior**, e.g. rendering only block faces exposed to a non-solid neighbor,
  reading a one-block neighbor overlap across chunk boundaries, unbounded chunk-streamed terrain
  with a fixed Y extent, and persisting only *edits* as sparse deltas layered on top of
  deterministically regenerated terrain rather than saving the whole world.

Where CNA Craft deliberately departs from Craft (Minecraft-style flight controls, a glow pass
instead of true light propagation, no compatibility with real Craft servers, and others), those
choices are recorded as such in `CRAFT_PARITY.md` and `plan.md` — the default intent is parity.

One further upstream credit: CNA Craft's Simplex noise (`Worlds/NoiseGenerator.cpp`) is an
independent reimplementation of the same classic Gustavson simplex-noise algorithm that Craft's
own `deps/noise/noise.c` implements, which is itself derived from
https://github.com/caseman/noise (MIT license, Copyright © 2008 Casey Duncan). Unlike Craft's
fixed global permutation table, this implementation permutes its gradient table per world seed.

CNA Craft's own [LICENSE](LICENSE) is MIT, so it is license-compatible with Craft's MIT license,
which is reproduced below in full as that license requires.

### Craft license (MIT)

```
Copyright (C) 2013 Michael Fogleman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## CNA and sharp-runtime

This project builds on **CNA** (https://github.com/openeggbert/cna, licensed under the Microsoft
Public License (Ms-PL)) and its **sharp-runtime** utility layer — see
`../cna/THIRD_PARTY_NOTICES.md` for CNA's own upstream notices (FNA, etc.). CNA Craft's own code
is MIT-licensed (see `LICENSE`); linking against CNA does not change that — CNA Craft ships as
source that a user builds and links themselves, the same relationship CNA has with FNA.
