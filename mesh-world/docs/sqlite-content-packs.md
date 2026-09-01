# MeshWorld — SQLite Content Packs

Large collections of Lua generators and taxonomy definitions can be packed into a single SQLite database for distribution. This avoids having tens of thousands of individual files in a release build.

## Two modes

### Development mode

Lua files and JSON definitions exist as normal files on disk:

```
generators/lua/
    object/chair.lua
    object/table.lua
    zone/park.lua
    map/basic_city.lua
data/taxonomy/
    taxonomy.json
    containment.json
```

Good for: Git diffs, Claude Code edits, code review, unit tests, rapid iteration.

### Runtime/release mode

All content packed into `meshworld_content.sqlite`:

```
meshworld_content.sqlite    ← single file content pack
meshworld_config.json       ← world/generation settings
```

Good for: distribution, downloads, content packs from contributors.

## Database schema

```sql
-- Lua generator scripts
CREATE TABLE lua_generator (
    id              TEXT PRIMARY KEY,
    category        TEXT NOT NULL,       -- "object", "zone", "building", "room", "map"
    name            TEXT NOT NULL,
    version         TEXT NOT NULL,
    source_code     TEXT NOT NULL,
    description     TEXT,
    parameters_json TEXT,               -- JSON schema of accepted parameters
    sha256          TEXT NOT NULL,      -- SHA-256 of source_code for integrity check
    enabled         INTEGER NOT NULL DEFAULT 1,
    created_at      TEXT,
    updated_at      TEXT
);

-- Taxonomy node definitions
CREATE TABLE taxonomy_node (
    id          TEXT PRIMARY KEY,
    kind        TEXT NOT NULL,          -- "region", "zone", "building", "room", "object"
    name        TEXT NOT NULL,
    description TEXT,
    metadata_json TEXT
);

-- Containment rules
CREATE TABLE containment_rule (
    parent_id       TEXT NOT NULL,
    child_id        TEXT NOT NULL,
    probability     REAL NOT NULL DEFAULT 1.0,
    min_count       INTEGER NOT NULL DEFAULT 0,
    max_count       INTEGER NOT NULL DEFAULT 1,
    lod_max         INTEGER NOT NULL DEFAULT 2,
    conditions_json TEXT,
    PRIMARY KEY (parent_id, child_id)
);

-- Material registry
CREATE TABLE material (
    id              TEXT PRIMARY KEY,
    roughness       REAL NOT NULL DEFAULT 0.8,
    metallic        REAL NOT NULL DEFAULT 0.0,
    base_color      TEXT NOT NULL,      -- "r g b a"
    base_color_tex  TEXT,               -- TextureRegistry id
    license         TEXT NOT NULL,      -- "CC0", "CC-BY-4.0", etc.
    author          TEXT,
    source_url      TEXT,
    attribution     TEXT
);

-- Texture registry
CREATE TABLE texture (
    id          TEXT PRIMARY KEY,
    uri         TEXT NOT NULL,
    wrap_u      TEXT NOT NULL DEFAULT 'repeat',
    wrap_v      TEXT NOT NULL DEFAULT 'repeat',
    filter      TEXT NOT NULL DEFAULT 'linear',
    color_space TEXT NOT NULL DEFAULT 'srgb',
    license     TEXT NOT NULL,
    author      TEXT,
    source_url  TEXT,
    attribution TEXT
);

-- Generated asset index (chunk cache index, not the MCB files themselves)
CREATE TABLE generated_asset (
    chunk_x         INTEGER NOT NULL,
    chunk_y         INTEGER NOT NULL,
    generator_id    TEXT NOT NULL,
    generator_version TEXT NOT NULL,
    variation_input INTEGER NOT NULL,
    generated_at    TEXT NOT NULL,
    mc3_path        TEXT,
    mcb_path        TEXT,
    PRIMARY KEY (chunk_x, chunk_y)
);
```

## ContentPackLoader API

```cpp
// include/ContentPackLoader.hpp
class ContentPackLoader {
public:
    // Dev mode: load from directory of .lua files + JSON defs
    void load_from_dir(const std::filesystem::path& lua_dir,
                       const std::filesystem::path& taxonomy_dir);

    // Release mode: load from SQLite pack
    void load_from_sqlite(const std::filesystem::path& db_path);

    // Access
    std::string get_lua_source(const std::string& generator_id) const;
    std::vector<std::string> list_generators(const std::string& category = "") const;
    bool has_generator(const std::string& id) const;
};
```

## MeshWorldPack tool

`MeshWorldPack` binary packs all dev-mode files into a SQLite database:

```
$ ./MeshWorldPack --input generators/lua/ --taxonomy data/taxonomy/ --output meshworld_content.sqlite
Packing 47 Lua generators...
Packing 312 taxonomy nodes...
Packing 1240 containment rules...
Packing 58 materials...
Done: meshworld_content.sqlite (2.3 MB)
```

Round-trip integrity: SHA-256 of each source file is stored and verified on load.

## Security note

SQLite is storage only — not a security boundary. Lua still runs in the sandbox regardless of whether the source code came from a file or from SQLite. A corrupt or malicious content pack cannot escape the sandbox.
