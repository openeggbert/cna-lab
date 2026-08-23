# CNA VB.NET template session history

## 2026-08-23 — replace the fake VB binding scaffold

### Baseline inspected

- Target repository started at `b53a559c16a37c14389b09c56b181c959ce1ef34`.
- CNA-CS source baseline: `b28ac6dd307e2e992e9804981c1678360321a45d`.
- C# template reference: `13ddd695bcafac61fe76bc501a81e66242b5306c`.
- Native CNA checkout was found at `../../cna`, revision
  `1bb2145d99ed572dd4eb15009c34e2e5f410fcf0`; its C ABI header reports 0.7.0.
- CNA-CS targets `net8.0`. `CNA.XnaCompat` is
  `src/CNA.XnaCompat/CNA.XnaCompat.csproj`, assembly `CNA.XnaCompat`, with project references to
  `CNA.Framework` and then `CNA.Interop`.
- CNA-CS expects C ABI 0.6.0 or a compatible newer minor. Its current uncommitted acceptance work
  keeps production projects non-packable by default while permitting exact local packages
  `CNA.Interop`, `CNA.Framework`, and `CNA.XnaCompat` when explicitly enabled.
- CNA-CS qualifies Linux x64 under Xvfb with OPENGLES3. No other platform qualification was
  inherited or claimed here.

The CNA-CS working tree contained unrelated in-progress changes and changed during this session.
It remained read-only. Verification froze the latest inspected state into a temporary copy; no
sibling repository was modified.

### Defects removed

- Deleted the nonexistent VB-binding project reference and every multi-engine/floating-package
  branch.
- Removed the hardcoded renderer identity, invented capability query, empty texture objects,
  placeholder 3D branch, nonexistent rectangle helper, and non-XNA exit call.
- Replaced the ten-frame smoke path with `--smoke-test` (60), `--stability-test` (600), and exact
  `--frames N` handling.
- Replaced duplicated VB root/source namespaces with one project `RootNamespace` and unqualified
  source declarations.
- Removed unsupported Windows/macOS/mobile/Web and MonoGame/FNA/Kni claims.

### Implementation

- Added the direct configurable project reference to `CNA.XnaCompat` and a repository-only sibling
  default excluded from generated projects.
- Added strict VB settings and a single `net8.0` target.
- Implemented raw PNG loading through `Texture2D.FromStream(GraphicsDevice, stream)`, checked real
  dimensions, real `SpriteBatch.Begin/Draw/End`, viewport-bounded movement, keyboard/mouse/gamepad
  polling, resize handling, and `Game.Exit()`.
- Added exception-safe partial loading and explicit, idempotent field release in `UnloadContent`.
- Added the repository-only VB compile/reflection probe and the `cna-game-vb` template.
- Added isolated Development and Package generation, path auditing, build, and runtime scripts.

### Native observations

- An existing environment OPENGLES3 `libcna_c_api.so` from local CNA revision
  `53b3c672650ef283bcebd6a3251a9a3540f8b673` reports ABI 0.8.0 and passed the real VB runtime
  checks. CNA-CS accepted it as a compatible newer 0.x ABI.
- A fresh temporary OPENGLES3 C ABI build was attempted from the requested `../../cna` revision. It
  reached C API compilation but that native checkout failed its own static assertion:
  `RendererIdentities.size() == CanonicalRendererCount()` reduced to `49 == 50`. No native source
  was modified. Runtime qualification therefore uses the compatible environment library rather
  than pretending the current native checkout produced a library.
- A normal no-frame-limit Xvfb startup reached OPENGLES3 and loaded the logo. Automated Escape
  injection did not produce a reliable clean exit in that headless session, so only deterministic
  frame modes are claimed as clean-shutdown evidence.

### Verification checkpoint

- Main Development build: passed, 0 warnings / 0 errors.
- VB compile/reflection probe: passed and emitted `CnaVbTemplate.HelloGame`.
- Source game: 60-frame smoke and 600-frame stability passed; both decoded the logo as 128 x 128
  and reported explicit `SpriteBatch`/`Texture2D` release.
- Isolated template install and `FreshVbGame` Development generation passed. Its source audit found
  no obsolete binding, embedded sibling checkout, repository root, or configured source path.
- Generated Development project: build passed with 0 warnings / 0 errors; native 60-frame smoke and
  600-frame stability passed with the same decode/draw/release evidence.
- CNA-CS local package acceptance produced `CNA.Interop`, `CNA.Framework`, and `CNA.XnaCompat`
  version `0.1.0-local.1`, including the Linux-x64 native asset.
- Generated Package project: no source-reference hook, isolated restore/build passed with 0 warnings
  / 0 errors, and 60/600 frames passed after both native-path environment variables were unset.
- Final source audit found no obsolete binding name, fake capability/member path, embedded absolute
  developer path, duplicated XNA namespace, or native declaration. `git diff --check` passed.

### Remaining work

- Publish and qualify CNA-CS packages before Package mode can become the default consumer path.
- Repair/verify the native CNA checkout separately; this repository must not modify it.
- Qualify additional platforms or optional engines only through independent measured runs.
