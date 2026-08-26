#!/usr/bin/env python3

from __future__ import annotations

import csv
import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/asset_registry.py")
REAL_PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path(".").resolve()
FIELDS = (
    "schema_version",
    "asset_id",
    "relative_path",
    "sha256",
    "source_url",
    "author",
    "license",
    "download_date",
    "modified",
    "attribution_required",
    "commercial_use_allowed",
    "redistribution_allowed",
    "ai_processing_allowed",
    "license_evidence",
    "license_evidence_sha256",
    "reviewer",
    "approval_status",
    "quality_state",
    "notes",
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(
    asset_id: str,
    relative_path: str,
    asset_sha256: str,
    evidence_path: str,
    evidence_sha256: str,
    *,
    external: bool,
    license_name: str,
) -> dict[str, str]:
    return {
        "schema_version": "1",
        "asset_id": asset_id,
        "relative_path": relative_path,
        "sha256": asset_sha256,
        "source_url": "https://example.test/assets" if external else "",
        "author": "External Author" if external else "Iron Shadows contributors",
        "license": license_name,
        "download_date": "2020-01-02" if external else "",
        "modified": "true" if asset_id == "embedded_font" else "false",
        "attribution_required": "false",
        "commercial_use_allowed": "true",
        "redistribution_allowed": "true",
        "ai_processing_allowed": "true",
        "license_evidence": evidence_path,
        "license_evidence_sha256": evidence_sha256,
        "reviewer": "Test reviewer",
        "approval_status": "approved",
        "quality_state": "shipping",
        "notes": f"Test provenance for {asset_id}",
    }


class AssetRegistryTests(unittest.TestCase):
    def create_project(self, root: Path) -> list[dict[str, str]]:
        for directory in (
            "assets/audio",
            "assets/config",
            "assets/cutscenes",
            "assets/dialogues",
            "assets/districts",
            "assets/missions",
            "assets/models",
            "assets/vehicles",
            "assets/source",
            "assets/licenses/evidence",
            "src/UI",
        ):
            (root / directory).mkdir(parents=True, exist_ok=True)
        (root / "LICENSE").write_text("fixture MIT license\n", encoding="utf-8")
        (root / "assets/config/game.json").write_text('{"fixture":true}\n', encoding="utf-8")
        (root / "assets/audio/external.wav").write_bytes(b"fixture external wave\0")
        (root / "src/UI/BitmapFont.cpp").write_text("// fixture embedded font\n", encoding="utf-8")
        (root / "assets/licenses/evidence/external.md").write_text(
            "# Fixture external evidence\n", encoding="utf-8"
        )
        license_sha = digest(root / "LICENSE")
        external_evidence_sha = digest(root / "assets/licenses/evidence/external.md")
        rows = [
            record(
                "game_config",
                "config/game.json",
                digest(root / "assets/config/game.json"),
                "LICENSE",
                license_sha,
                external=False,
                license_name="MIT",
            ),
            record(
                "external_sound",
                "audio/external.wav",
                digest(root / "assets/audio/external.wav"),
                "assets/licenses/evidence/external.md",
                external_evidence_sha,
                external=True,
                license_name="CC0",
            ),
            record(
                "embedded_font",
                "../src/UI/BitmapFont.cpp",
                digest(root / "src/UI/BitmapFont.cpp"),
                "assets/licenses/evidence/external.md",
                external_evidence_sha,
                external=True,
                license_name="Public Domain",
            ),
        ]
        self.write_registry(root, rows)
        return rows

    def write_registry(self, root: Path, rows: list[dict[str, str]]) -> None:
        path = root / "assets/licenses/asset-registry.csv"
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="") as destination:
            writer = csv.DictWriter(destination, fieldnames=FIELDS, lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

    def run_tool(self, root: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--project-root", str(root), *arguments],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_committed_registry_and_notice_are_current(self) -> None:
        result = self.run_tool(
            REAL_PROJECT_ROOT, "--check-notice", "THIRD_PARTY_ASSETS.md"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("21 approved shipping assets", result.stdout)
        self.assertIn("4 external assets", result.stdout)
        self.assertIn("notice verified", result.stdout)

    def test_notice_generation_is_deterministic_and_checkable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_project(root)
            result = self.run_tool(root, "--write-notice", "dist/THIRD_PARTY_ASSETS.md")
            self.assertEqual(result.returncode, 0, result.stderr)
            notice = root / "dist/THIRD_PARTY_ASSETS.md"
            first = notice.read_bytes()
            self.assertIn(b"external_sound", first)
            self.assertIn(b"embedded_font", first)
            result = self.run_tool(root, "--write-notice", "dist/THIRD_PARTY_ASSETS.md")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(notice.read_bytes(), first)
            result = self.run_tool(root, "--check-notice", "dist/THIRD_PARTY_ASSETS.md")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(list(notice.parent.glob(".THIRD_PARTY_ASSETS.md.*.tmp")), [])

    def test_unregistered_production_asset_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_project(root)
            (root / "assets/audio/unreviewed.wav").write_bytes(b"unreviewed")
            result = self.run_tool(root)
            self.assertEqual(result.returncode, 2)
            self.assertIn("unregistered production assets", result.stderr)
            self.assertIn("assets/audio/unreviewed.wav", result.stderr)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_project(root)
            (root / "assets/audio/linked.wav").symlink_to("external.wav")
            result = self.run_tool(root)
            self.assertEqual(result.returncode, 2)
            self.assertIn("production asset must not be a symlink", result.stderr)

    def test_asset_and_license_evidence_hashes_are_enforced(self) -> None:
        for target, expected in (
            ("assets/audio/external.wav", "asset SHA-256 mismatch"),
            ("assets/licenses/evidence/external.md", "license-evidence SHA-256 mismatch"),
        ):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.create_project(root)
                (root / target).write_bytes(b"tampered")
                result = self.run_tool(root)
                self.assertEqual(result.returncode, 2)
                self.assertIn(expected, result.stderr)

    def test_shipping_policy_rejects_unknown_or_unapproved_assets(self) -> None:
        cases = (
            ("license", "Unknown", "not in the shipping allow-list"),
            ("commercial_use_allowed", "false", "commercial use and redistribution"),
            ("redistribution_allowed", "false", "commercial use and redistribution"),
            ("approval_status", "pending", "approval_status=approved"),
            ("quality_state", "draft", "quality_state=shipping"),
            ("source_url", "http://example.test/asset", "absolute HTTPS URL"),
        )
        for field, value, expected in cases:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                rows = self.create_project(root)
                rows[1][field] = value
                self.write_registry(root, rows)
                result = self.run_tool(root)
                self.assertEqual(result.returncode, 2)
                self.assertIn(expected, result.stderr)

    def test_duplicate_ids_and_case_collisions_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rows = self.create_project(root)
            rows.append(dict(rows[0]))
            self.write_registry(root, rows)
            result = self.run_tool(root)
            self.assertEqual(result.returncode, 2)
            self.assertIn("duplicate asset_id", result.stderr)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rows = self.create_project(root)
            upper = root / "assets/config/Game.json"
            upper.write_text('{"fixture":false}\n', encoding="utf-8")
            rows.append(
                record(
                    "case_collision",
                    "config/Game.json",
                    digest(upper),
                    "LICENSE",
                    digest(root / "LICENSE"),
                    external=False,
                    license_name="MIT",
                )
            )
            self.write_registry(root, rows)
            result = self.run_tool(root)
            self.assertEqual(result.returncode, 2)
            self.assertIn("case-colliding registered asset paths", result.stderr)

    def test_stale_notice_and_output_alias_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rows = self.create_project(root)
            result = self.run_tool(root, "--write-notice", "THIRD_PARTY_ASSETS.md")
            self.assertEqual(result.returncode, 0, result.stderr)
            rows[1]["author"] = "Changed Author"
            self.write_registry(root, rows)
            result = self.run_tool(root, "--check-notice", "THIRD_PARTY_ASSETS.md")
            self.assertEqual(result.returncode, 2)
            self.assertIn("generated notice is stale", result.stderr)

            registry = root / "assets/licenses/asset-registry.csv"
            before = registry.read_bytes()
            result = self.run_tool(
                root, "--write-notice", "assets/licenses/asset-registry.csv"
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("must not overwrite registry evidence", result.stderr)
            self.assertEqual(registry.read_bytes(), before)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
