#pragma once

#include <string>

struct sqlite3;

namespace CnaCraft::Worlds {
class World;
class SignStore;
}

namespace CnaCraft::Persistence {

// SQLite-backed delta persistence (CRAFT_PARITY.md §4.1/§4.2, plan.md §12.1
// item 15 — user decision 2026-07-10: add SQLite). Ports Craft's real
// delta-storage model (src/db.c: a `block(p,q,x,y,z,w)` table, unique-
// indexed on the coordinate, storing only *edited* blocks over the
// regenerated procedural terrain — the world itself is never dumped to
// disk, only player edits are). Adapted schema: plain `block(x,y,z,w)`,
// since cna-craft's World has no per-chunk (p,q) addressing the way
// Craft's streamed-chunk model does — this project's fixed-size world uses
// absolute world-space coordinates directly.
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
    // same p,q-column drop as `block`). Unlike SaveEdits' incremental
    // delta approach, sign counts are expected to stay small (a handful to
    // low hundreds, not thousands), so SaveSigns does a full
    // delete-and-reinsert of the whole list every call rather than tracking
    // an incremental dirty set — simpler, and cheap enough at this scale.
    void LoadSignsInto(Worlds::SignStore& store);
    void SaveSigns(const Worlds::SignStore& store);

private:
    sqlite3* db_ = nullptr;
};

}
