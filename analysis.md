# Historical analysis and modern design

## Scope and project identity

The public working title is **Copper Boots**. It describes an original
courier-mechanic exploring colorful overgrown machine ruins. `mario-cna` remains
the repository and research name because this project studies the gameplay and
technical lessons of Mike Wiering's 1994 DOS game commonly known as *Mario &
Luigi*.

This is a loose remake, not a source translation. Historical observations are
marked as such below; proposed or implemented Copper Boots behavior is separate.
No Nintendo identity or original game content is part of the new work.

## Reference provenance and licensing

### Acquisition record

| Field | Finding |
|---|---|
| Official page | <https://www.wieringsoftware.nl/mario/source.html> |
| Author | Mike Wiering / Wiering Software |
| Original date | 1994, with archive/source notices updated through 2001 |
| Main reference | `MARIOSRC.ZIP`, complete Turbo Pascal 6/7 source |
| Download date | 2026-08-24 (Europe/Prague) |
| SHA-256 | `deb111b0a1751676c149dd3a4a69fd458116579d16ac4084a2ac097e5ef736d3` |
| Archive metadata | 368 files, 757,322 uncompressed bytes; timestamps 2001-12-07 |
| Language/platform | Turbo Pascal 6/7 with inline x86 assembly; DOS, VGA, 286+ |
| Local location | ignored `reference/original/MARIOSRC.ZIP` and `reference/original/tp67/` |
| Redistribution | the archive and extracted contents are not committed or shipped |

The official page also offers `MARSRC55.ZIP`, a Turbo Pascal 5.5 variant. It was
not needed for the initial analysis because the page identifies TP6/7 as the
original inline-assembly implementation and the user requested it as the main
technical reference.

For executable-only behavior measurement, the
[official download page](https://www.wieringsoftware.nl/mario/download.html)
also provided [`MARIO.EXE`](https://www.wieringsoftware.nl/mario/MARIO.EXE).
It was downloaded on 2026-08-24 into the same ignored reference area: 57,480
bytes, SHA-256
`955d184ce60a70ae83b9ad49c013eb070f55a80d4b3022ad574944188839852b`.
The executable is retained only as a private research copy and is not committed
or shipped.

### Actual license-language finding

There is no standard open-source license in the archive and no blanket grant
equivalent to MIT, GPL, BSD, or a modern permissive license.

`README.TXT` says the author released the source for learning, allows
experimentation, permits reuse of small fragments with credit, and expressly
accepts ports with a request to send a message. It also forbids merely changing
a few things and distributing the result as one's own, says a derived original
non-commercial game must change everything until totally unrecognizable, asks
the author to judge doubtful cases before distribution, and asks permission for
other purposes.

`MARIO.TXT` calls the compiled game freeware but permits copying only with no
changes to the game or associated files. Individual `VGA256.PAS`,
`KEYBOARD.PAS`, and `JOYSTICK.PAS` files carry Mike Wiering copyright notices.
The archive includes Nintendo-derived names and imagery; those rights could not
have been relicensed by the archive author.

Therefore:

- source availability is not treated as free/libre redistribution permission;
- the archive is a private behavioral and historical reference only;
- new C++ is written from observed mechanics and high-level facts, not copied
  line-for-line;
- no original levels, sprites, generated `$xx` sprite include files,
  executables, music, or game data are shipped;
- the project's MIT license applies only to newly written code and documents;
- a future proposal to include any original material must first document a
  specific rights grant and separately resolve Nintendo-derived content.

## Exact archive inventory

The archive contains 20 Pascal files:

```text
BACKGR.PAS   BLOCKS.PAS   BUFFERS.PAS  CPU286.PAS  ENEMIES.PAS
FIGURES.PAS  GLITTER.PAS  JOYSTICK.PAS KEYBOARD.PAS MARIO.PAS
MUSIC.PAS    PALETTES.PAS PLAY.PAS     PLAYERS.PAS STARS.PAS
STATUS.PAS   TMPOBJ.PAS   TXT.PAS      VGA256.PAS  WORLDS.PAS
```

Other inventory groups are 70 `.000`, 53 `.001`, 24 `.002`, 12 `.003`, 7
`.004`, and 1 `.005` binary sprite files; matching generated Pascal includes
comprise 70 `.$00`, 53 `.$01`, 24 `.$02`, 12 `.$03`, 7 `.$04`, and 1 `.$05`.
There are four `.BK` background-data includes, two `.EXE` tools, one `.OBJ`, one
`.PAL`, three `.TXT`, and three extensionless data files. `GRED.EXE` is the
included sprite editor; `BIN2PAS.EXE` converts data for Pascal inclusion;
`DEMOKEYS.OBJ` supports recorded demo input. These files are reference-only.

## Original architecture

The dependencies and responsibilities below are verified from unit interfaces
and implementations. “Useful” means behavioral research value, not permission
to reuse the code.

| Unit | Dependencies | Responsibility and important data/procedures | DOS-specific? | Reference value |
|---|---|---|---|---|
| `MARIO.PAS` | nearly all game units plus CRT/DOS | program entry, configuration/save records, command-line handling, intro/menu, one/two-player progression, six-stage dispatch | BIOS/DOS config and VGA lifecycle | stage order, initial lives/modes, progression |
| `PLAY.PAS` | player, entities, world drawing, palette, VGA, input/audio | `PlayWorld`, page-oriented frame loop, edge-column redraw, enemy activation, stage-clear/death sequencing, pause, pipe/subarea swap | deeply coupled to page flipping and palette/VGA restoration | authoritative orchestration, camera/streaming and transition timing |
| `PLAYERS.PAS` | collision/world, enemies, effects, input/audio | player state and pixel velocities, collision probes, jump/run input, damage, growth, star, projectile launch, pipe demos, camera tracking | background save/restore and raw keyboard conventions | primary player-feel oracle |
| `WORLDS.PAS` | `BUFFERS` | assembler procedures containing 13-byte map columns and packed `WorldOptions` records; six main stages and optional subareas | data hidden in code segment through assembler `db`/`dw` | map encoding, dimensions, options, pacing—not reusable public levels |
| `FIGURES.PAS` | buffers, palettes, VGA, background | sprite includes, recolor/mirror/rotate, tile-character rendering, neighbor-aware wall conversion, map preprocessing, sky/wall/pipe initialization | planar sprite buffers, palette indexes, code-segment data | tile semantics and preprocessing behavior |
| `ENEMIES.PAS` | figures, VGA, effects, temp objects, music, CRT | fixed 25-slot `EnemyRec` pool, activation/despawn, walkers, shell-like states, fish/fire hazards, plants, lifts, power-ups, projectile interactions | per-page saved backgrounds, embedded sprites | enemy archetype/state behavior and activation policy |
| `BACKGR.PAS` | buffers, VGA, palettes | procedural clouds, background map, arches/mountains, brick/pillar/window patterns, palette-driven parallax | inline assembly, direct VRAM and DAC manipulation | two parallax techniques and layer-speed ideas |
| `BLOCKS.PAS` | VGA, buffers, background | single bumping-block state, four-pixel rise/return, save/restore | VRAM image buffers | interactive-block timing |
| `BUFFERS.PAS` | CRT, VGA | global constants and buffers, `GameData`, `WorldOptions`, map orientation, subarea swap, score and PC-speaker switch | BIOS timer aliases, manual memory and PC speaker | central data model and collision sets |
| `VGA256.PAS` | hardware/BIOS | chained/planar Mode 13h, 360x182 virtual surface, two pages, CRTC scrolling, palette I/O, pixel/image routines, VRAM background stack | entirely VGA/BIOS/x86 assembly | historical rendering strategy only |
| `KEYBOARD.PAS` | DOS | interrupt-driven scan-code state, macro record/playback, key helpers | interrupt hooks and external object code | exact keyboard behavior; replace with CNA input |
| `JOYSTICK.PAS` | CRT | joystick detection/calibration and direction/button state | game-port timing loops | historical gamepad intent; replace with CNA gamepad |
| `MUSIC.PAS` | buffers, CRT | short PC-speaker frequency strings and playback counters | PC speaker | event timing only; no audio data reuse |
| `PALETTES.PAS` | VGA, buffers | 256-color palette storage, fades, locks, grass colors, cycling/blinking | VGA DAC I/O | fades/cycles as modern color/effect parameters |
| `TMPOBJ.PAS` | VGA, world/effects/audio | 20-slot temporary-object pool, block fragments, thrown coins, hit/fire/note effects, delayed tile replacement, extra life | saved per-page backgrounds | particles, interactive tiles, score/life behavior |
| `GLITTER.PAS` | VGA, buffers, CRT | 75-slot pixel sparkle system with timed cross-shaped stars | planar pixel writes/background bytes | particle behavior; do not reproduce per-pixel renderer |
| `STARS.PAS` | VGA, buffers, CRT | deterministic 320-column star field, slow camera offset and blink | direct VRAM | far parallax/star layer behavior |
| `STATUS.PAS` | buffers, text, VGA | lives, score, coins, level HUD with background stack | VRAM background restoration | HUD content |
| `TXT.PAS` | VGA, buffers | embedded proportional and 8x8 fonts, bold/shadow text | assembly font data/rendering | layout ideas only; original font is not reused |
| `CPU286.PAS` | CPU registers | rejects pre-286 CPUs | x86 assembly | preservation note only |

The actual VGA filename is `VGA256.PAS`, not `VGA.PAS` as the web prose and
archive README abbreviate it.

## Original world and level representation

### Geometry and orientation

- Tile width `W` is 20 pixels; tile height `H` is 14 pixels.
- The visible world is 16 columns by 13 rows: 320x182 pixels. The physical VGA
  mode is 320x200; the remainder supports presentation/status behavior.
- `MapBuffer` is `[1..236, 1..13]`. Source data is **column-major**: each `db`
  string is one 13-character vertical column.
- `ReadWorld` walks columns until the first byte of the next column is zero,
  reverses the 13 source characters vertically into world Y coordinates, adds
  one protected column on each horizontal side, and extends the bottom by three
  rows copied from the last map row. Eight rows of headroom exist above.
- Main stage widths in columns are 180, 234, 155, 202, 165, and 141. The game
  dispatch order is source levels 1, 2, 3, 5, 6, then 4.
- Subarea widths are 16, 0, 0, 117, 47, and 50 columns respectively. Zero-width
  stubs mean no second map for those stages.

### Level options

Each `WorldOptions` block stores two-word initial X/Y pixel coordinates followed
by sky type; three wall types; pipe palette index; two ground colors; horizon;
background type and two colors; star/cloud flags; design theme; two RGB triplets;
brick, wood and special-block colors. `BuildWall` and computed `XSize` fields are
filled at runtime. An alternate main-area options block (`Opt_Na`) enables the
post-completion “Turbo” variant.

### Tile/object semantics

The source uses characters as a compact authoring vocabulary and later mutates
many into renderer/collision codes:

- space is empty;
- `A`/`B` and `C`/`D` are neighbor-processed wall families converted to control
  bytes 1..13 and 14..26;
- `?`, `$`, `J`, and `K` are interactive/hidden/breakable/note blocks;
- `I`, `X`, and `W` are block, special block, and wood visuals;
- `0`..`3` are the four conduit/pipe quadrants;
- `*` is a collectible;
- `=` is a damaging spike/pin surface;
- `#` and `%` select theme-dependent waterfall/tree/window/lava visuals;
- several high-byte CP437 codes are enemy spawners, power-up contents,
  transition links, exits, decorative forms, and camera gates.

Collision sets are separate from rendering cases: `CanHoldYou` contains control
bytes 0..13 and ASCII `0`..`Z`; `CanStandOn` contains control bytes 14..16 and
`a`..`f`; `$` is hidden-solid for upward collision. This is an important design
lesson: the authoring glyph, transformed visual tile, and collision behavior are
related but not identical.

`BuildWorld` also expands macro markers (for example prefilled interactive
blocks and vertical copy/camera-gate markers), calculates wall adjacency, and
recolors sprite families. New Copper Boots levels will not copy this alphabet or
layout. They use an external row-major UTF-8 text format with an explicit legend
and independent tile semantics.

### Transitions and subareas

Conduits are two tiles wide. A player must be centered, stationary, aligned to a
tile row, and press Down to enter a valid downward route. Upward entry additionally
requires Up while jumping into a matching conduit. Two encoded link characters
form a route identity. The play loop either finds another matching link in the
same map, swaps main and saved subarea maps then finds its partner, or marks the
stage complete. Entry/exit is a short masked player animation followed by a
fade/rebuild where needed.

## Original gameplay behavior matrix

The old simulation advances once per displayed page. `ShowPage` waits outside
then inside vertical retrace, so nominal Mode 13h hardware ties gameplay to the
roughly 70 Hz VGA refresh rather than a clock-derived fixed timestep.

### DOSBox executable measurement

The official executable was run locally on 2026-08-24 in DOSBox 0.74-3. This is
a behavioral observation, not a redistribution or compatibility requirement.
The emulator used `machine=svga_s3`, `core=normal`, `cputype=486_slow`, fixed
20,000 cycles, `frameskip=0`, `output=surface`, `scaler=normal2x`, sound disabled
and joystick disabled. Its 640x400 X11 window was captured losslessly at 70 fps;
positions below are converted back to 320x200 logical VGA pixels. The manual
trace used level 1 with no save and one player. The repeatable built-in demo was
used for the camera/enemy trace.

Wall-clock results are specific to those emulator settings and have roughly
one captured-frame (14 ms) timing precision. Source constants are included as a
cross-check, not substituted for the observed values:

| Behavior | Executable observation | Source cross-check |
|---|---|---|
| standing held jump | first airborne to apex: about 0.329 s (23 nominal VGA frames); airtime: about 0.686 s (48 frames); apex 61 +/- 1 px above the floor | `JumpVel=4`, gravity step every `JumpDelay=6` loops, terminal `MaxYVel=8` |
| walking acceleration | after the input sampling delay, motion changed directly to 1 px on each consecutive 70-fps sample rather than passing through subpixel speeds | integer X velocity changes only when `Counter mod Slip=0`; `Slip=6`, walking cap 1 px/loop |
| walking stop | the timed release coasted approximately 4 px for 0.057 s before the X position became stable | walking velocity drops from 1 to 0 on the next sixth-loop gate, so the exact coast depends on release phase |
| camera lag/dead zone | in the demo, the player crossed from screen x about 52 to 188-192 before the world began following; at observed run speed this was about 1.0 s, after which the player remained near x=190 | `SCROLL_AT=112` starts rightward scrolling when player-left exceeds 188 px; view motion is capped at 2 px/loop (3 in Turbo) |
| first ground walker | with the camera still fixed, its left edge moved 12 px in 0.50 s: approximately 24 px/s | spawn velocity is 1 px with `MoveDelay=2`, producing one world pixel per three simulation loops through interpolation |

The horizontal trace also visually confirms the source's discrete character:
walking starts/stops on a six-loop gate, while a held walk is the exact integer
1 px/loop. Ctrl raises the source cap to 2 px/loop; it does not introduce a
separate continuous acceleration curve.

### Modern physics comparison

Copper Boots deliberately keeps deterministic 60 Hz units rather than tying
simulation to presentation. Exact current held-jump values follow directly from
the tested 330 px/s impulse and 1,200 px/s-squared gravity: apex at tick 16
(0.267 s), height 42.7 px, and landing at tick 32 (0.533 s). The comparison is:

| Property | 1994 observation/source | Copper Boots | Decision |
|---|---|---|---|
| walk cap | 1 px/roughly-70-Hz loop; observed about 70 px/s | 72 px/s | intentionally close |
| run cap | 2 px/loop, nominally about 140 px/s | 128 px/s | slightly slower for the denser 16 px tile layout |
| acceleration | one integer speed step every six loops; walk therefore starts in at most about 86 ms | 720 px/s-squared; walk cap in 6 fixed ticks (0.100 s) | preserve short momentum ramp without integer stutter |
| ground stop | observed about 4 px/0.057 s for that release phase; source permits 0-5 loops at walking speed | 900 px/s-squared; walk stops in 5 ticks (0.083 s), about 2.3 px after release | similarly prompt, deterministic at every phase |
| held jump | 61 px; apex 0.329 s; airtime 0.686 s | 42.7 px; apex 0.267 s; airtime 0.533 s | intentionally more compact; original used 14 px-high tiles while the new route uses 16 px squares |
| variable jump | releasing Alt applies the next gravity step immediately | 2.2x gravity while rising after release | same visible intent with continuous units |
| enemy bounce | base jump when not held; `-6` versus base `-4` when Alt is held on contact | automatic 300 px/s; held 390 px/s, with the high case clearing more than three new tiles | separately tuned after playability testing; not inferred from sprite identity |
| basic walker | observed about 24 px/s | clockwork crawler 24 px/s | direct feel match under new art/behavior |
| camera | wide 112 px edge band, then hard per-loop catch-up; observed right anchor about x=190 | continuous exponential follow, 0.28x velocity look-ahead capped at 34 px; 63% response in 0.125 s | intentionally earlier and smoother for widescreen/resizing, while preserving momentum look-ahead |

No constants were changed merely to make the numerical columns identical. The
external level geometry and deterministic tests remain the acceptance oracle;
future feel changes must update both this comparison and the tick-based tests.

### Player

| Behavior | Verified source behavior | Copper Boots direction |
|---|---|---|
| state | ground, jumping, falling; demo modes cover conduit travel and death | explicit grounded/jumping/falling/dead/transition flags with abilities orthogonal |
| acceleration | integer X velocity changes by 1 every sixth loop (`Slip=6`) | per-60-Hz acceleration with source-inspired momentum, tuned by tests |
| walk/run speed | max magnitude 1 pixel/tick walking, 2 with Ctrl; completed-game Turbo can add another 1 | 72 px/s walk and 128 px/s run starting targets |
| deceleration | velocity steps one unit toward zero every sixth loop without input | deterministic ground friction; lower air drag |
| reversal | direction changes immediately but velocity decelerates/accelerates through zero; simultaneous left/right preserves prior motion | preserve momentum and neutral simultaneous input |
| jump | base Y velocity -4; enemy-assisted held jump can be -6; Turbo subtracts one more | impulse-based jump, reproducible apex tests |
| variable height | while rising, gravity is applied every 6 or 7 loops; releasing Alt applies it immediately, shortening ascent | early-release gravity multiplier |
| high jump | full-speed horizontal motion or enemy bounce with Alt selects the slower rising gravity interval | running jump has modest extra reach, not a separate mode |
| air control | horizontal input uses the same velocity code in every vertical state | retain responsive but bounded air control |
| gravity/fall | Y velocity increments every sixth loop when unsupported, capped at 8; jump ascent transitions to fall at zero | continuous 60-Hz units, capped terminal fall speed |
| body | width 20; large/fire visual is 28 pixels high, small collision effectively uses lower 14 pixels | clear AABB independent from sprite art |
| horizontal collision | samples leading column at up to three heights and zeroes X velocity | swept per-axis tile collision |
| floor collision | samples both feet, includes one-way set, snaps to tile boundary | explicit Solid/OneWay semantics |
| ceiling collision | samples both head corners/center; activates blocks; normally cancels rise | explicit ceiling resolution and block event |
| damage | small state starts death; large/fire shrinks to small | plated state absorbs one hit; theme is original |
| invulnerability | damage blinking lasts 125 loops; star lasts 750; growth flash lasts 24 | timer values converted after executable timing measurement |
| death/respawn | hit or falling below map triggers death sequence, removes a life, resets size, restarts; initial lives are 3 | checkpoint respawn and short fail transition |
| animation | two walk frames, rising/falling frames, direction mirroring, flashing/growth recolor | state-driven procedural shapes first, sprite animation later |

### World interactions

| Interaction | Original behavior |
|---|---|
| solid walls | block from sides, floor, and ceiling according to collision sets |
| one-way surfaces | support the player/enemies when descending; not used as side walls |
| interactive blocks | bump four pixels; coin/content may emerge; used block replaces original |
| breakable block | large player breaks it from below into temporary fragments |
| note block | bounces the player upward and animates the block |
| collectible | direct touch or block hit adds coin/score and sparkle; 100 coins add a life |
| hazard | spike/pin contact sets player-hit; lava/waterfall meaning is theme-dependent |
| conduit | centered directional entry, matching route pair, same-area/subarea/exit behaviors |
| moving platform | lift entities carry player velocity; donut platform begins falling after standing |
| exit | encoded transition marks `Passed`, then score count and stage progression |
| camera gates | preprocessing records left/right gates in a sentinel row; camera cannot cross while the player's row is obstructed |

### Enemies and moving objects

| Archetype | Original behavior | Original-themed replacement direction |
|---|---|---|
| simple walker (`tpChibibo`) | walks, reverses at walls/entities, falls from edges, flattens when stomped, is launched when hit otherwise | clockwork crawler |
| red walker | similar ground movement and defeat path | spark beetle |
| shell walker | green/red subtype; stomp cycles through sleeping/waking/walking, side contact kicks fast shell, shell defeats enemies and some blocks; red subtype has edge-aware stopping | armored roller |
| vertical fish | waits below stage, leaps vertically at intervals, falls back; stomp/fire/star interactions | springfin automaton |
| vertical fire hazard | periodically leaps from below and damages on contact | furnace bolt |
| plant | rises, waits about 200 updates, descends, waits random interval; subtype changes player-proximity gating | conduit sentry |
| lift | horizontal/vertical platform selected from neighboring solids | rail platform |
| donut | stationary support starts falling after prolonged player contact | drop plate |

Enemies live in a 25-slot fixed pool. Spawner glyphs activate about two columns
beyond the viewport; entities more than five columns beyond the active region
are restored to the map so backtracking can reactivate them. The projectile
ability allows at most two live shots, supports upward/level/downward aim, moves
horizontally, falls, and bounces from floors. Power-ups emerge from blocks:
growth, extra life, projectile form, and temporary invulnerability. Copper Boots
will preserve selected mechanics under wholly original names and visuals.

### Score and lives

The game starts each player at three lives, small form, zero coins and score.
Typical enemy defeats award 100, power-ups 1000, block breaks 10. Coins accumulate;
`AddLife` subtracts 100 coins and adds a life when the threshold is reached.
Stage-end `LevelScore` counts into the total in 50-point steps.

### Verified controls

Archive documentation and input use agree on:

```text
Left / Right          walk
Ctrl + Left / Right   run
Alt                   jump
Space                 fire; Up/Down alter aim
Down                  enter selected downward conduits
Up                    enter selected upward conduits while jumping
Escape                quit/back
P                     pause
Q                     sound toggle
```

Joystick button 1 maps to jump; button 2 is used for run/fire in the shared
state. Modern keyboard and gamepad defaults will separate these actions.

## Historical rendering analysis

### VGA organization

The game enters BIOS mode 13h, then changes VGA sequencer/graphics-controller
registers into a planar, byte-addressable chained arrangement. Four planes hold
interleaved pixels. A 360-pixel virtual scanline (`320 + 2*20`) is wider than
the visible display and the world-height virtual area is 182 pixels.

Two pages begin at offsets 0 and `0x8000`. The CRTC start address and attribute
controller fine-pan register expose the chosen page and pixel X offset. The
game renders into the non-visible page, waits for vertical retrace, presents it,
then swaps page roles.

### Incremental scrolling and restoration

Because each page preserves most prior background pixels, horizontal movement
does not redraw the whole tile map. `PLAY.PAS` compares current X view with the
last view for that page and redraws complete 20x14 tiles only along the newly
exposed edge. The two extra virtual columns keep those edge tiles addressable.

Moving sprites do not composite from a retained scene graph. `PushBackGr`
copies the covered VGA region into remaining video memory, the sprite draws,
and `PopBackGr` restores the saved bytes on that page before the next update.
Player, enemies, HUD, particles, and temporary objects coordinate their saved
background stacks carefully.

### Palette and parallax

The 256-entry VGA DAC is both color storage and an animation device. Fades,
sparkles, water/lava changes, player flashes, and some background shading alter
palette entries rather than source pixels.

Parallax has at least two verified routes:

1. level-one arches/cloud structures retain a compact background map and redraw
   only pixels that become different as the slower offset moves;
2. bricks, pillars, windows, mountains, and related layers use camera-divided
   offsets plus palette/color-map manipulation, allowing apparent independent
   motion with little bus traffic.

Stars use a deterministic per-column map and an X offset derived from camera
position at a slower divisor. These are clever 486-era bandwidth strategies,
not modern rendering requirements.

### Embedded data and memory pressure

Turbo Pascal's data segment is limited to 64 KiB. GRED emits textual `.$00`,
`.$01`, and similar assembler includes so sprite bytes reside in the code
segment. Levels are assembler `db` procedures for the same reason. Large world,
picture, star-background, and enemy buffers are allocated manually on the heap.
`CPU286.PAS` rejects older processors; the stated performance target was a 25
MHz 486 and the finished executable remained under 64 KiB.

## CNA replacement map

| Historical mechanism | Copper Boots / CNA mechanism |
|---|---|
| BIOS mode and VGA registers | `Game`, `GraphicsDeviceManager`, CNA window/platform lifecycle |
| CRTC page flip | CNA backbuffer presentation and optional `RenderTarget2D` |
| 360-pixel virtual screen | visible-tile range plus one-column culling margin |
| VRAM sprite background stack | redraw ordered layers with `SpriteBatch` |
| planar embedded sprite bytes | generated `Texture2D` initially; content textures/atlas later |
| palette RGB writes | tint colors, texture variants, optional CNA effect parameters |
| edge-only tile restore | batched visible tile rendering; static chunk cache only if measured useful |
| DOS keyboard interrupt | `Keyboard::GetState()` and `KeyboardState` |
| game-port joystick | CNA `GamePad` APIs |
| PC speaker | CNA `SoundEffect`/audio APIs with new audio only |
| BIOS timer/page cadence | CNA `GameTime` feeding a clamped fixed-step accumulator |
| source-encoded maps | versioned external text levels opened through CNA `TitleContainer`/content APIs |
| DOS config file | versioned settings/save through CNA/Sharp Runtime storage APIs |
| assembly text | CNA `SpriteFont` or an original generated bitmap font |

Game source may include XNA-style public headers supplied by CNA but must not
include SDL, renderer backend, native window, or platform SDK headers.

## Logical presentation decision

The initial target is **320x180**, rendered to `RenderTarget2D` and scaled to the
backbuffer with `SamplerState::PointClamp`. It preserves 320-pixel DOS-era
horizontal density, is natively 16:9, and fits a clean 20x11.25 or 16x11.25 tile
view depending on final tile size. The new game uses 16-pixel tiles for simpler
square art and a 20x11.25 visible grid; subpixel physics remains independent.

Alternatives considered:

- 320x200 exactly matches the physical VGA mode but letterboxes on 16:9 and the
  original only used 182 lines for its world;
- 400x225 offers more view and even 16:9 scaling but loses some DOS intimacy;
- 640x360 is easier for detailed art but weakens the intended pixel language.

The backbuffer starts at 960x540, an exact 3x scale. Resize draws the largest
integer-scaled 16:9 destination centered with black bars. A future native
widescreen mode can change camera viewport without changing simulation.
Render-target support is present in the pinned CNA public API. If a selected CNA
renderer reports or demonstrates a defect, direct logical-coordinate drawing is
an explicit compatibility fallback to track, never a backend bypass.

The initial procedural palette is deliberately small and used through one
generated white texel plus `SpriteBatch` tinting. Key entries are:

| Role | RGB / hex |
|---|---|
| letterbox/night ink | `8,10,18` / `#080A12` |
| sky | `42,74,105` / `#2A4A69` |
| far ruins | `60,83,112` / `#3C5370` |
| mid green stone | `58,107,104` / `#3A6B68` |
| near foliage | `70,126,95` / `#467E5F` |
| ruin body/highlight | `118,82,53` / `#765235`; `166,142,69` / `#A68E45` |
| courier copper | `205,119,42` / `#CD772A` |
| plated green | `79,157,124` / `#4F9D7C` |
| hazard ember | `226,100,70` / `#E26446` |
| exit teal | `95,192,158` / `#5FC09E` |
| debug/reserved bright | `235,189,67` / `#EBBD43` |

All current art is composed from these original rectangles; the shipping game
requires no bitmap asset.

## Modern architecture

### Modules and ownership

```text
game/
  include/CopperBoots/   CNA-facing game, renderers, input adapter
  src/                   implementations and program entry
gameplay/
  include/CopperBoots/   renderer-free value types and simulation API
  src/                   level, player, collision, camera simulation
Content/Levels/          original row-major level files
test/                    deterministic simulation/loader tests
```

The `CopperBootsGame` object owns CNA graphics resources and converts current
keyboard/gamepad state to a small `PlayerInput` value. `WorldSimulation` owns
level state, player, camera target, and later entities. Rendering reads an
immutable snapshot; gameplay never stores textures or a graphics device.

No generic ECS is planned. Entities use clear concrete types and bounded
vectors until profiling or behavior proves a stronger need.

### Deterministic simulation

- fixed update step: 1/60 second;
- accumulator driven by CNA `GameTime`;
- frame delta clamped to 250 ms;
- maximum eight catch-up steps per rendered frame, dropping excess backlog;
- velocities stored as floating-point pixels/second initially, with all state
  mutation occurring only on fixed ticks;
- edge-triggered actions (jump/attack) latched across render frames;
- test inputs are scripted by exact tick number;
- optional render interpolation is deferred until needed.

The source-inspired initial controller target is roughly 72 px/s walking,
128 px/s running, 720 px/s² ground acceleration, 900 px/s² ground braking,
1200 px/s² gravity, a 330 px/s jump impulse, and a 2.2x early-release gravity
multiplier. These are new 60-Hz values, not claimed as exact conversions. Tests
pin acceleration time, apex tick/range, stop distance, collision, and camera
bounds so tuning is intentional.

The current jump policy is deliberately ground-only: there is no coyote time or
jump buffer yet. `JumpPressed` is an edge event latched by the CNA input adapter
until a fixed tick consumes it. `JumpHeld` controls early-release gravity and
selects the stronger stomp bounce, but landing while the button remains held
cannot trigger another ordinary jump.

### Tiles and collision

Visual tile ID and collision semantic are separate fields. Initial semantics:

```text
Empty  Solid  OneWay  Breakable  Hazard  Interactive  Exit  Transition
```

The player uses a floating position and AABB, integrates X and Y separately,
queries only overlapped cells, snaps to blocking faces, and records grounded,
ceiling-hit, hazard, and transition contacts. Tile lookup outside the map is
solid horizontally, while above and below are open; the open lower boundary is
required for deterministic fall death. Tests need no CNA device.

`OneWay` is implemented independently from `Solid`: horizontal and upward
queries ignore it, while a downward step lands only when the previous and
current player feet cross the platform top. This avoids treating its visual as
a wall and keeps stable landing snaps under repeated fixed updates. Slopes are
not supported and are rejected as unknown level glyphs rather than approximated.

### Camera and parallax

`Camera2D` owns viewport size, world bounds, current/target position,
velocity-based horizontal look-ahead, smoothing, vertical policy, and shake.
The player never writes camera coordinates directly. The first level uses
procedural layers at factors 0.10, 0.25, 0.50, 1.00, and optionally 1.10 for
foreground pieces. Repeating geometry is drawn from stable formulas without
per-frame heap allocation.

The implemented camera exposes clamped base coordinates separately from a
clamped shake offset and supports `Follow`/`Locked` vertical policies. Parallax
uses a renderer-neutral `ParallaxLayer` descriptor carrying factor, depth,
spacing, geometry, fixed/repeating flags and RGB tint. Its wrapped offset is
tested across positive, negative and seam coordinates; CNA rendering consumes a
stack `std::array` already ordered by depth.

### Data format

The first format is deliberately transparent, line-oriented text:

```text
copper-boots-level 1
name Green Ruins Relay
size 110 12
spawn 3 9
checkpoint 3 9
parallax 0.10 0.25 0.50
theme green-ruins
initial-area main
endpoint relay-hatch main 43 9
endpoint cache-hatch conduit 96 9
route relay-hatch cache-hatch
route cache-hatch relay-hatch
platform horizontal 54 7 2 5 30
platform vertical 66 8 2 -3 24
platform drop 99 7 2 2 48 30
legend
. empty
# solid
- one-way
B breakable
! hazard
E exit
d decoration
G cog object
? cog block
o empty interactive block
P plated-jacket block
A plated-jacket pickup object
R arc-capacitor block
K arc-capacitor pickup object
H checkpoint object
C clockwork crawler object
c ledge-falling crawler object
map
...row-major UTF-8/ASCII rows...
```

The loader validates version, dimensions, row widths, recognized glyphs,
exactly one spawn, reachable in-bounds coordinates where structural checks are
practical, and reports line-numbered errors. It converts glyphs to visual and
semantic tile records. The historical character codes are not reused.

The version-1 grammar is deliberately strict and ordered. Unknown directives,
duplicate directives, unknown themes, unknown glyphs, and non-empty data after
the declared map are errors rather than silently ignored extensions. An
optional `theme green-ruins|factory` directive selects presentation without
putting renderer state into the simulation; omitted theme remains Green Ruins
for compatibility. Compatible route metadata occupies the documented section
between `parallax` and `legend`:
`initial-area`, named `endpoint` records carrying area and standing-foot tile,
and directed `route` links. Existing files with an empty metadata section remain
valid. Endpoint names and route sources are unique, references and clear solid
landing cells are validated, and self-links fail with line-numbered errors.
Bidirectional and longer cycles are intentionally legal because every hop needs
a fresh aligned interaction after the input-release lock.

Moving geometry uses metadata rather than collision glyphs. Horizontal and
vertical records are `platform KIND X Y WIDTH_TILES TRAVEL_TILES SPEED`; travel
is signed and the platform oscillates between its origin and destination. A
drop record adds `DELAY_TICKS`, permits only positive/downward travel, waits
after its first supported rider, then falls once to the configured endpoint.
The parser bounds-checks origin, width, destination, speed and delay. All three
are one-way top surfaces with a six-pixel procedural body; riders inherit the
exact platform displacement before their own movement, then gravity re-snaps
feet to the current top. Static wall/ceiling resolution wins over carrying, so a
platform cannot force the courier into terrain. Tests cover stable horizontal
and vertical carrying, wall obstruction, delayed fall, landing from above,
endpoint clamping and state-hash participation. Green Ruins places one of each
kind in otherwise clear paths.

Route activation requires grounded Down/S input within four pixels of the
source center. Simulation then aligns and locks the courier for 30 fixed ticks,
fades through a CNA-drawn full cover, moves to the named destination at tick 15,
updates the logical area and snaps the camera, then restores grounded motion.
The transition exposes one-tick start/destination/completion events and a pure
fade amount. Same-area and cross-area routes share this small mechanism; maps
may place a physically separated subarea in the same external tile grid without
a general scene engine.

Green Ruins uses the model for an original optional cache. Its 80-tile main
route remains unchanged while a sealed 30-tile horizontal extension holds a
separate `conduit` area with three cogs and a free arc capacitor. A generated
hatch sits three tiles after the checkpoint; proximity draws a small `DOWN`
prompt. The destination hatch offers the same discoverable return interaction.
Because the cache is beyond the frozen main exit and enclosed by solids, normal
camera scrolling cannot reveal or enter it.

`G` is an object marker, not a visual/collision tile: loading extracts its tile
coordinate into a cog list and leaves empty terrain behind. The fixed-step world
owns collected state, emits a one-tick event, awards 100 points exactly once,
and resets transient cog progress when a level is reloaded. This establishes the
same data path future enemies and pickups can use without making decorative map
glyphs physically solid.

Interactive `?`/`o` blocks likewise carry content metadata outside `TileMap`.
Ceiling collision emits block events and drives an eight-tick visual offset;
the collision cell never moves. First contact changes an interactive tile to a
solid used-block visual and optionally emits a new cog. A `B` tile remains solid
for an unplated courier and is removed by a plated ceiling hit. These are clean
new mechanics and glyphs rather than translations of the historical map data.

Clockwork crawlers are extracted from `C`/`c` object markers. `C` patrols and
turns at walls or ledges; `c` uses the same patrol with deterministic gravity
after leaving a ledge. Crawler simulation sleeps outside the camera margin but
retains active/defeated state. Player contact compares previous and current
bounds for both bodies. A stomp requires downward relative motion, a top-plane
crossing, and at least two pixels of interpolated horizontal overlap; underside
and thin corner contacts remain harmful. When a tick contains both a stomp and
a side contact, the stomp wins, while harmful-only contacts apply damage at most
once. Crawler pairs restore their non-overlapping previous horizontal poses and
turn outward in stable level order. Generated body/eye/leg rectangles are
presentation only.

A crawler stomp now applies a 300 px/s automatic upward bounce. Holding Jump at
contact selects a 390 px/s bounce, echoing the historical game's stronger held
enemy-assisted jump and clearing more than three tiles in the deterministic
test room. This is intentionally stronger than the earlier 190 px/s placeholder,
which rose only about one tile and made the designed Green Ruins route feel
broken.

Courier animation is likewise procedural and presentation-only. A pure selector
maps motion state and simulation tick to idle, two-frame walk, faster two-frame
run, rise, fall, damage-blink, and flattened death poses. Facing mirrors limb
placement while leaving the fixed 12x20 collision body untouched; plating and
capacitor transitions change colors, not bounds.

Damage now has a bounded lifecycle. Plating is consumed by one crawler hit and
starts 75 invulnerable ticks; an unprotected enemy hit, hazard overlap, or fall
below the open lower map boundary enters a 45-tick dead state and decrements one
life exactly once. Respawn uses the external checkpoint rather than the initial
spawn, preserves level object progress, resets player transient state, and snaps
the camera. There is not yet a game-over screen when the life counter reaches
zero; that remains UI/campaign policy rather than collision behavior.

Plating is acquired through the normal level pipeline. A `P` interactive block
releases a jacket module that rises 12 pixels over 24 fixed ticks, then walks at
28 pixels/second with tile gravity and wall reversal. `A` creates the same state
as a free pickup. Collection is one-shot, grants plating, awards 500 points and
drives an 18-tick palette-like courier flash; crawler contact consumes the
protection under the damage rules above.

The arc capacitor follows the same pattern through `R` block and `K` free
markers. Collection enables an edge-triggered attack backed by a fixed two-slot
projectile array. Facing and an aim value select level, upward or downward
launch velocities; fixed gravity produces floor bounce, horizontal solid
contact retires the shot, crawler overlap defeats once, and camera/world margins
clean up survivors. Game input derives the attack edge from CNA Ctrl key state
and accepts Up/Down or W/S for aim.

Keyboard and CNA gamepad state feed a renderer-free action adapter. Digital
opposites are neutral, the left stick uses a 0.20 dead zone, and
jump/attack/pause edges remain pending until a fixed tick or UI toggle consumes
them. Defaults are A/D or arrows plus Shift/Space/Ctrl, and gamepad left
stick/D-pad plus X/A/B; Y/Down is interaction and Start/Escape pauses. The pause
path does not feed wall time into the accumulator and offers resume, player
restart and quit without latent action edges.

Settings use a strict line-oriented `copper-boots-settings 1` document rather
than serialized C++ memory. The renderer-free codec stores master/effects
volume, fullscreen, integer/fit presentation, and two keyboard keys for each of
nine actions. Version 0's single sound volume migrates into the new model while
new fields receive documented defaults; missing or malformed documents reset to
the same canonical defaults and are rewritten. Round-trip, migration and reset
paths are logic-tested. Presentation maps the bindings to CNA `Keys`, applies
volume before CNA `SoundEffect::Play`, requests fullscreen through
`GraphicsDeviceManager`, and chooses the render-target destination rectangle.
F11/F2 changes are persisted immediately.

The storage adapter uses CNA `StorageDevice`/`StorageContainer` and
sharp-runtime `StreamReader`/`StreamWriter` only. It sets the application name
to `CopperBoots`, opens the `Settings` all-player container, and accesses the
fixed relative `settings.cfg` name. Storage failure is non-fatal and leaves the
session on defaults. Automated graphics smoke tests pass `--no-settings`, so
they cannot mutate a user profile; a separate isolated runtime check under a
temporary `XDG_DATA_HOME` verified create-then-load behavior and canonical file
contents.

Progress uses a separate strict version-1 document with explicit unlocked-stage,
best-score and best-completion-tick fields. CNA storage intentionally lacks a
public atomic rename primitive, so the implementation does not fake one with a
native API. It alternates `progress-a.cfg` and `progress-b.cfg`; each slot has a
monotonic generation and 64-bit FNV-1a checksum over its canonical payload.
Loading selects the highest valid generation. Saving writes only the other slot,
leaving the current valid generation recoverable if creation, writing or flush is
interrupted. Corrupt-newest fallback, two-invalid reset, generation selection,
slot alternation and score/time update policy are deterministic logic tests.
A CNA-only storage CTest runs in an isolated build-local data root and verified
settings plus two successive progress generations without opening a window.

F1 independently toggles a CNA-rendered debug overlay. It outlines visible tile
collision cells, the player, active crawlers and the 320x180 camera viewport;
text reports player tile, signed velocity, camera edges, simulation tick, world
sprite submissions, active entities, gameplay vector growth, frame time, update
CPU time and prior draw CPU time. With
the overlay disabled, timing clocks and collision-grid traversal are skipped;
the normal path pays only the F1 edge check and a lightweight draw counter.

The pinned CNA public `GraphicsDevice` has no renderer-neutral draw-call
statistic. A few renderer-internal types expose bespoke counters, but consuming
them would violate the abstraction and make the overlay backend-specific;
therefore it explicitly displays `DC NA` and reports the exact game-side
SpriteBatch submission count instead. Entity counting walks fixed live vectors
without allocation. Level load pre-counts cog/plating/capacitor block contents
and reserves their final vector capacities, so later block release cannot
reallocate. The only simulation push sites use a capacity-growth counter, and
tests release every content type while requiring that counter to remain zero.
Tile rendering reads the persistent `TileMap` in visible bounds and never
rebuilds static tile data per frame.

Brassworks Shift is a new 96x12 industrial stage and is not derived from the
historical six layouts. Its route is deliberately organized around measured
moving geometry: a horizontal carrier crosses the first pit, a vertical lift
opens the middle route, a triggered drop platform spans a hazard bed, and a
faster horizontal carrier crosses the final pit. It has its own parallax and
tile palette, eight cogs, two crawler variants, a later checkpoint, plating and
capacitor blocks, hazards, and an exit. The shipping-content test parses the
external file and checks those structural counts and pit/exit geometry.
An SDL_RENDERER run on the real Linux desktop loaded stage 2 from the copied
shipping content, initialized the 320x180 target and generated audio, and
presented at 960x540; forced X11 capture was unavailable because the active
desktop session uses Wayland, so this is startup evidence rather than a stored
pixel capture or full playthrough claim.

Campaign order is renderer-free data: Green Ruins Relay is stage 1 and
Brassworks Shift is stage 2. Completion persists the next unlock, presents the
existing 60-tick transition, then loads the next external stage through CNA's
`TitleContainer`; final-stage completion remains on its result screen. A
validated one-based `--stage NUMBER` option supports direct development runs.

The milestone HUD is also asset-free: a new 3x5 glyph table renders the external
level name, cogs, lives and score through the same one-texel CNA `SpriteBatch`
path, with colored plating/capacitor indicators. It reads const world accessors,
uses arithmetic fixed-width number drawing instead of per-frame strings, and
has no effect on simulation state.

Audio is likewise generated and independent of historical assets. A pure
renderer-free generator produces deterministic 22.05 kHz mono signed 16-bit
PCM for jump, cog, hit, crawler defeat, projectile, block, completion and UI
cues. World simulation emits short-lived semantic counters; presentation maps
them to CNA `SoundEffect` instances without exposing CNA types to gameplay
logic. `--no-audio` skips sound construction, while construction or playback
failure is caught and permanently degrades the session to silence. Tests compare
repeated PCM bytes and exercise both enabled and silent CNA runtime paths.

Optional `H` objects activate a later respawn coordinate while leaving terrain
empty; death preserves collected, used-block and defeated-enemy state. `E` tile
overlap produces one `LevelResult` containing score, cog count and completion
tick, changes player presentation to transition, and freezes the fixed-step
world. CNA presentation grows a 60-tick top/bottom-bar transition; campaign
code consumes the structured result only after that transition completes.

## CNA and sharp-runtime baseline

Initial local dependency inspection on 2026-08-24 found:

- CNA branch `develop`, commit
  `1bb2145d99ed572dd4eb15009c34e2e5f410fcf0` (2026-08-20), with one unrelated
  untracked discovery JSON already present;
- sharp-runtime branch `develop`, commit
  `54578590b328aa9612fe38bfddca9fd8ca795144` (2026-08-22);
- CNA exposes aggregate target `CNA`, which transitively composes framework,
  selected renderer, platform, and sharp-runtime components;
- public API names verified include `Game`, `GraphicsDeviceManager`,
  `SpriteBatch`, `Texture2D`, `RenderTarget2D`, `GraphicsDevice::SetRenderTarget`,
  `SamplerState::PointClamp`, `Keyboard::GetState`, and `GamePad::GetState`;
- CNA's `ContentManager` surface is intentionally partial; text levels can be
  opened with `TitleContainer::OpenStream` as current CNA samples do;
- `SDL_RENDERER` is CNA's mature 2D-focused initial lane; game code still links
  only the public `CNA` target.

Two initially recorded embedded-consumer issues were already resolved inside
the pinned CNA history by upstream commit
`6000e7936aaa3d364a233aa1066f9aa7c766e40e`: both the layout validator and
vendored `cgltf`/`stb` include paths use `CNA_SOURCE_DIR`. A minimal consumer
with its own root `src/main.cpp` configured successfully against the pinned CNA,
and Copper Boots builds `cna_content` without inherited include workarounds.
The existing `game/` and `gameplay/` organization remains a project choice, not
a CNA constraint.

## Testing and compatibility strategy

Logic tests cover level parsing, tile lookup, acceleration, run cap,
deceleration, jump apex, early release, floor/wall/ceiling collision, camera
bounds, enemy state transitions, relative-motion contact classification,
simultaneous-contact priority, projectiles, and transitions. Tests link the
gameplay library only. A canonical 64-bit FNV-1a hash serializes persistent
simulation fields explicitly (including float bit patterns, dynamic tiles,
entities, camera, progress and route state) without hashing C++ padding or raw
object layouts. Two independent 180-tick scripted worlds must match after every
tick, while divergent input must change the hash. CTest labels the suite
`logic;deterministic` and the runtime lane `graphics;smoke;cna`.

A graphics smoke test creates a CNA game, programmatic 1x1 texture, logical
render target, SpriteBatch pass, point-scaled presentation, and exits after a
small frame count. Runtime reports selected renderer and important
capabilities. Later compatibility records use:

| Renderer / platform | Configure | Build | Startup | SpriteBatch | Texture | RT | Point | Input | Audio | Boundaries / defects |
|---|---|---|---|---|---|---|---|---|---|---|
| SDL_RENDERER / SDL3 | pass, Debug | pass, game + tests | pass offscreen and real Linux desktop | API smoke pass | generated 1x1 pass | 320x180 bind/sample pass | API smoke pass | keyboard + disconnected gamepad pass | generated `SoundEffect` pass on dummy device | offscreen cannot minimize/restore or find an exclusive-fullscreen mode; real desktop lifecycle passes; no pixel readback in game test |
| SOFTWARE / SDL3 | pass, Debug | pass, game + tests | pass, CPU framebuffer/no renderer window | API smoke pass | generated 1x1 pass | 320x180 bind/sample pass | API smoke pass | keyboard + disconnected gamepad pass through CNA SDL3 platform | generated `SoundEffect` pass on dummy CNA SDL3 audio | no visible OS presentation by renderer design, so fullscreen is not visually validated; no pixel readback in game test |

Only available/mature lanes are tested; a compile result is never mislabeled as
a runtime result. The renderer and platform are separate CNA axes: SOFTWARE is
a genuine non-SDL renderer even though this configuration deliberately retains
the SDL3 platform services for input and audio. Both matrix rows ran all six
project CTests at CNA `1bb2145d99ed572dd4eb15009c34e2e5f410fcf0` and
sharp-runtime `54578590b328aa9612fe38bfddca9fd8ca795144` on 2026-08-24.
No orientation, first-use, render-target preservation or point-sampling defect
was reproduced, so `MAR-CNA-003` is closed rather than carrying a speculative
framework bug. A future visual mismatch requires capture-level expected/actual
evidence before that task is reopened.

The initial Debug build configured with CMake/Ninja and
`CNA_GRAPHICS_RENDERER=SDL_RENDERER`, then built with two jobs. Renderer-free
CTest coverage passed. Two three-frame runtime smoke tests passed using CNA's
compiled-in offscreen platform route with a dummy audio device; it exercised
window/game initialization, `Texture2D`, `SpriteBatch`, a 320x180
`RenderTarget2D`, point-filtered presentation, keyboard state acquisition, and
clean game exit. The enabled test constructed every generated `SoundEffect`;
the explicit `--no-audio` test verified the silent path. Dummy audio validates
the CNA API path and lifetime, not acoustic output or subjective mix quality.
`xvfb-run` could not provide an SDL video device in this
container, so Linux CTest explicitly selects the offscreen driver for repeatable
headless validation.

Presentation sizing is also isolated in a renderer-free integer calculation.
Tests pin exact 16:9 integer scaling, a 2:1 output, an output smaller than the
320x180 logical surface, and the zero-sized viewport observed while minimized.
The CNA display smoke lane resizes through 640x360, 1000x500, 256x144 and back
to 960x540, retaining the original render target and checking finite player and
bounded camera coordinates while CNA keyboard/gamepad polling continues.
The offscreen platform reports minimize/restore and exclusive fullscreen as
unsupported rather than pretending to exercise them. A real Linux desktop run
on 2026-08-24 completed minimize/restore and fullscreen/windowed transitions
with SDL_RENDERER and no validation failure.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| unclear/restrictive original license | derived-code redistribution may be impermissible | clean new implementation; no archive content; document only facts/behavior; seek permission before any reuse |
| Nintendo-derived identity/content | trademark/copyright exposure | original title, character, world, enemies, pickups, levels, art and sound; explicit non-affiliation |
| feel differs from 1994 executable | weak historical connection | source constants as oracle, DOSBox measurement task, deterministic tuning tests, document deviations |
| refresh-coupled original timing | constants cannot be naively converted | measure at nominal 70 Hz and tune perceptually at fixed 60 Hz |
| CNA pre-release API changes | consumer breaks | pin/report commits, use public target/API only, isolate adapter, record `MAR-CNA-*` defects |
| renderer differences in render targets/filtering | inconsistent pixels or startup | capability query, smoke tests, renderer matrix, CNA-level reproductions |
| content API gaps | external data loading may be awkward | use verified `TitleContainer` stream API; keep parser independent from transport |
| generated placeholder art looks too abstract | milestone undersells the game | coherent VGA-inspired palette and procedural motifs; later original asset pass |
| audio provenance | accidental copyrighted reuse | silence/generated new effects initially; mandatory provenance ledger |
| overengineering | project becomes a general engine | no premature ECS/physics engine/asset pipeline; extract only exercised modules |

## Open research questions

- Measure real/DOSBox update frequency, base jump apex time, stop distance,
  camera lag, and enemy activation timing.
- Decode every high-byte CP437 map glyph by numeric value without reproducing
  original level strings in shipping artifacts.
- Verify the original alternate `Opt_Na` Turbo changes beyond global velocity
  scaling and theme options.
- Determine which CNA renderers provide the most stable logical render-target,
  resize, input, and audio behavior on supported platforms.
- Decide whether 320x180 remains the sole presentation mode after playtesting or
  becomes the retro option beside a native-width camera.
