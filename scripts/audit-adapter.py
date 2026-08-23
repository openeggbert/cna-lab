#!/usr/bin/env python3
"""Fail when CNA-Kotlin stops being a thin, JVM-only CNA-Java adapter."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


EXPECTED_CLASSES = {
    "org/openeggbert/cna/kotlin/content/CnaContentExtensions.class",
    "org/openeggbert/cna/kotlin/math/CnaMathOperators.class",
    "org/openeggbert/cna/kotlin/services/CnaServiceExtensions.class",
}
NATIVE_SUFFIXES = (".so", ".dll", ".dylib", ".o", ".obj", ".a", ".c", ".cc", ".cpp", ".h", ".hpp")
FORBIDDEN_SOURCE = {
    r"\bexternal\s+fun\b": "Kotlin native declaration",
    r"\bSystem\s*\.\s*load(?:Library)?\s*\(": "native library loader",
    r"\bcom\.sun\.jna\b": "JNA dependency",
    r"\bjava\.lang\.foreign\b": "Panama/FFM dependency",
    r"\borg\.openeggbert\.cna\.internal\b": "CNA-Java implementation package",
    r"\bNativeHandle\b": "raw/native handle API",
    r"\bCNA_KOTLIN_NATIVE": "second native configuration",
}
FICTIONAL_ARTIFACTS = {
    "cna-kotlin-common",
    "cna-kotlin-desktop",
    "cna-kotlin-android",
    "cna-kotlin-js",
}
ALLOWED_PUBLIC_TYPES = {
    "Microsoft.Xna.Framework.Color",
    "Microsoft.Xna.Framework.Content.ContentManager",
    "Microsoft.Xna.Framework.Matrix",
    "Microsoft.Xna.Framework.Quaternion",
    "Microsoft.Xna.Framework.ServiceProvider",
    "Microsoft.Xna.Framework.Vector2",
    "Microsoft.Xna.Framework.Vector3",
    "Microsoft.Xna.Framework.Vector4",
    "java.lang.String",
}


def fail(message: str) -> None:
    raise RuntimeError(message)


def audit_jar(path: Path) -> list[str]:
    with zipfile.ZipFile(path) as archive:
        entries = {name for name in archive.namelist() if not name.endswith("/")}
    class_entries = {name for name in entries if name.endswith(".class")}
    if class_entries != EXPECTED_CLASSES:
        fail(f"unexpected adapter classes: {sorted(class_entries ^ EXPECTED_CLASSES)}")
    if any(name.startswith("Microsoft/Xna/Framework/") for name in entries):
        fail("CNA-Kotlin JAR contains CNA-Java-owned Microsoft.Xna.Framework classes")
    native = sorted(name for name in entries if name.lower().endswith(NATIVE_SUFFIXES))
    if native:
        fail(f"CNA-Kotlin JAR contains native/JNI material: {native}")
    unintended = sorted(
        name
        for name in entries
        if not name.startswith("META-INF/") and not name.startswith("org/openeggbert/cna/kotlin/")
    )
    if unintended:
        fail(f"CNA-Kotlin JAR contains unintended packages: {unintended}")
    return sorted(class_entries)


def audit_sources(source_root: Path) -> None:
    sources = sorted(source_root.rglob("*.kt"))
    if len(sources) != 3:
        fail(f"expected exactly three production Kotlin files, found {len(sources)}")
    for source in sources:
        text = source.read_text(encoding="utf-8")
        if not re.search(r"^package org\.openeggbert\.cna\.kotlin(?:\.|$)", text, re.MULTILINE):
            fail(f"production source uses an unintended package: {source}")
        for pattern, label in FORBIDDEN_SOURCE.items():
            if re.search(pattern, text):
                fail(f"{source} contains forbidden {label}")


def audit_pom(path: Path) -> None:
    root = ET.parse(path).getroot()
    namespace = {"m": "http://maven.apache.org/POM/4.0.0"}
    dependencies = root.findall("m:dependencies/m:dependency", namespace)
    coordinates = {
        (
            dependency.findtext("m:groupId", default="", namespaces=namespace),
            dependency.findtext("m:artifactId", default="", namespaces=namespace),
            dependency.findtext("m:scope", default="compile", namespaces=namespace),
        )
        for dependency in dependencies
    }
    if not any(group == "org.openeggbert" and artifact == "cna-java" and scope == "compile"
               for group, artifact, scope in coordinates):
        fail(f"published metadata lacks an API/compile dependency on CNA-Java: {sorted(coordinates)}")
    present_fictional = sorted(artifact for _, artifact, _ in coordinates if artifact in FICTIONAL_ARTIFACTS)
    if present_fictional:
        fail(f"published metadata depends on obsolete fictional artifacts: {present_fictional}")


def audit_public_bytecode(jar: Path, classpath: str, classes: list[str]) -> None:
    class_names = [entry.removesuffix(".class").replace("/", ".") for entry in classes]
    result = subprocess.run(
        ["javap", "-public", "-classpath", f"{jar}{os.pathsep}{classpath}", *class_names],
        check=True,
        capture_output=True,
        text=True,
    )
    public_api = result.stdout
    forbidden = ("org.openeggbert.cna.internal", "NativeHandle", " java.lang.foreign.", " com.sun.jna.")
    if any(token in public_api for token in forbidden):
        fail("public adapter bytecode exposes an internal/native implementation type")
    if " extends Microsoft.Xna.Framework." in public_api:
        fail("adapter introduces an XNA wrapper/subclass")
    methods = [line.strip() for line in public_api.splitlines()
               if line.strip().startswith("public static final")]
    if len(methods) != 41:
        fail(f"expected 41 public extension methods, found {len(methods)}")
    exposed_types = {
        token
        for method in methods
        for token in re.findall(r"(?:[a-zA-Z_$][\w$]*\.)+[A-Z][\w$]*", method)
    }
    unexpected_types = sorted(exposed_types - ALLOWED_PUBLIC_TYPES)
    if unexpected_types:
        fail(f"public extension signature exposes an unintended type: {unexpected_types}")
    exposed_primitives = {
        token
        for method in methods
        for token in re.findall(r"\b(?:boolean|byte|char|double|float|int|long|short)\b", method)
    }
    if exposed_primitives - {"float"}:
        fail(f"public extension signature exposes an unintended primitive: {sorted(exposed_primitives)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--pom", required=True, type=Path)
    parser.add_argument("--classpath", required=True)
    arguments = parser.parse_args()
    classes = audit_jar(arguments.jar)
    audit_sources(arguments.source_root)
    audit_pom(arguments.pom)
    audit_public_bytecode(arguments.jar, arguments.classpath, classes)
    print("CNA-Kotlin adapter audit: PASS")
    print(f"  production Kotlin files: 3")
    print(f"  adapter classes: {len(classes)}")
    print("  public adapter declarations: 41")
    print("  Microsoft.Xna.Framework classes: 0")
    print("  native/JNI artifact entries: 0")
    print("  CNA-Java API dependency: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print(f"CNA-Kotlin adapter audit: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
