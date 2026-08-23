# CNA-Kotlin desktop starter

This is the end-to-end Kotlin/JVM canary and reusable starter for
`org.openeggbert:cna-kotlin:0.1.0-SNAPSHOT`. It uses the actual CNA-Java classes
under `Microsoft.Xna.Framework.*`; CNA-Kotlin contributes only optional extension functions.

## Verified scope

The starter follows the current CNA-Java desktop canary: a real `Game` lifecycle,
`GraphicsDeviceManager`, mapped window title, keyboard and mouse polling, raw PNG decoding through
`Texture2D.FromStream`, `GraphicsDevice.Clear`, a moving `SpriteBatch` texture, deterministic
resource close, and 60/600-frame modes. Movement uses CNA-Kotlin's delegating `Vector2.plus`
operator, so a native run exercises both artifacts before CNA-Java enters its own JNI bridge.

It does not claim XNB texture content, 3D, renderer capabilities, Android, iOS, or browser support.

| Target | Status |
| --- | --- |
| Linux x86-64/JVM, CNA 0.7.0 HEADLESS/NULL | Runtime verified: 60 and 600 frames |
| Windows/JVM | Planned; inherited from future CNA-Java runtime evidence |
| macOS/JVM | Planned; inherited from future CNA-Java runtime evidence |
| Android | Planned only after CNA-Java ships and verifies an Android backend/package |
| Kotlin/JS/browser | Not supported by this architecture; use the CNA-TS ecosystem |
| Kotlin/Native/iOS | Not supported by this architecture; use CNA-Swift for Apple-native work |

## Build and run

JDK 17 or newer is required. Supply a Maven repository containing the exact CNA-Kotlin and
CNA-Java publications; the sibling verification workflow uses a fresh temporary repository rather
than the global Maven cache.

```bash
./gradlew clean check -PcnaRepository=/absolute/path/to/a/maven/repository
```

CNA-Java owns native discovery. Use its existing variables—there is no Kotlin-specific loader:

```bash
CNA_JNI_LIBRARY=/path/to/libcna_java_jni.so \
CNA_NATIVE_LIBRARY=/path/to/libcna_c_api.so \
./gradlew run \
  -PcnaRepository=/absolute/path/to/a/maven/repository \
  --args='--smoke-test'
```

Modes are `--smoke-test` (60 frames), `--stability-test` (600 frames), and `--frames N` or
`--frames=N`. Without a limit, the game runs until the platform requests exit.

## Generate a standalone project

```bash
python3 scripts/generate_project.py \
  --output /tmp/asteroids-kotlin \
  --project-name 'Asteroids Kotlin' \
  --package com.example.asteroids.game \
  --main-package com.example.asteroids.desktop \
  --game-class AsteroidsGame \
  --group com.example \
  --artifact-id asteroids-kotlin
```

The generated copy has no sibling or absolute-path dependency. Build it with the same
`-PcnaRepository=...` argument. When the CNA-Java native libraries are supplied, the generated
project supports the same real 60/600-frame runs.

Licensed under the [MIT License](LICENSE).
