#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "System/Threading/Tasks/Task.hpp"

#include "Net/GameClient.hpp"
#include "Persistence/WorldStore.hpp"
#include "Render/ChunkRenderer.hpp"
#include "Render/Hud.hpp"
#include "Render/PlayerCube.hpp"
#include "Render/SelectionOutline.hpp"
#include "Render/SignBillboard.hpp"
#include "Render/SkyDome.hpp"
#include "Worlds/DayNightCycle.hpp"
#include "Worlds/Hotbar.hpp"
#include "Worlds/MeshData.hpp"
#include "Worlds/PlayerController.hpp"
#include "Worlds/RemotePlayer.hpp"
#include "Worlds/Sign.hpp"
#include "Worlds/World.hpp"
#include "Worlds/WorldEditor.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft {

// Game subclass wiring the engine-agnostic Worlds/ layer to CNA: input
// (Keyboard/Mouse) -> PlayerController/VoxelRaycast, World -> ChunkRenderer
// per chunk, BasicEffect + a placeholder texture atlas for drawing
// (plan.md §5/§6/§7). Reuses house3d_demo.cpp's Game-subclass shape.
class CnaCraftGame final : public Microsoft::Xna::Framework::Game {
public:
    CnaCraftGame();

    const std::string& GetTypeName() const override;

    // Mirrors house3d_demo.cpp's --smoke flag: quit after N Update() calls, for
    // headless CI verification without a human at the keyboard.
    void SetSmokeFrames(int n) { smokeFramesLeft_ = n; }

    // Multiplayer client mode (MULTIPLAYER_PLAN.md §5, plan.md §12.1 item
    // 18 phase M4): set from main.cpp's `--server HOST [PORT]` before
    // Run(). Empty host (the default) = classic offline single-player.
    void SetServer(const std::string& host, int port) {
        serverHost_ = host;
        serverPort_ = port;
    }

protected:
    void Initialize() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    void CaptureScreenshot(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    // Chunk streaming (plan.md §12.1 item 19). Loads any not-yet-loaded,
    // not-already-in-flight column within kCreateRadius of the player's
    // current position (dispatch-capped per frame), unloads any loaded
    // column beyond kDeleteRadius.
    void UpdateStreaming(int playerCx, int playerCz);

    // Dispatches a background column-generation Task (plan.md §12.1 item
    // 19 phase 4): fetches this column's persisted edits synchronously
    // (cheap, main-thread-only SQLite access -- see WorldStore's own
    // reasoning for why per-edit access stays synchronous), then hands
    // (seed,cx,cz,edits) as plain copyable data to a background Task that
    // builds a throwaway scratch World, generates+applies-edits into it,
    // and returns the resulting column as a copyable
    // std::array<Worlds::Chunk,Y> (not the live World's own
    // std::unique_ptr-based storage, which isn't copy-constructible --
    // TaskT<T>::getResultProperty() requires T copyable). Also loads this
    // column's signs synchronously (cheap, same reasoning). Actually
    // creating the World column + chunkRenderers_ entries happens later,
    // in PollGenerationJobs, once the background task completes.
    void DispatchColumnGeneration(int cx, int cz);
    // Applies completed generation jobs, merging each into world_
    // (World::AdoptColumnCopy), creating matching chunkRenderers_ entries,
    // and marking the column + its already-loaded neighbors dirty. The
    // apply cap (separate from the dispatch cap above -- bounds
    // frame-time spikes from a burst of simultaneous completions) DEFERS
    // excess completed jobs to a later frame's call rather than dropping
    // them -- discarding a completed result outright is only safe for a
    // job that's genuinely no longer wanted (its column got loaded some
    // other way first), never merely because this frame's apply budget
    // ran out (a real bug this exact mistake caused once already -- see
    // PollMeshJobs' comment for why it's worse there). Never erases an
    // incomplete job's TaskT -- sharp-runtime's Task/TaskT wrap
    // std::async, whose future destructor blocks until the task finishes
    // unless already awaited; only ever polling
    // getIsCompletedProperty()/erasing once true avoids that stall.
    void PollGenerationJobs();

    // Dispatches a background meshing Task per dirty chunk, dispatch-capped
    // per frame (plan.md §12.1 item 19 phase 4): builds a small snapshot of
    // copyable per-chunk data (the target chunk + its up-to-6 face-adjacent
    // neighbors, each a plain Worlds::Chunk value, not a pointer into the
    // live World -- a background task must never read the live World
    // concurrently with main-thread edits) and hands that to a Task that
    // reconstructs a throwaway World from it and calls ChunkMesher::Build.
    // Clears the dirty flag immediately on dispatch, matching Craft's own
    // dirty-cleared-when-dispatched-not-when-complete timing -- a dirty
    // chunk that doesn't get dispatched this frame (cap reached) simply
    // stays dirty and is picked up by a later call instead.
    void DispatchMeshingForDirtyChunks();
    // Applies completed mesh jobs (apply-capped per frame, deferring
    // excess to a later call -- same reasoning as PollGenerationJobs, but
    // load-bearing here in a way it isn't there: DispatchMeshingForDirty
    // Chunks already cleared this chunk's dirty flag back when the job was
    // dispatched, so dropping a completed-but-over-cap result outright
    // would leave that chunk permanently un-meshed, with nothing left to
    // ever mark it dirty again. Confirmed by a real bug during this
    // phase's own visual verification: distant terrain showed permanent
    // rectangular holes that never filled in, traced to exactly this
    // mistake in an earlier version of this function). Uploads the
    // computed ChunkMeshData into the matching ChunkRenderer
    // (GraphicsDevice/VertexBuffer/IndexBuffer calls, main-thread-only);
    // a completed job whose column unloaded before it finished is
    // genuinely stale and is dropped.
    void PollMeshJobs();

    // Generates (or loads from world.db on top of fresh terrain) one
    // column synchronously and creates its matching chunkRenderers_
    // entries. Used only for the player's own spawn column in Initialize()
    // (plan.md §12.1 item 31) — one column costs ~80ms, cheap enough not to
    // delay the first presented frame perceptibly, and it guarantees the
    // ground under the player's feet exists before physics ever runs. (The
    // full-radius synchronous force-load this is a remnant of was removed
    // by item 28 for blocking the window ~14s; removing the player's OWN
    // column along with it was a real regression — the player free-fell
    // through not-yet-loaded ground and got entombed when it arrived. See
    // HealPlayerIfEmbedded below.)
    void LoadColumnSynchronously(int cx, int cz);
    // If the player's AABB overlaps solid terrain (a state normal movement
    // can never produce — see PlayerController::IsEmbedded), snap them to
    // the surface, preserving yaw/pitch/flying. Called from two places:
    // Initialize() (heals world.db states corrupted by the item-28
    // async-spawn regression, where positions kept being saved while the
    // player was entombed — those states outlive the code fix, so they
    // must be healed at load) and PollGenerationJobs() (a column
    // materializing around a player who walked/flew into not-yet-loaded
    // territory entombs them at load-apply time — the only moment
    // embedding can arise during normal play).
    void HealPlayerIfEmbedded();
    // Frees one column's World data and chunkRenderers_ entries. Marks
    // still-loaded face-adjacent neighbor columns dirty, so their shared
    // boundary faces re-appear now that this column is gone (same
    // reasoning as loading, in reverse).
    void UnloadColumn(int cx, int cz);
    void MarkNeighborColumnsDirty(int cx, int cz);
    [[nodiscard]] bool IsColumnGenerationInFlight(int cx, int cz) const;

    // --- Multiplayer (MULTIPLAYER_PLAN.md §5, plan.md §12.1 item 18 M4) ---
    // Connects + performs the version/handshake exchange synchronously
    // (bounded wait) BEFORE the store opens and any column generates: the
    // per-server cache path and the server's terrain seed both shape what
    // Initialize() does next. On any failure the game falls back to plain
    // offline single-player with a console/HUD notice -- never a crash
    // (deliberate departure from Craft's exit(1)).
    void ConnectToServer();
    // Per-frame inbox drain, capped, called from BOTH per-frame pipeline
    // sites (the main path and the typing-mode early return) right before
    // PollGenerationJobs -- same defer-don't-discard discipline: whatever
    // the cap leaves stays queued for the next frame.
    void PollNetworkMessages();
    void ApplyServerMessage(const Net::ServerMessage& msg);
    // Sends this frame's freshly recorded edits as B lines BEFORE
    // WorldStore::SaveEdits clears them -- one hook covers every edit
    // source (clicks AND WorldEditor /commands) because they all funnel
    // through World::RecordedEdits.
    void SendPendingEditsToServer();
    // Craft's position cadence: at most every 0.1s, skipped while still.
    void SendPositionIfDue(float totalSeconds);
    [[nodiscard]] bool IsOnline() const { return netClient_ && netClient_->IsConnected(); }

    // Runtime mode switching (item 18 M6) -- the /online HOST [PORT] and
    // /offline commands, Craft's own outer-loop teardown/rebuild model
    // (main.c:2729-2751) done in place: drain in-flight background jobs
    // (never erase an incomplete TaskT -- NEXT.md §6), stop the client,
    // unload every column, then reconnect + reopen the right store +
    // respawn. Empty host = offline.
    void SwitchMode(const std::string& host, int port);
    // The shared spawn sequence Initialize() and SwitchMode() both use:
    // saved-position load (offline only -- online spawns are
    // server-authoritative), synchronous spawn-column load, player
    // creation, embed-heal.
    void SpawnPlayerForCurrentMode();

    // In-flight background jobs (plan.md §12.1 item 19 phase 4). TResult
    // must be copy-constructible (TaskT<T>::getResultProperty() returns by
    // value from a member reached through a shared_ptr, which needs a copy,
    // not a move) -- std::array<Worlds::Chunk,Y> and Worlds::ChunkMeshData
    // both qualify; a std::array<std::unique_ptr<Chunk>,Y> would not.
    struct InFlightGenerationJob {
        int cx = 0, cz = 0;
        System::Threading::Tasks::TaskT<std::array<Worlds::Chunk, Worlds::WORLD_CHUNKS_Y>> task;
    };
    struct InFlightMeshJob {
        int cx = 0, cy = 0, cz = 0;
        System::Threading::Tasks::TaskT<Worlds::ChunkMeshData> task;
    };
    std::vector<InFlightGenerationJob> inFlightGenerationJobs_;
    std::vector<InFlightMeshJob> inFlightMeshJobs_;

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> atlasTexture_;
    // The sky gradient texture (plan.md §12.1 item 33) -- built once in
    // Initialize(), swapped in for the atlas during the sky dome pass in
    // Draw(), swapped back for terrain right after.
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> skyTexture_;

    Worlds::World world_;
    std::unique_ptr<Worlds::PlayerController> player_;
    // Keyed identically to Worlds::World's own column storage (Worlds::
    // World::PackColumnKey) -- a real correctness fix over the old flat
    // std::vector, which was only ever kept in lockstep with World's
    // indexing by construction order (both built by an identical triple-
    // nested loop), a fragile assumption a hash-map-keyed lookup removes
    // entirely.
    std::unordered_map<Worlds::ColumnKey, std::array<std::unique_ptr<Render::ChunkRenderer>, Worlds::WORLD_CHUNKS_Y>>
        chunkRenderers_;
    // Count of chunkRenderers_ entries currently carrying a non-empty glow
    // mesh (plan.md §12.1 item 27 follow-up) -- kept in sync incrementally
    // at the two places a renderer's glow buffer can change (PollMeshJobs'
    // ApplyMesh call, UnloadColumn), so Draw() can skip its entire glow pass
    // (map iteration + frustum tests, not just draw calls) whenever it's 0,
    // the overwhelmingly common case.
    int glowChunkCount_ = 0;
    Worlds::Hotbar hotbar_;

    // World persistence (CRAFT_PARITY.md §4.1/§4.2) — loaded once in
    // Initialize() (after World::Generate, before the initial chunk mesh
    // build), saved synchronously right after any player edit in Update().
    std::unique_ptr<Persistence::WorldStore> worldStore_;

    std::unique_ptr<Render::Hud> hud_;
    std::vector<std::string> hotbarSlotNames_;

    // Signs (CRAFT_PARITY.md §4.3, plan.md §12.1 item 16). signStore_ is the
    // engine-agnostic data model, loaded once in Initialize() (after
    // worldStore_->LoadInto). signBillboard_ holds the GPU resources and is
    // rebuilt only when the sign list actually changes (placed/erased),
    // never every frame -- same convention as chunkRenderers_.
    Worlds::SignStore signStore_;
    Render::SignBillboard signBillboard_;
    bool signsNeedRebuild_ = true;

    // Typing state machine (mirrors Craft's own g->typing / g->typing_buffer
    // in main.c). While typingMode_ != None, WASD/look/click input is
    // suspended (matching Craft's handle_movement gating movement polling on
    // !g->typing) but gravity/physics still integrates. Backtick opens Sign
    // typing (buffer starts empty); `/` opens Command typing (buffer starts
    // as "/", matching Craft's own g->typing_buffer[0]='/' exactly) --
    // plan.md §12.1 item 17. Both edge-triggered; Enter submits (Sign
    // re-raycasts fresh at Enter-time, matching Craft calling hit_test then
    // rather than caching the block from when typing started; Command calls
    // Worlds::ExecuteCommand), Escape cancels either mode without quitting.
    // `Chat` (plan.md §12.1 item 18 M6): Craft's bare-Enter empty typing
    // buffer, sent to the server as plain chat -- the exact feature NEXT.md
    // §9's "don't add a bare-Enter chat trigger" note deferred "until
    // multiplayer actually lands"; it has. Online-only: offline Enter does
    // nothing, preserving the old behavior exactly.
    enum class TypingMode { None, Sign, Command, Chat };
    TypingMode typingMode_ = TypingMode::None;
    std::string typingBuffer_;
    bool backtickWasDown_ = false;
    bool slashWasDown_ = false;
    bool backspaceWasDown_ = false;
    bool enterWasDown_ = false;
    bool escapeWasDown_ = false;

    // World-editing command state (plan.md §12.1 item 17, CRAFT_PARITY.md
    // §4.5) -- mark0_/mark1_ mirror Craft's own g->block0/g->block1 (the
    // last two edited positions+types, updated via RecordMark right after
    // every successful break/place), clipboard_ mirrors g->copy0/g->copy1
    // (just the two corner positions `/copy` remembers, not their
    // contents -- see Worlds::WorldEditor::PasteRegion's doc comment).
    // radii_ replaces the old compile-time-fixed kCreateRadius/
    // kDeleteRadius as the mutable source of truth `/view` mutates at
    // runtime; seeded from those same constants in Initialize().
    Worlds::BlockMark mark0_;
    Worlds::BlockMark mark1_;
    Worlds::ClipboardRegion clipboard_;
    Worlds::CommandRadii radii_;
    void RecordMark(int x, int y, int z, Worlds::BlockType type);

    // Visible targeted-block feedback (CRAFT_PARITY.md §2.4) — updated each
    // frame in Update() from the same raycast used for break/place, drawn in
    // Draw() only when hasTargetedBlock_ is true (looking at nothing within
    // reach draws no outline, matching Craft's own hit-test-gated behavior).
    Render::SelectionOutline selectionOutline_;
    Render::SkyDome skyDome_;
    bool hasTargetedBlock_ = false;
    int targetedBlockX_ = 0;
    int targetedBlockY_ = 0;
    int targetedBlockZ_ = 0;

    // Cursor capture (CRAFT_PARITY.md §1.2, user decision 2026-07-10: match
    // Craft's real behavior). Escape releases the cursor rather than
    // quitting; left-click while released re-captures it instead of
    // breaking/placing on that same click -- matches Craft's own
    // exclusive/on_mouse_button model exactly. There is no in-game quit key
    // in real Craft at all; closing the window (Alt+F4 / the X button) is
    // the only way to quit, already handled by CNA's own
    // SDL_EVENT_QUIT -> Game::Exit().
    bool cursorCaptured_ = true;

    bool leftClickWasDown_ = false;
    bool rightClickWasDown_ = false;
    bool middleClickWasDown_ = false;
    bool eKeyWasDown_ = false;
    bool rKeyWasDown_ = false;
    bool tabWasDown_ = false;
    bool scrollWheelInitialized_ = false;
    int previousScrollWheelValue_ = 0;
    bool f12WasDown_ = false;
    bool f11WasDown_ = false;
    bool screenshotPending_ = false;
    int screenshotCounter_ = 0;
    int smokeFramesLeft_ = 0;
    // Throttle for SavePlayerState (plan.md §12.1 item 31): each save is a
    // synchronous SQLite write whose commit fsyncs the disk — doing it
    // every frame (as the first version of player-position persistence
    // did) means 60+ fsyncs/second, a real, user-visible per-frame stutter
    // on ordinary hardware. Saving once per second loses at most one
    // second of position on a crash, which is nothing.
    float playerStateSaveAccumulator_ = 0.0f;

    // --- Multiplayer state (MULTIPLAYER_PLAN.md §5, item 18 M4) ---
    std::string serverHost_;  // empty = offline
    int serverPort_ = Net::kDefaultPort;
    std::unique_ptr<Net::GameClient> netClient_;
    int myPlayerId_ = -1;
    bool netDropAnnounced_ = false;
    // The terrain seed. Offline it stays at the classic single-player
    // constant; online the server's W message overrides it during
    // ConnectToServer, BEFORE any column generates -- every GenerateColumn
    // call reads this member, never a constant, so all clients of one
    // server produce identical base terrain (the core Option-B guarantee).
    // SwitchMode resets it to the default when going offline.
    static constexpr std::uint32_t kDefaultWorldSeed = 1337;
    std::uint32_t worldSeed_ = kDefaultWorldSeed;
    // Day/night clock: totalSeconds = TotalGameTime + clockOffsetSeconds_,
    // wrapped by dayLengthSeconds_. Offline both stay at the classic
    // defaults (mid-morning start); the server's E message overwrites both
    // so every player shares one sky.
    float clockOffsetSeconds_ = Worlds::kDefaultDayLengthSeconds / 3.0f;
    float dayLengthSeconds_ = Worlds::kDefaultDayLengthSeconds;
    // TotalGameTime as of the current Update() -- the E handler needs "now"
    // to convert the server's elapsed clock into an offset.
    float lastKnownTotalSeconds_ = 0.0f;
    // Position-send throttle state (Craft: 0.1s cadence + still-skip).
    float lastPositionSentSeconds_ = -1.0f;
    float sentEyeX_ = 0.0f, sentEyeY_ = 0.0f, sentEyeZ_ = 0.0f, sentYaw_ = 0.0f, sentPitch_ = 0.0f;

    // Remote players (item 18 M5): P creates/updates (name defaults to
    // "player<id>" until N arrives, Craft's own placeholder), D removes.
    std::unordered_map<int, Worlds::RemotePlayer> remotePlayers_;
    Render::PlayerCube playerCube_;
};

}
