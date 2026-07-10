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

    std::printf("\n");
    if (checksFailed == 0) {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::printf("%d check(s) FAILED.\n", checksFailed);
    return 1;
}
