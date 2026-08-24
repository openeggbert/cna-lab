# Assets

`assets/source/` contains editable source assets. MC3 XML files are intended to be opened in Mesh Craft and converted through the documented pipeline; the hand-authored skinned test character remains glTF because MC3 has no rigging support.

The warehouse, four composed sedan parts, and test character are converted to CNJ and loaded by the current executable, with procedural fallback when generated output is absent. `content-budgets.json` contains reviewed bootstrap triangle/material/texture-count limits for these sources and the older district prototype. From the repository root, run `./scripts/content_budget.py` before authoring/import changes; `build-assets.sh` enforces the matching group automatically.

Every production source asset must be recorded in `licenses/asset-registry.csv` before it is
committed. The versioned row includes exact asset and local license-evidence hashes, rights review,
reviewer, approval, and shipping state. Do not assume that “free download” means commercial use or
redistribution is permitted.

Validate the complete production set and generated notice with:

```bash
./scripts/asset_registry.py --project-root . --check-notice THIRD_PARTY_ASSETS.md
```

After an approved registry change, regenerate the deterministic notice with
`--write-notice THIRD_PARTY_ASSETS.md`. `build-assets.sh` runs the check before conversion, and CTest
also covers the committed registry plus malformed/missing/unapproved cases. Generated GLB/CNJ
outputs inherit provenance from their registered MC3/glTF source and conversion tools; they are not
independent downloaded assets.
