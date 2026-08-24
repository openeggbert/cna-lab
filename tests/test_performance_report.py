#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/performance_report.py")
VRAM_SCRIPT = (
    Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path("scripts/vram_evidence.py")
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture_fixture() -> dict:
    measurement = lambda samples, p95: {
        "samples": samples,
        "average_ms": p95,
        "p95_ms": p95,
        "maximum_ms": p95,
    }
    return {
        "schema_version": 8,
        "capture_session": {
            "process": {"executable": "iron_gang", "pid_known": True, "pid": 123},
            "started_utc": "2026-08-24T10:00:05Z",
            "ended_utc": "2026-08-24T10:00:55Z",
        },
        "backend": "OPENGLES3",
        "build_configuration": "Release",
        "scenario": "mixed",
        "resolution": {"width": 1280, "height": 720},
        "timing": {
            "vertical_sync_requested": True,
            "fixed_timestep": True,
            "target_frame_ms": 16.667,
        },
        "swap_interval": {
            "requested": 1,
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
    def bind_capture(
        self,
        directory: Path,
        index: int,
        capture: dict,
    ) -> tuple[Path, tuple[Path, Path, Path]]:
        original_path = directory / f"original-{index}.json"
        evidence_path = directory / f"evidence-{index}.json"
        artifact_path = directory / f"artifact-{index}.bin"
        enriched_path = directory / f"capture-{index}.json"

        original = deepcopy(capture)
        video = original["video_memory"]
        evidence_template = video.pop("complete_evidence")
        logical_bytes = video.pop("logical_tracked_bytes")
        video["tracked_bytes"] = logical_bytes
        video["game_owned_bytes"] = logical_bytes
        video["imported_model_buffer_bytes"] = 0
        video["imported_model_texture_bytes"] = 0
        video["tracking_complete"] = False
        video["tracked_budget_pass"] = True
        video["coverage"] = "logical report-test resources only"
        original_path.write_text(json.dumps(original), encoding="utf-8")
        artifact_path.write_bytes(f"raw report-test profiler artifact {index}".encode())
        evidence = {
            "schema_version": 1,
            "measurement_scope": evidence_template["measurement_scope"],
            "hardware_identity": evidence_template["hardware_identity"],
            "tool": evidence_template["tool"],
            "process": evidence_template["process"],
            "measurement": evidence_template["measurement"],
            "profile_capture_sha256": sha256(original_path),
            "source_artifact": {
                "file_name": artifact_path.name,
                "sha256": sha256(artifact_path),
            },
        }
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        binding = subprocess.run(
            [
                sys.executable,
                str(VRAM_SCRIPT),
                "--capture",
                str(original_path),
                "--evidence",
                str(evidence_path),
                "--artifact",
                str(artifact_path),
                "--output",
                str(enriched_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(binding.returncode, 0, binding.stderr)
        return enriched_path, (original_path, evidence_path, artifact_path)

    def run_report(
        self,
        captures: list[dict],
        *arguments: str,
        include_bundles: bool = True,
        tamper_first_artifact: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            paths: list[str] = []
            bundle_arguments: list[str] = []
            qualifying = "--qualifying-hardware" in arguments
            for index, capture in enumerate(captures):
                if qualifying and include_bundles and capture["video_memory"]["tracking_complete"]:
                    path, bundle = self.bind_capture(root, index, capture)
                    if index == 0 and tamper_first_artifact:
                        bundle[2].write_bytes(b"tampered report-test profiler artifact")
                    bundle_arguments.extend(
                        ["--vram-bundle", *(str(bundle_path) for bundle_path in bundle)]
                    )
                else:
                    path = root / f"capture-{index}.json"
                    path.write_text(json.dumps(capture), encoding="utf-8")
                paths.append(str(path))
            return subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    arguments[0],
                    *arguments[1:],
                    *bundle_arguments,
                    *paths,
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_unqualified_virtual_capture_stays_diagnostic(self) -> None:
        result = self.run_report([capture_fixture()], "Xvfb llvmpipe diagnostic")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **DIAGNOSTIC**", result.stdout)
        self.assertIn("## Evidence provenance", result.stdout)
        self.assertIn("| — | — | — |", result.stdout)
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
        self.assertIn("## Evidence provenance", result.stdout)
        self.assertIn(
            hashlib.sha256(b"raw report-test profiler artifact 0").hexdigest(),
            result.stdout,
        )

        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
            include_bundles=False,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("require one --vram-bundle", result.stderr)

        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
            tamper_first_artifact=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("VRAM bundle verification failed", result.stderr)
        self.assertIn("source_artifact.sha256 does not match", result.stderr)

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

    def test_swap_failure_and_incomplete_diagnostic_are_reported(self) -> None:
        first = capture_fixture()
        second = deepcopy(first)
        second["measurements"]["frame_interval"]["p95_ms"] = 17.1
        first["swap_interval"]["apply_succeeded"] = False
        first["swap_interval"]["applied"] = None
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("lacks a successful platform acknowledgement", result.stdout)

        incomplete = capture_fixture()
        incomplete["video_memory"]["tracking_complete"] = False
        result = self.run_report([incomplete], "Test diagnostic hardware")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **DIAGNOSTIC**", result.stdout)
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

        mismatched_swap = capture_fixture()
        mismatched_swap["swap_interval"]["applied"] = 0
        result = self.run_report([mismatched_swap], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("applied must equal swap_interval.requested", result.stderr)

        mismatched_vsync = capture_fixture()
        mismatched_vsync["timing"]["vertical_sync_requested"] = False
        result = self.run_report([mismatched_vsync], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("vertical_sync_requested must agree", result.stderr)

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

    def test_output_is_atomic_and_never_overwrites_evidence_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture_path = root / "diagnostic.json"
            capture_path.write_text(json.dumps(capture_fixture()), encoding="utf-8")
            capture_before = capture_path.read_bytes()

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Test diagnostic hardware",
                    "--output",
                    str(capture_path),
                    str(capture_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from every capture input", result.stderr)
            self.assertEqual(capture_path.read_bytes(), capture_before)

            hardlink_path = root / "diagnostic-hardlink.json"
            hardlink_path.hardlink_to(capture_path)
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Test diagnostic hardware",
                    "--output",
                    str(hardlink_path),
                    str(capture_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from every capture input", result.stderr)
            self.assertEqual(capture_path.read_bytes(), capture_before)

            first_path, first_bundle = self.bind_capture(root, 0, capture_fixture())
            second = capture_fixture()
            second["measurements"]["frame_interval"]["p95_ms"] = 17.1
            second_path, second_bundle = self.bind_capture(root, 1, second)
            artifact_before = first_bundle[2].read_bytes()
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Minimum Linux EasyGL GPU",
                    "--qualifying-hardware",
                    "--vram-bundle",
                    *(str(path) for path in first_bundle),
                    "--vram-bundle",
                    *(str(path) for path in second_bundle),
                    "--output",
                    str(first_bundle[2]),
                    str(first_path),
                    str(second_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from every raw evidence artifact input", result.stderr)
            self.assertEqual(first_bundle[2].read_bytes(), artifact_before)

            report_path = root / "reports" / "release.md"
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Test diagnostic hardware",
                    "--output",
                    str(report_path),
                    str(capture_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertIn("Overall status: **DIAGNOSTIC**", report_path.read_text(encoding="utf-8"))
            self.assertEqual(list(report_path.parent.glob("release.md.*.tmp")), [])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
