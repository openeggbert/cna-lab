# CNA-Kotlin continuation handoff

**Updated:** 2026-08-23

## Initial state

The adapter contained only a two-line README saying it was not functional: 0 production source
files, no Gradle build, no wrapper, and no tests. The sibling template was clean at revision
`94688685ff07905e57531f6d3aec7d560d859f9a` but used an obsolete aspirational architecture:

- KMP `commonMain` plus desktop, Android, and Web modules;
- fictional `cna-kotlin-common`, `cna-kotlin-desktop`, `cna-kotlin-android`, and
  `cna-kotlin-js` dependencies;
- fake three-frame smoke, renderer name, 3D/depth capabilities, and BasicEffect cube;
- unsupported Windows/macOS/Android/iOS/Web claims;
- no Gradle Wrapper and no buildable CNA-Kotlin dependency.

Nothing from that API/build architecture was preserved. Only its valid PNG fixture and MIT license
were retained in the replacement template.

## CNA-Java source of truth

Initial inspection began while CNA-Java HEAD was `dd2040d1d1c6c93e528c1c2b5383a3f3a43f0eee`
with substantial pre-existing uncommitted work. That work was committed externally during this
session. Final integration rebuilt and tested the same resulting source at exact revision:

```text
48920088b389cfe9033bda8391e646972268c71b
org.openeggbert:cna-java:0.1.0-SNAPSHOT
Java baseline=17
Gradle=8.12
TARGET_TYPES=184
TARGET_MEMBERS=2492
MISSING_MEMBER=0
MISSING_TYPE=81
HEADER_ABI=0.7.0
BOUND_FUNCTIONS=338
LIBRARY_SYMBOL_CHECK=PASS (338/338)
```

At the verification checkpoint, the only CNA-Java worktree entry was its pre-existing untracked
`out`. A concurrent documentation-only CNA-Java `README.md` edit appeared afterward; the tested
revision and compiled source remained unchanged. CNA-Java and its template were not source-edited
by this task.

The Java API facts used here are current: `ContentManager.Load(Class<T>, String)`,
`ServiceProvider.GetService(Class<?>)`, JavaBean properties, CNA-Java `AutoCloseable` ownership,
and native discovery through `CNA_JNI_LIBRARY`, `CNA_NATIVE_LIBRARY`, and `CNA_NATIVE_DIR`.
CNA-Java currently publishes no nullability annotations.

## Adapter result

One artifact now exists: `org.openeggbert:cna-kotlin:0.1.0-SNAPSHOT`.

- 3 production Kotlin files;
- 39 math operators over `Vector2`, `Vector3`, `Vector4`, `Matrix`, `Quaternion`, and `Color`;
- 1 nullable reified `ContentManager.load<T>(String)`;
- 1 nullable reified `ServiceProvider.getService<T>()`;
- 41 public declarations total;
- no wrappers or production classes other than the three Kotlin file facades;
- no custom `use`, events, coroutines, DSL, or nullability fiction.

Every numerical operator delegates to the matching CNA-Java method. The content helper passes the
real current token-first signature; the service helper performs a safe cast and returns the exact
object.

## Tests and audits

Adapter `check`: **PASS**, 13 test cases.

```text
operator test cases=7
operator equivalence assertions=39
content helper cases=2
service helper cases=2
AutoCloseable/use/explicit-close cases=2
mutable value-semantics case=1 (within operator cases)
```

The artifact audit passed with exactly 3 adapter classes, 0
`Microsoft/Xna/Framework` classes, 0 native/JNI entries, a compile/API CNA-Java dependency, no
internal/native public types, and no wrapper subclass. Archive tasks disable file timestamps and
use reproducible entry order.

## Template result

The sibling is now a single-module Kotlin/JVM desktop application with:

```text
build.gradle.kts
settings.gradle.kts
gradle.properties
gradlew / gradlew.bat / gradle/wrapper/*
src/main/kotlin/com/openeggbert/cna/template/{HelloGame,Main}.kt
src/main/resources/Content/logo.png
src/test/kotlin/com/openeggbert/cna/template/MainTest.kt
scripts/generate_project.py
```

KMP removed: yes. Web removed from the active build: yes. Android removed from the active build:
yes. The 5 template tests pass. A generated standalone consumer with separate game/main packages
passes `clean check installDist`, contains no sibling/absolute developer path, and resolves only the
temporary Maven publications.

Real features exercised are Game/GameTime lifecycle, GraphicsDeviceManager, mapped window title,
GraphicsDevice.Clear, Keyboard, Mouse, real raw PNG decoding, Texture2D, SpriteBatch, moving
Vector2 through CNA-Kotlin, deterministic child close, Game `use`, and clean exit.

## Native and publication evidence

`scripts/verify-template.sh` passed end to end using:

```text
OS=Linux x86_64
JVM executing build/runtime=OpenJDK 21.0.11
compiled bytecode baseline=Java 17
CNA_NATIVE_LIBRARY=/tmp/cna-java-native-working-070/modules/c-api/libcna_c_api.so
native file=ELF 64-bit x86-64
native ABI=0.7.0
renderer=HEADLESS
audio=NULL
maintained template smoke=60/60 PASS
generated consumer smoke=60/60 PASS
maintained template stability=600/600 PASS
process exit=0
```

CNA-Java and CNA-Kotlin were published with `publishToMavenLocal` redirected via
`-Dmaven.repo.local` to a fresh `mktemp` repository, which was deleted on exit. No global Maven
snapshot was installed. This proves the runtime chain:

```text
Kotlin → CNA-Kotlin → CNA-Java → CNA-Java JNI → CNA C ABI → CNA C++
```

## Continuation rules

Preserve the one-artifact Kotlin/JVM architecture and all audit zeros. Add an ergonomic declaration
only after direct Kotlin use shows real friction, and test it against CNA-Java behavior/identity.
Re-run the isolated verifier whenever the CNA-Java snapshot changes. Advance platform claims only
with new CNA-Java runtime evidence; never introduce Kotlin/JS, Kotlin/Native, or another JNI bridge.
