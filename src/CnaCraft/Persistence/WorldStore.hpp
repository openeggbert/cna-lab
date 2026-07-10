#pragma once

#include <string>

struct sqlite3;

namespace CnaCraft::Worlds {
class World;
class SignStore;
struct Sign;
}

namespace CnaCraft::Persistence {

// SQLite-backed delta persistence (CRAFT_PARITY.md §4.1/§4.2, plan.md §12.1
// item 15 — user decision 2026-07-10: add SQLite). Ports Craft's real
// delta-storage model (src/db.c: a `block(p,q,x,y,z,w)` table, unique-
// indexed on (p,q,x,y,z), storing only *edited* blocks over the
// regenerated procedural terrain — the world itself is never dumped to
// disk, only player edits are). Schema now matches Craft's exactly,
// including the `p,q` chunk-address columns (plan.md §12.1 item 19, user
// decision 2026-07-10) — added once the world itself gained per-chunk-
// column streaming; `LoadInto`/`LoadSignsInto` below are still whole-
// database loads for now (a column-scoped `LoadColumnInto`/
// `LoadColumnSignsInto` pair, using the new `WHERE p=? AND q=?` query the
// schema now supports efficiently, is a tracked follow-up once
// `CnaCraftGame` actually streams chunks on demand instead of loading the
// whole world at startup).
//
// Deliberately NOT a faithful port of Craft's async worker-thread/
// COMMIT_INTERVAL batching (src/db.c db_worker_run) — cna-craft has no
// equivalent background-thread architecture, and the edit rate here is low
// (a handful of player-driven single-block edits per session, not Craft's
// potentially-networked-multiplayer scale), so SaveEdits() is called
// synchronously right after each edit instead (see CnaCraftGame::Update).
// This is a deliberate simplification for this prototype's scale, not a
// missing feature.
//
// Kept out of the engine-agnostic CnaCraftWorlds library (unlike
// Worlds/World.h's plain edit-tracking, which has zero dependencies) since
// it depends on SQLite — CnaCraftWorlds' whole point is staying
// dependency-free and buildable/testable with no GPU/display/extra libs.
class WorldStore {
public:
    // Opens (creating if needed) the SQLite database at `path`. If opening
    // or schema creation fails, this becomes a harmless no-op store (all
    // methods below silently do nothing) rather than crashing the game —
    // persistence is a nice-to-have, not something that should take the
    // whole game down if the filesystem is read-only or the file is locked.
    explicit WorldStore(const std::string& path);
    ~WorldStore();

    WorldStore(const WorldStore&) = delete;
    WorldStore& operator=(const WorldStore&) = delete;

    // True if the database opened and its schema is ready. Callers don't
    // need to check this before calling LoadInto/SaveEdits (both are
    // no-ops when false), but CnaCraftGame logs a warning once at startup
    // if this is false, so a broken save path isn't silently invisible.
    [[nodiscard]] bool IsOpen() const { return db_ != nullptr; }

    // Loads every persisted edit and applies it to `world` via the plain
    // (non-recording) World::SetBlock — loaded edits must not be
    // re-recorded as new pending edits, or every load would immediately
    // re-queue a full-world save.
    void LoadInto(Worlds::World& world);

    // Writes every edit in `world.RecordedEdits()` (INSERT OR REPLACE, so
    // only the latest value per coordinate survives — matches Craft's own
    // unique-index-per-coordinate delta model) and then clears them.
    // No-op if there are no recorded edits, so calling this every frame
    // (as CnaCraftGame does, gated on there being at least one new edit)
    // is cheap.
    void SaveEdits(Worlds::World& world);

    // Signs (CRAFT_PARITY.md §4.3) — a separate `sign(x,y,z,face,text)`
    // table (Craft's real `sign(p,q,x,y,z,face,text)` schema, src/db.c,
    // same p,q-column drop as `block` above). Incremental per-row writes,
    // mirroring Craft's own `db_insert_sign`/`db_delete_sign`/
    // `db_delete_signs` (src/db.c) rather than a bulk delete-and-reinsert
    // of the whole list — an earlier version of this code did the latter
    // for simplicity; changed to track Craft's real approach more closely.
    void LoadSignsInto(Worlds::SignStore& store);

    // Insert-or-replace a single sign, keyed on (x,y,z,face) — matches
    // Craft's own `db_insert_sign` (called from `set_sign` with non-empty
    // text). Call after CnaCraftGame's typing flow submits a non-empty
    // sign.
    void UpsertSign(const Worlds::Sign& sign);

    // Delete a single sign at (x,y,z,face) — matches Craft's own
    // `db_delete_sign` (called from `unset_sign_face`, e.g. submitting an
    // empty-text sign onto an existing one).
    void DeleteSign(int x, int y, int z, int face);

    // Delete every sign at (x,y,z) regardless of face — matches Craft's
    // own `db_delete_signs` (called from `unset_sign`, itself called by
    // `_set_block` whenever a block is broken: a sign can't outlive the
    // block face it was attached to).
    void DeleteSignsAt(int x, int y, int z);

private:
    sqlite3* db_ = nullptr;
};

}
