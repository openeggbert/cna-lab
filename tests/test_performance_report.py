#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/performance_report.py")


def capture_fixture() -> dict:
    measurement = lambda samples, p95: {
        "samples": samples,
        "average_ms": p95,
        "p95_ms": p95,
        "maximum_ms": p95,
    }
    return {
        "schema_version": 8,
        "backend": "OPENGLES3",
        "build_configuration": "Release",
        "scenario": "mixed",
        "resolution": {"width": 1280, "height": 720},
        "swap_interval": {
            "apply_result_known": True,
            "apply_succeeded": True,
            "applied": 1,
        },
        "measurements": {
            "frame_interval": measurement(4, 16.8),
            "update_cpu": measurement(4, 0.3),
            "physics_cpu": measurement(4, 0.2),
            "ai_cpu": measurement(4, 0.01),
            "audio_cpu": measurement(4, 0.02),
            "render_cpu": measurement(4, 1.0),
            "present_cpu": measurement(4, 2.0),
            "gpu_render": measurement(4, 4.0),
            "district_load_cpu": measurement(1, 0.3),
        },
        "frame_pacing": {
            "samples": 4,
            "histogram": {
                "at_or_below_recommended_budget": {"count": 1},
                "above_recommended_at_or_below_minimum_budget": {"count": 3},
                "above_minimum_at_or_below_hitch": {"count": 0},
                "above_hitch_at_or_below_severe_hitch": {"count": 0},
                "above_severe_hitch": {"count": 0},
            },
            "hitches": {"count": 0},
            "severe_hitches": {"count": 0},
            "district_transition_boundaries": {"maximum_ms": 17.0},
        },
        "memory": {
            "peak_resident_bytes": 128 * 1024 * 1024,
            "known": True,
        },
        "video_memory": {
            "tracked_bytes": 64 * 1024 * 1024,
            "logical_tracked_bytes": 64 * 1024 * 1024,
            "tracking_complete": True,
            "complete_evidence": {
                "schema_version": 1,
                "source": "external_capture",
                "measurement_scope": "complete_process_gpu_residency_peak",
                "hardware_identity": "Minimum Linux EasyGL GPU",
                "tool": {"name": "Test VRAM tool", "version": "1.0"},
                "process": {"executable": "iron_gang", "pid": 123},
                "measurement": {
                    "peak_resident_bytes": 64 * 1024 * 1024,
                    "started_utc": "2026-08-24T10:00:00Z",
                    "ended_utc": "2026-08-24T10:01:00Z",
                },
                "profile_capture_sha256": "a" * 64,
                "source_artifact": {"file_name": "capture.bin", "sha256": "b" * 64},
                "evidence_manifest_sha256": "c" * 64,
            },
        },
    }


class PerformanceReportTests(unittest.TestCase):
    def run_report(self, captures: list[dict], *arguments: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            paths: list[str] = []
            for index, capture in enumerate(captures):
                path = Path(directory) / f"capture-{index}.json"
                path.write_text(json.dumps(capture), encoding="utf-8")
                paths.append(str(path))
            return subprocess.run(
                [sys.executable, str(SCRIPT), "--hardware", arguments[0], *arguments[1:], *paths],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_unqualified_virtual_capture_stays_diagnostic(self) -> None:
        result = self.run_report([capture_fixture()], "Xvfb llvmpipe diagnostic")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **DIAGNOSTIC**", result.stdout)
        self.assertIn("--qualifying-hardware was not supplied", result.stdout)
        self.assertIn("diagnostic software/virtual display", result.stdout)
        self.assertIn("at least two mixed captures", result.stdout)

    def test_two_complete_physical_captures_pass(self) -> None:
        first = capture_fixture()
        second = deepcopy(first)
        second["measurements"]["frame_interval"]["p95_ms"] = 17.1
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **PASS**", result.stdout)
        self.assertIn("- None.", result.stdout)
        self.assertEqual(result.stdout.count("| PASS |"), 2)

    def test_copied_capture_does_not_meet_repeatability_requirement(self) -> None:
        first = capture_fixture()
        second = deepcopy(first)
        second["video_memory"]["complete_evidence"]["evidence_manifest_sha256"] = "d" * 64
        second["video_memory"]["complete_evidence"]["source_artifact"]["sha256"] = "e" * 64
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("mixed captures with distinct canonical contents", result.stdout)

    def test_incomplete_capture_fails_qualification(self) -> None:
        first = capture_fixture()
        second = deepcopy(first)
        first["swap_interval"]["apply_succeeded"] = False
        first["swap_interval"]["applied"] = None
        second["video_memory"]["tracking_complete"] = False
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("lacks a successful platform acknowledgement", result.stdout)
        self.assertIn("VRAM tracking is incomplete", result.stdout)

    def test_complete_evidence_must_match_hardware_label(self) -> None:
        first = capture_fixture()
        second = deepcopy(first)
        result = self.run_report(
            [first, second],
            "Different physical GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("external VRAM evidence hardware identity does not match", result.stdout)

        first["video_memory"]["complete_evidence"]["process"]["executable"] = "other_game"
        second = deepcopy(first)
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("must identify iron_gang", result.stderr)

    def test_schema_and_histogram_mismatch_is_rejected(self) -> None:
        bad_schema = capture_fixture()
        bad_schema["schema_version"] = 7
        result = self.run_report([bad_schema], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("schema_version must be 8", result.stderr)

        bad_histogram = capture_fixture()
        bad_histogram["frame_pacing"]["histogram"]["at_or_below_recommended_budget"]["count"] = 2
        result = self.run_report([bad_histogram], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("do not match frame_interval samples", result.stderr)

        with tempfile.TemporaryDirectory() as directory:
            duplicate_path = Path(directory) / "duplicate.json"
            serialized = json.dumps(capture_fixture())
            duplicate_path.write_text(serialized[:-1] + ', "schema_version": 8}', encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Test hardware",
                    str(duplicate_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("duplicate JSON object key 'schema_version'", result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
