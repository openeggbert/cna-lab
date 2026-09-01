# MeshWorld — Formats

## MC3 — editable source format

`mc3.xml` is the MeshCraft editable scene format. It is an XML file describing:

- scene geometry (primitives, meshes)
- object instances (position, rotation, scale, material)
- scene metadata

MeshWorld's chunk generator writes MC3 as its native output format. MC3 is human-readable and can be edited in MeshCraft.

MC3 is the **source of truth** for chunk content. MCB is derived from it.

### MC3 in MeshWorld

- Generator output: `source_chunks/<x>_<y>.mc3.xml`
- AI output (when enabled): also mc3.xml, validated before use
- Can be inspected and hand-edited for debugging or authoring

---

## MCB — binary runtime format

`mcb` is the MeshCraft binary format. It is compiled from MC3 and optimized for fast loading.

MCB characteristics:

- compact binary representation
- faster to parse than XML
- suitable for runtime streaming
- suitable for distribution (smaller than MC3)

MeshWorld's runtime always loads MCB, never MC3. The MCB files are:

- stored in `chunks/<x>_<y>.mcb`
- regenerated from the corresponding MC3 source when missing
- treated as a cache: safe to delete and regenerate

### MCB as a cache

MCB files are the chunk cache. They can always be regenerated from the MC3 source. However, regenerating requires the generator to run again, which takes time.

In practice:

- MCB is generated once and reused on all subsequent visits.
- MCB is gitignored; it is not committed to the repository.
- Example pre-generated MCB files may be shipped in a demo archive.

---

## Why MCB instead of always using MC3

1. Parsing 400+ XML files at startup is slow.
2. MCB loads in a fraction of the time.
3. MCB can be memory-mapped or streamed efficiently.
4. Distribution of MCB hides source structure (though not real DRM — see below).

---

## MCB is not real DRM

Distributing MCB instead of MC3 hides the readable XML structure, but it is not real digital rights management:

- MCB can be decompiled to MC3 with the right tools.
- The format is documented in MeshCraft.
- For an open-source demo, this distinction is irrelevant — the MC3 sources are committed.

MCB's value is runtime performance and distribution size, not content protection.

---

## Optional GLB export

MeshWorld may support exporting chunks as GLB (glTF binary) for external tools (Blender, web viewers, game engines). This is a future feature, not a first-version requirement.

GLB export would:

- allow inspection in standard 3D viewers
- enable porting chunk content to other engines
- serve as a demonstration of MeshCraft's export capabilities

---

## world.json

`world.json` is the top-level world configuration. It is a simple JSON file:

- world seed
- map dimensions
- chunk size
- style name
- district layout overrides

`world.json` is committed to the repository as an example. Local modifications for testing use a separate file name (e.g., `world.local.json`).

---

## config/ai.local.json

Local AI configuration. Gitignored. See [ai-generation.md](ai-generation.md).

---

## Format summary

| File | Format | Committed | Role |
|------|--------|-----------|------|
| `world.json` | JSON | Yes (example) | World config |
| `source_chunks/*.mc3.xml` | MC3/XML | No (gitignored) | Generator source output |
| `chunks/*.mcb` | MCB/binary | No (gitignored) | Runtime cache |
| `cache/*` | Various | No (gitignored) | Generator scratch |
| `config/ai.local.json` | JSON | No (gitignored) | Local AI config |
| `examples/*.json` | JSON | Yes | Example configs |
