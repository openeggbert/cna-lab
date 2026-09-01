# Renaming the project

`Iron Gang` is a provisional working title. Rename it before public branding when a proper clearance process selects the final name.

## Identifiers that must change together

| Current value | Purpose | Example replacement form |
|---|---|---|
| `Iron Gang` | Human-readable title | `Final Title` |
| `IronGang` | CMake project, namespace, class prefixes | `FinalTitle` |
| `iron_gang` | CMake targets, executable, save filename | `final_title` |
| `iron-gang` | Install path and save format slug | `final-title` |
| `IRON_GANG` | CMake options and compile definitions | `FINAL_TITLE` |
| `IG-` | Plan task prefix | Optional stable project prefix |

## Rename procedure

1. Commit or copy the repository so the operation can be reviewed.
2. Rename `include/IronGang/` and `IronGangGame.hpp/.cpp`.
3. Replace all five identifier forms across text source files.
4. Update the title and city metadata under `assets/config/`.
5. Decide whether old save files should remain compatible. If they should, keep the old `format=` reader as a migration path instead of replacing it blindly.
6. Update package/install paths, scripts, documentation, license attribution, and asset-registry author fields.
7. Run:

```bash
./scripts/check-syntax.sh
./scripts/validate-mc3.sh
cmake --preset dev-easygl
cmake --build --preset dev-easygl --parallel 4
ctest --preset dev-easygl
```

8. Search for stale identifiers:

```bash
grep -RInE 'Iron Gang|IronGang|iron_gang|iron-gang|IRON_GANG' .
```

9. Perform the final name, trademark, company, domain, store, and repository search before publishing artwork or announcements.

Do not change only the visible title. Namespace, target, install, save, and package identifiers are intentionally explicit so accidental partial renames are detectable.
