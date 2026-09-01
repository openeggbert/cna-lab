# Architecture

## Boundary

CNA-Kotlin is an additive Kotlin/JVM source-syntax layer. The object graph is always CNA-Java's:

```text
Kotlin application
    ├── direct calls/properties ──────────────┐
    └── org.openeggbert.cna.kotlin extensions │
                                              ↓
                              Microsoft.Xna.Framework.* (CNA-Java)
                                              ↓
                              org.openeggbert.cna.internal.*
                                              ↓
                                      CNA-Java JNI adapter
                                              ↓
                                   CNA stable C ABI 0.7.0
                                              ↓
                                           CNA C++
```

There is exactly one JVM implementation of `Microsoft.Xna.Framework.*`, in CNA-Java. The
CNA-Kotlin JAR may contain only `org.openeggbert.cna.kotlin.*` adapter bytecode and metadata. It is
not shaded, and CNA-Java is published as an API/compile dependency so consumers see the original
classes transitively.

## Measured CNA-Java contract

Integration was run against:

- revision `48920088b389cfe9033bda8391e646972268c71b`;
- `org.openeggbert:cna-java:0.1.0-SNAPSHOT`;
- Java 17 bytecode/API baseline and Gradle 8.12;
- XNA 4.0 Windows-runtime projection measuring 184 target types / 2,492 target members;
- zero missing members for present types, with 81 dependency-group types still missing;
- CNA stable ABI 0.7.0 and 338/338 verified bound native symbols;
- Linux x86-64, HEADLESS graphics, NULL audio as the only runtime-qualified platform.

CNA-Java's public projection packages are `Microsoft.Xna.Framework` and its `Content`, `Graphics`,
`Graphics.PackedVector`, `Input`, and `Input.Touch` subpackages. Required Java projections also
exist under `System`, `System.Collections.Generic`, `System.IO`, and `System.Resources`.
`org.openeggbert.cna.content` contains CNA-Java's opt-in content-reader registry.
`org.openeggbert.cna.internal` is implementation/JNI territory and is forbidden from CNA-Kotlin's
public API.

## Kotlin interop decisions

CNA-Java maps XNA properties to JavaBean `getFoo`/`setFoo`; Kotlin already exposes those naturally
as `foo`. XNA methods remain PascalCase. No property aliases are added because Kotlin's built-in
Java interop already solves the problem.

The math operators call only the corresponding CNA-Java `Add`, `Subtract`, `Negate`, `Multiply`,
or `Divide` method. They never duplicate a numerical formula, so operation order and binary32
behavior remain CNA-Java's. The returned mutable value object is the same result CNA-Java would
return; there is no wrapper or duplicate state.

`ContentManager.load<T>(name)` passes `T::class.java` to the real current signature
`Load(Class<T>, String)`. It remains nullable because the current managed reader route may return
null and CNA-Java publishes no nullability metadata. `ServiceProvider.getService<T>()` delegates to
`GetService(Class<?>)`, performs only a safe cast, and preserves identity. Neither helper owns or
caches anything.

CNA-Java's `Game`, `ContentManager`, `GraphicsDevice`, and `GraphicsResource` hierarchy implement
`AutoCloseable`. Kotlin standard-library `use` already provides deterministic close with suppressed
exception behavior; CNA-Kotlin adds no close abstraction. Tests cover `use`, explicit close, and
idempotence, while the native template gates cover real child cleanup and game destruction.

## Native loading and runtime proof

CNA-Java alone recognizes `cna.java.jniLibrary`/`CNA_JNI_LIBRARY`,
`cna.native.library`/`CNA_NATIVE_LIBRARY`, and `CNA_NATIVE_DIR`. CNA-Kotlin defines no environment
variable, `external fun`, library loader, native declaration, or C/C++ source.

The integration verifier reuses CNA-Java's `publishToMavenLocal` task with
`-Dmaven.repo.local=<temporary-directory>`. It then publishes CNA-Kotlin to the same isolated
repository and builds both the maintained template and a generated external consumer using only
Maven coordinates. Native runs point `CNA_JNI_LIBRARY` at the adapter built by CNA-Java. Template
tests additionally prove that `Game`, `Vector2`, and `org.openeggbert.cna.internal.NativeBindings`
share the CNA-Java code source while the operator bytecode has the distinct CNA-Kotlin code source.

The resulting measured path is:

```text
HelloGame.kt
→ CNA-Kotlin Vector2.plus
→ CNA-Java Vector2.Add
→ CNA-Java Game/SpriteBatch/Texture2D
→ CNA-Java-built lib(cna_java_jni)
→ CNA C ABI 0.7.0
→ CNA C++ HEADLESS/NULL
```

## Deliberate exclusions

- no Kotlin Multiplatform, `commonMain`, Kotlin/JS, or Kotlin/Native;
- no Android module until CNA-Java publishes and verifies Android JNI/runtime integration;
- no second XNA type graph or native bridge;
- no wrappers around resources, devices, content, Game, or services;
- no coroutine/event Flow layer and no Game DSL;
- no independent XNA compatibility score;
- no renderer/capability inference and no unverified 3D demo.

Kotlin/JS belongs with CNA-TS. Apple-native/iOS belongs with CNA-Swift. Desktop and future Android
claims can advance only after the matching CNA-Java runtime evidence exists.
