#include "CnaCraftGame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <utility>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/TextInputEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include "Render/TextureAtlas.hpp"
#include "Worlds/ChunkMesher.hpp"
#include "Worlds/DayNightCycle.hpp"
#include "Worlds/NoiseGenerator.hpp"
#include "Worlds/VoxelRaycast.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;

namespace CnaCraft {

namespace {
constexpr float kPiOver4 = 0.78539816339744830962f;
// Zoom (plan.md §11.4): narrows the FOV while Left Shift is held, mirroring
// Craft's own hold-to-zoom behavior (`g->fov = ... ? 15 : 65` in main.c) —
// not a toggle. Craft's absolute FOV numbers don't carry over 1:1 since our
// base FOV (kPiOver4, 45 degrees) already differs from Craft's default (65
// degrees) for unrelated reasons; kZoomFov keeps the same "much narrower"
// relationship instead.
constexpr float kZoomFov = 0.26179938779914943654f; // 15 degrees, in radians
constexpr float kMouseSensitivity = 0.0025f;
// Matches Craft's own hardcoded hit-test distance exactly (main.c) --
// changed from an earlier 6.0f approximation per user decision 2026-07-10
// to minimize differences from Craft.
constexpr float kMaxReach = 8.0f;
constexpr std::uint32_t kWorldSeed = 1337;
// Orthographic toggle (plan.md §11.4): also hold-to-activate in Craft
// (`g->ortho = ... ? 64 : 0`), not a toggle despite the backlog's wording.
constexpr float kOrthoViewHeight = 24.0f; // world units (blocks) of vertical view extent

// Chunk streaming radii (plan.md §12.1 item 19), in chunk-grid units (each
// chunk = Worlds::CHUNK_SIZE=16 blocks) -- deliberately NOT copying Craft's
// literal 10/10/14 (its own CHUNK_SIZE=32 doubles the absolute-blocks
// math); this project's own starting point, the same rough order of
// magnitude as the old fixed 128x128 world (8 chunks across) but now
// unbounded. render == create (no separate render-distance check in
// Draw() -- frustum culling alone decides visibility among loaded chunks,
// same as before streaming existed; nothing loaded is ever farther than
// the delete radius anyway). delete > create (hysteresis margin, matches
// Craft's own create/delete gap, so a column right at the boundary
// doesn't load/unload every frame as the player moves back and forth
// across it). These are only the *initial* values now (plan.md §12.1 item
// 17) -- CnaCraftGame::radii_ (Worlds::CommandRadii) is the actual mutable
// source of truth every streaming/fog computation reads, seeded from these
// two constants in Initialize() and mutated at runtime by the `/view`
// command.
constexpr int kCreateRadius = 6;
constexpr int kDeleteRadius = 9;
// Budget cap on new column loads per frame, even while this whole pass
// stays synchronous (a later phase backgrounds it) -- bounds worst-case
// frame cost from a fast fly-mode dash into unloaded territory. The
// initial spawn force-load in Initialize() ignores this cap (loads
// everything within radii_.createRadius up front, all at once, matching
// Craft's own startup force_chunks call, so the player never sees an empty
// world even briefly).
constexpr int kMaxColumnLoadsPerFrame = 2;

// Distance fog (CRAFT_PARITY.md §5.2): Craft's own block_vertex.glsl fades
// toward a sampled sky-texture color by camera distance, with
// `fog_distance = render_radius * CHUNK_SIZE` (320 units in real Craft) --
// tying fog to the render radius isn't just cosmetic in Craft, it hides
// the pop-in at the edge of the loaded/streamed region. This project has
// no sky dome texture yet (separate backlog item), so FogColor is set to
// the same flat sky clear color already computed below each frame instead
// — same "fade toward the sky" intent, simpler source. Verified NOT
// blocked by the shader-backend limitations in missing.md: BasicEffect's
// fog is standard XNA surface (FogEnabled/FogColor/FogStart/FogEnd), not a
// custom ShaderEffect — CNA has real fog support for the lit+textured
// BasicEffect path on EASYGL, VULKAN, and BGFX alike (see
// ../cna/examples/{easygl,vulkan,bgfx}_basiceffect_lit_fog_test.cpp).
// Fog start/end are computed every frame in Draw() from radii_.createRadius
// (plan.md §12.1 item 17), not a compile-time constant -- so a runtime
// `/view` change immediately extends/shrinks the fade distance to match,
// same as Craft's own fog_distance tracking g->render_radius live.
}

CnaCraftGame::CnaCraftGame() : graphics_(this) {
    static constexpr int kFps = 60;
    setTargetElapsedTimeProperty(System::TimeSpan::FromTicks(static_cast<long>(10000000L / kFps)));
}

const std::string& CnaCraftGame::GetTypeName() const {
    static const std::string name = "CnaCraftGame";
    return name;
}

void CnaCraftGame::Initialize() {
    Game::Initialize();

    auto& device = getGraphicsDeviceProperty();
    device.SetDepthTestEnabled(true);

    Mouse::setIsRelativeMouseModeEXTProperty(true);

    // radii_ (plan.md §12.1 item 17) starts at these compile-time defaults;
    // `/view` mutates it at runtime from here on -- everything below reads
    // radii_, never the constants directly, so a later /view immediately
    // takes effect everywhere (streaming, fog).
    radii_ = Worlds::CommandRadii{kCreateRadius, kDeleteRadius};

    // World persistence (CRAFT_PARITY.md §4.1/§4.2): opened before any
    // column loads, so LoadColumn (called by the spawn force-load below)
    // can immediately overlay persisted edits/signs on top of each
    // freshly generated column, same order as before streaming existed.
    // `world.db` in the working directory, matching Craft's own simple
    // single-default-file approach (`DB_PATH`, src/db.c) rather than a
    // save-slot system.
    worldStore_ = std::make_unique<Persistence::WorldStore>("world.db");
    if (!worldStore_->IsOpen()) {
        std::printf("WorldStore: could not open world.db -- edits will not be saved this session\n");
        std::fflush(stdout);
    }

    // Text input for signs (CRAFT_PARITY.md §4.3) and commands (plan.md
    // §12.1 item 17) — a single persistent handler, gated on typingMode_ so
    // keystrokes are ignored whenever the player isn't actively typing
    // either. Backtick/`/` themselves must NOT be captured here
    // (StartTextInput() isn't active yet when either is pressed) — those
    // are handled as edge-triggered Keys::OemTilde/OemQuestion checks in
    // Update() instead. Cap matches Craft's own MAX_TEXT_LENGTH (main.c) --
    // one shared buffer-length limit for both sign text and commands, same
    // as Craft's single g->typing_buffer.
    TextInputEXT::TextInput = [this](SharpRuntime::charcs c) {
        if (typingMode_ == TypingMode::None) return;
        if (c < 0x20 || c > 0x7e) return;  // printable ASCII only
        constexpr std::size_t kMaxTypingTextLength = 256;
        if (typingBuffer_.size() < kMaxTypingTextLength) {
            typingBuffer_.push_back(static_cast<char>(c));
        }
    };

    // Player-position persistence (plan.md §12.1 item 17 follow-up, user
    // decision 2026-07-10): try loading a saved eye position/look direction
    // before deciding the spawn force-load region below, so a returning
    // player's force-load happens around where they actually were, not
    // always world-origin. `loadedX/Z` default to the same world-origin
    // spawn used when nothing was ever saved (see the fallback branch
    // further down) so the force-load loop below is correct either way.
    float loadedX = 0.5f, loadedY = 0.0f, loadedZ = 0.5f, loadedYaw = 0.0f, loadedPitch = 0.0f;
    const bool hasSavedPlayerState = worldStore_->LoadPlayerState(loadedX, loadedY, loadedZ, loadedYaw, loadedPitch);
    const int spawnColumnCx = Worlds::ChunkCoordOf(static_cast<int>(std::floor(loadedX)));
    const int spawnColumnCz = Worlds::ChunkCoordOf(static_cast<int>(std::floor(loadedZ)));

    // Force-generate+mesh every column within radii_.createRadius of the
    // spawn column synchronously, all at once, ignoring the per-frame
    // budget cap UpdateStreaming uses later — mirrors Craft's own startup
    // force_chunks call, so the player never spawns into (or briefly sees)
    // an ungenerated void. Spawn column is world-origin (0,0) by default
    // (CRAFT_PARITY.md §1.2/§3.1, plan.md §12.1 item 19) — matches Craft's
    // own actual behavior exactly, there's no "center of a bounded world"
    // once the world streams — or the saved column above if one exists.
    for (int dz = -radii_.createRadius; dz <= radii_.createRadius; ++dz) {
        for (int dx = -radii_.createRadius; dx <= radii_.createRadius; ++dx) {
            LoadColumnSynchronously(spawnColumnCx + dx, spawnColumnCz + dz);
        }
    }
    RebuildDirtyChunks();

    effect_ = std::make_unique<BasicEffect>(device);
    effect_->VertexColorEnabled = false;
    effect_->setTextureEnabledProperty(true);
    effect_->EnableDefaultLighting();
    // EnableDefaultLighting()'s 3-light rig leaves faces angled away from all
    // three lights essentially unlit (visibly black on flat terrain faces);
    // floor it with a moderate ambient term so no face ever goes pure black,
    // matching Craft's block_fragment.glsl ("ambient = value*0.3+0.2", never
    // zero — see THIRD_PARTY_NOTICES.md). Set here as the first frame's
    // starting value; Draw() recomputes it every frame from the day/night
    // cycle (plan.md §11.3) using the same formula with a live `daylight`.
    effect_->setAmbientLightColorProperty(Vector3(0.5f, 0.5f, 0.5f));

    atlasTexture_ = std::make_unique<Texture2D>(Render::BuildProceduralAtlas(device));
    effect_->setTextureProperty(atlasTexture_.get());

    // Bug fix: spawning at an *integer* coordinate puts the player's 0.6-wide
    // hitbox (kPlayerHalfWidth=0.3) exactly on the boundary between two
    // block columns, straddling both equally. With Simplex noise's steeper
    // local height changes (§11.1) a neighboring column can be much taller
    // right next to spawn, permanently wedging the player against it --
    // unable to reach its own column's true floor or move in any direction.
    // Spawning at block *center* (integer + 0.5) keeps the hitbox fully
    // inside its own column instead. Spawn column is now world-origin
    // (0,0) (see the force-load loop above), not "the center of the fixed
    // world" -- there's no such thing once the world streams.
    //
    // Player-position persistence (plan.md §12.1 item 17 follow-up): if a
    // saved state exists, spawn there instead -- matches Craft's own real
    // `db_load_state` behavior exactly (loaded position overrides the
    // default spawn), converting Craft's real EYE-position storage
    // (loadedY) to PlayerController's own feet-based storage via
    // kEyeHeight. Falls back to the block-center/height+2 default spawn if
    // nothing was ever saved, matching Craft's own
    // `if (!loaded) s->y = highest_block(...) + 2`.
    if (hasSavedPlayerState) {
        player_ = std::make_unique<Worlds::PlayerController>(
            Core::Vec3f{loadedX, loadedY - Worlds::PlayerController::kEyeHeight, loadedZ}, loadedYaw, loadedPitch);
    } else {
        const float spawnX = 0.5f;
        const float spawnZ = 0.5f;
        const int spawnHeight = Worlds::NoiseGenerator::Height(kWorldSeed, 0, 0);
        player_ = std::make_unique<Worlds::PlayerController>(
            Core::Vec3f{spawnX, static_cast<float>(spawnHeight + 2), spawnZ});
    }

    hud_ = std::make_unique<Render::Hud>(device);
    hotbarSlotNames_.reserve(Worlds::Hotbar::kSlots.size());
    for (Worlds::BlockType type : Worlds::Hotbar::kSlots) {
        hotbarSlotNames_.emplace_back(Worlds::GetBlockName(type));
    }
    hud_->RebuildHotbar(device, hotbarSlotNames_.data(), static_cast<int>(hotbarSlotNames_.size()),
                        hotbar_.SelectedIndex(), player_->IsFlying());

    signBillboard_.Rebuild(device, signStore_.Signs());
    signsNeedRebuild_ = false;
}

void CnaCraftGame::RebuildDirtyChunks() {
    auto& device = getGraphicsDeviceProperty();
    // Iterates chunkRenderers_ (not World::LoadedColumns()) since every
    // loaded column always has a matching chunkRenderers_ entry (LoadColumn
    // creates both together) -- this is the exact key both containers
    // share, replacing the old flat-vector "index i means the same chunk in
    // both containers" assumption with an explicit lookup.
    for (auto& [key, renderers] : chunkRenderers_) {
        int cx = 0, cz = 0;
        Worlds::World::UnpackColumnKey(key, cx, cz);
        for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
            Worlds::Chunk& chunk = world_.ChunkAt(cx, cy, cz);
            if (chunk.IsDirty()) {
                renderers[static_cast<std::size_t>(cy)]->Rebuild(device, world_);
                chunk.ClearDirty();
            }
        }
    }
}

void CnaCraftGame::RecordMark(int x, int y, int z, Worlds::BlockType type) {
    // Mirrors Craft's own record_block (main.c): mark1_ becomes the
    // previous mark0_, mark0_ becomes this new one -- a simple 2-slot
    // history of the last two edited positions.
    mark1_ = mark0_;
    mark0_ = Worlds::BlockMark{x, y, z, type};
}

void CnaCraftGame::MarkNeighborColumnsDirty(int cx, int cz) {
    // Whenever a column loads or unloads, its face-adjacent neighbors'
    // shared boundary faces need re-meshing: a neighbor meshed before this
    // column existed culled that shared face against "phantom Air" (or, on
    // unload, needs to show it again now that the neighbor is gone) --
    // see LoadColumn/UnloadColumn.
    static constexpr int kOffsetsX[4] = {-1, 1, 0, 0};
    static constexpr int kOffsetsZ[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        const int ncx = cx + kOffsetsX[i], ncz = cz + kOffsetsZ[i];
        if (!world_.IsColumnLoaded(ncx, ncz)) continue;
        for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
            world_.ChunkAt(ncx, cy, ncz).MarkDirty();
        }
    }
}

void CnaCraftGame::LoadColumnSynchronously(int cx, int cz) {
    world_.GenerateColumn(cx, cz, kWorldSeed);
    worldStore_->LoadColumnInto(world_, cx, cz);
    worldStore_->LoadColumnSignsInto(signStore_, cx, cz);
    signsNeedRebuild_ = true; // this column may have contributed persisted signs

    auto& renderers = chunkRenderers_[Worlds::World::PackColumnKey(cx, cz)];
    for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
        renderers[static_cast<std::size_t>(cy)] = std::make_unique<Render::ChunkRenderer>(
            cx * Worlds::CHUNK_SIZE, cy * Worlds::CHUNK_SIZE, cz * Worlds::CHUNK_SIZE);
    }
    // Each freshly-created Chunk already starts dirty (Chunk's own default),
    // so this column's own chunks don't need marking here -- only the
    // already-loaded neighbors, whose meshes predate this column.
    MarkNeighborColumnsDirty(cx, cz);
}

void CnaCraftGame::UnloadColumn(int cx, int cz) {
    world_.UnloadColumn(cx, cz);
    chunkRenderers_.erase(Worlds::World::PackColumnKey(cx, cz));
    MarkNeighborColumnsDirty(cx, cz);
    // A sign can't survive after the World data it was attached to is gone
    // (plan.md §12.1 item 19) -- otherwise it'd either accumulate
    // unboundedly in memory over a long streamed-world session, or desync
    // from a later re-generated column that doesn't know about it until
    // WorldStore's own LoadColumnSignsInto reloads it.
    if (signStore_.RemoveAllInColumn(cx, cz)) signsNeedRebuild_ = true;
}

bool CnaCraftGame::IsColumnGenerationInFlight(int cx, int cz) const {
    for (const auto& job : inFlightGenerationJobs_) {
        if (job.cx == cx && job.cz == cz) return true;
    }
    return false;
}

void CnaCraftGame::DispatchColumnGeneration(int cx, int cz) {
    // Main thread, synchronous, cheap (see this method's doc comment in
    // CnaCraftGame.hpp for why this stays synchronous rather than also
    // being backgrounded).
    const auto edits = worldStore_->LoadColumnEdits(cx, cz);
    worldStore_->LoadColumnSignsInto(signStore_, cx, cz);
    signsNeedRebuild_ = true; // this column may have contributed persisted signs

    const std::uint32_t seed = kWorldSeed;
    // TaskT has no default constructor (only TaskT(std::function<T()>)),
    // so the job struct must be aggregate-initialized with an
    // already-constructed task, not default-constructed then assigned.
    auto task = System::Threading::Tasks::TaskT<std::array<Worlds::Chunk, Worlds::WORLD_CHUNKS_Y>>::Run(
        [cx, cz, seed, edits]() -> std::array<Worlds::Chunk, Worlds::WORLD_CHUNKS_Y> {
            // Runs on a background thread -- captures only plain data
            // (ints, a std::vector<BlockEdit>), builds its own throwaway
            // World with no connection to the live one, touches no
            // SQLite/GraphicsDevice at all. Reuses GenerateColumn/SetBlock
            // unmodified; only the target (a scratch World, not `this`
            // game's live world_) differs from the synchronous path.
            Worlds::World scratch;
            scratch.GenerateColumn(cx, cz, seed);
            for (const Worlds::BlockEdit& edit : edits) {
                scratch.SetBlock(edit.x, edit.y, edit.z, edit.type);
            }
            return scratch.CopyColumn(cx, cz);
        });
    inFlightGenerationJobs_.push_back(InFlightGenerationJob{cx, cz, std::move(task)});
}

void CnaCraftGame::PollGenerationJobs() {
    // The apply cap THROTTLES how many completed results get merged into
    // world_ this frame -- it must never cause a completed job to be
    // dropped outright, only deferred to a later frame's call (real bug,
    // found via visual verification: an earlier version of this function
    // discarded a completed-but-over-cap job unconditionally, reasoning
    // that UpdateStreaming would just re-dispatch it later -- true for
    // generation only because IsColumnLoaded/IsColumnGenerationInFlight
    // both naturally stay false until real work happens, so nothing is
    // lost; the equivalent mistake in PollMeshJobs was NOT self-healing,
    // since a mesh job's dirty flag is already cleared at dispatch time --
    // see PollMeshJobs' own note). Both functions now use the same
    // defer-don't-discard shape for consistency, even though generation
    // technically could get away with the simpler discard-and-redo.
    constexpr int kMaxAppliedPerFrame = 2;
    int appliedThisFrame = 0;
    std::size_t i = 0;
    while (i < inFlightGenerationJobs_.size()) {
        if (appliedThisFrame >= kMaxAppliedPerFrame) break;
        InFlightGenerationJob& job = inFlightGenerationJobs_[i];
        if (!job.task.getIsCompletedProperty()) {
            ++i;
            continue;
        }
        // Completed -- safe to erase this entry (the future's
        // blocking-destructor hazard only applies to an INCOMPLETE
        // future; a finished one destructs instantly).
        const int cx = job.cx, cz = job.cz;
        if (!world_.IsColumnLoaded(cx, cz)) {
            const auto chunks = job.task.getResultProperty();
            world_.AdoptColumnCopy(cx, cz, chunks);

            auto& renderers = chunkRenderers_[Worlds::World::PackColumnKey(cx, cz)];
            for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
                renderers[static_cast<std::size_t>(cy)] = std::make_unique<Render::ChunkRenderer>(
                    cx * Worlds::CHUNK_SIZE, cy * Worlds::CHUNK_SIZE, cz * Worlds::CHUNK_SIZE);
            }
            MarkNeighborColumnsDirty(cx, cz);
            ++appliedThisFrame;
        }
        // Else: already loaded some other way -- genuinely stale, drop it.
        inFlightGenerationJobs_.erase(inFlightGenerationJobs_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void CnaCraftGame::DispatchMeshingForDirtyChunks() {
    // Dispatch cap, same reasoning as DispatchColumnGeneration's
    // kMaxColumnLoadsPerFrame: sharp-runtime's ThreadPool/TaskT spawn a
    // genuine new OS thread per Task::Run call rather than drawing from a
    // real bounded pool, so uncapped dispatch is a real hazard, not just a
    // theoretical one -- one column loading can dirty up to 20 chunks at
    // once (its own 4 Y-levels + up to 4 neighbor columns' 4 Y-levels
    // each), and multiple columns can load in the same frame. A dirty
    // chunk that doesn't get dispatched this frame stays dirty (its flag
    // is only cleared right when a task is actually dispatched for it) and
    // is picked up by a later frame's call instead.
    constexpr int kMaxMeshDispatchesPerFrame = 8;
    int dispatchesThisFrame = 0;
    for (auto& [key, renderers] : chunkRenderers_) {
        if (dispatchesThisFrame >= kMaxMeshDispatchesPerFrame) break;
        int cx = 0, cz = 0;
        Worlds::World::UnpackColumnKey(key, cx, cz);
        for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
            if (dispatchesThisFrame >= kMaxMeshDispatchesPerFrame) break;
            Worlds::Chunk& chunk = world_.ChunkAt(cx, cy, cz);
            if (!chunk.IsDirty()) continue;
            chunk.ClearDirty(); // cleared on dispatch, not completion -- matches Craft's own timing
            ++dispatchesThisFrame;

            // Snapshot the target chunk + its up-to-6 face-adjacent
            // neighbors as plain copyable (cx,cy,cz,Chunk) tuples -- the
            // background task reconstructs its own throwaway World from
            // this, never touching the live world_.
            struct ChunkSnapshot {
                int cx = 0, cy = 0, cz = 0;
                Worlds::Chunk chunk;
            };
            std::vector<ChunkSnapshot> snapshots;
            static constexpr int kOffsetsX[7] = {0, -1, 1, 0, 0, 0, 0};
            static constexpr int kOffsetsY[7] = {0, 0, 0, -1, 1, 0, 0};
            static constexpr int kOffsetsZ[7] = {0, 0, 0, 0, 0, -1, 1};
            for (int n = 0; n < 7; ++n) {
                const int ncx = cx + kOffsetsX[n], ncy = cy + kOffsetsY[n], ncz = cz + kOffsetsZ[n];
                if (const Worlds::Chunk* neighbor = world_.TryChunkAt(ncx, ncy, ncz)) {
                    snapshots.push_back(ChunkSnapshot{ncx, ncy, ncz, *neighbor});
                }
            }

            const int originX = cx * Worlds::CHUNK_SIZE, originY = cy * Worlds::CHUNK_SIZE,
                      originZ = cz * Worlds::CHUNK_SIZE;
            auto meshTask = System::Threading::Tasks::TaskT<Worlds::ChunkMeshData>::Run(
                [snapshots, originX, originY, originZ]() -> Worlds::ChunkMeshData {
                    Worlds::World scratch;
                    for (const auto& s : snapshots) scratch.InstallChunkCopy(s.cx, s.cy, s.cz, s.chunk);
                    return Worlds::ChunkMesher::Build(scratch, originX, originY, originZ);
                });
            inFlightMeshJobs_.push_back(InFlightMeshJob{cx, cy, cz, std::move(meshTask)});
        }
    }
}

void CnaCraftGame::PollMeshJobs() {
    // The apply cap THROTTLES uploads this frame -- it must never cause a
    // completed job to be dropped outright, only deferred to a later
    // frame's call. Real bug, found via visual verification (persistent
    // holes in distant terrain that never filled in even after waiting):
    // an earlier version discarded a completed-but-over-cap job
    // unconditionally, reasoning it was safe because "a still-dirty chunk
    // gets re-dispatched later" -- false here, since
    // DispatchMeshingForDirtyChunks already cleared this chunk's dirty
    // flag back when the job was DISPATCHED, not when it completes, so a
    // discarded result left the chunk permanently un-meshed (its
    // ChunkRenderer never got its first real upload) with nothing left to
    // ever mark it dirty again.
    constexpr int kMaxAppliedPerFrame = 4; // meshing is cheaper to apply (upload-only) than generation
    auto& device = getGraphicsDeviceProperty();
    int appliedThisFrame = 0;
    std::size_t i = 0;
    while (i < inFlightMeshJobs_.size()) {
        if (appliedThisFrame >= kMaxAppliedPerFrame) break;
        InFlightMeshJob& job = inFlightMeshJobs_[i];
        if (!job.task.getIsCompletedProperty()) {
            ++i;
            continue;
        }
        const auto it = chunkRenderers_.find(Worlds::World::PackColumnKey(job.cx, job.cz));
        if (it != chunkRenderers_.end()) {
            const Worlds::ChunkMeshData mesh = job.task.getResultProperty();
            it->second[static_cast<std::size_t>(job.cy)]->ApplyMesh(device, mesh);
            ++appliedThisFrame;
        }
        // Else: column unloaded before meshing finished -- genuinely
        // stale, drop it (matches Craft's own equivalent race).
        inFlightMeshJobs_.erase(inFlightMeshJobs_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void CnaCraftGame::UpdateStreaming(int playerCx, int playerCz) {
    int dispatchesThisFrame = 0;
    for (int dz = -radii_.createRadius; dz <= radii_.createRadius && dispatchesThisFrame < kMaxColumnLoadsPerFrame;
         ++dz) {
        for (int dx = -radii_.createRadius; dx <= radii_.createRadius && dispatchesThisFrame < kMaxColumnLoadsPerFrame;
             ++dx) {
            const int cx = playerCx + dx, cz = playerCz + dz;
            if (world_.IsColumnLoaded(cx, cz) || IsColumnGenerationInFlight(cx, cz)) continue;
            DispatchColumnGeneration(cx, cz);
            ++dispatchesThisFrame;
        }
    }

    // Collect first, then unload -- erasing from chunkRenderers_ while
    // iterating it would invalidate the iterator.
    std::vector<std::pair<int, int>> toUnload;
    for (const auto& [key, renderers] : chunkRenderers_) {
        (void)renderers;
        int ucx = 0, ucz = 0;
        Worlds::World::UnpackColumnKey(key, ucx, ucz);
        if (std::max(std::abs(ucx - playerCx), std::abs(ucz - playerCz)) > radii_.deleteRadius) {
            toUnload.emplace_back(ucx, ucz);
        }
    }
    for (const auto& [ucx, ucz] : toUnload) UnloadColumn(ucx, ucz);
}

void CnaCraftGame::Update(GameTime& gameTime) {
    Game::Update(gameTime);

    if (smokeFramesLeft_ > 0) {
        if (--smokeFramesLeft_ == 0) {
            Exit();
            return;
        }
    }

    const float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

    const auto kb = Keyboard::GetState();
    const bool escapeDown = kb.IsKeyDown(Keys::Escape);

    // Typing state machine (CRAFT_PARITY.md §4.3, plan.md §12.1 item 17) --
    // handled before any WASD/look/click input is read, so typing fully
    // suspends normal gameplay input, matching Craft's own handle_movement
    // gating all movement/look polling on !g->typing (src/main.c). Backtick
    // opens Sign typing; `/` (Keys::OemQuestion, CNA's XNA-standard name for
    // the main `/`/`?` key) opens Command typing with the buffer
    // pre-seeded as "/" (matches Craft's own g->typing_buffer[0]='/'
    // exactly). Both are mutually exclusive with each other and with an
    // already-open typing session.
    const bool backtickDown = kb.IsKeyDown(Keys::OemTilde);
    if (backtickDown && !backtickWasDown_ && typingMode_ == TypingMode::None) {
        typingMode_ = TypingMode::Sign;
        typingBuffer_.clear();
        TextInputEXT::StartTextInput();
    }
    backtickWasDown_ = backtickDown;

    const bool slashDown = kb.IsKeyDown(Keys::OemQuestion);
    if (slashDown && !slashWasDown_ && typingMode_ == TypingMode::None) {
        typingMode_ = TypingMode::Command;
        typingBuffer_ = "/";
        TextInputEXT::StartTextInput();
    }
    slashWasDown_ = slashDown;

    if (typingMode_ != TypingMode::None) {
        const bool backspaceDown = kb.IsKeyDown(Keys::Back);
        if (backspaceDown && !backspaceWasDown_ && !typingBuffer_.empty()) {
            typingBuffer_.pop_back();
        }
        backspaceWasDown_ = backspaceDown;

        if (escapeDown && !escapeWasDown_) {
            typingMode_ = TypingMode::None;
            typingBuffer_.clear();
            TextInputEXT::StopTextInput();
        }
        escapeWasDown_ = escapeDown;

        const bool enterDown = kb.IsKeyDown(Keys::Enter);
        if (enterDown && !enterWasDown_ && typingMode_ == TypingMode::Sign) {
            // Re-raycast fresh at submit time rather than reusing a raycast
            // from when typing started -- matches Craft's own Enter-time
            // hit_test in on_key's g->typing branch (src/main.c).
            const auto signHit = Worlds::VoxelRaycast::Cast(
                world_, player_->EyePosition(), player_->LookDirection(), kMaxReach);
            // Craft's own Enter handler calls set_sign() unconditionally
            // once there's a hit (src/main.c:2214-2219), even with empty
            // text -- set_sign() itself routes an empty string to
            // unset_sign_face() (deleting any existing sign at that face)
            // rather than storing a blank one. Mirrored here: PlaceSign
            // already has that same empty-text-deletes behavior, so this
            // isn't gated on typingBuffer_ being non-empty.
            if (signHit) {
                int face = -1;
                if (signHit->nx > 0) face = 0;
                else if (signHit->nx < 0) face = 1;
                else if (signHit->ny > 0) face = 2;
                else if (signHit->ny < 0) face = 3;
                else if (signHit->nz > 0) face = 4;
                else if (signHit->nz < 0) face = 5;
                if (face >= 0) {
                    signStore_.PlaceSign(signHit->x, signHit->y, signHit->z, face, typingBuffer_);
                    // Incremental writes matching Craft's own
                    // db_insert_sign/db_delete_sign (src/db.c), not a bulk
                    // resave of the whole sign list.
                    if (typingBuffer_.empty()) {
                        worldStore_->DeleteSign(signHit->x, signHit->y, signHit->z, face);
                    } else {
                        worldStore_->UpsertSign(Worlds::Sign{signHit->x, signHit->y, signHit->z, face, typingBuffer_});
                    }
                    signsNeedRebuild_ = true;
                }
            }
            typingMode_ = TypingMode::None;
            typingBuffer_.clear();
            TextInputEXT::StopTextInput();
        } else if (enterDown && !enterWasDown_ && typingMode_ == TypingMode::Command) {
            // Worlds::ExecuteCommand (plan.md §12.1 item 17, Craft's real
            // parse_command) is a pure function of its inputs -- CnaCraftGame
            // just feeds it the current marks/clipboard/radii and surfaces
            // the returned feedback message the same dual way Craft's own
            // add_message does (console + the new on-screen message log).
            const std::string message =
                Worlds::ExecuteCommand(world_, typingBuffer_, mark0_, mark1_, clipboard_, radii_);
            if (!message.empty()) {
                std::printf("%s\n", message.c_str());
                std::fflush(stdout);
                hud_->PushMessage(getGraphicsDeviceProperty(), message);
            }
            typingMode_ = TypingMode::None;
            typingBuffer_.clear();
            TextInputEXT::StopTextInput();
        }
        enterWasDown_ = enterDown;

        hud_->SetTyping(getGraphicsDeviceProperty(), typingMode_ != TypingMode::None,
                         typingMode_ == TypingMode::Command ? "Command" : "Sign", typingBuffer_);

        // Gravity/physics still integrates while typing (frozen player-driven
        // input, real dt), matching Craft's own substep loop running even
        // while g->typing is set.
        player_->Update(world_, Worlds::PlayerInput{}, dt);
        if (signsNeedRebuild_) {
            signBillboard_.Rebuild(getGraphicsDeviceProperty(), signStore_.Signs());
            signsNeedRebuild_ = false;
        }
        // No new column dispatch while typing (the player's (cx,cz) can't
        // change -- WASD is suspended), but already-in-flight background
        // jobs keep draining every frame, same "chunk-rebuild keeps
        // running while frozen" precedent as before backgrounding existed --
        // this also covers any block edits a just-submitted command made
        // (e.g. /cube), which already marked their chunks dirty the same
        // way a player-driven edit would.
        PollGenerationJobs();
        DispatchMeshingForDirtyChunks();
        PollMeshJobs();
        return;
    }

    // Cursor capture (CRAFT_PARITY.md §1.2): matches Craft's own on_key --
    // Escape releases the mouse cursor rather than quitting (see
    // CnaCraftGame.hpp's cursorCaptured_ comment for why this doesn't
    // strand the player with no way to quit).
    if (escapeDown && !escapeWasDown_ && cursorCaptured_) {
        cursorCaptured_ = false;
        Mouse::setIsRelativeMouseModeEXTProperty(false);
    }
    escapeWasDown_ = escapeDown;

    const auto mouse = Mouse::GetState();

    Worlds::PlayerInput input;
    if (kb.IsKeyDown(Keys::W)) input.moveForward += 1.0f;
    if (kb.IsKeyDown(Keys::S)) input.moveForward -= 1.0f;
    if (kb.IsKeyDown(Keys::D)) input.moveRight += 1.0f;
    if (kb.IsKeyDown(Keys::A)) input.moveRight -= 1.0f;
    // Space, in both modes: jump (game mode) / force full ascend (fly mode
    // -- PlayerController now flies pitch-coupled like Craft's own
    // get_motion_vector, with no dedicated descend key at all, matching
    // Craft exactly per user decision 2026-07-10; see PlayerController.hpp).
    input.jumpPressed = kb.IsKeyDown(Keys::Space);
    // Mouse-look is gated on cursor capture (CRAFT_PARITY.md §1.2): matches
    // Craft's own handle_mouse_input, which only applies mouse deltas while
    // `exclusive` (cursor captured) -- moving the OS cursor around while
    // released must not spin the camera.
    if (cursorCaptured_) {
        input.lookDeltaYaw = static_cast<float>(mouse.getXProperty()) * kMouseSensitivity;
        input.lookDeltaPitch = -static_cast<float>(mouse.getYProperty()) * kMouseSensitivity;
    }
    // Arrow keys as a keyboard alternative to mouse-look (some players don't
    // want to use the mouse for turning; also more reliable to test than
    // relative mouse motion). Additive with the mouse deltas above. NOT
    // gated on cursor capture -- matches Craft's own arrow-key look, which
    // lives inside handle_movement's `if (!g->typing)` block, entirely
    // separate from handle_mouse_input's `exclusive` gate. Rate matches
    // Craft's own literal `m = dt * 1.0` exactly (main.c handle_movement) --
    // changed from an earlier 1.6f approximation per user decision
    // 2026-07-10 to minimize differences from Craft.
    const float rotSpeed = 1.0f * dt;
    if (kb.IsKeyDown(Keys::Left)) input.lookDeltaYaw -= rotSpeed;
    if (kb.IsKeyDown(Keys::Right)) input.lookDeltaYaw += rotSpeed;
    if (kb.IsKeyDown(Keys::Up)) input.lookDeltaPitch += rotSpeed;
    if (kb.IsKeyDown(Keys::Down)) input.lookDeltaPitch -= rotSpeed;

    const auto rebuildHud = [this]() {
        hud_->RebuildHotbar(getGraphicsDeviceProperty(), hotbarSlotNames_.data(),
                            static_cast<int>(hotbarSlotNames_.size()), hotbar_.SelectedIndex(),
                            player_->IsFlying());
    };

    // Screenshot capture (plan.md §11.7): F12 is not a Craft key (its README's
    // "Screenshot" section is just a marketing image, not a documented
    // hotkey) but it's a common convention and CNA already exposes the
    // needed GraphicsDevice::GetBackBufferData/Texture2D::SaveAsPng — the
    // actual capture happens in Draw() once the frame is fully rendered.
    const bool f12Down = kb.IsKeyDown(Keys::F12);
    if (f12Down && !f12WasDown_) {
        screenshotPending_ = true;
    }
    f12WasDown_ = f12Down;

    const bool tabDown = kb.IsKeyDown(Keys::Tab);
    if (tabDown && !tabWasDown_) {
        player_->ToggleFlying();
        std::printf("Flying: %s\n", player_->IsFlying() ? "on" : "off");
        std::fflush(stdout);
        rebuildHud();
    }
    tabWasDown_ = tabDown;

    player_->Update(world_, input, dt);

    // Chunk streaming (plan.md §12.1 item 19): load/unload columns around
    // the player's new position every frame. Not gated on movement having
    // actually happened -- IsColumnLoaded checks make repeated calls at the
    // same position cheap no-ops, same "safe to call every frame" pattern
    // as WorldStore::SaveEdits.
    {
        const Core::Vec3f eye = player_->EyePosition();
        UpdateStreaming(Worlds::ChunkCoordOf(static_cast<int>(std::floor(eye.x))),
                         Worlds::ChunkCoordOf(static_cast<int>(std::floor(eye.z))));
    }

    // Raycast once per frame -- reused for break/place, the middle-click
    // eyedropper, and the visible targeted-block outline (CRAFT_PARITY.md
    // §2.4), instead of a separate cast per click as before.
    const auto hit = Worlds::VoxelRaycast::Cast(world_, player_->EyePosition(), player_->LookDirection(), kMaxReach);
    hasTargetedBlock_ = hit.has_value();
    if (hit) {
        targetedBlockX_ = hit->x;
        targetedBlockY_ = hit->y;
        targetedBlockZ_ = hit->z;
    }

    const int previousHotbarIndex = hotbar_.SelectedIndex();
    // Craft's on_key (CRAFT_PARITY.md §2.1) maps keys 1-9 to slots 0-8 and
    // key 0 to slot 9 (a 10th direct-key slot) -- kMaxNumberKeySlots stays
    // capped at 9 (Hotbar.hpp), key 0 is handled separately as slot 10.
    const int numberKeySlots = std::min(Worlds::Hotbar::kMaxNumberKeySlots, Worlds::Hotbar::SlotCount());
    for (int slot = 1; slot <= numberKeySlots; ++slot) {
        if (kb.IsKeyDown(static_cast<Keys>(static_cast<int>(Keys::D1) + slot - 1))) {
            hotbar_.SelectSlot(slot);
        }
    }
    if (Worlds::Hotbar::SlotCount() >= 10 && kb.IsKeyDown(Keys::D0)) {
        hotbar_.SelectSlot(10);
    }
    const bool eDown = kb.IsKeyDown(Keys::E);
    if (eDown && !eKeyWasDown_) {
        hotbar_.CycleNext();
    }
    eKeyWasDown_ = eDown;
    // R = reverse-cycle, mirrors Craft's E/R pair (CRAFT_PARITY.md §2.1).
    const bool rDown = kb.IsKeyDown(Keys::R);
    if (rDown && !rKeyWasDown_) {
        hotbar_.CyclePrev();
    }
    rKeyWasDown_ = rDown;
    // Scroll wheel also cycles the hotbar, matching Craft's on_scroll
    // (CRAFT_PARITY.md §2.1). CNA's ScrollWheelValue is cumulative (XNA
    // convention), so compare against the previous frame's value; the first
    // frame just captures a baseline (no synthetic cycle on startup).
    const int scrollWheelValue = mouse.getScrollWheelValueProperty();
    if (scrollWheelInitialized_) {
        const int scrollDelta = scrollWheelValue - previousScrollWheelValue_;
        if (scrollDelta > 0) {
            hotbar_.CycleNext();
        } else if (scrollDelta < 0) {
            hotbar_.CyclePrev();
        }
    }
    scrollWheelInitialized_ = true;
    previousScrollWheelValue_ = scrollWheelValue;

    const bool leftDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;
    const bool rightDown = mouse.getRightButtonProperty() == ButtonState::Pressed;
    const bool middleDown = mouse.getMiddleButtonProperty() == ButtonState::Pressed;

    if (hotbar_.SelectedIndex() != previousHotbarIndex) {
        std::printf("Selected block: %s\n", Worlds::GetBlockName(hotbar_.Selected()));
        std::fflush(stdout);
        rebuildHud();
    }

    if (!cursorCaptured_) {
        // Cursor released (CRAFT_PARITY.md §1.2): matches Craft's own
        // on_mouse_button exactly -- every mouse button is inert while
        // released, except left-click, which just re-captures the cursor
        // instead of breaking/placing/eyedropping on that same click.
        if (leftDown && !leftClickWasDown_) {
            cursorCaptured_ = true;
            Mouse::setIsRelativeMouseModeEXTProperty(true);
        }
    } else {
        // Middle-click "eyedropper" (CRAFT_PARITY.md §2.7): selects the
        // hotbar slot matching the targeted block's type, ports Craft's
        // real on_middle_click. Silently does nothing if the target isn't
        // in the placeable roster (e.g. Bedrock), matching Craft's own
        // behavior.
        if (hit && middleDown && !middleClickWasDown_) {
            hotbar_.SelectByBlockType(world_.GetBlock(hit->x, hit->y, hit->z));
        }

        // Place the selected block adjacent to `hit`'s face, rejecting a
        // placement that would overlap the player's own body
        // (CRAFT_PARITY.md §2.6, ports Craft's on_right_click
        // `!player_intersects_block` guard). Uses SetBlockAndRecordEdit
        // (CRAFT_PARITY.md §4.1/§4.2), not plain SetBlock, so this
        // player-driven change gets persisted. RecordMark (plan.md §12.1
        // item 17) mirrors Craft's own record_block call right after
        // set_block in on_right_click -- every successful placement updates
        // mark0_/mark1_, the anchor points /cube, /sphere, etc. read.
        const auto tryPlaceBlock = [&]() {
            const int px = hit->x + hit->nx, py = hit->y + hit->ny, pz = hit->z + hit->nz;
            if (!player_->IntersectsBlock(px, py, pz)) {
                world_.SetBlockAndRecordEdit(px, py, pz, hotbar_.Selected());
                RecordMark(px, py, pz, hotbar_.Selected());
            }
        };

        // CRAFT_PARITY.md §2.7: Ctrl+left-click acts as right-click (place)
        // instead of break, matching Craft's real on_mouse_button
        // (`control ? on_right_click() : on_left_click()` for the left
        // button).
        const bool ctrlDown = kb.IsKeyDown(Keys::LeftControl) || kb.IsKeyDown(Keys::RightControl);

        if (hit && leftDown && !leftClickWasDown_) {
            if (ctrlDown) {
                tryPlaceBlock();
            } else if (world_.IsBreakable(hit->x, hit->y, hit->z)) {
                // CRAFT_PARITY.md §2.5: only break blocks World::IsBreakable
                // allows (ports Craft's `is_destructable` guard in
                // on_left_click) — Bedrock, a cna-craft-only
                // "world-boundary, not meant to be placed" block, could
                // previously be mined away with no protection at all.
                world_.SetBlockAndRecordEdit(hit->x, hit->y, hit->z, Worlds::BlockType::Air);
                // RecordMark (plan.md §12.1 item 17) mirrors Craft's own
                // record_block call in on_left_click, with w=0/Air -- a
                // broken block is just as valid a mark as a placed one.
                RecordMark(hit->x, hit->y, hit->z, Worlds::BlockType::Air);
                // A sign can't outlive the block face it was attached to --
                // matches Craft's own _set_block calling unset_sign()
                // whenever a block is set to type 0 (src/main.c).
                if (signStore_.RemoveAllAt(hit->x, hit->y, hit->z)) {
                    worldStore_->DeleteSignsAt(hit->x, hit->y, hit->z);
                    signsNeedRebuild_ = true;
                }
            }
        }
        if (hit && rightDown && !rightClickWasDown_) {
            tryPlaceBlock();
        }
    }

    // Save any new edits right away (CRAFT_PARITY.md §4.1/§4.2) -- no-op
    // when there's nothing new to save (WorldStore::SaveEdits checks
    // RecordedEdits().empty() first), so calling this every frame is cheap.
    // Synchronous, not batched/async like Craft's own worker thread (see
    // WorldStore.hpp) -- acceptable at this prototype's low single-player
    // edit rate.
    worldStore_->SaveEdits(world_);

    // Player-position persistence (plan.md §12.1 item 17 follow-up) --
    // saved every frame (cheap single-row delete+insert), not just at clean
    // exit like Craft's own single `db_save_state` call, so a crashed or
    // killed process still resumes close to the last frame's position
    // instead of losing it entirely -- see WorldStore.hpp's doc comment.
    // Converts PlayerController's own feet-based storage to Craft's real
    // eye-based storage via kEyeHeight (the inverse of the Initialize()
    // load-time conversion).
    {
        const Core::Vec3f eyePos = player_->EyePosition();
        worldStore_->SavePlayerState(eyePos.x, eyePos.y, eyePos.z, player_->Yaw(), player_->Pitch());
    }
    leftClickWasDown_ = leftDown;
    rightClickWasDown_ = rightDown;
    middleClickWasDown_ = middleDown;

    if (signsNeedRebuild_) {
        signBillboard_.Rebuild(getGraphicsDeviceProperty(), signStore_.Signs());
        signsNeedRebuild_ = false;
    }

    // Background generation/meshing pipeline (plan.md §12.1 item 19 phase
    // 4): apply any completed column-generation results first (so a
    // freshly-loaded column's chunks are already in world_ before this
    // same frame's dirty scan below), then dispatch meshing for anything
    // dirty (including chunks the just-applied generation results marked
    // dirty), then apply any completed mesh results.
    PollGenerationJobs();
    DispatchMeshingForDirtyChunks();
    PollMeshJobs();
}

void CnaCraftGame::Draw(const GameTime& gameTime) {
    auto& device = getGraphicsDeviceProperty();

    // Day/night cycle (plan.md §11.3): a daylight value in [0, 1] driven by
    // GameTime::TotalGameTime, matching Craft's own get_daylight()/
    // time_of_day() curve shape (src/main.c) — dawn/dusk sigmoid transitions
    // bracketing long full-day/full-night plateaus. Drives BasicEffect's
    // ambient term with the same `value*0.3+0.2` formula as
    // block_fragment.glsl, and tints the (still-flat, no sky dome yet —
    // that's a separate backlog item) clear color between night and day.
    const float daylight = Worlds::ComputeDaylight(
        static_cast<float>(gameTime.getTotalGameTimeProperty().getTotalSecondsProperty()));
    const float ambient = daylight * 0.3f + 0.2f;
    effect_->setAmbientLightColorProperty(Vector3(ambient, ambient, ambient));

    const auto lerpChannel = [daylight](int night, int day) {
        return static_cast<int>(static_cast<float>(night) + static_cast<float>(day - night) * daylight);
    };
    const int skyR = lerpChannel(12, 135), skyG = lerpChannel(14, 196), skyB = lerpChannel(36, 235);
    // Zenith tint for the sky dome (CRAFT_PARITY.md §5.3) — a deeper blue
    // than the horizon color at both day and night, same lerp function.
    const int zenithR = lerpChannel(4, 60), zenithG = lerpChannel(6, 120), zenithB = lerpChannel(18, 200);
    device.Clear(Color(skyR, skyG, skyB, 255), 1.0f);
    device.SetDepthTestEnabled(true);

    // Distance fog (CRAFT_PARITY.md §5.2) — fades geometry toward the same
    // flat sky color used for the clear, so the streamed region's edge
    // recedes instead of hard-cutting at the far clip plane. Computed from
    // radii_.createRadius every frame (not a compile-time constant) so a
    // runtime `/view` change immediately extends/shrinks the fade distance
    // to match, same as Craft's own fog_distance tracking g->render_radius
    // live. See the anonymous-namespace comment above for why this isn't
    // blocked by shader limits.
    const float fogEnd = static_cast<float>(radii_.createRadius * Worlds::CHUNK_SIZE);
    effect_->setFogEnabledProperty(true);
    effect_->setFogColorProperty(Vector3(static_cast<float>(skyR) / 255.0f, static_cast<float>(skyG) / 255.0f,
                                          static_cast<float>(skyB) / 255.0f));
    effect_->setFogStartProperty(fogEnd * 0.5f);
    effect_->setFogEndProperty(fogEnd);
    // Nearest-neighbor sampling: the atlas has no padding between tiles, so
    // linear filtering bleeds each tile's neighbor color (visible as magenta
    // speckling from the unused-tile fallback color) across every tile edge.
    // Craft's own texture.png atlas is sampled the same way (GL_NEAREST, see
    // main.c) — see THIRD_PARTY_NOTICES.md.
    device.getSamplerStatesProperty()[0] = SamplerState::PointClamp;

    const auto& vp = device.getViewportProperty();
    const float aspect = (vp.getHeightProperty() > 0)
        ? static_cast<float>(vp.getWidthProperty()) / static_cast<float>(vp.getHeightProperty())
        : 1.0f;

    const Core::Vec3f eye = player_->EyePosition();
    const Core::Vec3f dir = player_->LookDirection();
    const Vector3 eyeVec(eye.x, eye.y, eye.z);
    const Vector3 targetVec(eye.x + dir.x, eye.y + dir.y, eye.z + dir.z);

    effect_->View = Matrix::CreateLookAt(eyeVec, targetVec, Vector3::Up);

    const auto kb = Keyboard::GetState();
    if (kb.IsKeyDown(Keys::F)) {
        effect_->Projection = Matrix::CreateOrthographic(
            kOrthoViewHeight * aspect, kOrthoViewHeight, 0.1f, 500.0f);
        // Craft's own shader disables fog in ortho mode (`if (bool(ortho))
        // fog_factor = 0.0`, block_vertex.glsl) — matched here.
        effect_->setFogEnabledProperty(false);
    } else {
        const float fov = kb.IsKeyDown(Keys::LeftShift) ? kZoomFov : kPiOver4;
        effect_->Projection = Matrix::CreatePerspectiveFieldOfView(fov, aspect, 0.1f, 500.0f);
    }

    // Sky dome (CRAFT_PARITY.md §5.3) — a plain vertex-colored gradient
    // hemisphere replacing the flat clear color, drawn first with depth
    // writes off so it never occludes anything drawn afterward. Fog is
    // switched off for this draw (fading the sky into itself would be a
    // no-op at best, visibly wrong at worst); vertex-color/unlit mode is
    // used the same way SelectionOutline uses it below, then restored.
    skyDome_.Update(device, Color(skyR, skyG, skyB, 255), Color(zenithR, zenithG, zenithB, 255));
    device.SetDepthWriteEnabled(false);
    const bool fogWasEnabled = effect_->getFogEnabledProperty();
    effect_->setFogEnabledProperty(false);
    effect_->setTextureEnabledProperty(false);
    effect_->VertexColorEnabled = true;
    effect_->setLightingEnabledProperty(false);
    skyDome_.Draw(device, *effect_, eyeVec);
    effect_->setLightingEnabledProperty(true);
    effect_->VertexColorEnabled = false;
    effect_->setTextureEnabledProperty(true);
    effect_->setFogEnabledProperty(fogWasEnabled);
    device.SetDepthWriteEnabled(true);

    // Per-chunk frustum culling (plan.md §11.2): only draw chunks whose AABB
    // intersects the current view frustum — mirrors Craft's own naive
    // AABB-vs-frustum test in src/main.c's render_chunks. No separate
    // render-distance check: kCreateRadius == kRenderRadius (see the
    // constant's comment), so nothing loaded is ever farther away than
    // kDeleteRadius, and frustum culling alone decides per-frame visibility
    // among loaded chunks, same as before streaming existed.
    const BoundingFrustum frustum(effect_->View * effect_->Projection);
    for (auto& [key, renderers] : chunkRenderers_) {
        (void)key;
        for (auto& renderer : renderers) {
            if (!frustum.Intersects(renderer->Bounds())) continue;
            renderer->DrawOpaque(device, *effect_);
        }
    }

    // Signs (CRAFT_PARITY.md §4.3) — lit/textured quads, drawn with the same
    // effect_ state as opaque chunk geometry (VertexPositionNormalTexture,
    // no per-quad state changes needed beyond the per-quad texture already
    // handled inside SignBillboard::Draw).
    signBillboard_.Draw(device, *effect_);

    // Visible targeted-block outline (CRAFT_PARITY.md §2.4) — drawn right
    // after opaque geometry, same ordering as Craft's own render_wireframe
    // call (right after the solid-block render pass, before transparent
    // blocks/HUD). Temporarily switches the shared BasicEffect to plain
    // vertex-color/unlit mode (VertexPositionColor has no UV/normal), then
    // restores it for the textured/lit chunk geometry that follows.
    if (hasTargetedBlock_) {
        selectionOutline_.Update(device, targetedBlockX_, targetedBlockY_, targetedBlockZ_);
        effect_->setTextureEnabledProperty(false);
        effect_->VertexColorEnabled = true;
        effect_->setLightingEnabledProperty(false);
        selectionOutline_.Draw(device, *effect_);
        effect_->setLightingEnabledProperty(true);
        effect_->VertexColorEnabled = false;
        effect_->setTextureEnabledProperty(true);
    }

    // Transparent geometry (plan.md §11.2 "Transparency for glass") is drawn
    // last, with blending on and depth writes off, so the opaque scene
    // behind it shows through — same order/state as house3d_demo.cpp's
    // solid-then-glass pass.
    device.SetBlendEnabled(true);
    device.SetDepthWriteEnabled(false);
    for (auto& [key, renderers] : chunkRenderers_) {
        (void)key;
        for (auto& renderer : renderers) {
            if (!frustum.Intersects(renderer->Bounds())) continue;
            renderer->DrawTransparent(device, *effect_);
        }
    }
    device.SetDepthWriteEnabled(true);
    device.SetBlendEnabled(false);

    hud_->Draw(device);

    if (screenshotPending_) {
        CaptureScreenshot(device);
        screenshotPending_ = false;
    }
}

void CnaCraftGame::CaptureScreenshot(GraphicsDevice& device) {
    const auto& vp = device.getViewportProperty();
    const int width = vp.getWidthProperty();
    const int height = vp.getHeightProperty();
    if (width <= 0 || height <= 0) return;

    std::vector<Color> pixels(static_cast<std::size_t>(width) * height, Color(0, 0, 0, 255));
    device.GetBackBufferData(pixels.data(), static_cast<int>(pixels.size()));

    Texture2D frame(device, width, height);
    frame.SetData(pixels.data(), static_cast<int>(pixels.size()));

    std::filesystem::create_directories("screenshots");
    char filename[64];
    std::snprintf(filename, sizeof(filename), "screenshots/cnacraft_%04d.png", ++screenshotCounter_);
    frame.SaveAsPng(filename);

    std::printf("Screenshot saved: %s\n", filename);
    std::fflush(stdout);
}

}
