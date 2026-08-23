#!/usr/bin/env python3
"""Create a standalone Kotlin/JVM CNA starter from this maintained template."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil


SOURCE_PACKAGE = "com.openeggbert.cna.template"
SOURCE_GAME_CLASS = "HelloGame"


def kotlin_name(value: str, label: str) -> str:
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", value):
        raise argparse.ArgumentTypeError(f"{label} is not a Kotlin identifier: {value}")
    return value


def kotlin_package(value: str) -> str:
    parts = value.split(".")
    if not parts or any(not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", part) for part in parts):
        raise argparse.ArgumentTypeError(f"invalid Kotlin package: {value}")
    return value


def add_import(text: str, imported: str) -> str:
    package_line = re.search(r"^package .+$", text, re.MULTILINE)
    if package_line is None:
        raise RuntimeError("Kotlin source has no package declaration")
    return text[: package_line.end()] + f"\n\nimport {imported}" + text[package_line.end() :]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--project-name", default="My CNA Game")
    parser.add_argument("--package", default="com.example.game", type=kotlin_package)
    parser.add_argument("--main-package", default=None, type=kotlin_package)
    parser.add_argument(
        "--game-class",
        default="MyGame",
        type=lambda value: kotlin_name(value, "game class"),
    )
    parser.add_argument("--group", default="com.example", type=kotlin_package)
    parser.add_argument("--artifact-id", default="my-cna-game")
    arguments = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9_.-]+", arguments.artifact_id):
        parser.error("artifact ID may contain only letters, digits, dots, underscores, and hyphens")

    source = Path(__file__).resolve().parents[1]
    output = arguments.output.resolve()
    main_package = arguments.main_package or arguments.package
    shutil.copytree(
        source,
        output,
        ignore=shutil.ignore_patterns(
            ".git", ".gradle", ".kotlin", "build", ".cna-repository", "__pycache__"
        ),
    )

    old_source = output / "src/main/kotlin" / Path(SOURCE_PACKAGE.replace(".", "/"))
    old_tests = output / "src/test/kotlin" / Path(SOURCE_PACKAGE.replace(".", "/"))
    hello_source = (old_source / f"{SOURCE_GAME_CLASS}.kt").read_text(encoding="utf-8")
    main_source = (old_source / "Main.kt").read_text(encoding="utf-8")
    test_source = (old_tests / "MainTest.kt").read_text(encoding="utf-8")

    hello_source = hello_source.replace(SOURCE_PACKAGE, arguments.package)
    hello_source = hello_source.replace(SOURCE_GAME_CLASS, arguments.game_class)
    main_source = main_source.replace(SOURCE_PACKAGE, main_package)
    main_source = main_source.replace(SOURCE_GAME_CLASS, arguments.game_class)
    test_source = test_source.replace(SOURCE_PACKAGE, main_package)
    test_source = test_source.replace(SOURCE_GAME_CLASS, arguments.game_class)
    if arguments.package != main_package:
        imported = f"{arguments.package}.{arguments.game_class}"
        main_source = add_import(main_source, imported)
        test_source = add_import(test_source, imported)

    shutil.rmtree(old_source)
    shutil.rmtree(old_tests)
    game_dir = output / "src/main/kotlin" / Path(arguments.package.replace(".", "/"))
    main_dir = output / "src/main/kotlin" / Path(main_package.replace(".", "/"))
    test_dir = output / "src/test/kotlin" / Path(main_package.replace(".", "/"))
    game_dir.mkdir(parents=True, exist_ok=True)
    main_dir.mkdir(parents=True, exist_ok=True)
    test_dir.mkdir(parents=True, exist_ok=True)
    (game_dir / f"{arguments.game_class}.kt").write_text(hello_source, encoding="utf-8")
    (main_dir / "Main.kt").write_text(main_source, encoding="utf-8")
    (test_dir / "MainTest.kt").write_text(test_source, encoding="utf-8")

    settings = (output / "settings.gradle.kts").read_text(encoding="utf-8")
    (output / "settings.gradle.kts").write_text(
        settings.replace('rootProject.name = "cna-kotlin-template"',
                         f'rootProject.name = "{arguments.artifact_id}"'),
        encoding="utf-8",
    )
    (output / "gradle.properties").write_text(
        "org.gradle.jvmargs=-Xmx1g -Dfile.encoding=UTF-8\n"
        "kotlin.code.style=official\n"
        f"projectGroup={arguments.group}\n"
        f"artifactId={arguments.artifact_id}\n"
        f"applicationMainClass={main_package}.MainKt\n",
        encoding="utf-8",
    )
    readme = (output / "README.md").read_text(encoding="utf-8")
    readme = readme.replace("CNA-Kotlin desktop starter", arguments.project_name)
    (output / "README.md").write_text(readme, encoding="utf-8")
    print(f"Generated {arguments.project_name} at {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
