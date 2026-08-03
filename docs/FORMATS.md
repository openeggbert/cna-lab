# CNA Editor File Formats

Every editor-authored file is JSON. That is a deliberate choice for the authoring formats, and it
is not a claim about the *runtime* formats — the content builder is free to emit something compact
and binary. Authoring files are optimised for a different thing: being read by a human, diffed in a
pull request, merged after a branch, and migrated across versions.

Three properties are maintained on purpose:

- **Order is stable.** Object members keep insertion order on read and write; component properties
  serialise alphabetically. Saving a file you did not change produces no diff.
- **Integers are written as integers.** `120`, never `120.0`. Coordinates, counts and layer indices
  stay readable in a diff.
- **Two relaxations for hand editing.** The parser accepts `//` line comments and trailing commas.
  The editor never writes either.

Every format carries a `formatVersion`. A file from the future is **rejected with a clear message**
rather than partially read; a file from the past is read and upgraded.

---

## `.cnaproject`

One per project, at the project root. Its directory *is* the project root; every other path in the
project is relative to it and uses forward slashes on all platforms.

```json
{
  "formatVersion": 1,
  "name": "MyGame",
  "kind": "CnaNative",
  "startupScene": "Scenes/MainMenu.cnascene",
  "assetDirectory": "Assets",
  "sceneDirectory": "Scenes",
  "defaultGraphicsBackend": "easygl",
  "targetPlatforms": ["linux-x64", "windows-x64"],
  "modules": ["cna-core", "cna-graphics-2d", "cna-audio"],
  "plugins": ["org.openeggbert.mc3"]
}
```

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| `formatVersion` | int | — | Required. Currently `1` |
| `name` | string | `"Untitled"` | Display name |
| `kind` | enum | `"CnaNative"` | `CnaNative` or `XnaCompatible` — see below |
| `startupScene` | string | `""` | Project-relative. Ignored for `XnaCompatible` |
| `assetDirectory` | string | `"Assets"` | Scanned by the asset database |
| `sceneDirectory` | string | `"Scenes"` | Where new scenes are created |
| `defaultGraphicsBackend` | string | `"easygl"` | Which player build the Play button prefers |
| `targetPlatforms` | string[] | `["linux-x64"]` | Offered by the build dialog |
| `modules` | string[] | `["cna-core"]` | CNA modules the game links |
| `plugins` | string[] | `[]` | Plugin ids to load for this project |

### `kind`

This field is how CNA avoids becoming a mandatory engine.

**`CnaNative`** opts into the editor's model: scenes, entities, components, the inspector, gizmos,
prefabs and the runtime bridge.

**`XnaCompatible`** does not. The editor offers the asset browser, importer settings, content
preview, backend configuration and Play — and stops there. The game keeps its own hand-written
`Game::Initialize`/`LoadContent`/`Update`/`Draw`, with no editor concepts in it. Declaring a
`startupScene` for such a project produces a warning, because the editor will not load it and the
mismatch is worth surfacing.

### `defaultGraphicsBackend`

The lower-case command-line name of a CNA backend (`cna-editor --list-backends` prints them). This
selects which **player build** the Play button launches, not anything about the editor: CNA fixes
its backend at compile time, so the editor binary is bound to whatever it was built against. An
unrecognised value loads with a warning rather than failing.

---

## `.cnascene`

One per scene. Lives under `sceneDirectory` by convention, but any path works.

```json
{
  "formatVersion": 1,
  "sceneId": "c486b3f0-2a41-4d6b-9f18-7e0c5a1b4d92",
  "name": "Level01",
  "entities": [
    {
      "id": "f392a1b2-c3d4-4e5f-a607-182930415263",
      "name": "Player",
      "components": {
        "CNA.Transform": {
          "position": [100, 220, 0],
          "rotation": [0, 0, 0, 1],
          "scale": [1, 1, 1]
        },
        "CNA.SpriteRenderer": {
          "layerDepth": 0.5,
          "origin": [0, 0],
          "sourceRectangle": [0, 0, 0, 0],
          "spriteEffects": "None",
          "texture": "67ecaf68-3d2a-4b1c-8e9f-0a1b2c3d4e5f",
          "tint": [255, 255, 255, 255]
        }
      },
      "editorState": {
        "expanded": true
      }
    },
    {
      "id": "a1b2c3d4-e5f6-4708-9a1b-2c3d4e5f6071",
      "name": "Weapon",
      "parent": "f392a1b2-c3d4-4e5f-a607-182930415263",
      "components": {
        "CNA.Transform": {
          "position": [24, 0, 0],
          "rotation": [0, 0, 0, 1],
          "scale": [1, 1, 1]
        }
      }
    }
  ]
}
```

### Entity fields

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| `id` | UUID string | generated | Stable identity. References point at this, never at the name |
| `name` | string | `"Entity"` | Display name; need not be unique |
| `parent` | UUID string | absent | Omitted for a root entity |
| `enabled` | bool | `true` | Omitted when true |
| `sortOrder` | int | `0` | Sibling ordering; omitted when zero |
| `components` | object | `{}` | Keyed by component type id |
| `editorState` | object | absent | Editor-only, never seen by the runtime |

`parent` is stored on the child rather than as a child list on the parent, because every operation
the editor performs — reparent, delete, "which entities are roots" — is cheaper and harder to
corrupt that way.

### `editorState`

Cosmetic, editor-only data: tree expansion, layer colour, notes, icon overrides. Keeping it in a
named sub-object means the runtime scene compiler drops it wholesale rather than needing to know
which of a component's fields are cosmetic. It is deliberately *not* a separate `.cnascene.user`
sidecar — that would double the file count and guarantee the two drift apart under version control.

### Property encoding

Types are **not** embedded. The `ComponentDescriptor` supplies them on load, which keeps scene files
small and readable.

| Property type | JSON form | Example |
|---------------|-----------|---------|
| `bool` | boolean | `true` |
| `int` | number | `42` |
| `float` | number | `0.5` |
| `string` | string | `"Player"` |
| `enum` | string (the option name) | `"FlipHorizontally"` |
| `color` | `[r, g, b, a]`, 0–255 | `[255, 0, 0, 128]` |
| `vector2` | `[x, y]` | `[10, 20]` |
| `vector3` | `[x, y, z]` | `[100, 220, 0]` |
| `vector4` | `[x, y, z, w]` | `[1, 0, 0, 1]` |
| `quaternion` | `[x, y, z, w]` | `[0, 0, 0, 1]` |
| `rectangle` | `[x, y, width, height]` | `[0, 0, 32, 32]` |
| `asset` | UUID string, or `null` | `"67ecaf68-…"` |
| `entity` | UUID string, or `null` | `"f392a1b2-…"` |

An absent or malformed **quaternion** defaults to identity `[0, 0, 0, 1]`, not to all-zero: an
all-zero quaternion is not a rotation and would collapse the transform.

An **empty rectangle** (`width` or `height` ≤ 0) means "the whole texture", matching XNA's own
`SpriteBatch::Draw` convention for a null source rectangle.

### Loading is forgiving on purpose

An editor that refuses to open a slightly broken file is an editor you cannot use to *fix* a broken
file. A scene loads with warnings, never a partial failure:

| Problem | Behaviour |
|---------|-----------|
| Unknown component type | Data is preserved verbatim and round-trips through save; the inspector marks it unregistered. A missing plugin must not become missing data |
| Missing `parent` | The entity becomes a root; warning recorded |
| Parent cycle | The entity becomes a root; warning recorded |
| Missing or malformed `id` | A new id is generated; warning recorded |
| Duplicate `id` | The later entity is dropped; warning recorded |
| Property missing from the file | Filled from the descriptor's declared default |
| Property present but the wrong shape | Falls back to the type's zero value |
| `formatVersion` newer than this build | **Load fails** with an explicit message. This is the one hard failure |

---

## `.cnaasset`

A sidecar named after the full source file name: `player.png` → `player.png.cnaasset`. Commit it
alongside the asset.

```json
{
  "formatVersion": 1,
  "id": "67ecaf68-3d2a-4b1c-8e9f-0a1b2c3d4e5f",
  "type": "Texture2D",
  "importer": "CNA.TextureImporter",
  "settings": {
    "generateMipmaps": true,
    "premultiplyAlpha": true
  },
  "dependencies": [],
  "sourceStamp": {
    "size": 20481,
    "modifiedTime": 1754209823
  }
}
```

| Field | Type | Meaning |
|-------|------|---------|
| `formatVersion` | int | Required |
| `id` | UUID string | **The reason this file exists.** Scenes reference this, never the path |
| `type` | enum | `Unknown`, `Texture2D`, `SpriteFont`, `SoundEffect`, `Song`, `Effect`, `Model`, `Scene`, `RawData` |
| `importer` | string | Importer type id; empty means "track but do not import" |
| `settings` | object | Importer-specific, written verbatim |
| `dependencies` | UUID string[] | Assets this one references; drives reimport ordering |
| `sourceStamp` | object | Size in bytes and modification time in **seconds** |

The sidecar holds the identity; the *path* is discovered by scanning. Moving or renaming a source
file is therefore free: the id follows the sidecar, the database updates the path, and no scene
changes.

### Why size and time rather than a content hash

Hashing every asset on every project open is how an editor comes to take thirty seconds to start.
Size plus modification time is enough for reimport decisions and costs a `stat`. A content hash can
be added later as an opt-in for pipelines that need the guarantee.

`modifiedTime` is in **seconds**, deliberately. The filesystem clock's native tick count is around
4.6 × 10¹⁸ nanoseconds, which is outside the range a `double` represents exactly — and JSON numbers
are doubles, so the native value round-tripped through this file *changed*, making every asset look
modified on every scan. That was a real bug, caught by
`AssetSidecarStampSurvivesAJsonRoundTrip`.

### A missing source file keeps its record

A file that is gone today may be one `git checkout` away from returning. Deleting the record would
permanently break every reference to it, so the database reports it as missing instead.

---

## Editor ↔ player wire protocol

One JSON object per line over a stream socket. The framing is the newline, so a message body
contains exactly one and it is last.

```
{"type":"hello","payload":{"protocolVersion":1,"projectRoot":"/home/me/MyGame"}}
{"type":"ready","payload":{"backend":"SOFTWARE","protocolVersion":1}}
{"type":"loadScene","payload":{"scenePath":"Scenes/Level01.cnascene"}}
{"type":"setProperty","payload":{"entityId":"f392…","component":"CNA.Transform","property":"position","valueType":"vector3","value":[100,220,0]}}
{"type":"reportLog","payload":{"severity":"info","text":"Loaded 3 entities"}}
```

| Field | Type | Meaning |
|-------|------|---------|
| `type` | string | Message kind |
| `requestId` | int | Correlates a reply with its request; omitted when unsolicited |
| `payload` | object | Type-specific fields |

### Messages

**Editor → player:** `hello`, `loadScene`, `reloadAsset`, `setProperty`, `pause`, `resume`,
`stepFrame`, `selectEntity`, `screenshot`, `quit`.

**Player → editor:** `ready`, `reportException`, `reportLog`, `reportFrameStats`,
`screenshotReady`.

### Three design points

**Unknown fields and unknown message types are ignored, never fatal.** Both ends must tolerate a
peer built from a different revision. A malformed line is counted and skipped; a peer from a newer
revision cannot kill a play session.

**`setProperty` carries its own `valueType`,** unlike a scene file. The player resolves component
schemas from its own registry, which may not match the editor's after a plugin reload, so the wire
has to be self-describing.

**Reading goes through a stream decoder.** A stream socket delivers arbitrary chunks. A reader that
assumes one `recv()` equals one message works right up until a message straddles a packet boundary,
and then fails in a way that is very hard to reproduce.

### Why line-delimited JSON

The traffic is a handful of messages per second. Being able to watch a session with `nc` during
bring-up is worth far more than the bytes a binary format would save. The framing can be replaced
later without changing the message model.

---

## `plugin.json`

One per plugin directory.

```json
{
  "id": "org.openeggbert.mc3",
  "name": "MC3 Tools",
  "version": "0.1.0",
  "description": "Mesh-Craft import, export and primitive editing",
  "author": "OpenEggbert",
  "editorApiVersion": 1,
  "library": "libmc3-editor-plugin.so",
  "dependencies": []
}
```

| Field | Type | Meaning |
|-------|------|---------|
| `id` | string | Reverse-DNS, unique. Also the component type id prefix a plugin should use |
| `editorApiVersion` | int | Must match the editor's exactly, or the plugin is rejected unloaded |
| `library` | string | Shared library file name, relative to the manifest |
| `dependencies` | string[] | Plugin ids that must load first |

The manifest is read and validated **before** the library is opened. An ABI mismatch that reaches
`dlopen` is a crash, not an error message — so version checking, dependency ordering and library
existence are all settled first, and rejection is always reported rather than fatal.

---

## Format evolution

| Change | Requires a version bump? |
|--------|--------------------------|
| Adding an optional field with a default | No — older readers ignore it, newer readers default it |
| Adding a component type or property | No — the descriptor system handles it |
| Renaming a field | **Yes**, plus a migration |
| Changing a field's type or units | **Yes**, plus a migration |
| Changing the meaning of an existing value | **Yes**, plus a migration |

Version gating and rejection are implemented today; the migration framework is `plan.md` ED-902.
Until it exists, the practical rule is: only make additive changes.
