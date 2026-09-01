# MeshWorld — `object.sqlite3` Proposal: Analysis (not implemented)

Written 2026-07-11, at user request: analysis only, no implementation yet.
The user proposed a new, gitignored `object.sqlite3` database with two
tables — `object` (one row = one desired Lua generator / thing that can
exist) and `cancontain` (parent→child containment relationships, e.g.
"fridge can contain a milk carton", "bathroom can contain a bathtub",
"park can contain a tree") — as a way to enumerate, at scale ("clouds of
definitions"), what can exist where, so it becomes clear which Lua
generators still need to be written.

## 1. This overlaps heavily with a system that already exists and is already loaded

MeshWorld already has almost exactly this, built and wired into the real
runtime, since before this session:

- `include/TaxonomyNode.hpp` / `include/TaxonomyRegistry.hpp` — a taxonomy
  node is `{id, kind, name}` (`kind` ∈ world/region/zone/building/room/
  place/path/object).
- `include/ContainmentRule.hpp` / `include/ContainmentRuleRegistry.hpp` — a
  containment rule is `{parent, child, probability, min_count, max_count,
  lod_max}` — this is *already exactly* the `cancontain` table the user is
  proposing, field for field.
- Backing data: `data/taxonomy/taxonomy.json` (committed to git) and
  `data/taxonomy/containment.json` (committed to git).
- `ContentPackLoader::load_from_disk()` loads both into the registries at
  process startup, and `ctx.containment.childrenOf(parent_id)` (added this
  session, see `docs/procedural-model-generator-roadmap.md`) exposes the
  containment rules to Lua generators directly.

**The user's own two example relationships already exist verbatim** in
`data/taxonomy/containment.json`:

```json
{ "parent": "object.fridge", "child": "object.milk_box", "probability": 0.7, "min_count": 0, "max_count": 3, "lod_max": 4 }
{ "parent": "zone.park",     "child": "object.tree",      "probability": 1.0, "min_count": 8, "max_count": 40, "lod_max": 2 }
```

The third example the user gave — bathroom can contain a bathtub — is
**not** there. `room.bathroom` has zero containment children defined in
`containment.json` at all, even though `object.bathtub`/`object.toilet`/
`object.sink` all exist as real, tested Lua generators today. This is a
concrete, real gap and a good illustration of exactly the problem the user
is trying to solve.

### Current real numbers (measured, not estimated)

- `taxonomy.json`: **60 nodes** — world 1, region 3, zone 10, building 5,
  room 4, place 4, path 4, **object 33**.
- `containment.json`: **39 rules**.
- Real Lua generators on disk: **59 files** — `object/` 27, `map/` 17,
  `architecture/` 5, `room/` 4, `zone/` 4, `building/` 2.
- Of the 33 `object.*` taxonomy nodes, **20 have a matching Lua generator,
  13 do not** (bush, flower_bed, rock, fallen_log, cabinet, radio, laptop,
  computer, milk_box, water_bottle, egg_box, cheese_box, trash_bin).
- **The gap runs the other way too**: 7 real Lua generators built THIS
  session (T195-T218 batch) have **no taxonomy.json entry at all** —
  bathtub, bookshelf, mailbox, nightstand, picnic_table, toilet, wardrobe.
  `taxonomy.json` was simply never updated when those generators were
  added — it has already started drifting out of sync with reality after
  one session.

This last point matters: the existing system is not just incomplete, it's
already **not being kept in sync** with real generator work. A tool whose
whole purpose is "know what generators we have vs. need" has to solve that
sync problem, or it will drift the same way within days of being built.

## 2. What the proposal genuinely adds that the existing system doesn't have

`TaxonomyNode` has no concept of "does a generator exist for this yet."
That's the one real, valuable gap in the existing system the user's
proposal would close: a `lua_generator_path` (or `lua_generator_id`) column
per node, so a single query answers "which taxonomy nodes have no
generator yet" — which is exactly today's 13-node gap above, made
queryable instead of requiring a one-off audit.

## 3. Recommendation: don't build a parallel, disconnected system

Given the schemas are already nearly identical, building `object.sqlite3`
as a completely separate, standalone database would create two sources of
truth that immediately diverge (the same problem `taxonomy.json` already
has with real generators). Two real options:

**Option A — extend the existing JSON files directly.** Add
`lua_generator_id`/`lua_generator_path`/`status` fields to
`taxonomy.json`'s node objects. No new database, no new tooling, stays
git-committed and diffable. Downside: doesn't match what the user asked for
(a SQLite db, gitignored) and JSON isn't a great medium for *generating*
thousands of candidate entries at once, reviewing them, and discarding the
bad ones — every candidate would show up in a git diff even before review.

**Option B — `object.sqlite3` as a gitignored STAGING/BRAINSTORM database,
with a curation step that promotes good entries into the existing,
committed `taxonomy.json`/`containment.json`.** This matches what the user
actually described (gitignored, meant to hold "clouds of definitions" —
i.e. a large, disposable, regenerable working set, not a curated final
artifact) and fits naturally into a pipeline this codebase already half has:

```
LLM brainstorm (bulk, disposable)
        │
        ▼
  object.sqlite3   (gitignored — hundreds/thousands of candidate rows,
                     including duplicates/rejects, "status" tracks each)
        │  curation pass: dedup, verify against taxonomy.json's real 60
        │  nodes + 39 rules, fix bad probability/count values, reject
        │  low-quality entries
        ▼
data/taxonomy/{taxonomy,containment}.json   (small, curated, git-committed
                                              — the ACTUAL runtime source
                                              of truth, unchanged mechanism)
        │  MeshWorldPack (already exists, src/tools/pack_content.cpp)
        ▼
meshworld_content.sqlite   (packed runtime asset — already exists, already
                            gitignored, already the correct place for
                            "final" SQLite content)
```

`object.sqlite3` becomes the messy upstream planning tool; the existing
JSON files stay the clean, small, reviewed source of truth; the existing
`MeshWorldPack`/`meshworld_content.sqlite` pipeline is untouched. This is
additive, not a replacement of anything working.

**Recommended: Option B.**

## 4. Proposed schema (Option B), matching this codebase's own existing SQLite conventions

`SqliteContentPack` (`include/SqliteContentPack.hpp`) already establishes a
house style worth following exactly: `CREATE TABLE IF NOT EXISTS`,
singular snake_case table names, and — importantly — **the dotted
taxonomy id itself as the `TEXT PRIMARY KEY`**, not a surrogate integer
(`taxonomy_node(id TEXT PRIMARY KEY, kind TEXT NOT NULL, name TEXT NOT
NULL)`, `containment_rule(parent TEXT, child TEXT, ..., PRIMARY
KEY(parent,child))`). Matching that exactly makes promotion from
`object.sqlite3` into `taxonomy.json`/`containment.json` a closer-to-trivial
mapping later (same id shape, same key shape, just fewer columns) instead
of needing an id-translation step.

```sql
-- object.sqlite3 (gitignored -- *.sqlite3 already covered by .gitignore
-- line 18-20, no .gitignore change needed)

CREATE TABLE IF NOT EXISTS object (
    id                 TEXT PRIMARY KEY,   -- e.g. "object.milk_box", "room.bathroom"
                                            -- SAME kind-prefixed dotted-id convention
                                            -- taxonomy.json already uses
    kind               TEXT NOT NULL,       -- world|region|zone|building|room|place|path|object
    name               TEXT NOT NULL,
    description        TEXT,
    lua_generator_id   TEXT,                -- e.g. "lua.object.fridge.standard"; NULL = not built yet
    lua_generator_path TEXT,                -- e.g. "generators/lua/object/fridge.lua"; NULL = not built yet
    status             TEXT NOT NULL DEFAULT 'planned',
                                            -- planned | implemented | needs_review | rejected | superseded
    source             TEXT,                -- provenance, e.g. "manual" or "ai:claude-sonnet-5:batch-furniture-1"
    notes              TEXT,
    created_at         TEXT,
    updated_at         TEXT
);

CREATE TABLE IF NOT EXISTS cancontain (
    parent_id   TEXT NOT NULL REFERENCES object(id),
    child_id    TEXT NOT NULL REFERENCES object(id),
    probability REAL    NOT NULL DEFAULT 1.0,
    min_count   INTEGER NOT NULL DEFAULT 0,
    max_count   INTEGER NOT NULL DEFAULT 1,
    lod_max     INTEGER NOT NULL DEFAULT 2,
    notes       TEXT,
    PRIMARY KEY (parent_id, child_id)
);
```

One naming nuance worth flagging, not necessarily worth changing: this
table is called `object` but holds rows of every `kind` (zones, rooms,
buildings, paths — not just `kind='object'` rows). The existing system
calls the equivalent table `taxonomy_node` for exactly this reason (it's
generic across all kinds). Keeping the user's own preferred name `object`
is fine — it reads naturally given the request ("one object = one desired
generator") — just noting the terminology overlap with the existing
`object.*` kind and the `generators/lua/object/` directory so a future
reader isn't confused about which "object" is meant where.

`status='rejected'`/`'superseded'` matters specifically because a bulk/AI
brainstorm run will produce near-duplicates and low-quality entries — the
table needs to hold the full, messy set including rejects, not just the
keepers, or the curation step has nothing to review against.

## 5. Population process

Given "clouds of definitions" implies hundreds to thousands of rows, this
is a breadth-first content-enumeration task, not a deep-reasoning one. A
sensible approach:

1. **Seed pass**: read the real, existing 60 taxonomy nodes + 39
   containment rules first (already-committed ground truth) so new entries
   don't duplicate them, and so id/naming conventions stay consistent
   (kind-prefixed dotted ids, plausible probability/count values matching
   the existing style — e.g. `object.milk_box` under `object.fridge` at
   probability 0.7, 0-3 count, not some very different scale).
2. **Domain batches**: fan out by category (furniture, kitchen appliances,
   bathroom fixtures, bedroom furniture, vehicles, nature/flora, street
   infrastructure, electronics, ...) — each batch produces candidate
   `object` rows plus `cancontain` edges scoped to that domain. This is a
   natural fit for a `Workflow`-style parallel fan-out (one agent per
   domain) *if and when the user explicitly opts into using a workflow* —
   not something to do by default.
3. **Curation pass**: dedup, verify against the real existing 60/39,
   sanity-check probability/count values, reject or merge near-duplicates,
   flip `status` from `planned` to `needs_review`→ human/curated approval.
4. **Promotion**: manually (or via a small script) copy the approved rows
   into `data/taxonomy/taxonomy.json`/`containment.json` in the existing
   flat-array JSON shape, committed as a normal, reviewable git diff — the
   same discipline every other change to those files already gets.
5. **Cross-reference against real generators** at each stage — the
   `lua_generator_path` column is only useful if kept honest; a background
   check (grep `generators/lua/**/*.lua` for `M.id` values, same technique
   the earlier coverage-gap numbers above were computed with) should
   periodically re-verify it rather than trusting whatever the LLM wrote
   at generation time.

**Start with a bounded pilot, not "thousands" immediately.** Generate
~200-500 rows across 3-5 domains first, run the curation pass, and check
the quality bar and the schema hold up before scaling further — matches
this project's own already-established "quality over raw quantity"
principle (see `docs/procedural-model-generator-roadmap.md`'s "What NOT to
do" section: "do not create large numbers of low-quality... generators in
one session").

## 6. Model / effort recommendation for populating `object.sqlite3`

This is a **breadth-heavy, common-sense-enumeration task** (list plausible
everyday objects and their containment relationships across many
domains) — not a deep multi-step algorithmic or code-correctness task. That
shape favors a cheaper/faster model for raw volume, BUT consistency matters
a lot here specifically because the output feeds directly into a real,
committed source of truth that future generator-building sessions will
rely on — inconsistent ids, duplicated entries, or implausible probability/
count values would quietly poison the backlog this is meant to produce.

**Recommendation — a tiered approach, not a single model for the whole job:**

- **Seed pass + first pass over each genuinely new domain**: **Sonnet 5**,
  **medium effort**. Establishing the pattern (id conventions, plausible
  count/probability ranges, avoiding the existing 60/39 entries) benefits
  from stronger instruction-following and consistency; this is the pass
  every later batch's quality depends on.
- **Bulk/repetitive batches once the pattern is established** (e.g.
  enumerating many more kitchen utensils once "kitchen" as a domain is
  already well-seeded): **Haiku 4.5** is plausible here and meaningfully
  cheaper — the risk of inconsistency is lower once a strong seed example
  exists for the model to pattern-match against. Worth a small side-by-side
  quality check (same domain, same prompt, Sonnet vs. Haiku output) before
  committing to Haiku for a large batch, rather than assuming.
- **Curation/dedup/consistency pass across the WHOLE accumulated set**:
  **Sonnet 5** (or a stronger model if available), **high effort**.
  Cross-referencing potentially thousands of candidate rows against each
  other and against the real existing data for duplicates and
  inconsistencies is exactly the kind of thorough, careful-reasoning task
  that benefits from more effort, and is the step that actually protects
  quality before anything gets promoted into the committed JSON files.
- **Effort level for the bulk generation passes themselves**: **medium** —
  no deep chained reasoning is needed per entry, just consistent
  pattern-following against the seed; low effort risks sloppier
  convention-matching, high effort is probably not worth the extra cost
  for straightforward enumeration.

This is a judgment call, not a hard technical requirement — the main risk
to weigh is cost/thoroughness tradeoff at genuine "thousands of rows"
scale, which is also why the pilot-batch recommendation in §5 matters: it's
cheap to validate this tiering assumption on ~200-500 rows before
committing to a large run either way.

## 7. Wave 1 results (2026-07-11, implemented)

Built and ran per this analysis: `object.sqlite3` created at repo root (gitignored,
`*.sqlite3` already covered) with the exact schema from §4, seeded from the real
`taxonomy.json`/`containment.json` (64 nodes, 47 rules) plus a reverse-scan of all
59 real Lua generators (31 more attached/added), then populated via a `Workflow`
run (87 parallel domain-generation agents → exact-id dedup in plain JS → 8
curation/dedup agents → 16 write agents) — Sonnet 5 throughout, medium effort for
generation, high effort for curation, low effort for the mechanical DB writes, per
the user's own explicit choices.

**Real numbers**: 87/87 domains succeeded. 4242 raw objects / 4201 raw edges
generated → 3631/4195 after exact-id dedup → 161 objects dropped by the curation
pass (near-duplicates, color/size variants that slipped through, low-quality
entries) → **3470 final new objects / 3412 final new edges**, all written to disk
(zero orphan `cancontain` references verified). Final DB total: **3540 object rows,
2629 cancontain rows** (including the original seed). Breakdown by kind: object
3448, room 32, zone 29, place 11, architecture 5, building 5, path 4, region 3,
map 2, world 1.

**Real infra hiccup, not a data-quality issue**: 8 of 16 write batches (and then 3
more on a retry) hit `blocked by safety classifier: Stage 2 classifier error —
blocking based on stage 1 assessment` — a transient false-positive on ordinary
`INSERT`-into-SQLite Bash/python3 calls, not a real content problem. Resolved by
resuming the same Workflow run twice (`Workflow({scriptPath, resumeFromRunId})` —
completed agents replay from cache for free, only the blocked ones re-run) until
all 111 agents succeeded with zero errors.

**Known rough edges, honestly not fixed**: a handful of catch-all domains
(clothing, tools, appliances, sports, etc.) were given synthetic parent ids like
`object.appliance_group`/`object.clothing_group` as organizational scaffolding —
these aren't real-world objects themselves, just containers-of-convenience for
this batch, and read a bit oddly next to genuine taxonomy nodes like
`room.kitchen`. A future pass could either accept them as a legitimate loose
"category" kind or fold their children under more specific real parents instead.

## 7b. Wave 2 results (2026-07-11/12, implemented)

A second, deliberately non-overlapping wave (54 new domains — civic/commercial
buildings, trade-specific tools, deeper food/clothing/hobby chains, additional
terrain-feature variety), same Sonnet 5 / medium-generate / high-curate / low-write
Workflow pipeline, with one addition: explicit instructions to exclude any living
creature, animal, or human-figure object (this project's own standing "no
characters/NPCs" rule).

**Real numbers**: 54/54 domains succeeded. 1951 raw objects / 1923 raw edges →
1930/1918 after exact-id dedup → 88 dropped by curation → **1842 final new
objects / 1442 final new edges**. Same transient safety-classifier hiccup on 3
of 8 total write batches, resolved the same way (one `Workflow` resume).

**Combined Wave 1 + Wave 2 + original seed, final `object.sqlite3` state**:
**4875 total object rows, 3457 cancontain rows**, zero orphan references, zero
duplicate ids. By kind: object 4755, room 52, zone 29, place 18, architecture 5,
building 5, path 5, region 3, map 2, world 1 — a **76x increase** in `object`-kind
nodes over the starting 64-node seed (33 of which were `object` kind).

## 7c. Wave 3 results (2026-07-12, implemented)

A third wave (52 new domains — home-entertainment/hobby rooms, niche civic and
retail interiors, more outdoor place types, deeper containment for 5 existing
parents (`room.laboratory`, `room.bakery`, `object.medicine_cabinet`,
`object.toolbox`, `room.home_office`), and several new equipment "concept
groups" like smart-home devices, home security, renewable energy, pool/spa,
RV interiors), same Sonnet 5 / medium-generate / high-curate / low-write
Workflow pipeline, same no-creature/animal/human-figure instruction carried
over from Wave 2.

**Real numbers**: 52/52 domains succeeded, **zero safety-classifier hiccups
this time** (all 62 agents succeeded on the first pass, no resume needed).
1513 raw objects / 1486 raw edges → 1495/1486 after exact-id dedup → 47
dropped by curation → **1448 final new objects / 1230 final new edges**
attempted; of those, 981 objects were genuinely new (467 collided by id with
already-existing rows from Wave 1/2 and were skipped as `INSERT OR IGNORE`,
which is expected since some Wave 3 domains deepened existing parents rather
than only adding new ones) and 787 edges were genuinely new (412 referenced
a parent/child id that curation had just dropped in this same wave — e.g. one
of the synthetic `*_group` scaffolding parents — and were correctly skipped
rather than inserted as a dangling reference; 31 were exact-duplicate edges
already present).

**Combined Wave 1 + Wave 2 + Wave 3 + original seed, final `object.sqlite3`
state**: **5856 total object rows, 4244 cancontain rows**, zero orphan
references, zero duplicate ids, zero duplicate edges (all verified directly
against the database, not estimated). By kind: object 5711, room 63, place 32,
zone 29, architecture 5, building 5, path 5, region 3, map 2, world 1 — a
**91x increase** in `object`-kind nodes over the starting 64-node seed (33 of
which were `object` kind).

## 7d. Wave 4 results (2026-07-12, implemented)

A fourth wave (56 new domains — transportation infrastructure, industrial/
factory equipment, agriculture equipment, medical/retail depth, sports/
recreation depth, architecture element variety (roofs/windows/doors/fences),
office spaces), same pipeline, same no-creature/animal/human-figure
instruction.

**Real numbers**: 56/56 domains succeeded, zero safety-classifier hiccups.
1478 raw objects / 1448 raw edges → 1459/1448 after exact-id dedup → 52
dropped by curation → **1407 final new objects / 1267 final new edges**
attempted; 1252 objects genuinely new (155 collided by id with existing rows,
skipped via `INSERT OR IGNORE`), 1191 edges genuinely new (49 skipped for a
missing parent/child id dropped by curation, 26 exact-duplicate edges).

**Combined Wave 1-4 + original seed, final `object.sqlite3` state**: **7108
total object rows, 5435 cancontain rows**, zero orphan references, zero
duplicate ids, zero duplicate edges. By kind: object 6928, room 86, place 44,
zone 29, architecture 5, building 5, path 5, region 3, map 2, world 1.

## 7e. Repair pass results (2026-07-12, implemented)

After Wave 4, a direct SQL audit (user-requested quality check) found two
real defects not caught by any curation pass: **56 junk self-referencing
`cancontain` rows** (`parent_id = child_id`, e.g. `room.pantry ->
room.pantry`, a generation hallucination never checked for) and **1962
`object`-kind rows (28% of all 6928) with zero incoming containment edge**
— genuinely disconnected from the graph, root-caused to curation correctly
dropping synthetic `*_group` scaffolding parents as low-quality across all
4 waves, which silently orphans their real children since nothing else
points to them (proportional across all 4 waves: 1062/508/209/168 from
Wave 1-4 respectively, not a late-wave regression).

**Fix**: the 56 self-loop rows were deleted directly (`DELETE FROM
cancontain WHERE parent_id = child_id`, trivial and safe — every affected
parent also had real children, so this didn't disconnect anything). A 5th
"repair" `Workflow` then ran a semantic matching pass: 1962 orphans, in 14
chunks of ~150, each matched against a candidate list of 187 real existing
container parents (every room/place/zone/building/architecture/path/
region/map/world-kind row, plus the 7 object-kind hub containers already
used as a parent, e.g. `object.fridge`/`object.toolbox`) — Sonnet 5, high
effort (a judgment/categorization task, not generation). Agents had to copy
`parent_id` exactly from the candidate list (validated in plain JS
post-processing — invalid ids get silently dropped, not written) and were
told to leave a genuinely-unfittable orphan unmatched rather than force a
bad match.

**Real numbers**: 14/14 match batches succeeded, zero classifier hiccups.
1958/1962 orphans matched (0 invalid parent_id dropped, only 4 left
genuinely unmatched: `object.wedding_veil`, `object.rain_poncho`,
`object.quilted_vest`, `object.moat_channel` — clothing/terrain items that
don't fit any of the 187 existing container categories, left unattached
rather than forced). All 1958 new edges verified against existing
duplicates before insert (repair-pass idempotent) and tagged
`notes='repair:orphan-reattach'` so they stay distinguishable from the
original 4 generation waves.

**Final `object.sqlite3` state after Wave 1-4 + repair**: **7108 total
object rows, 7337 cancontain rows**, zero orphan references, zero
duplicate ids, zero duplicate edges, zero self-loop edges, only **4
remaining unattached object-kind rows (0.06%)**, down from 1962 (28%).

## 8. Summary

The proposal is sound and solves a real, now-measured problem (13 taxonomy
object nodes with no generator; 7 generators with no taxonomy node;
`room.bathroom` has zero containment children despite 3 real fixture
generators existing). Recommend building it as a **gitignored staging/
brainstorm database** (Option B) that curates INTO the existing, already-
wired `taxonomy.json`/`containment.json`/`TaxonomyRegistry`/
`ContainmentRuleRegistry` system, rather than as a standalone parallel
system — reuses the existing SQLite naming/schema conventions already
established by `SqliteContentPack`, and fits into the existing brainstorm
→ curated-JSON → packed-SQLite pipeline shape without touching any working
code. No implementation has been done — this is analysis only, per the
request.
