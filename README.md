# CNA VB.NET Template

> **Status: In progress - NOT YET FUNCTIONAL**


A cross-platform game template for VB.NET using the CNA (Common Native Abstraction) framework.

## Features

- **XNA 4.0 API**: Familiar PascalCase API for Game lifecycle.
- **Multi-Engine Support**: Easily switch between `CNA`, `MonoGame`, `FNA`, and `Kni`.
- **Adaptive Rendering**: Automatically switches between 3D (HiDef) and 2D (Reach) modes.
- **Multi-platform**: Supports Windows, Linux, macOS, Android, iOS, and Web.
- **Smoke Test Support**: Includes `--smoke-test` flag for CI/CD validation.

## Prerequisites

- [.NET 8.0 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) or newer.

## Getting Started

### 1. Build and Run

By default, the template uses the `CNA` engine:

```bash
dotnet run
```

To use a different engine (e.g., MonoGame or Kni):

```bash
dotnet run -p:Engine=MonoGame
# or
dotnet run -p:Engine=Kni
```

### 2. Smoke Test

Verify the game loop and initialization:

```bash
dotnet run -- --smoke-test
```

## Project Structure

- `src/HelloGame.vb`: Main game logic (Update/Draw).
- `src/Program.vb`: Entry point and smoke test handling.
- `CNA.VB.Template.vbproj`: Project configuration with engine selection logic.
- `Content/`: Graphic assets (textures, etc.).

## Multi-platform Support

- **Desktop**: Run directly with `dotnet run`.
- **Mobile (Android/iOS)**: Use MonoGame/Kni mobile workloads.
- **Web**: Use `.NET WASM` targeting via MonoGame or Kni.

## License

This template is licensed under the MIT License.
