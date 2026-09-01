# CNA VB.NET game template

This repository is both a minimal VB.NET game and an installable `dotnet new` template. It uses
the strict `Microsoft.Xna.Framework` CLR API implemented by CNA-CS directly:

```text
VB.NET game
    -> CNA.XnaCompat
    -> CNA.Framework
    -> CNA.Interop
    -> CNA C ABI
    -> CNA C++
```

There is no CNA-VB binding and none is needed. The game contains no duplicate XNA types or native
declarations. Its one CNA-specific capability query uses CNA-CS's typed extension API and is
isolated in `EngineDiagnostics.vb`; normal gameplay remains strict XNA API.

## Measured status

The project targets `net8.0`, matching the current CNA-CS baseline. The measured runtime
qualification is Linux x64 with CNA's OPENGLES3 renderer under Xvfb. Windows, macOS, Android, iOS,
and Web have not been verified by this repository.

The game creates a real `GraphicsDeviceManager`, decodes the checked `Content/logo.png` with
`Texture2D.FromStream`, and polls keyboard, mouse, and gamepad state. A 3D-capable renderer draws
the logo on a rotating 36-vertex `BasicEffect` cube through `DrawUserPrimitives`; other renderers
use the real `SpriteBatch.Begin/Draw/End` bouncing-logo fallback. The decoded 128 x 128 dimensions
and selected rendering path are reported at startup. All graphics resources are explicitly
released during `UnloadContent`, including partial-load failure paths.

## Development/source mode

The default template mode project-references the actual CNA-CS project:

```text
src/CNA.XnaCompat/CNA.XnaCompat.csproj
```

This repository discovers a sibling `../cna-cs` through its repository-only
`Directory.Build.props`. For any other layout, configure the checkout explicitly:

```bash
CNA_CS_ROOT=/path/to/cna-cs dotnet restore CnaVbTemplate.vbproj
CNA_CS_ROOT=/path/to/cna-cs dotnet build CnaVbTemplate.vbproj --no-restore -m:1
```

The game uses `Option Strict On`, `Option Explicit On`, and `Option Infer On`. `RootNamespace` is
the single namespace source; the VB files deliberately do not repeat it.

## Native runtime

Point CNA-CS at a compatible CNA C ABI library using an absolute file or directory path:

```bash
CNA_NATIVE_LIBRARY=/path/to/libcna_c_api.so dotnet run --project CnaVbTemplate.vbproj
CNA_NATIVE_DIR=/path/to/native-directory dotnet run --project CnaVbTemplate.vbproj
```

Escape or the first gamepad's Back button exits interactive mode. In the 2D fallback, holding the
left mouse button moves the logo; otherwise it bounces inside the current viewport.

Deterministic modes exit by rendered-frame count, not elapsed time:

```bash
dotnet run --project CnaVbTemplate.vbproj -- --smoke-test       # 60 frames
dotnet run --project CnaVbTemplate.vbproj -- --stability-test   # 600 frames
dotnet run --project CnaVbTemplate.vbproj -- --frames 240       # exactly 240
```

`CNA_SMOKE_FRAMES` retains the current C# template convention for overriding the two named test
defaults. `--frames N` always wins and requires a positive integer. Successful completion prints
`Completed N frames` and a resource-release line.

## VB.NET compatibility probe

The repository-only executable probe compiles representative inheritance, overrides, properties,
events, generic content APIs, graphics-resource types, effects, audio types, value types, input,
and `SpriteBatch` overloads directly against `CNA.XnaCompat`:

```bash
dotnet build tests/CNA.VB.CompileProbe/CNA.VB.CompileProbe.vbproj -m:1
dotnet run --project tests/CNA.VB.CompileProbe/CNA.VB.CompileProbe.vbproj --no-build
```

It also asserts that the emitted game type is exactly `CnaVbTemplate.HelloGame`, guarding against
VB root-namespace duplication.

## Install and generate

The short name is `cna-game-vb`. Development mode is the default because the CNA packages are not
published:

```bash
dotnet new install /path/to/this-repository
dotnet new cna-game-vb -n FreshGame
CNA_CS_ROOT=/path/to/cna-cs dotnet build FreshGame/FreshGame.vbproj -m:1
```

Generated projects contain only the starter sources, content, project file, and `.gitignore`.
They contain no embedded sibling checkout or developer path. A Development-mode consumer accepts
any CNA-CS checkout through `CNA_CS_ROOT`/`CnaCsRoot`; it need not be a sibling.

## Local package-consumer acceptance

Current CNA-CS keeps production projects non-packable by default, but exposes an explicit local
acceptance switch. It produces the exact package IDs `CNA.Interop`, `CNA.Framework`, and
`CNA.XnaCompat`, currently with the test version `0.1.0-local.1`. These packages are unpublished
and do not constitute a supported RID promise.

To pack the current CNA-CS source, include a selected Linux-x64 native library, generate a clean
Package-mode VB consumer, restore from an isolated feed, and run 60/600 frames without native-path
environment variables:

```bash
DOTNET_COMMAND=/path/to/dotnet \
CNA_CS_ROOT=/path/to/cna-cs \
CNA_NATIVE_LIBRARY=/absolute/path/to/libcna_c_api.so \
CNA_TEMPLATE_USE_XVFB=1 \
CNA_TEMPLATE_REQUIRE_3D=1 \
scripts/verify-package-consumer.sh
```

Given an existing feed, package mode can also be generated directly:

```bash
dotnet new cna-game-vb -n FreshGame \
  --consumerMode Package --cnaPackageVersion 0.1.0-local.1
```

The generated Package-mode project contains `PackageReference` only: no `CnaCsRoot`,
`CNA_CS_ROOT`, or `ProjectReference` hook.

## Reproducible verification

`scripts/verify-template.sh` installs the template into an isolated hive, generates
`FreshVbGame`, audits its paths, and builds it. Development mode also builds/runs the VB probe.
With a native library configured it runs both source and generated runtime checks. Set
`CNA_TEMPLATE_RUN_STABILITY=1` for the 600-frame runs, `CNA_TEMPLATE_USE_XVFB=1` on Linux CI, and
`CNA_TEMPLATE_REQUIRE_3D=1` to make the test fail unless the real cube path is selected:

```bash
DOTNET_COMMAND=/path/to/dotnet \
CNA_CS_ROOT=/path/to/cna-cs \
CNA_NATIVE_LIBRARY=/absolute/path/to/libcna_c_api.so \
CNA_TEMPLATE_USE_XVFB=1 \
CNA_TEMPLATE_RUN_STABILITY=1 \
CNA_TEMPLATE_REQUIRE_3D=1 \
scripts/verify-template.sh --mode development
```

## Optional engines and platforms

MonoGame, FNA, and Kni are not configured or verified here. They are not advertised by this CNA
template. The same strict-XNA source may be evaluated separately only with exact dependencies and
independent build/runtime evidence.

| Platform | Status |
| --- | --- |
| Linux x64 | Runtime verified with OPENGLES3 under Xvfb |
| Windows | Not verified |
| macOS | Not verified |
| Android | Not verified |
| iOS | Not verified |
| Web | Not verified |

See [`plan.md`](plan.md) for the normative roadmap and [`NEXT.md`](NEXT.md) for measured session
history.
