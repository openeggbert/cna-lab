// No-op WorldStore for platforms without SQLite (the Emscripten/web build --
// plan.md §12.1 item 18 follow-up "webovy emscripten build"). Compiled
// INSTEAD OF WorldStore.cpp (never alongside it) when CMake selects the
// stub; WorldStore.hpp only forward-declares sqlite3, so the header itself
// needs no SQLite to compile. Every method matches the real store's
// graceful-degradation contract exactly: IsOpen() is false (the game logs
// its one startup warning), loads produce nothing, saves discard -- the
// same behavior the real store already has when its database fails to
// open, so the game code needs zero platform branches. The one semantic
// that MUST be preserved: SaveEdits still clears the world's recorded
// edits, or they would accumulate unboundedly frame over frame.
#include "WorldStore.hpp"

#include "../Worlds/Sign.hpp"
#include "../Worlds/World.hpp"

namespace CnaCraft::Persistence {

WorldStore::WorldStore(const std::string&) {}

WorldStore::~WorldStore() = default;

void WorldStore::LoadInto(Worlds::World&) {}

void WorldStore::LoadColumnInto(Worlds::World&, int, int) {}

std::vector<Worlds::BlockEdit> WorldStore::LoadColumnEdits(int, int) { return {}; }

void WorldStore::SaveEdits(Worlds::World& world) { world.ClearRecordedEdits(); }

void WorldStore::LoadSignsInto(Worlds::SignStore&) {}

void WorldStore::LoadColumnSignsInto(Worlds::SignStore&, int, int) {}

void WorldStore::UpsertSign(const Worlds::Sign&) {}

void WorldStore::DeleteSign(int, int, int, int) {}

void WorldStore::DeleteSignsAt(int, int, int) {}

void WorldStore::SavePlayerState(float, float, float, float, float) {}

bool WorldStore::LoadPlayerState(float&, float&, float&, float&, float&) { return false; }

void WorldStore::UpsertLight(int, int, int, bool) {}

void WorldStore::LoadColumnLightsInto(Worlds::World&, int, int) {}

void WorldStore::UpsertBlock(int, int, int, int) {}

std::vector<Worlds::BlockEdit> WorldStore::LoadColumnEditsSince(int, int, std::int64_t, std::int64_t&) {
    return {};
}

std::vector<WorldStore::LightRow> WorldStore::LoadColumnLightRows(int, int) { return {}; }

std::vector<Worlds::Sign> WorldStore::LoadColumnSignRows(int, int) { return {}; }

void WorldStore::SetColumnKey(int, int, std::int64_t) {}

std::int64_t WorldStore::GetColumnKey(int, int) { return 0; }

}
