#!/usr/bin/env python3
"""plan_03 IG-03-006: enforce Iron Gang's module boundaries.

docs/architecture.md states the dependency direction and which target may see what. Those are
rules, and a rule nothing checks is a convention that holds until someone is in a hurry. One of
them has already been broken once: a public header exposed sharp-runtime's JsonDocument, which
compiled for the library and broke the test build, because Text.Json is a *private* dependency of
iron_gang_core.

The rules, and why each one exists:

1. A public header (include/IronGang/**) must not include sharp-runtime (System/...) or a CNA
   internal header (CNA/Internal/...). Both are private dependencies of iron_gang_core; a public
   header that names their types forces every consumer to find them.

2. A public header must not reach into src/ with a relative include. Private headers live under
   src/ precisely so they are not part of the surface.

3. Only the executable may include CNA::Runtime (Game.hpp, GraphicsDeviceManager.hpp).
   iron_gang_core links CNA::GraphicsCore only, and the moment a library source includes Game.hpp
   the split stops being real.

Module membership is read from CMakeLists.txt rather than hard-coded here, so moving a source
between targets is checked against the same file that decides it.
"""

import argparse
import re
import sys
from pathlib import Path

PRIVATE_DEPENDENCY_PREFIXES = ("System/", "CNA/Internal/")
RUNTIME_HEADERS = (
    "Microsoft/Xna/Framework/Game.hpp",
    "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp",
)
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)


class LayeringError(ValueError):
    pass


def target_sources(cmake_text: str, declaration: str) -> list[str]:
    """The source list of one add_library/add_executable call, as written in CMakeLists.txt."""
    start = cmake_text.find(declaration)
    if start < 0:
        raise LayeringError(f"CMakeLists.txt has no {declaration!r}")
    end = cmake_text.find(")", start)
    if end < 0:
        raise LayeringError(f"unterminated {declaration!r} in CMakeLists.txt")
    body = cmake_text[start + len(declaration):end]
    return [line.strip() for line in body.split() if line.strip().endswith((".cpp", ".c"))]


def includes(path: Path) -> list[str]:
    return INCLUDE_PATTERN.findall(path.read_text(encoding="utf-8"))


def check(project_root: Path) -> list[str]:
    """Returns a list of violations; empty means the boundaries hold."""
    violations: list[str] = []
    cmake = (project_root / "CMakeLists.txt").read_text(encoding="utf-8")
    core_sources = target_sources(cmake, "add_library(iron_gang_core STATIC")
    if not core_sources:
        raise LayeringError("iron_gang_core has no sources; the checker would pass vacuously")

    include_root = project_root / "include"
    public_headers = sorted(include_root.rglob("*.hpp"))
    if not public_headers:
        raise LayeringError("no public headers found; the checker would pass vacuously")

    # The executable's own module. Its headers live in include/ for convenience but they are not
    # part of the library's surface, and they are the one place Game.hpp belongs.
    executable_module = include_root / "IronGang" / "Application"

    for header in public_headers:
        relative = header.relative_to(project_root).as_posix()
        is_executable_header = executable_module in header.parents
        for included in includes(header):
            if included.startswith(PRIVATE_DEPENDENCY_PREFIXES):
                violations.append(
                    f"{relative} includes {included}: a public header must not name a type from a "
                    f"private dependency of iron_gang_core")
            if included.startswith("../"):
                violations.append(
                    f"{relative} includes {included}: a public header must not reach into src/, "
                    f"where private headers live")
            if included in RUNTIME_HEADERS and not is_executable_header:
                violations.append(
                    f"{relative} includes {included}: only the executable may use CNA::Runtime")

    for source in core_sources:
        path = project_root / source
        if not path.is_file():
            raise LayeringError(f"CMakeLists.txt lists {source}, which does not exist")
        for included in includes(path):
            if included in RUNTIME_HEADERS:
                violations.append(
                    f"{source} includes {included}: it is an iron_gang_core source, and the library "
                    f"links CNA::GraphicsCore only")

    return violations


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project_root", nargs="?", default=".")
    options = parser.parse_args(argv)
    root = Path(options.project_root).resolve()
    try:
        violations = check(root)
    except LayeringError as error:
        print(f"check-layering: {error}", file=sys.stderr)
        return 2
    if violations:
        for violation in violations:
            print(f"check-layering: {violation}", file=sys.stderr)
        return 1
    print("check-layering: module boundaries hold")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
