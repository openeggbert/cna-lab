# CNA VB.NET template roadmap

Last measured: 2026-08-23. Chronological evidence lives in [`NEXT.md`](NEXT.md); this file is the
normative plan.

## Current verified state

- One strict `net8.0` VB.NET game directly consumes `CNA.XnaCompat`.
- Raw PNG decoding, real `SpriteBatch` drawing, XNA input, exact 60/600-frame exits, and explicit
  cleanup pass in both source and generated projects.
- The compile probe builds and asserts the exact `CnaVbTemplate.HelloGame` namespace.
- Development/project-reference and isolated local-package consumers both pass.
- Linux x64 OPENGLES3 is the only runtime-qualified platform.
- CNA-CS packages remain unpublished and non-packable unless its acceptance switch is explicit.

## Architecture and relationship to CNA-CS

```text
VB.NET -> CNA.XnaCompat -> CNA.Framework -> CNA.Interop -> CNA C ABI -> CNA C++
```

`CNA.XnaCompat` owns the XNA facade and CNA-CS owns every managed/native binding concern. This
repository must never add a CNA-VB layer, duplicate XNA classes, P/Invoke, native handles, or
ownership machinery.

## Template goals

- Keep `HelloGame` a small pure-XNA 2D starter.
- Preserve strict VB compilation and deliberate root-namespace handling.
- Exercise real content, device/window lifecycle, viewport movement, input, and cleanup.
- Keep generated projects free of repository verification infrastructure and embedded local paths.

## Development reference mode

Use `CnaCsRoot`/`CNA_CS_ROOT` to project-reference
`src/CNA.XnaCompat/CNA.XnaCompat.csproj`. A repository-only sibling default is acceptable and is
excluded from generated projects.

## Package-consumer mode

Use only CNA-CS's explicit `CnaPackageAcceptance=true` path and exact package IDs. The verified
Linux-x64 experiment packs a selected native library into `CNA.Interop`, restores an isolated
Package-mode generated project, and runs without native-path environment variables. Keep Package
mode opt-in until packages are published; do not imply general RID support.

## Runtime verification

- Require a real CNA library through `CNA_NATIVE_LIBRARY`, `CNA_NATIVE_DIR`, or an accepted RID
  package asset.
- Run 60 frames for smoke and 600 for stability, with exact completion output.
- Verify both this repository and a freshly generated project.
- Continue checking decoded image dimensions and explicit `SpriteBatch`/`Texture2D` release.

## VB.NET compile compatibility

Maintain a focused repository-only probe for inheritance, overrides, properties, events, generics,
overloads, enums, value types, graphics resources, effects, content, audio, keyboard, mouse, and
gamepad APIs. Add cases only for meaningful CLR/VB compatibility risks.

## dotnet new generation

The `cna-game-vb` template substitutes project name, root namespace, assembly name, startup object,
and project filename. Development mode retains a configurable source hook but no embedded path.
Package mode must contain only the exact `CNA.XnaCompat` package reference.

## Optional MonoGame/FNA/Kni portability

Not configured and not claimed. Consider separate exact-version source/runtime probes only after
the required CNA path remains green; never add optional-engine complexity to the default project
without measured value.

## Platform matrix

| Platform | Required status |
| --- | --- |
| Linux x64 | Runtime verified: OPENGLES3 under Xvfb |
| Windows | Not verified |
| macOS | Not verified |
| Android | Not verified |
| iOS | Not verified |
| Web | Not verified |

## Definition of done

- Direct CNA.XnaCompat reference; zero VB binding or duplicated XNA/native surface.
- Game and compile probe build with zero warnings.
- Real PNG decoding and SpriteBatch drawing pass for 60 and 600 frames.
- Resources unload cleanly for source and generated projects.
- Isolated template install, generation, path audit, build, and 60-frame run pass.
- Local package acceptance passes without source references or native-path environment variables.
- README, plan, NEXT, and platform claims match measured evidence.

## Remaining blockers

- The CNA-CS packages are local acceptance artifacts, not published releases.
- The checked native CNA source currently fails its own OPENGLES3 build assertion, so this session's
  runtime proof uses a compatible environment library rather than a fresh build from that checkout.
- Additional operating systems and optional engines need independent runtime qualification.
