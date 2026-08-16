# cna-kotlin-template

Modern cross-platform Kotlin Multiplatform (KMP) starter template for the [CNA](https://github.com/openeggbert/cna) framework, following XNA 4.0 patterns.

## Features

- **Multi-platform support**: Windows, Linux, macOS, Android, iOS, and Web.
- **Kotlin Multiplatform**: Shared code between Desktop, Android, and Web.
- **XNA 4.0 API Style**: Familiar lifecycle (Initialize, LoadContent, Update, Draw) using PascalCase naming.
- **Adaptive Graphics**:
  - **3D**: Renders a rotating textured cube on hardware with 3D capabilities.
  - **2D**: Falling back to a bouncing animated logo on 2D-only renderers.
- **Integrated Renderer Banner**: Displays the active graphics driver name using an internal bitmap font.
- **Gradle Kotlin DSL**: Modern build configuration using `.gradle.kts`.
- **Smoke Test Mode**: Automatic termination for CI/CD validation using the `--smoke-test` flag.

## Project Structure

- `game/`: Shared Kotlin Multiplatform module containing game logic.
  - `src/commonMain/kotlin/`: Shared `HelloGame` logic.
- `android/`: Android-specific module and Activity.
- `desktop/`: JVM launcher for Windows, Linux, and macOS.
- `web/`: Kotlin/JS project for running in the browser.

## Getting Started

### Prerequisites

- [JDK 17](https://adoptium.net/) or newer.
- [Gradle](https://gradle.org/install/) (or use the included wrapper).

### Building and Running (Desktop)

To run the game on your desktop:

```bash
./gradlew :desktop:run
```

### Running a Smoke Test

To verify the game starts and renders correctly:

```bash
./gradlew :desktop:run --args="--smoke-test"
```

### Android

To build the Android APK:

```bash
./gradlew :android:assembleDebug
```

### Web (Kotlin/JS)

To run the game in your browser using Vite/Webpack (development):

```bash
./gradlew :web:jsBrowserRun
```

To build a production distribution:

```bash
./gradlew :web:jsBrowserDistribution
```

The output will be in `web/build/distributions/`.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
