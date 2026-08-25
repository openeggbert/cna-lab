#!/usr/bin/env python3
"""Stage, validate, archive, extract, and smoke-test the Linux EasyGL release."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import os
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath


REQUIRED_LICENSES = (
    "CNA.txt",
    "Jolt-Physics.txt",
    "SDL3.txt",
    "SDL3_mixer.txt",
    "cgltf.txt",
    "easy-gl.txt",
    "meta-gl.txt",
    "nlohmann-json.txt",
    "sharp-runtime.txt",
    "stb.txt",
)
REQUIRED_ASSETS = (
    "audio/engine_loop.wav",
    "audio/footstep.wav",
    "audio/horn.wav",
    "config/game.json",
    "cutscenes/prologue_intro.cutscene.json",
    "dialogues/prologue.dialogue.txt",
    "missions/prologue.mission.json",
    "generated/models/cnj/warehouse.cnj",
    "generated/models/cnj/vehicle_body.cnj",
    "generated/models/cnj/vehicle_cabin.cnj",
    "generated/models/cnj/vehicle_wheel.cnj",
    "generated/models/cnj/vehicle_windshield.cnj",
    "generated/models/cnj/test_character.cnj",
)
EXPECTED_SMOKE_LINES = (
    "[IronGang] Loaded generated warehouse.cnj",
    "[IronGang] Loaded generated vehicle_{body,cabin,windshield,wheel}.cnj",
    "[IronGang] Loaded generated test_character.cnj",
    "[IronGang] Loaded engine_loop.wav",
    "[IronGang] Loaded footstep.wav",
    "[IronGang] Loaded horn.wav",
)
FORBIDDEN_DIRECT_LIBRARIES = ("libavcodec", "libavformat", "libavutil", "libswresample")


class ReleaseArchiveError(RuntimeError):
    pass


def _run(
    arguments: list[str],
    *,
    cwd: Path | None = None,
    environment: dict[str, str] | None = None,
    timeout: int = 120,
) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            arguments,
            cwd=cwd,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ReleaseArchiveError(f"could not run {' '.join(arguments)}: {error}") from error
    if result.returncode != 0:
        output = "\n".join(part for part in (result.stdout, result.stderr) if part).strip()
        raise ReleaseArchiveError(
            f"command failed with exit {result.returncode}: {' '.join(arguments)}"
            + (f"\n{output}" if output else "")
        )
    return result


def _required_regular_file(path: Path, label: str, *, executable: bool = False) -> None:
    if not path.is_file() or path.stat().st_size <= 0:
        raise ReleaseArchiveError(f"missing or empty {label}: {path}")
    if executable and not os.access(path, os.X_OK):
        raise ReleaseArchiveError(f"{label} is not executable: {path}")


def validate_layout(package_root: Path) -> None:
    package_root = package_root.resolve()
    if not package_root.is_dir():
        raise ReleaseArchiveError(f"package root is not a directory: {package_root}")

    _required_regular_file(package_root / "bin/iron_gang", "game executable", executable=True)
    notice_root = package_root / "share/iron-gang"
    for name in (
        "README.md",
        "LICENSE",
        "THIRD_PARTY.md",
        "THIRD_PARTY_ASSETS.md",
        "release-packaging.md",
    ):
        _required_regular_file(notice_root / name, "package notice")
    for name in REQUIRED_LICENSES:
        _required_regular_file(notice_root / "licenses" / name, "dependency license")
    for relative in REQUIRED_ASSETS:
        _required_regular_file(notice_root / "assets" / relative, "runtime asset")

    library_root = package_root / "lib/iron-gang"
    for name in ("libSDL3.so.0", "libSDL3_mixer.so.0"):
        _required_regular_file(library_root / name, "SDL runtime library")

    forbidden = (
        package_root / "include",
        package_root / "lib/libJolt.a",
        package_root / "lib/cmake/Jolt",
        notice_root / "assets/source",
        notice_root / "assets/licenses",
        notice_root / "assets/generated/models/glb",
    )
    present = [str(path.relative_to(package_root)) for path in forbidden if path.exists()]
    if present:
        raise ReleaseArchiveError(
            "development-only content leaked into runtime package: " + ", ".join(present)
        )


def verify_linux_runtime(package_root: Path, *, run_smoke: bool = True) -> None:
    package_root = package_root.resolve()
    executable = package_root / "bin/iron_gang"
    dynamic = _run(["readelf", "-d", str(executable)]).stdout
    for required in ("libSDL3.so.0", "libSDL3_mixer.so.0"):
        if f"[{required}]" not in dynamic:
            raise ReleaseArchiveError(f"installed executable does not require {required}")
    for forbidden in FORBIDDEN_DIRECT_LIBRARIES:
        if forbidden in dynamic:
            raise ReleaseArchiveError(
                f"unused video dependency leaked into installed executable: {forbidden}"
            )
    if "$ORIGIN/../lib/iron-gang" not in dynamic:
        raise ReleaseArchiveError("installed executable lacks the relative private-library RUNPATH")
    if "/cnanext/" in dynamic or "/iron-gang/cmake-build-" in dynamic:
        raise ReleaseArchiveError("installed executable retains a build-workspace RUNPATH")

    linkage = _run(["ldd", str(executable)]).stdout
    if "not found" in linkage:
        raise ReleaseArchiveError("installed executable has an unresolved shared library:\n" + linkage)
    package_text = str(package_root)
    for name in ("libSDL3.so.0", "libSDL3_mixer.so.0"):
        matching = [line for line in linkage.splitlines() if name in line]
        if len(matching) != 1 or package_text not in matching[0]:
            raise ReleaseArchiveError(
                f"{name} did not resolve from the extracted private runtime directory"
            )

    if not run_smoke:
        return
    environment = os.environ.copy()
    environment.pop("DISPLAY", None)
    environment.pop("WAYLAND_DISPLAY", None)
    environment["SDL_VIDEODRIVER"] = "offscreen"
    environment["SDL_AUDIODRIVER"] = "dummy"
    smoke = _run(
        [str(executable), "--smoke", "5", "--vsync", "off"],
        cwd=package_root,
        environment=environment,
    )
    output = smoke.stdout + smoke.stderr
    missing = [line for line in EXPECTED_SMOKE_LINES if line not in output]
    if missing:
        raise ReleaseArchiveError(
            "installed smoke run did not load the packaged runtime assets: " + ", ".join(missing)
        )


def read_cmake_cache(cache_path: Path) -> dict[str, str]:
    try:
        lines = cache_path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ReleaseArchiveError(f"could not read CMake cache {cache_path}: {error}") from error
    values: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        key, _type = key_and_type.split(":", 1)
        values[key] = value
    return values


def _cache_value(cache: dict[str, str], key: str) -> str:
    value = cache.get(key, "")
    if not value:
        raise ReleaseArchiveError(f"CMake cache is missing {key}")
    return value


def package_identity(cache: dict[str, str]) -> tuple[str, str, str]:
    if _cache_value(cache, "CMAKE_BUILD_TYPE") != "Release":
        raise ReleaseArchiveError("release archive requires CMAKE_BUILD_TYPE=Release")
    if _cache_value(cache, "CNA_GRAPHICS_RENDERER") != "OPENGLES3":
        raise ReleaseArchiveError("release archive requires CNA_GRAPHICS_RENDERER=OPENGLES3")
    if _cache_value(cache, "CNA_ENABLE_VIDEO") != "OFF":
        raise ReleaseArchiveError("release archive requires CNA_ENABLE_VIDEO=OFF")
    system = _cache_value(cache, "IRON_GANG_PACKAGE_SYSTEM_NAME")
    if system != "Linux":
        raise ReleaseArchiveError(f"this release archive path currently supports Linux, not {system}")
    version = _cache_value(cache, "CMAKE_PROJECT_VERSION")
    architecture = _cache_value(cache, "IRON_GANG_PACKAGE_SYSTEM_PROCESSOR")
    if architecture.casefold() in ("amd64", "x64"):
        architecture = "x86_64"
    token = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
    if token.fullmatch(version) is None or token.fullmatch(architecture) is None:
        raise ReleaseArchiveError("version and architecture must be archive-safe tokens")
    return version, system.casefold(), architecture.casefold()


def _tar_info(name: str, mode: int, epoch: int) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.mode = mode
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "root"
    info.mtime = epoch
    return info


def create_reproducible_archive(
    package_root: Path, archive_path: Path, top_level_name: str, epoch: int
) -> None:
    package_root = package_root.resolve()
    if epoch < 0:
        raise ReleaseArchiveError("SOURCE_DATE_EPOCH must be non-negative")
    paths = sorted(package_root.rglob("*"), key=lambda item: item.relative_to(package_root).as_posix())
    try:
        with archive_path.open("wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch) as compressed:
                with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                    root_info = _tar_info(top_level_name, 0o755, epoch)
                    root_info.type = tarfile.DIRTYPE
                    archive.addfile(root_info)
                    for path in paths:
                        relative = path.relative_to(package_root).as_posix()
                        name = f"{top_level_name}/{relative}"
                        details = path.lstat()
                        info = _tar_info(name, stat.S_IMODE(details.st_mode), epoch)
                        if path.is_symlink():
                            info.type = tarfile.SYMTYPE
                            info.linkname = os.readlink(path)
                            archive.addfile(info)
                        elif path.is_dir():
                            info.type = tarfile.DIRTYPE
                            archive.addfile(info)
                        elif path.is_file():
                            info.size = details.st_size
                            with path.open("rb") as source:
                                archive.addfile(info, source)
                        else:
                            raise ReleaseArchiveError(f"unsupported package file type: {path}")
    except (OSError, tarfile.TarError) as error:
        raise ReleaseArchiveError(f"could not create archive {archive_path}: {error}") from error


def _validate_archive_members(members: list[tarfile.TarInfo], top_level_name: str) -> None:
    prefix = PurePosixPath(top_level_name)
    names: set[str] = set()
    for member in members:
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != prefix.name:
            raise ReleaseArchiveError(f"unsafe or unexpected archive member: {member.name}")
        if member.name in names:
            raise ReleaseArchiveError(f"duplicate archive member: {member.name}")
        names.add(member.name)
        if member.issym() or member.islnk():
            target = PurePosixPath(member.linkname)
            if target.is_absolute() or ".." in target.parts:
                raise ReleaseArchiveError(
                    f"unsafe archive link target: {member.name} -> {member.linkname}"
                )


def extract_verified_archive(archive_path: Path, destination: Path, top_level_name: str) -> Path:
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            members = archive.getmembers()
            _validate_archive_members(members, top_level_name)
            archive.extractall(destination, members=members, filter="data")
    except (OSError, tarfile.TarError) as error:
        raise ReleaseArchiveError(f"could not extract archive {archive_path}: {error}") from error
    root = destination / top_level_name
    if not root.is_dir():
        raise ReleaseArchiveError("archive did not contain its declared top-level directory")
    return root


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _source_epoch(project_root: Path, explicit: int | None) -> int:
    if explicit is not None:
        return explicit
    environment = os.environ.get("SOURCE_DATE_EPOCH")
    if environment is not None:
        try:
            return int(environment)
        except ValueError as error:
            raise ReleaseArchiveError("SOURCE_DATE_EPOCH must be an integer") from error
    result = _run(["git", "log", "-1", "--format=%ct"], cwd=project_root)
    try:
        return int(result.stdout.strip())
    except ValueError as error:
        raise ReleaseArchiveError("could not derive SOURCE_DATE_EPOCH from git") from error


def _atomic_text(path: Path, contents: str) -> None:
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as destination:
            destination.write(contents)
            temporary = Path(destination.name)
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def build_archive(
    build_dir: Path,
    output_dir: Path,
    epoch: int,
) -> tuple[Path, str]:
    cache = read_cmake_cache(build_dir / "CMakeCache.txt")
    version, system, architecture = package_identity(cache)
    top_level_name = f"iron-gang-{version}-{system}-{architecture}"
    output_dir.mkdir(parents=True, exist_ok=True)
    final_archive = output_dir / f"{top_level_name}.tar.gz"

    with tempfile.TemporaryDirectory(prefix=".iron-gang-package-", dir=build_dir) as staging_name:
        staging = Path(staging_name)
        package_root = staging / top_level_name
        _run(["cmake", "--install", str(build_dir), "--prefix", str(package_root)])
        validate_layout(package_root)
        verify_linux_runtime(package_root)

        temporary_archive: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                dir=output_dir, prefix=f".{final_archive.name}.", suffix=".tmp", delete=False
            ) as temporary:
                temporary_archive = Path(temporary.name)
            create_reproducible_archive(package_root, temporary_archive, top_level_name, epoch)
            with tempfile.TemporaryDirectory(prefix=".iron-gang-extract-", dir=build_dir) as extract_name:
                extracted = extract_verified_archive(
                    temporary_archive, Path(extract_name), top_level_name
                )
                validate_layout(extracted)
                verify_linux_runtime(extracted)
            os.replace(temporary_archive, final_archive)
            temporary_archive = None
        finally:
            if temporary_archive is not None:
                temporary_archive.unlink(missing_ok=True)

    digest = sha256(final_archive)
    _atomic_text(final_archive.with_suffix(final_archive.suffix + ".sha256"), f"{digest}  {final_archive.name}\n")
    return final_archive, digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", default=".")
    parser.add_argument("--build-dir", default="cmake-build-release-easygl")
    parser.add_argument("--output-dir")
    parser.add_argument("--source-date-epoch", type=int)
    arguments = parser.parse_args()
    try:
        project_root = Path(arguments.project_root).resolve()
        build_dir = Path(arguments.build_dir)
        build_dir = build_dir.resolve() if build_dir.is_absolute() else (project_root / build_dir).resolve()
        output_dir = (
            Path(arguments.output_dir).resolve()
            if arguments.output_dir and Path(arguments.output_dir).is_absolute()
            else (project_root / arguments.output_dir).resolve()
            if arguments.output_dir
            else build_dir / "dist"
        )
        if not project_root.is_dir() or not build_dir.is_dir():
            raise ReleaseArchiveError("project root and configured build directory must exist")
        epoch = _source_epoch(project_root, arguments.source_date_epoch)
        archive, digest = build_archive(build_dir, output_dir, epoch)
        print(f"release-archive: verified {archive}")
        print(f"release-archive: sha256 {digest}")
        return 0
    except ReleaseArchiveError as error:
        print(f"release-archive: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
