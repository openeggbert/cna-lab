# MULTIPLAYER_PLAN.md — design document (planning only)

Status: **IMPLEMENTED — all phases M0-M7 shipped 2026-07-11** (user
go-ahead "pust se prosim do implementace vsech 4 bodu" the same day the
design and its §11 scope decisions were recorded). Each phase landed as
its own verified commit on `develop` (protocol M0, transport M1, server
M2, world sync M3, game integration M4, remote players M5, chat + mode
switching M6, PIP observation + @nick PMs M7) — see plan.md §12.1 item
18's writeup for the per-phase detail and verification evidence. This
document remains the protocol/architecture reference; §2's message table
and §4's dialect deltas describe the shipped wire format exactly. Written
2026-07-11 from three research passes over the real Craft checkout
(`/rv/data/development/github.com/other/Craft`), CNA + sharp-runtime, and
cna-craft's own integration surface.

---

## 1. How real Craft multiplayer works (reference summary)

Full details in §2's protocol table. The architecture in six sentences:

1. **Transport**: plain TCP (default port 4080), ASCII lines,
   comma-separated fields, `\n`-terminated, protocol version `1`. Client:
   one blocking-recv background thread appending to a 1 MB byte queue
   (mutex-guarded); the main loop drains complete lines once per frame
   (`client.c:155-210`, `main.c:2803-2807`). Sends are synchronous from the
   main thread. No reconnect logic — a drop calls `exit(1)`.
2. **World sync is delta-only**: client AND server generate identical base
   terrain from the *same C code* (`world.c`; the Python server loads it
   via ctypes — `world.py:1-18`), so only user edits travel. The server's
   `block` table **rowid** is the version counter: the client requests
   `C,p,q,key` with its last-seen key, the server streams only
   `rowid > key` block rows, then `K,p,q,max_rowid` (`server.py:339-376`).
   Lights and signs are always re-sent in full. The client caches deltas in
   a **per-server sqlite file** (`cache.<addr>.<port>.db`, `main.c:2706`).
3. **Edits are optimistic + server-authoritative**: the client applies its
   own edit locally and sends `B/L/S`; the server validates (auth required,
   y-bounds, allowed items, cloud indestructible, place-on-empty rules —
   `server.py:377-433`) and on rejection echoes the *previous* value back
   (`B,p,q,x,y,z,previous` + `R,p,q` + a `T` message) to force a revert.
   No reach/anti-cheat checks.
4. **Players**: positions sent at most every 0.1 s (skipped when
   still); remote players are single textured cubes (`make_player`)
   rendered from an interpolation of the last two received states
   (`update_player`/`interpolate_player`, `main.c:421-461`). Names arrive
   via `N` and render as **2D HUD text**, not 3D billboards. Join is
   implicit (first `P` for an unknown id); leave is an explicit `D`.
5. **Time**: one `E,unixtime,day_length` at connect sets the client clock;
   no periodic re-sync.
6. **Auth**: optional two-step token flow against
   `craft.michaelfogleman.com` (`/identity`, `/login`, `/logout`;
   `auth.c:21-49`, `server.py:320-338`) — **that hosted service is dead**
   (Craft README:68), so real deployments run guest-only or self-host auth.
   With `AUTH_REQUIRED=True` (default) guests can move/chat but not build.

## 2. Craft wire-protocol reference (verified against source)

ASCII lines, `,`-separated, `\n`-terminated. Version = 1.

**Client → server** (`client.c`; handlers `server.py:174-183`):

| Op | Format | When |
|----|--------|------|
| V | `V,version` | once, immediately after connect |
| A | `A,username,access_token` | after V; empty fields = anonymous |
| P | `P,x,y,z,rx,ry` (floats, 2dp) | ≤ every 0.1 s, skipped when still |
| C | `C,p,q,key` | on chunk load — request deltas newer than `key` |
| B | `B,x,y,z,w` | on local place/break (w=0 breaks) |
| L | `L,x,y,z,w` | on light toggle |
| S | `S,x,y,z,face,text` | on sign place/clear |
| T | `T,text` | chat lines and unhandled `/`-commands |

**Server → client** (`server.py`; parsed `main.c:2471-2566`):

| Op | Format | Meaning |
|----|--------|---------|
| U | `U,id,x,y,z,rx,ry` | your id + authoritative position (connect, /spawn, /goto) |
| E | `E,unixtime,day_length` | clock sync, once at connect |
| B | `B,p,q,x,y,z,w` | block delta (chunk response, broadcast edit, or **revert**) |
| L | `L,p,q,x,y,z,w` | light delta (full resend per chunk request) |
| S | `S,p,q,x,y,z,face,text` | sign (full resend per chunk request) |
| K | `K,p,q,key` | new version key after a chunk's block deltas |
| R | `R,p,q` | force remesh of chunk p,q |
| C | `C,p,q` | end-of-chunk-response marker (real client ignores it) |
| P | `P,pid,x,y,z,rx,ry` | another player moved |
| N | `N,id,nick` | player name |
| D | `D,id` | player disconnected |
| T | `T,text` | chat/system/command output line |

Server internals worth porting faithfully: one **Model thread** owns all
world state + the single sqlite connection (handlers enqueue into it —
`server.py:207-219`), per-client outbound send queues, sqlite commit every
5 s, token-bucket rate limiter (present but off by default).

## 3. What cna-craft already has (and what's missing)

**Already in place — no engine work needed:**

- **TCP + line framing, ready today**: sharp-runtime ships a real
  `System::Net::Sockets::TcpClient`/`TcpListener`/`NetworkStream`
  (POSIX + Winsock, `sharp-runtime/src/System/Net/Sockets/`), and
  `System::IO::StreamReader::ReadLine()` / `StreamWriter` give line framing
  for free. `CnaCraft` already links `SHARP_RUNTIME` — zero CMake changes.
  (Caveats: keep the `shared_ptr<NetworkStream>` alive while a reader holds
  its raw `Stream*`; Emscripten throws — desktop-only feature.)
- **Threading**: `System::Threading::Thread` (explicit `Start`/`Join`;
  destructor DETACHES, so shutdown must join explicitly) +
  `System::Collections::Concurrent::ConcurrentQueue<T>` — the natural
  recv-thread → main-loop handoff. Precedent: sharp-runtime's own loopback
  socket tests (`tests/System/Net/Sockets/SocketTests.cpp:24-68`).
- **Protocol string handling**: `String::Split(line, ',')`,
  `Int32::TryParse`, `Single::TryParse`.
- **Per-frame capped work pattern**: `PollGenerationJobs`/`PollMeshJobs`
  (defer-don't-discard) is exactly the shape a `PollNetworkMessages` should
  take; it must be called from BOTH pipeline sites (`CnaCraftGame.cpp:
  1031-1033` and the typing-mode early return at `:729-731`).
- **Craft-shaped persistence**: `WorldStore`'s schema already matches
  Craft's byte-for-byte (block/sign/light with p,q; state), and the
  generate-then-overlay-deltas column load
  (`DispatchColumnGeneration`, `CnaCraftGame.cpp:359-387`) is exactly
  Craft's client model. Missing pieces: a `key` table + rowid-style
  versioning, and a per-server cache path (store is hardcoded
  `"world.db"`, `CnaCraftGame.cpp:129`).
- **Chat surface**: `TypingMode::Command`, `ExecuteCommand`, and the
  4-line `Hud::PushMessage` log — inbound `T` messages plug straight into
  the message log.
- **Optimistic-edit flow**: edits already apply locally first
  (`SetBlockAndRecordEdit`) and flush per frame (`SaveEdits`,
  `CnaCraftGame.cpp:997`) — the send hook goes next to that flush; the
  revert path is just "apply inbound `B` like any remote edit".

**Missing entirely:**

- Any networking code (CRAFT_PARITY.md §4.6: zero hits for
  net/socket/tcp — deliberate, documented).
- Remote-player representation and rendering (exactly one `player_`;
  no entity list, no humanoid/cube renderer). Reusable precedents exist:
  `SignBillboard` (world-space textured quad + `BuildSignTexture` text
  rasterizer), `SelectionOutline`/`SkyDome` (own-VB/IB draws under the
  shared `BasicEffect`).
- Bare-Enter chat trigger (deliberately unported — NEXT.md §9 says don't
  add it *"without a real reason (e.g. multiplayer actually landing)"* —
  this plan is that reason, gated on implementation go-ahead).
- The 5 auth/mode commands (`/identity`, `/login`, `/logout`, `/online`,
  `/offline`) — `ExecuteCommand` currently returns "Unknown command".
- `--server host port` argument parsing (main.cpp parses only `--smoke`).
- **CNA's `Net` layer is the wrong tool** — plan.md §0 suggested it as the
  natural home, but it is XNA `NetworkSession` over ENet/**UDP**,
  single-threaded poll-based, with no TCP/line-protocol facility. Craft's
  protocol needs the sharp-runtime TCP stack above. (plan.md §0/§11.6
  should be amended when implementation starts.)

## 4. THE central design decision: compatibility target

Craft's sync model means **wire compatibility ≠ world compatibility**.
Three hard mismatches block playing against a real `server.py`:

1. **Terrain generation identity.** Craft's client and server generate the
   same base terrain from the same `world.c` (server loads it via ctypes)
   and the server validates every edit against ITS generated world
   ("removing already-empty" etc.). cna-craft's `NoiseGenerator`
   deliberately differs (per-seed permuted gradient table vs Craft's
   single global table; `NoiseGenerator.hpp:10-16`), and there is no seed
   in the protocol — Craft's generation is unseeded-global. Result: a
   cna-craft client on a real Craft server would see different ground than
   the server validates against. Fixing this = porting Craft's exact
   simplex + `world.c` as a second, Craft-exact generation mode.
2. **CHUNK_SIZE 32 vs 16.** Protocol `p,q` are 32-block chunk coords;
   cna-craft columns are 16. A compatibility layer would map each protocol
   chunk to a 2×2 block of cna-craft columns (requests, K keys, R redraws,
   and Craft's negative-`w` edge-marker rows all need translating).
3. **Block-ID mapping.** Craft item ids (GRASS=1, SAND=2, STONE=3,
   BRICK=4, …, CLOUD=16, TALL_GRASS=17) do NOT match cna-craft's
   `BlockType` ordinals (Grass=1, Dirt=2, Sand=3, Stone=4, …, Cloud=14,
   TallGrass=16). Protocol `w` values (and `ALLOWED_ITEMS`/
   `INDESTRUCTIBLE_ITEMS`) need a bidirectional map. (Side finding, worth
   its own note: `WorldStore`'s "byte-identical to a Craft world.db" claim
   holds for the *schema*, not the `w` semantics — opening a real Craft
   `craft.db` in cna-craft would misinterpret block types. Not a
   multiplayer issue per se, but the same map fixes both.)

**Option A — full compatibility with real Craft servers** (connect to any
existing `server.py`): requires all three fixes above *before any
multiplayer works at all*. High fidelity value, large prerequisite cost,
and the terrain-generation port doubles every future generation change.

**Option B — cna-craft↔cna-craft multiplayer, Craft's protocol as the
blueprint** *(recommended)*: keep Craft's exact message shapes, opcodes,
versioning model, and server semantics, but with cna-craft's own terrain,
16-block columns as the chunk unit, and `BlockType` ordinals as `w`. This
requires writing our own server (§6) — which Option A **also** effectively
requires for self-hosting anyway, since `server.py` needs the compiled
`./world` C library and the dead auth service handled. Two honest protocol
deltas vs Craft v1, both explicit: a different version number (proposal:
`V,1001` so real Craft clients are cleanly rejected) and a `W,seed,...`
world-info message at connect (Craft never needs one because its
generation is unseeded — we do, so remote clients generate identical
terrain). Everything else stays Craft-shaped 1:1, so a later Option-A
compatibility mode remains possible (the three fixes are additive: a
Craft-exact generation mode, a 32↔16 mapping layer, an ID map).

**Recommendation: B.** It delivers playable LAN/self-hosted multiplayer
with the least new risk, reuses `Worlds/`+`Persistence/` on the server
side (which fixes the validation-terrain problem *by construction* — the
server runs the same `GenerateColumn`), and keeps A as a clearly-scoped
future fidelity milestone rather than a prerequisite.

## 5. Client design (new `src/CnaCraft/Net/`, engine-agnostic)

New module directory `src/CnaCraft/Net/` — depends on sharp-runtime +
`Worlds/`, **no CNA/SDL includes** (same purity rule as `Worlds/`; keeps
it fully unit-testable in the worlds-style CTest suite):

- **`Net/Protocol.hpp/.cpp`** — pure functions: `ParseServerMessage(line)
  -> variant/struct`, `Format*` builders for every outbound message.
  No I/O. Exhaustively unit-tested against §2's table (including Craft's
  quirks: sign text may contain commas — parse it with a field-count cap,
  mirroring Craft's `%63[^\n]` sscanf; float formatting at 2dp).
- **`Net/GameClient.hpp/.cpp`** — owns `TcpClient`, one
  `System::Threading::Thread` running a blocking
  `StreamReader::ReadLine()` loop that parses and enqueues into a
  `ConcurrentQueue<ServerMessage>`; a `Send(msg)` that serializes +
  writes under a small lock (Craft sends from the main thread only — we
  will too, so the lock is belt-and-braces); explicit `Stop()` that
  closes the socket to unblock the reader and **joins** the thread
  (sharp-runtime `Thread` detaches in its destructor — never rely on
  RAII here). Connection failure/drop policy: unlike Craft's `exit(1)`,
  surface a HUD message and revert to offline mode (a deliberate,
  documented improvement).
- **Game integration (`CnaCraftGame`)**:
  - `PollNetworkMessages()` joins the per-frame pipeline trio at BOTH
    call sites, with a per-frame cap following the defer-don't-discard
    rule (NEXT.md §6). Inbound `B`/`L`/`S` apply through the existing
    non-recording paths (`World::SetBlock`, `SetLightSource`,
    `SignStore`) — the AO cross-product dirty rule and glow counters all
    fire automatically; `R` maps to `MarkDirty`; `K` updates the key
    store; `T` → `hud_->PushMessage`; `U` teleports + `force` loads the
    player's column (reuse `LoadColumnSynchronously` + heal); `E` sets a
    clock offset (§8); `P`/`N`/`D` update the remote-player table (§7).
  - Outbound hooks: position throttle (≥0.1 s + min-delta, in `Update`);
    `B`/`L`/`S` sends adjacent to the existing local-apply sites; `T` from
    chat/unknown commands when online.
  - Chunk requests: `DispatchColumnGeneration` additionally sends
    `C,cx,cz,key` when online. Server deltas that arrive while the column
    is still generating in the background are safe by construction —
    they apply to the live `World` only after `AdoptColumnCopy`, so the
    poller must **defer** (not drop) messages for not-yet-loaded columns;
    simplest correct rule: hold them in a small per-column pending list
    drained when the column installs (the same moment
    `HealPlayerIfEmbedded` already hooks).
- **Offline mode unchanged**: `GameClient` absent → every hook no-ops.
  `--server host [port]` in `main.cpp` (plus `/online`/`/offline`
  commands later, M6).

## 6. Server design (new `CnaCraftServer` target, same repo)

A small console binary reusing `CnaCraftWorlds` + `Persistence/` (no CNA,
no graphics — builds under `-DCNA_CRAFT_BUILD_GAME=OFF` too):

- **Model-thread architecture, ported from `server.py`**: N
  connection threads (accept via `TcpListener`, one reader thread per
  client + per-client outbound queue) funnel parsed commands into ONE
  model thread owning the `World` + `WorldStore` — serialized world
  access, no fine-grained locking, sqlite commit batched (~5 s, Craft's
  `COMMIT_INTERVAL`).
- **Validation = Craft's rules** (`server.py:377-482`): y-bounds, place-on-
  empty/remove-non-empty against the server's own generated+edited world,
  cloud indestructible, sign length, light range; reject ⇒ echo previous
  value + `R` + `T` message. Auth: **guest-only initially**
  (`AUTH_REQUIRED=False` equivalent) — the upstream auth service is dead;
  an `A` message just sets the nick (see §9 open question).
- **World state**: `GenerateColumn(seed)` on demand + edit overlay — the
  same code path as the client, which is what makes validation coherent.
  Persistence into a server-side `world.db` (same store class; the `key`
  versioning uses the block table's rowid exactly like `server.py:344`).
- **Chunk responses**: per §2 — incremental blocks by key, lights+signs in
  full, then `K`, `R`, `C`.
- Time (`E` once at connect), spawn `U`, join/leave broadcasts, `/list`,
  `/spawn`, `/goto`, `/nick` — straight ports.

## 7. Remote players

- `Worlds/RemotePlayer` (or a plain struct in `Net/`): id, name, and
  Craft's two-sample interpolation state (`state1`/`state2` + rendered
  state; port `update_player`/`interpolate_player` incl. the rx-wraparound
  handling, `main.c:421-461`) — pure math, unit-testable.
- Rendering: a `Render::PlayerBillboard`-style cube — port `make_player`'s
  oriented textured cube (Craft tiles 226/224/241/209/225/227 in ITS
  atlas; ours needs a small player-skin tile block added to
  `TextureAtlas`, append-only). Own VB/IB per player, drawn in the opaque
  pass state (they're textured+lit in Craft; here: vertex-color path with
  a fixed neutral shade — consistent with terrain's new model).
- Nametags: match Craft exactly — 2D HUD text (`Hud`/`BitmapFont`) for
  the crosshair-targeted player only, NOT 3D billboards (CRAFT_PARITY.md
  §4.4 already documents that real Craft has no in-world nametags).
- Position sends: 0.1 s throttle + min-delta, from `Update`.

## 8. Clock sync

`Draw` currently derives daylight from
`TotalGameTime + kDefaultDayLengthSeconds/3` (`CnaCraftGame.cpp:1051-1055`).
Add a `clockOffsetSeconds_` (default `dayLength/3`) that an inbound `E`
overwrites (`fmod(serverElapsed, dayLength) - totalGameTime` at receipt) —
one field, no other changes; day length itself also comes from `E`.

## 9. Chat & commands

- **Bare-Enter chat trigger**: add `TypingMode` usage with Enter opening an
  empty buffer ONLY when online (offline keeps today's behavior). This
  satisfies NEXT.md §9's "don't add it without a real reason" — the reason
  is multiplayer landing; still forbidden until implementation go-ahead.
- Online `/`-commands: unknown commands and plain chat forward as `T`
  (Craft's fallback); local world-editing commands (`/cube` etc.) — Craft
  sends each resulting `set_block` through the normal `B` path; we do the
  same via `SetBlockAndRecordEdit`'s existing flush → send hook, so
  WorldEditor needs **zero changes**.
- `/online HOST [PORT]` / `/offline`: mode switch = teardown/rebuild of
  store+client (Craft's outer-loop model, `main.c:2729-2751`), with our
  per-server cache path `cache.<host>.<port>.db` (needs `WorldStore` to
  accept a path — it already does; only the hardcoded call site changes).
- `/identity`/`/login`/`/logout`: **defer** (M7, only if a self-hosted auth
  service ever matters). Guest nicks via `/nick` cover LAN play.

## 10. Phasing (each independently buildable + testable)

- **M0 — Protocol module**: `Net/Protocol` parse/format + exhaustive unit
  tests (worlds-style CTest binary, links SHARP_RUNTIME). No game changes.
- **M1 — GameClient transport**: thread + queue + loopback CTest against a
  scripted `TcpListener` echo server (sharp-runtime SocketTests pattern).
- **M2 — Server skeleton**: `CnaCraftServer` accepting connections, V/A/U/E
  handshake, position relay; CTest: two GameClients see each other's `P`.
- **M3 — World sync**: C/B/K/R/edit validation/revert + per-server client
  cache; CTest: edit on client A appears on client B, rejected edit
  reverts on A; key-based incremental refetch verified across reconnect.
- **M4 — In-game integration**: `--server`, PollNetworkMessages, edit/
  position hooks, `E` clock; Xvfb two-instance visual check (staged
  world.db camera trick from item 12 works here too).
- **M5 — Remote players**: interpolation port + cube rendering + N/D
  handling; unit tests for interpolation math, Xvfb screenshot of a second
  player cube.
- **M6 — Chat + mode switching**: Enter-chat (online only), T routing,
  `/online`/`/offline`, sign/light sync polish.
- **M7a — Picture-in-picture observation** (decided IN scope, §11): Craft's
  O/P-key cycling, rendered as a second viewport + `BasicEffect.View` pass
  from the observed player's interpolated state.
- **M7b — `@nick` private messages** (decided IN scope, §11): server-side
  routing per Craft's `on_talk`.
- Dropped from the roadmap per §11's decisions: the auth trio and the
  Option-A real-Craft compatibility mode.

## 11. Scope decisions — DECIDED by the user, 2026-07-11

All four questions were put to the user individually and answered
("ja odpovim jak si to predstavuji"):

1. **Compatibility target: Option B** — cna-craft↔cna-craft only, own
   server, protocol kept Craft-shaped with the two documented deltas
   (§4). Real-Craft-server compatibility (Option A) is NOT in scope and
   was not chosen even as a later milestone — don't build toward it
   beyond the cheap protocol-shape discipline already in the design.
2. **Server: C++ `CnaCraftServer` in this repo**, reusing
   `Worlds/` + `Persistence/` (§6). Craft's `server.py` will not be
   adapted.
3. **Auth: none — guest names + `/nick`** (§9). The `/identity`,
   `/login`, `/logout` trio stays unported; `A` messages only carry an
   optional nick.
4. **M7 extras: BOTH selected** — picture-in-picture observation
   (Craft's O/P keys; second viewport + View matrix pass) AND `@nick`
   private messages (server-side routing, Craft's own behavior). These
   join the roadmap as M7a/M7b after M6; the Option-A compatibility mode
   drops off the roadmap entirely per decision 1.

## 12. Constraints carried over (do not violate during implementation)

- All NEXT.md §9 rules stay: defer-don't-discard applies to the network
  poller; never erase an in-flight TaskT; `Worlds/` stays CNA-free (and
  `Net/` too, per §5); BlockType/Hotbar append-only (the protocol `w` uses
  those ordinals in Option B — one more reason they're frozen).
- sharp-runtime `Thread` detaches on destruction — every thread owner
  needs an explicit join-on-stop path, including on error.
- `TcpClient::GetStream()`'s `shared_ptr` must outlive any
  `StreamReader`/`StreamWriter` holding the raw `Stream*`.
- Position/edit message rates follow Craft's constants (0.1 s position
  throttle; server commit ~5 s) unless measurement says otherwise.
