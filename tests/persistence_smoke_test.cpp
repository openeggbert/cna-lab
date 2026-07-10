// Exercises Persistence::WorldStore against a real SQLite file on disk --
// no CNA/GPU/display needed (WorldStore only depends on SQLite + Worlds/),
// but it does need real filesystem + SQLite access, unlike
// worlds_smoke_test.cpp, so it's a separate target/ctest (CRAFT_PARITY.md
// §4.1/§4.2, plan.md §12.1 item 15).
//
// Verifying this via the real graphical CnaCraft executable turned out to
// be impractical in this sandbox: synthetic mouse clicks into a
// relative-mouse-mode SDL window are unreliable here (the same class of
// flakiness already documented elsewhere in this project's history for
// mouse-look), so a directed unit test against WorldStore/World directly is
// both more reliable AND becomes permanent regression coverage, unlike a
// one-off manual screenshot check would have been.

#include <cstdio>
#include <filesystem>
#include <string>

#include <sqlite3.h>

#include "CnaCraft/Persistence/WorldStore.hpp"
#include "CnaCraft/Worlds/BlockType.hpp"
#include "CnaCraft/Worlds/Sign.hpp"
#include "CnaCraft/Worlds/World.hpp"

using namespace CnaCraft::Worlds;
using namespace CnaCraft::Persistence;

namespace {
int checksRun = 0;
int checksFailed = 0;

void Check(bool condition, const char* description) {
    ++checksRun;
    if (condition) {
        std::printf("ok:   %s\n", description);
    } else {
        std::printf("FAIL: %s\n", description);
        ++checksFailed;
    }
}
}

int main() {
    const std::string dbPath = "persistence_smoke_test.db";
    std::filesystem::remove(dbPath); // start from a clean slate

    // Round 1: generate a world, make some player-driven edits, save them.
    {
        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        Check(store.IsOpen(), "WorldStore opens/creates a fresh database file");

        world.SetBlockAndRecordEdit(5, 5, 5, BlockType::Glass);
        world.SetBlockAndRecordEdit(6, 6, 6, BlockType::Brick);
        Check(world.RecordedEdits().size() == 2, "two edits are recorded before saving");

        store.SaveEdits(world);
        Check(world.RecordedEdits().empty(), "SaveEdits clears the recorded-edits list after writing");
    }

    // Round 2: fresh World (same seed -> same deterministic base terrain) +
    // fresh WorldStore against the same file -- confirm the edits round-trip.
    {
        World beforeLoad;
        beforeLoad.Generate(42);
        Check(beforeLoad.GetBlock(5, 5, 5) != BlockType::Glass,
              "before loading, the edited cell still has its generated (non-Glass) value");

        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        Check(store.IsOpen(), "WorldStore re-opens the existing database file");
        store.LoadInto(world);

        Check(world.GetBlock(5, 5, 5) == BlockType::Glass, "a loaded edit correctly overwrites the generated block (Glass)");
        Check(world.GetBlock(6, 6, 6) == BlockType::Brick, "a loaded edit correctly overwrites the generated block (Brick)");
        Check(world.RecordedEdits().empty(), "LoadInto does not re-record loaded edits as new pending edits");
    }

    // Round 3: overwrite the same coordinate again -- confirm only the
    // latest value survives (matches Craft's own unique-index-per-
    // coordinate delta model, "INSERT OR REPLACE").
    {
        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        world.SetBlockAndRecordEdit(5, 5, 5, BlockType::Stone);
        store.SaveEdits(world);
    }
    {
        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        store.LoadInto(world);
        Check(world.GetBlock(5, 5, 5) == BlockType::Stone,
              "re-saving the same coordinate replaces the old value instead of duplicating it");
    }

    // Round 4: breaking a block back to Air is itself a real, persistable
    // edit (not a special case) -- confirm Air round-trips too.
    {
        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        world.SetBlockAndRecordEdit(6, 6, 6, BlockType::Air);
        store.SaveEdits(world);
    }
    {
        World world;
        world.Generate(42);
        WorldStore store(dbPath);
        store.LoadInto(world);
        Check(world.GetBlock(6, 6, 6) == BlockType::Air, "a block broken back to Air correctly round-trips as Air, not as missing/ignored");
    }

    // Round 5: signs (CRAFT_PARITY.md §4.3) -- a separate table from block
    // edits, incremental per-row writes (UpsertSign/DeleteSign/
    // DeleteSignsAt) mirroring Craft's own db_insert_sign/db_delete_sign/
    // db_delete_signs (src/db.c), not a bulk resave of the whole list.
    {
        WorldStore store(dbPath);
        store.UpsertSign(Sign{10, 11, 12, 4, "Hello, Craft!"});
        store.UpsertSign(Sign{20, 21, 22, 0, "Second sign"});
    }
    {
        SignStore signs;
        Check(signs.Signs().empty(), "a fresh SignStore has no signs before loading");
        WorldStore store(dbPath);
        store.LoadSignsInto(signs);
        Check(signs.Signs().size() == 2, "both saved signs are loaded back");
        bool foundFirst = false, foundSecond = false;
        for (const auto& sign : signs.Signs()) {
            if (sign.x == 10 && sign.y == 11 && sign.z == 12 && sign.face == 4 && sign.text == "Hello, Craft!") {
                foundFirst = true;
            }
            if (sign.x == 20 && sign.y == 21 && sign.z == 22 && sign.face == 0 && sign.text == "Second sign") {
                foundSecond = true;
            }
        }
        Check(foundFirst, "the first sign's coordinates, face, and text all round-trip correctly");
        Check(foundSecond, "the second sign's coordinates, face, and text all round-trip correctly");
    }
    // Re-upserting the same (x,y,z,face) replaces the text instead of
    // duplicating the row (matches the unique index on (x,y,z,face)).
    {
        WorldStore store(dbPath);
        store.UpsertSign(Sign{10, 11, 12, 4, "Updated text"});
    }
    {
        SignStore signs;
        WorldStore store(dbPath);
        store.LoadSignsInto(signs);
        Check(signs.Signs().size() == 2, "upserting an existing sign's coordinates+face replaces it, not duplicates it");
        bool foundUpdated = false;
        for (const auto& sign : signs.Signs()) {
            if (sign.x == 10 && sign.y == 11 && sign.z == 12 && sign.face == 4 && sign.text == "Updated text") {
                foundUpdated = true;
            }
        }
        Check(foundUpdated, "the upserted sign's text was actually replaced");
    }
    // DeleteSign removes exactly one (x,y,z,face) row, leaving the other
    // sign at the same (x,y,z) but a different face untouched.
    {
        WorldStore store(dbPath);
        store.UpsertSign(Sign{10, 11, 12, 1, "Same cell, other face"});
        store.DeleteSign(10, 11, 12, 4);
    }
    {
        SignStore signs;
        WorldStore store(dbPath);
        store.LoadSignsInto(signs);
        Check(signs.Signs().size() == 2, "DeleteSign removes only the targeted (x,y,z,face) row");
        bool faceFourGone = true, faceOneSurvives = false;
        for (const auto& sign : signs.Signs()) {
            if (sign.x == 10 && sign.y == 11 && sign.z == 12 && sign.face == 4) faceFourGone = false;
            if (sign.x == 10 && sign.y == 11 && sign.z == 12 && sign.face == 1) faceOneSurvives = true;
        }
        Check(faceFourGone, "the deleted face's sign is actually gone");
        Check(faceOneSurvives, "a different face at the same (x,y,z) survives DeleteSign");
    }
    // DeleteSignsAt removes every sign at (x,y,z) regardless of face --
    // matches Craft's own unset_sign(), called when the underlying block is
    // broken (a sign can't outlive the block face it was attached to).
    {
        WorldStore store(dbPath);
        store.DeleteSignsAt(10, 11, 12);
    }
    {
        SignStore signs;
        WorldStore store(dbPath);
        store.LoadSignsInto(signs);
        Check(signs.Signs().size() == 1, "DeleteSignsAt removes every face at that coordinate, leaving unrelated signs alone");
        Check(signs.Signs()[0].x == 20 && signs.Signs()[0].y == 21 && signs.Signs()[0].z == 22,
              "the surviving sign is the unrelated one at a different coordinate");
    }

    // Round 7 (plan.md §12.1 item 19): column-scoped loading (LoadColumnInto/
    // LoadColumnSignsInto) only pulls in the requested chunk-column's data --
    // proves the new `WHERE p=? AND q=?` queries actually filter correctly,
    // not just that the write side happens to no-op for an unloaded column.
    {
        WorldStore store(dbPath);
        // (5,5,5) is chunk-column (0,0); (20,5,20) is chunk-column (1,1).
        World seedWorld;
        seedWorld.AllocateColumn(0, 0);
        seedWorld.AllocateColumn(1, 1);
        seedWorld.SetBlockAndRecordEdit(5, 5, 5, BlockType::Cobblestone);
        seedWorld.SetBlockAndRecordEdit(20, 5, 20, BlockType::Snow);
        store.SaveEdits(seedWorld);

        // (50,6,50) is chunk-column (3,3) -- deliberately far from every
        // coordinate any earlier round in this shared dbPath ever touched
        // (round 5/6's signs at (10,11,12)/(20,21,22) fall in columns
        // (0,0)/(1,1), so reusing either of those columns here would pick
        // up that pre-existing leftover sign data too and make this round's
        // assertions depend on exactly what earlier rounds left behind).
        store.UpsertSign(Sign{5, 6, 5, 0, "Column zero sign"});
        store.UpsertSign(Sign{50, 6, 50, 0, "Column three-three sign"});
    }
    {
        World world;
        world.AllocateColumn(0, 0);
        world.AllocateColumn(1, 1); // allocated too, so a wrongly-cross-loaded edit would actually be visible
        WorldStore store(dbPath);
        store.LoadColumnInto(world, 0, 0);
        Check(world.GetBlock(5, 5, 5) == BlockType::Cobblestone,
              "LoadColumnInto loads the requested column's edit");
        Check(world.GetBlock(20, 5, 20) == BlockType::Air,
              "LoadColumnInto does not load a different column's edit, despite that column being allocated");

        SignStore signs;
        store.LoadColumnSignsInto(signs, 0, 0);
        Check(signs.Signs().size() == 1 && signs.Signs()[0].text == "Column zero sign",
              "LoadColumnSignsInto loads only the requested column's sign");

        store.LoadColumnSignsInto(signs, 3, 3);
        Check(signs.Signs().size() == 2,
              "a second LoadColumnSignsInto call for a different column accumulates, not replaces");
    }

    std::filesystem::remove(dbPath);

    // Round 8 (plan.md §12.1 item 19): a pre-existing world.db from before
    // the p,q chunk-address columns were added must fail loudly once at
    // open time, not silently accept edits that then fail per-INSERT --
    // CREATE INDEX on the new schema's (p,q,x,y,z)/​(p,q) columns fails
    // against an old table that lacks them, which the existing
    // schema-creation error path already treats as "harmless no-op store".
    {
        const std::string oldSchemaPath = "persistence_smoke_test_old_schema.db";
        std::filesystem::remove(oldSchemaPath);
        sqlite3* raw = nullptr;
        sqlite3_open(oldSchemaPath.c_str(), &raw);
        sqlite3_exec(raw,
                     "CREATE TABLE block (x INTEGER NOT NULL, y INTEGER NOT NULL, z INTEGER NOT NULL, "
                     "w INTEGER NOT NULL);"
                     "CREATE UNIQUE INDEX block_xyz_idx ON block (x, y, z);"
                     "CREATE TABLE sign (x INTEGER NOT NULL, y INTEGER NOT NULL, z INTEGER NOT NULL, "
                     "face INTEGER NOT NULL, text TEXT NOT NULL);"
                     "CREATE UNIQUE INDEX sign_xyzface_idx ON sign (x, y, z, face);",
                     nullptr, nullptr, nullptr);
        sqlite3_close(raw);

        WorldStore store(oldSchemaPath);
        Check(!store.IsOpen(), "opening a pre-p,q-migration world.db fails loudly (IsOpen() false), not silently");
        std::filesystem::remove(oldSchemaPath);
    }

    // Round 9 (plan.md §12.1 item 19 phase 5): the actual streaming
    // lifecycle a real play session exercises -- generate a column, edit and
    // save a block, unload the column (as CnaCraftGame::UnloadColumn does
    // when the player wanders away), then reload just that column from
    // scratch and confirm the edit survived. Round 7 above proves
    // column-scoped loading filters correctly; this proves the specific
    // World::UnloadColumn -> WorldStore::LoadColumnInto round trip that
    // streaming actually performs is not lossy.
    {
        const std::string streamPath = "persistence_smoke_test_stream.db";
        std::filesystem::remove(streamPath);

        World world;
        world.GenerateColumn(7, 7, 42); // column (7,7): fresh, untouched by earlier rounds
        WorldStore store(streamPath);
        Check(store.IsOpen(), "WorldStore opens a fresh database for the streaming round trip");

        const int x = 7 * 16 + 3, z = 7 * 16 + 3;
        world.SetBlockAndRecordEdit(x, 20, z, BlockType::Cobblestone);
        store.SaveEdits(world);

        world.UnloadColumn(7, 7);
        Check(!world.IsColumnLoaded(7, 7), "the column is actually unloaded before reloading it");

        world.GenerateColumn(7, 7, 42); // simulates streaming back in: regenerate terrain fresh...
        Check(world.GetBlock(x, 20, z) != BlockType::Cobblestone,
              "sanity check: freshly-regenerated terrain does not itself contain the edit");
        store.LoadColumnInto(world, 7, 7); // ...then re-apply this column's persisted edits.
        Check(world.GetBlock(x, 20, z) == BlockType::Cobblestone,
              "an edit survives a real unload -> regenerate -> reload cycle for its own column");

        std::filesystem::remove(streamPath);
    }

    // Round 10 (plan.md §12.1 item 17 follow-up): player-position
    // persistence -- matches Craft's own single-row state(x,y,z,rx,ry)
    // table (db_save_state/db_load_state, src/db.c).
    {
        const std::string statePath = "persistence_smoke_test_state.db";
        std::filesystem::remove(statePath);

        float x = 0, y = 0, z = 0, rx = 0, ry = 0;
        {
            WorldStore store(statePath);
            Check(!store.LoadPlayerState(x, y, z, rx, ry),
                  "LoadPlayerState returns false when nothing has ever been saved");
        }
        {
            WorldStore store(statePath);
            store.SavePlayerState(12.5f, 20.25f, -7.5f, 1.25f, -0.5f);
        }
        {
            WorldStore store(statePath);
            const bool loaded = store.LoadPlayerState(x, y, z, rx, ry);
            Check(loaded, "LoadPlayerState returns true once a state has been saved");
            Check(x == 12.5f && y == 20.25f && z == -7.5f && rx == 1.25f && ry == -0.5f,
                  "the loaded position/look direction exactly matches what was saved");
        }
        // Re-saving replaces the single row (Craft's own DELETE-then-INSERT,
        // not an accumulating history) -- checks the raw row count directly
        // via sqlite3, not just LoadPlayerState's return, since a stray
        // leftover row could otherwise go unnoticed (SELECT with no
        // ORDER BY/LIMIT just returns whichever row SQLite finds first).
        {
            WorldStore store(statePath);
            store.SavePlayerState(1.0f, 2.0f, 3.0f, 0.0f, 0.0f);
            (void)store.LoadPlayerState(x, y, z, rx, ry);
            Check(x == 1.0f && y == 2.0f && z == 3.0f, "re-saving replaces the previous state, not accumulates it");
        }
        {
            sqlite3* raw = nullptr;
            sqlite3_open(statePath.c_str(), &raw);
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM state;", -1, &stmt, nullptr);
            sqlite3_step(stmt);
            const int rowCount = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            sqlite3_close(raw);
            Check(rowCount == 1, "the state table never has more than one row, even after multiple saves");
        }

        std::filesystem::remove(statePath);
    }

    // Round 11 (plan.md §12.1 item 17 follow-up, "light toggle"): matches
    // Craft's own light(p,q,x,y,z,w) table (db_insert_light/db_load_lights,
    // src/db.c) -- same shape/index as `block`'s own table, same
    // column-scoped load query as signs.
    {
        const std::string lightPath = "persistence_smoke_test_light.db";
        std::filesystem::remove(lightPath);

        {
            WorldStore store(lightPath);
            Check(store.IsOpen(), "WorldStore opens a fresh database for the light round trip");
            // (5,5,5) is chunk-column (0,0); (20,5,20) is chunk-column (1,1).
            store.UpsertLight(5, 5, 5, true);
            store.UpsertLight(20, 5, 20, true);
        }
        {
            World world;
            world.AllocateColumn(0, 0);
            world.AllocateColumn(1, 1); // allocated too, so a wrongly-cross-loaded light would actually be visible
            WorldStore store(lightPath);
            store.LoadColumnLightsInto(world, 0, 0);
            Check(world.IsLightSource(5, 5, 5), "LoadColumnLightsInto loads the requested column's light");
            Check(!world.IsLightSource(20, 5, 20),
                  "LoadColumnLightsInto does not load a different column's light, despite that column being "
                  "allocated");
        }
        // Toggling off (w=0) round-trips too, matching Craft's own
        // INSERT OR REPLACE semantics (the row survives with w=0, it isn't
        // deleted) -- confirmed both via LoadColumnLightsInto's own result
        // and a direct row-count check, same rigor as Round 10's re-save
        // check above.
        {
            WorldStore store(lightPath);
            store.UpsertLight(5, 5, 5, false);
        }
        {
            World world;
            world.AllocateColumn(0, 0);
            WorldStore store(lightPath);
            store.LoadColumnLightsInto(world, 0, 0);
            Check(!world.IsLightSource(5, 5, 5), "toggling a light off round-trips through persistence too");
        }
        {
            sqlite3* raw = nullptr;
            sqlite3_open(lightPath.c_str(), &raw);
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM light;", -1, &stmt, nullptr);
            sqlite3_step(stmt);
            const int rowCount = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            sqlite3_close(raw);
            Check(rowCount == 2,
                  "toggling a light off replaces its row (w=0), it doesn't delete it -- still 2 rows total");
        }

        std::filesystem::remove(lightPath);
    }

    std::printf("\n");
    if (checksFailed == 0) {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::printf("%d check(s) FAILED.\n", checksFailed);
    return 1;
}
