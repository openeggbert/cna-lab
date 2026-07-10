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

    std::filesystem::remove(dbPath);

    std::printf("\n");
    if (checksFailed == 0) {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::printf("%d check(s) FAILED.\n", checksFailed);
    return 1;
}
