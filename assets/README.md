# Assets

The project does not ship third-party image or audio assets.

The floor, ceiling and door panels of the world atlas, stylized blood-pool decal,
framed painting, peace-symbol banner and ceiling lamp are generated at runtime in
`src/WolfGame.cpp`. Original generated wall and wood images under `materials/`
are sampled into that atlas through CNA's public texture API.

Original generated sprites under `sprites/`, `weapons/`, `pickups/`, `props/` and
`decorations/` are project-owned assets. Their source, generation date and complete
prompts are recorded in `../ASSET_PROVENANCE.md`.

The generated title artwork under `title/` is also project-owned and recorded in
`../ASSET_PROVENANCE.md`; exact menu text is rendered separately by game code.

`levels/starter.level` is the starter's original text level. Its symbols are documented in the
top-level README.

Future external assets must be recorded in `../THIRD_PARTY.md` before they are committed.
