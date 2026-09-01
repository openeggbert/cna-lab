#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import io
import os
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/release_archive.py")
SPEC = importlib.util.spec_from_file_location("iron_gang_release_archive", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"could not load {SCRIPT}")
ARCHIVE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ARCHIVE)


class ReleaseArchiveTests(unittest.TestCase):
    def create_package(self, root: Path) -> None:
        executable = root / "bin/iron_gang"
        executable.parent.mkdir(parents=True)
        executable.write_text("#!/usr/bin/env sh\nexit 0\n", encoding="utf-8")
        executable.chmod(0o755)

        share = root / "share/iron-gang"
        share.mkdir(parents=True)
        for notice in (
            "README.md",
            "LICENSE",
            "THIRD_PARTY.md",
            "THIRD_PARTY_ASSETS.md",
            "release-packaging.md",
        ):
            (share / notice).write_text(f"fixture {notice}\n", encoding="utf-8")
        licenses = share / "licenses"
        licenses.mkdir()
        for name in ARCHIVE.REQUIRED_LICENSES:
            (licenses / name).write_text(f"fixture {name}\n", encoding="utf-8")
        assets = share / "assets"
        for relative in ARCHIVE.REQUIRED_ASSETS:
            path = assets / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(f"fixture {relative}\n".encode())

        libraries = root / "lib/iron-gang"
        libraries.mkdir(parents=True)
        (libraries / "libSDL3.so.0.5.0").write_bytes(b"fixture SDL3\n")
        (libraries / "libSDL3_mixer.so.0.3.0").write_bytes(b"fixture SDL3_mixer\n")
        (libraries / "libSDL3.so.0").symlink_to("libSDL3.so.0.5.0")
        (libraries / "libSDL3_mixer.so.0").symlink_to("libSDL3_mixer.so.0.3.0")

    def test_layout_accepts_runtime_and_rejects_missing_or_development_content(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "package"
            self.create_package(root)
            ARCHIVE.validate_layout(root)

            missing = root / "share/iron-gang/licenses/Jolt-Physics.txt"
            missing.unlink()
            with self.assertRaisesRegex(ARCHIVE.ReleaseArchiveError, "dependency license"):
                ARCHIVE.validate_layout(root)
            missing.write_text("restored\n", encoding="utf-8")

            leaked = root / "include/Jolt/Jolt.h"
            leaked.parent.mkdir(parents=True)
            leaked.write_text("leaked SDK\n", encoding="utf-8")
            with self.assertRaisesRegex(ARCHIVE.ReleaseArchiveError, "development-only content"):
                ARCHIVE.validate_layout(root)

    def test_archive_is_deterministic_and_round_trips_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            package = temporary / "package"
            self.create_package(package)
            first = temporary / "first.tar.gz"
            second = temporary / "second.tar.gz"
            top = "iron-gang-0.1.0-linux-x86_64"
            ARCHIVE.create_reproducible_archive(package, first, top, 1_700_000_000)
            ARCHIVE.create_reproducible_archive(package, second, top, 1_700_000_000)
            self.assertEqual(first.read_bytes(), second.read_bytes())

            destination = temporary / "extracted"
            destination.mkdir()
            extracted = ARCHIVE.extract_verified_archive(first, destination, top)
            ARCHIVE.validate_layout(extracted)
            self.assertTrue((extracted / "lib/iron-gang/libSDL3.so.0").is_symlink())
            self.assertEqual(
                os.readlink(extracted / "lib/iron-gang/libSDL3.so.0"),
                "libSDL3.so.0.5.0",
            )

    def test_archive_extraction_rejects_path_escape(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive_path = root / "unsafe.tar.gz"
            with tarfile.open(archive_path, "w:gz") as archive:
                contents = b"escape"
                member = tarfile.TarInfo("iron-gang-0.1.0-linux-x86_64/../../escape")
                member.size = len(contents)
                archive.addfile(member, io.BytesIO(contents))
            with self.assertRaisesRegex(ARCHIVE.ReleaseArchiveError, "unsafe or unexpected"):
                ARCHIVE.extract_verified_archive(
                    archive_path, root / "destination", "iron-gang-0.1.0-linux-x86_64"
                )
            self.assertFalse((root / "escape").exists())

    def test_package_identity_requires_locked_linux_release_policy(self) -> None:
        valid = {
            "CMAKE_BUILD_TYPE": "Release",
            "CNA_GRAPHICS_RENDERER": "OPENGLES3",
            "CNA_ENABLE_VIDEO": "OFF",
            "IRON_GANG_PACKAGE_SYSTEM_NAME": "Linux",
            "CMAKE_PROJECT_VERSION": "0.1.0",
            "IRON_GANG_PACKAGE_SYSTEM_PROCESSOR": "AMD64",
        }
        self.assertEqual(
            ARCHIVE.package_identity(valid), ("0.1.0", "linux", "x86_64")
        )
        cases = (
            ("CMAKE_BUILD_TYPE", "Debug", "CMAKE_BUILD_TYPE=Release"),
            ("CNA_GRAPHICS_RENDERER", "SOFTWARE", "CNA_GRAPHICS_RENDERER=OPENGLES3"),
            ("CNA_ENABLE_VIDEO", "AUTO", "CNA_ENABLE_VIDEO=OFF"),
            ("IRON_GANG_PACKAGE_SYSTEM_NAME", "Windows", "currently supports Linux"),
        )
        for key, value, expected in cases:
            with self.subTest(key=key):
                changed = dict(valid)
                changed[key] = value
                with self.assertRaisesRegex(ARCHIVE.ReleaseArchiveError, expected):
                    ARCHIVE.package_identity(changed)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
