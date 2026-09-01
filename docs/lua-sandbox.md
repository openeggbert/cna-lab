# MeshWorld — Lua Sandbox

Lua generators run inside a restricted sandbox. This prevents rogue or broken generators from accessing the filesystem, network, or system resources.

## What is allowed

| Category | Available |
|----------|-----------|
| Scene builder API | `scene:addBox`, `scene:addCylinder`, `scene:addPlane`, `scene:addInstance`, `scene:addMaterial`, `scene:setMetadata`, `scene:callGenerator` |
| Context access | `ctx.variation`, `ctx.zone`, `ctx.region`, `ctx.style`, `ctx.parameters`, `ctx.exits`, `ctx.lod` |
| Safe math | `math.*` (all standard math functions) |
| Safe string | `string.*` (all standard string functions) |
| Safe table | `table.*` (all standard table functions) |
| Deterministic random | `scene:random(min, max)`, `scene:randomInt(min, max)` |
| Type helpers | `type()`, `pairs()`, `ipairs()`, `next()`, `select()`, `tostring()`, `tonumber()`, `unpack()` |
| Approved generators | `scene:callGenerator(id, ctx, placement)` |

## What is blocked

| Blocked | Reason |
|---------|--------|
| `io` | No filesystem access |
| `os` | No system calls, no time, no environment |
| `debug` | No introspection |
| `package` | No module loading |
| `dofile` | No arbitrary file execution |
| `loadfile` | No arbitrary file loading |
| `load` | Restricted (no loading of arbitrary code strings) |
| `require` | Custom implementation — only approved modules |
| `print` / `io.write` | Redirected to sandbox logger, not stdout |

## Implementation

```cpp
// include/LuaRuntime.hpp
class LuaRuntime {
public:
    LuaRuntime();
    // Register scene builder API into the Lua state
    void register_scene_api(Mc3SceneBuilder& scene_builder);
    // Execute sandboxed Lua script; returns MC3 XML string
    std::string execute(const std::string& source, const GenerationContext& ctx);
};
```

Internally, `LuaRuntime` uses sol2 to bind C++ methods to Lua tables. Before executing any generator script, the runtime:

1. Creates a fresh `sol::state` (or resets a pooled state).
2. Opens only safe standard libs: `sol::lib::base`, `sol::lib::math`, `sol::lib::string`, `sol::lib::table`.
3. Explicitly nils out: `io`, `os`, `debug`, `package`, `dofile`, `loadfile`.
4. Registers the `scene` userdata bound to an `Mc3SceneBuilder` instance.
5. Registers the `ctx` table from `GenerationContext`.
6. Loads and executes the generator script.
7. Calls `generate(ctx, scene)`.
8. Returns `scene:buildToString()`.

## Error handling

If a generator throws a Lua error:
- The exception is caught by the C++ harness.
- An error is logged with the generator ID and message.
- The C++ fallback generator runs instead.
- No crash, no invalid MC3 written to cache.

```
[WARN] Lua generator "lua.zone.park.v2" failed: attempt to index global 'io' (a nil value)
[INFO] Falling back to cpp.chunk.park for chunk (5, 3)
```

## `require` whitelist

Custom `require` implementation only allows:

```lua
require "meshworld.math"   -- extra math helpers
require "meshworld.random" -- deterministic random utilities
require "meshworld.style"  -- style palette helpers
```

All other `require` calls throw: `"require is restricted: <module_name> is not approved"`.

## Testing sandbox restrictions

```cpp
// tests/LuaGeneratorTests.cpp
TEST(LuaSandbox, BlocksIOAccess) {
    LuaRuntime rt;
    auto result = rt.execute(R"(
        function generate(ctx, scene)
            io.open("test.txt", "w")  -- should throw
        end
    )", ctx);
    EXPECT_FALSE(result.ok);
    EXPECT_THAT(result.error, HasSubstr("io"));
}

TEST(LuaSandbox, BlocksOSExecute) {
    LuaRuntime rt;
    auto result = rt.execute(R"(
        function generate(ctx, scene)
            os.execute("rm -rf /")  -- should throw
        end
    )", ctx);
    EXPECT_FALSE(result.ok);
}
```
