# CNA-Kotlin measured engineering plan

**Status:** thin Kotlin/JVM adapter, managed integration, generated consumer, and native template gates green

**Updated:** 2026-08-23

## Current verified state

- Artifact: `org.openeggbert:cna-kotlin:0.1.0-SNAPSHOT`.
- Baseline: Java 17, Kotlin/JVM 2.1.10, Gradle 8.12.
- Dependency: API/compile on `org.openeggbert:cna-java:0.1.0-SNAPSHOT`.
- Tested CNA-Java revision: `48920088b389cfe9033bda8391e646972268c71b`.
- Production: 3 Kotlin files, 41 public extension declarations, 0 wrapper classes.
- Artifact: 0 `Microsoft.Xna.Framework` classes and 0 native/JNI entries.
- Tests: 13 adapter cases and 5 maintained-template cases.
- Runtime: Linux x86-64, CNA ABI 0.7.0, HEADLESS graphics, NULL audio; 60/600 frames pass.

## Permanent architecture decision

CNA-Kotlin remains a Kotlin/JVM-only ergonomic layer over CNA-Java. CNA-Java owns the complete JVM
XNA projection, native handles, JNI callbacks, native library discovery, ownership, and XNA
compatibility measurement. CNA-Kotlin must never implement another copy of any of those layers.

## Kotlin ergonomics

Keep helpers only when direct Kotlin-to-Java syntax has recurring friction and the implementation
can delegate completely. Current justified groups are:

1. exact XNA math operators delegating to CNA-Java named methods;
2. reified nullable `ContentManager.load<T>` over the current class-token API;
3. reified nullable `ServiceProvider.getService<T>` preserving identity.

Kotlin's automatic Java properties and standard `AutoCloseable.use` need no adapter declaration.
Future additions require focused equivalence/identity/ownership tests and must not add dependencies
for coroutines, serialization, Compose, DSLs, or other unrelated Kotlin ecosystems.

## Adapter quality gates

`adapterAudit` and `check` enforce:

- CNA-Java remains a published API/compile dependency;
- only the three intended `org.openeggbert.cna.kotlin.*` bytecode classes exist;
- no XNA-owned classes, native binary/source entries, JNI declarations, loaders, internal types,
  raw handles, or XNA wrapper subclasses appear;
- no old `cna-kotlin-common`, `-desktop`, `-android`, or `-js` dependency enters metadata;
- all 39 operators equal their CNA-Java calls, helpers preserve nullability/identity, and standard
  `use`/explicit close retain CNA-Java behavior.

Do not reproduce CNA-Java's 257-reference-type metadata verifier here. Record its exact dependency
revision and measured state during integrations.

## Template and integration

The canonical template is one Kotlin/JVM application. It mirrors CNA-Java's verified moving-2D-
sprite canary and contains no KMP, Android, Web, fake renderer banner, fake capability switch, 3D,
or pretend XNB path. Its generator supports project name, game package/class, main package, group,
and artifact ID.

`scripts/verify-template.sh` must continue to:

1. build current CNA-Java and publish it to a fresh temporary Maven repository;
2. build/audit/publish CNA-Kotlin against that exact artifact;
3. build the maintained template from Maven artifacts;
4. generate, path-scan, build, and test an external consumer;
5. when `CNA_NATIVE_LIBRARY` is supplied, run both template and generated 60-frame gates;
6. when `CNA_RUN_STABILITY_TEST=1`, run the maintained 600-frame gate and require clean exit.

## Platform matrix

| Target | State |
| --- | --- |
| Linux x86-64/JVM, ABI 0.7.0 HEADLESS/NULL | Runtime verified |
| Windows/JVM | Planned, gated by CNA-Java evidence |
| macOS/JVM | Planned, gated by CNA-Java evidence |
| Android | Planned, gated by a real CNA-Java Android package/backend |
| Kotlin/JS | Not supported by this architecture |
| Kotlin/Native/iOS | Not supported by this architecture |

## Packaging and future work

Keep one unshaded artifact, `cna-kotlin`, with sources JAR and Maven publication. A documentation
JAR/Dokka task is intentionally absent until public documentation volume justifies it.

Possible future work is limited to ergonomics observed in real Kotlin games—for example a proven
class-token API or safe collection adaptation. Do not plan XNA types, JNI graphics/audio, XNB
backends, or native platforms here; those are CNA-Java responsibilities.
