# CNA-Kotlin

CNA-Kotlin is a functional, deliberately small Kotlin/JVM ergonomic layer over
[CNA-Java](https://github.com/openeggbert/cna-java). It is **not** a native binding and does not
implement XNA 4.0 itself.

```text
Kotlin/JVM game
    ↓
org.openeggbert.cna.kotlin.*   optional extension functions
    ↓
Microsoft.Xna.Framework.*     the exact CNA-Java classes
    ↓
CNA-Java JNI
    ↓
CNA stable C ABI 0.7.0
    ↓
CNA C++
```

CNA-Java is an API dependency, so consumers directly import its authoritative XNA projection.
CNA-Kotlin contains no `Microsoft.Xna.Framework` classes, JNI, native libraries, native handles,
resource wrappers, or second ownership mechanism.

## Dependency and baseline

```kotlin
dependencies {
    implementation("org.openeggbert:cna-kotlin:0.1.0-SNAPSHOT")
}
```

The baseline is Java 17, Gradle 8.12, and Kotlin/JVM 2.1.10. The current integration is pinned to
`org.openeggbert:cna-java:0.1.0-SNAPSHOT`; XNA coverage is whatever that exact CNA-Java publication
implements. CNA-Kotlin does not run or duplicate CNA-Java's XNA metadata verifier.

## Direct CNA-Java use

Kotlin can already use most of CNA-Java naturally:

```kotlin
import Microsoft.Xna.Framework.Vector2

val velocity = Vector2(10f, 5f)
val moved = Vector2.Multiply(velocity, 0.5f)
```

JavaBean getters and setters appear as Kotlin properties, while mapped XNA methods retain their
PascalCase names. CNA-Java does not currently publish nullability annotations, so unadapted Java
references remain platform types.

## Optional Kotlin ergonomics

The math package maps only operators actually represented by CNA-Java methods, and every operator
delegates to that method rather than repeating its formula:

```kotlin
import Microsoft.Xna.Framework.Vector2
import org.openeggbert.cna.kotlin.math.*

val velocity = Vector2(10f, 5f)
val moved = velocity * 0.5f
```

Operators cover `Vector2`, `Vector3`, `Vector4`, `Matrix`, `Quaternion`, and XNA's defined
`Color * Float`. Reified helpers remove class-token boilerplate without changing the underlying
registry, cache, service identity, or ownership:

```kotlin
import Microsoft.Xna.Framework.Content.ContentManager
import Microsoft.Xna.Framework.Vector2
import org.openeggbert.cna.kotlin.content.load

fun loadSpawn(content: ContentManager): Vector2? = content.load("spawn")
```

`load<T>` is intentionally nullable because CNA-Java's managed reader path can return `null` for a
reference asset. `ServiceProvider.getService<T>()` is also nullable and returns the same registered
object. CNA-Java resources already implement `AutoCloseable`, so use Kotlin's standard `use`:

```kotlin
HelloGame(60).use { game -> game.Run() }
```

There is no custom close helper, finalizer, Flow/coroutine layer, Game DSL, or resource wrapper.

## Build and verify

Supply a Maven repository containing the exact CNA-Java snapshot. The repository's integration
script creates one temporarily and never writes development snapshots to the global Maven cache.

```bash
./gradlew clean check -PcnaRepository=/path/to/temporary/maven
./gradlew test -PcnaRepository=/path/to/temporary/maven
./gradlew jar sourcesJar -PcnaRepository=/path/to/temporary/maven
scripts/verify-template.sh
```

`check` runs 13 focused adapter tests plus an artifact/API audit. The audit requires a compile/API
dependency on CNA-Java, permits only the three intended adapter bytecode classes, and rejects XNA
classes, native/JNI material, internal/native public types, wrapper subclasses, and obsolete
platform artifacts.

Set CNA-Java's existing variables to add real native execution:

```bash
CNA_NATIVE_LIBRARY=/path/to/libcna_c_api.so \
CNA_RUN_STABILITY_TEST=1 \
scripts/verify-template.sh
```

The script builds current CNA-Java, publishes CNA-Java and CNA-Kotlin to a fresh temporary Maven
repository, builds the maintained template, generates and builds an external consumer, checks it
for repository paths, and runs 60/600-frame native gates. It passes the CNA-Java-built JNI library
through `CNA_JNI_LIBRARY`; CNA-Kotlin performs no loading itself.

## Platform truth

| Target | Status |
| --- | --- |
| Linux x86-64/JVM, CNA 0.7.0 HEADLESS/NULL | Runtime verified: 60 and 600 frames |
| Windows/JVM | Planned; no current CNA-Java runtime evidence |
| macOS/JVM | Planned; no current CNA-Java runtime evidence |
| Android | Planned only after CNA-Java has a verified Android backend/package |
| Kotlin/JS/browser | Not supported by this architecture; use the CNA-TS ecosystem |
| Kotlin/Native/iOS | Not supported by this architecture; use CNA-Swift for Apple-native work |

See [architecture](docs/architecture.md), the measured [plan](plan.md), and [session history](NEXT.md).
CNA-Kotlin is licensed under the [Microsoft Public License](LICENSE).
