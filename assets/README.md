# Assets

The project does not ship third-party image or audio assets.

The wall/floor/ceiling atlas, stylized blood-pool decal, framed painting,
peace-symbol banner and ceiling lamp are generated at runtime in `src/WolfGame.cpp`.

Original generated sprites under `sprites/`, `weapons/` and `pickups/` are project-owned assets. Their source,
generation date and complete prompts are recorded in `../ASSET_PROVENANCE.md`.

The generated title artwork under `title/` is also project-owned and recorded in
`../ASSET_PROVENANCE.md`; exact menu text is rendered separately by game code.

`levels/starter.level` is the starter's original text level. Its symbols are documented in the
top-level README.

Future external assets must be recorded in `../THIRD_PARTY.md` before they are committed.
