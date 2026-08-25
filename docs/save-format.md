# Save format

What Iron Shadows writes when you save, what it guarantees, and how to change it without breaking
saves people already have. Implemented in `include/IronGang/Persistence/SaveGame.hpp` and
`src/Persistence/SaveGame.cpp`; plan entries `plan/plan_29-save-games-checkpoints-profiles-and-migration.md`
`IG-29-001`-`IG-29-004`, `IG-29-022`, `IG-29-023`, `IG-29-025`.

## Files

A save at `<path>` owns three names:

| File | Purpose |
| --- | --- |
| `<path>` | The save. |
| `<path>.bak` | The previous save, rotated here by the next write. One generation, no deeper history. |
| `<path>.tmp` | A write in progress. Never read; replaced by the next write if one is left behind. |

## Format

Line-based `key=value`, UTF-8, `\n` endings. The first two lines are the header:

```text
format=iron-gang-save-v2
checksum=6f1b0c3a9d2e4b57
mission_state_id=drive_to_warehouse
player_position=1,1.7,2
player_yaw=0.25
vehicle_position=3,0.65,4
vehicle_yaw=-0.5
vehicle_speed=8
player_driving=1
district_id=1
mission_var.cargo_secured=bool:true
mission_var.deliveries_made=int:0
mission_checkpoint_state_id=drive_to_warehouse
mission_checkpoint_var.cargo_secured=bool:true
```

| Key | Meaning |
| --- | --- |
| `format` | `iron-gang-save-v<N>`. Must be the first line. |
| `checksum` | FNV-1a 64-bit hex over every byte after this line. Must be the second line (version 2+). |
| `mission_state_id` | The mission's current state id. |
| `player_position`, `player_yaw` | Where the player stands and faces. |
| `vehicle_position`, `vehicle_yaw`, `vehicle_speed` | The player's sedan. |
| `player_driving` | `1` while driving it. |
| `district_id` | Which district is loaded. Absent means `WarehouseBlock`. |
| `mission_var.<name>` | One mission variable as `<type>:<value>` — see `docs/mission-scripting.md`. |
| `mission_checkpoint_state_id` | The mission's last checkpoint, absent when none was reached. |
| `mission_checkpoint_var.<name>` | The variables recorded with that checkpoint. |

One line per mission variable is deliberate: the reader splits at the **first** `=`, and a variable
name is an identifier, so a string value may contain anything except a newline.

## Guarantees

**Atomic replace.** `Write()` builds the whole document, writes it to `<path>.tmp`, rotates any
existing save to `<path>.bak`, then renames the temporary into place. A crash, a full disk, or a
failed rename never leaves a half-written file at `<path>` — the previous save survives either
where it was or as the backup. A leftover `<path>.tmp` from an interrupted run is ignored and
replaced by the next write.

**One rolling backup.** The previous save is always one generation behind. If the primary file is
missing, corrupt, or unreadable, `Read()` falls back to it and reports that through
`SaveReadDiagnostics::usedBackup` and `primaryError`; the game logs the reason and shows
"Loaded backup save" rather than recovering silently.

**Corruption detection.** The checksum covers everything after the header. A truncated, torn, or
hand-edited file fails to load rather than loading partially — a half-applied save is worse than
no save. This is FNV-1a, not a cryptographic hash: it detects damage, it does not prevent editing.
Anyone who wants to edit a save by hand can, as long as they recompute the checksum, or drop the
file back to `format=iron-gang-save-v1` (which has no checksum).

**Version refusal.** A file claiming a version newer than this build understands is refused with a
message saying so, instead of being read for the fields that happen to be recognisable.

**Named failures.** A missing required field is reported by name (`Save file is missing
"player_position"`), not as whatever exception the parse happened to throw.

## Versions

| Version | Written by | Notes |
| --- | --- | --- |
| 1 | Builds before 2026-08-25 | No checksum. `mission_state` is an int index into a fixed five-state enum. |
| 2 | Current | Adds the checksum line; the mission state is the state's own id. |

Reading a version-1 file works and reports `SaveReadDiagnostics::formatVersion == 1`; the next save
is written in the current format. `mission_state` is mapped to `mission_state_id` through the exact
table the deleted enum had, and an out-of-range index is refused rather than clamped.

## Changing the format

1. **Additive field, no reader change needed?** Add it to `SaveSnapshot`, write it in `Write()`,
   and read it with a default when absent — the way `district_id` and the mission variables were
   added. No version bump: an older build ignores the unknown key, and a newer build defaults it.
2. **Anything an older build would misread** (a changed meaning, a removed field a newer build
   needs, a re-encoded value): bump `kCurrentSaveFormatVersion`, keep reading the previous version,
   and convert on read. The `mission_state` → `mission_state_id` migration is the worked example.
3. Add a case to `TestSaveFormatRobustness` (`tests/CoreTests.cpp`) for the version you just left
   behind, so the migration keeps being exercised after the build that wrote it is gone.
4. Update the table above and `docs/validation.md`.

A migration **registry** with per-version functions (`IG-29-026`) does not exist yet; with two
versions the conversion is a branch in `ReadOne()`. Add the registry when a third version arrives.

## Not implemented yet

Profiles and save slots (`IG-29-006`/`032`), autosave scheduling (`IG-29-010`/`036`), settings kept
separate from campaign data (`IG-29-005`), thumbnails (`IG-29-013`), a CLI inspection tool
(`IG-29-019`), and per-district persistence of world entities (`IG-29-008`/`034`). The world half
of a mission checkpoint — where the player and vehicle stood — is also not saved, so a retry
straight after a load restarts the mission instead (`IG-29-009`/`029`).
