This project's architecture is designed with reference to and inspired by **Craft**
(https://github.com/fogleman/Craft), a small Minecraft clone written in C/OpenGL.

No source code is copied verbatim from Craft — CNA Craft is an independent C++ implementation
against the CNA/`Microsoft::Xna::Framework` API — but the following design decisions are
consciously modeled on Craft's approach, and are called out as such in `plan.md`:

- Rendering only geometry for block faces that are actually exposed to a non-solid neighbor
  (Craft's "only exposed faces are rendered" optimization).
- Reading a one-block neighbor overlap across chunk boundaries so edge faces cull correctly.
- Storing world edits as a sparse list of `(chunk, x, y, z, blockType)` deltas layered on top of
  procedurally regenerated terrain, rather than persisting the full world (Craft's SQLite `block`
  delta-table schema), as the intended approach for the save/load stretch goal.
- Deterministic, position-seeded noise for terrain height (Craft uses Simplex noise; this
  prototype uses a self-contained value-noise implementation for the same purpose, with Simplex
  noted as a possible upgrade).

Because this project's own [LICENSE](LICENSE) is already MIT, it is license-compatible with
Craft's MIT license (reproduced below in full per its terms) without any change to CNA Craft's
own licensing.

## Craft license (MIT)

```
Copyright (c) 2013 Michael Fogleman

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

This project also builds on **CNA** (https://github.com/openeggbert/cna, licensed under the
Microsoft Public License (Ms-PL)) and its **sharp-runtime** utility layer — see
`../cna/THIRD_PARTY_NOTICES.md` for CNA's own upstream notices (FNA, etc.). CNA Craft's own code
is MIT-licensed (see `LICENSE`); linking against CNA does not change that — CNA Craft ships as
source that a user builds and links themselves, the same relationship CNA has with FNA.
