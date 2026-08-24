#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/performance_compare.py")


def measurement(samples: int, p95: float) -> dict:
    return {"samples": samples, "average_ms": p95, "p95_ms": p95, "maximum_ms": p95}


def capture_fixture() -> dict:
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
        "gpu_timing": {"supported": True, "scope": "draw_commands_excluding_present"},
        "measurements": {
            "frame_interval": measurement(100, 16.0),
            "update_cpu": measurement(100, 0.3),
            "physics_cpu": measurement(100, 0.2),
            "ai_cpu": measurement(100, 0.01),
            "audio_cpu": measurement(100, 0.02),
            "render_cpu": measurement(100, 1.0),
            "present_cpu": measurement(100, 2.0),
            "gpu_render": measurement(99, 4.0),
            "district_load_cpu": measurement(1, 0.3),
        },
        "frame_pacing": {
            "samples": 100,
            "histogram": {
                "at_or_below_recommended_budget": {"upper_bound_ms": 16.667, "count": 80},
                "above_recommended_at_or_below_minimum_budget": {
                    "lower_bound_exclusive_ms": 16.667,
                    "upper_bound_ms": 33.333,
                    "count": 20,
                },
                "above_minimum_at_or_below_hitch": {
                    "lower_bound_exclusive_ms": 33.333,
                    "upper_bound_ms": 50.0,
                    "count": 0,
                },
                "above_hitch_at_or_below_severe_hitch": {
                    "lower_bound_exclusive_ms": 50.0,
                    "upper_bound_ms": 100.0,
                    "count": 0,
                },
                "above_severe_hitch": {"lower_bound_exclusive_ms": 100.0, "count": 0},
            },
            "minimum_budget_misses": {
                "threshold_ms": 33.333,
                "comparison": "greater_than",
                "count": 0,
                "percent": 0.0,
            },
            "hitches": {
                "threshold_ms": 50.0,
                "comparison": "greater_than",
                "count": 0,
                "percent": 0.0,
            },
            "severe_hitches": {
                "threshold_ms": 100.0,
                "comparison": "greater_than",
                "count": 0,
                "percent": 0.0,
            },
            "district_transition_boundaries": {
                "transitions": 1,
                "measured_samples": 1,
                "hitch_count": 0,
                "maximum_ms": 17.0,
            },
        },
        "memory": {"peak_resident_bytes": 128 * 1024 * 1024, "known": True},
        "video_memory": {
            "tracked_bytes": 64 * 1024 * 1024,
            "logical_tracked_bytes": 64 * 1024 * 1024,
            "tracking_complete": True,
            "coverage": "complete test coverage",
            "complete_evidence": {
                "schema_version": 1,
                "source": "external_capture",
                "measurement_scope": "complete_process_gpu_residency_peak",
                "hardware_identity": "Test GPU",
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
        "workload": {
            "physics_bodies": 9,
            "traffic_vehicles": 2,
            "pedestrians": 2,
            "police_vehicles": 0,
        },
        "budgets": {
            "minimum_frame_p95_ms": 33.333,
            "recommended_frame_p95_ms": 16.667,
            "update_cpu_p95_ms": 8.0,
            "physics_cpu_p95_ms": 3.0,
            "ai_cpu_p95_ms": 2.0,
            "audio_cpu_p95_ms": 1.0,
            "render_cpu_p95_ms": 8.0,
            "district_load_p95_ms": 1000.0,
            "ram_bytes": 2 * 1024 * 1024 * 1024,
            "vram_bytes": 512 * 1024 * 1024,
        },
    }


class PerformanceCompareTests(unittest.TestCase):
    def run_compare(
        self,
        baseline: dict,
        candidate: dict,
        *arguments: str,
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            baseline_path = Path(directory) / "baseline.json"
            candidate_path = Path(directory) / "candidate.json"
            baseline_path.write_text(json.dumps(baseline), encoding="utf-8")
            candidate_path.write_text(json.dumps(candidate), encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--baseline",
                    str(baseline_path),
                    "--candidate",
                    str(candidate_path),
                    "--baseline-hardware",
                    "Test GPU",
                    "--candidate-hardware",
                    "Test GPU",
                    "--baseline-kind",
                    "diagnostic",
                    "--candidate-kind",
                    "diagnostic",
                    *arguments,
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_identical_compatible_capture_has_no_regression(self) -> None:
        baseline = capture_fixture()
        result = self.run_compare(baseline, deepcopy(baseline))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall result: **NO REGRESSION**", result.stdout)
        self.assertIn("| Frame interval p95 | 16.000 ms | 16.000 ms | +0.000 ms", result.stdout)
        self.assertIn("| Physics bodies | 9 | 9 |", result.stdout)

    def test_regression_returns_one_and_names_metrics(self) -> None:
        baseline = capture_fixture()
        candidate = deepcopy(baseline)
        candidate["measurements"]["frame_interval"]["p95_ms"] = 20.0
        candidate["measurements"]["update_cpu"]["p95_ms"] = 0.5
        result = self.run_compare(baseline, candidate)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("Overall result: **REGRESSION**", result.stdout)
        self.assertIn("| Frame interval p95 | 16.000 ms | 20.000 ms", result.stdout)
        self.assertIn("| Update CPU p95 | 0.300 ms | 0.500 ms", result.stdout)
        self.assertGreaterEqual(result.stdout.count("| REGRESSION |"), 2)

    def test_explicit_tolerance_can_accept_known_variance(self) -> None:
        baseline = capture_fixture()
        candidate = deepcopy(baseline)
        candidate["measurements"]["frame_interval"]["p95_ms"] = 20.0
        result = self.run_compare(
            baseline,
            candidate,
            "--relative-tolerance-percent",
            "30",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Relative: `30.000%`", result.stdout)

    def test_hardware_and_kind_mismatch_are_refused(self) -> None:
        capture = capture_fixture()
        with tempfile.TemporaryDirectory() as directory:
            baseline_path = Path(directory) / "baseline.json"
            candidate_path = Path(directory) / "candidate.json"
            baseline_path.write_text(json.dumps(capture), encoding="utf-8")
            candidate_path.write_text(json.dumps(capture), encoding="utf-8")
            common = [
                sys.executable,
                str(SCRIPT),
                "--baseline",
                str(baseline_path),
                "--candidate",
                str(candidate_path),
            ]
            result = subprocess.run(
                [
                    *common,
                    "--baseline-hardware",
                    "GPU A",
                    "--candidate-hardware",
                    "GPU B",
                    "--baseline-kind",
                    "diagnostic",
                    "--candidate-kind",
                    "diagnostic",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("cross-hardware comparison is refused", result.stderr)

            result = subprocess.run(
                [
                    *common,
                    "--baseline-hardware",
                    "GPU A",
                    "--candidate-hardware",
                    "GPU A",
                    "--baseline-kind",
                    "diagnostic",
                    "--candidate-kind",
                    "qualifying",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("diagnostic-vs-qualifying comparison is refused", result.stderr)

    def test_changed_budget_and_measurement_availability_are_refused(self) -> None:
        baseline = capture_fixture()
        candidate = deepcopy(baseline)
        candidate["budgets"]["minimum_frame_p95_ms"] = 40.0
        result = self.run_compare(baseline, candidate)
        self.assertEqual(result.returncode, 2)
        self.assertIn("incompatible budgets.minimum_frame_p95_ms", result.stderr)

        candidate = deepcopy(baseline)
        candidate["measurements"]["gpu_render"]["samples"] = 0
        result = self.run_compare(baseline, candidate)
        self.assertEqual(result.returncode, 2)
        self.assertIn("incompatible gpu_render sample availability", result.stderr)

        candidate = deepcopy(baseline)
        candidate["video_memory"]["complete_evidence"]["tool"]["version"] = "2.0"
        result = self.run_compare(baseline, candidate)
        self.assertEqual(result.returncode, 2)
        self.assertIn("incompatible video_memory.complete_evidence.tool.version", result.stderr)

        candidate = deepcopy(baseline)
        candidate["swap_interval"]["applied"] = 0
        result = self.run_compare(baseline, candidate)
        self.assertEqual(result.returncode, 2)
        self.assertIn("applied must equal swap_interval.requested", result.stderr)

    def test_qualifying_mode_rejects_incomplete_evidence(self) -> None:
        baseline = capture_fixture()
        candidate = deepcopy(baseline)
        baseline["video_memory"]["tracking_complete"] = False
        candidate["video_memory"]["tracking_complete"] = False
        result = self.run_compare(
            baseline,
            candidate,
            "--baseline-kind",
            "qualifying",
            "--candidate-kind",
            "qualifying",
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("qualifying capture has incomplete VRAM tracking", result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
