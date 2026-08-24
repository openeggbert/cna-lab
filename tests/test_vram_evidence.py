#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_performance_report import capture_fixture


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/vram_evidence.py")
REPORT_SCRIPT = (
    Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path("scripts/performance_report.py")
)


def raw_capture_fixture() -> dict:
    capture = capture_fixture()
    capture["capture_session"] = {
        "process": {"executable": "iron_gang", "pid_known": True, "pid": 4242},
        "started_utc": "2026-08-24T10:00:05Z",
        "ended_utc": "2026-08-24T10:00:55Z",
    }
    capture["video_memory"] = {
        "tracked_bytes": 64 * 1024 * 1024,
        "game_owned_bytes": 63 * 1024 * 1024,
        "imported_model_buffer_bytes": 1024 * 1024,
        "imported_model_texture_bytes": 0,
        "tracking_complete": False,
        "tracked_budget_pass": True,
        "coverage": "logical test resources only",
    }
    return capture


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evidence_fixture(
    capture_path: Path,
    artifact_path: Path,
    peak_bytes: int = 96 * 1024 * 1024,
) -> dict:
    return {
        "schema_version": 1,
        "measurement_scope": "complete_process_gpu_residency_peak",
        "hardware_identity": "Evidence GPU",
        "tool": {"name": "Vendor profiler", "version": "2.1"},
        "process": {"executable": "iron_gang", "pid": 4242},
        "measurement": {
            "peak_resident_bytes": peak_bytes,
            "started_utc": "2026-08-24T10:00:00Z",
            "ended_utc": "2026-08-24T10:01:00Z",
        },
        "profile_capture_sha256": sha256(capture_path),
        "source_artifact": {"file_name": artifact_path.name, "sha256": sha256(artifact_path)},
    }


class VramEvidenceTests(unittest.TestCase):
    def write_inputs(self, directory: Path) -> tuple[Path, Path, Path, Path]:
        capture_path = directory / "capture.json"
        evidence_path = directory / "evidence.json"
        artifact_path = directory / "vendor-capture.bin"
        output_path = directory / "enriched.json"
        capture_path.write_text(json.dumps(raw_capture_fixture()), encoding="utf-8")
        artifact_path.write_bytes(b"raw vendor profiler capture fixture")
        evidence_path.write_text(
            json.dumps(evidence_fixture(capture_path, artifact_path)), encoding="utf-8"
        )
        return capture_path, evidence_path, artifact_path, output_path

    def run_binding(
        self,
        capture_path: Path,
        evidence_path: Path,
        artifact_path: Path,
        output_path: Path,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--capture",
                str(capture_path),
                "--evidence",
                str(evidence_path),
                "--artifact",
                str(artifact_path),
                "--output",
                str(output_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def run_verification(
        self,
        capture_path: Path,
        evidence_path: Path,
        artifact_path: Path,
        enriched_path: Path,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--capture",
                str(capture_path),
                "--evidence",
                str(evidence_path),
                "--artifact",
                str(artifact_path),
                "--verify-enriched",
                str(enriched_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_valid_evidence_binds_and_qualifies_through_release_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(directory)
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 0, result.stderr)

            enriched = json.loads(output_path.read_text(encoding="utf-8"))
            video = enriched["video_memory"]
            self.assertTrue(video["tracking_complete"])
            self.assertEqual(video["logical_tracked_bytes"], 64 * 1024 * 1024)
            self.assertEqual(video["tracked_bytes"], 96 * 1024 * 1024)
            evidence = video["complete_evidence"]
            self.assertEqual(evidence["profile_capture_sha256"], sha256(capture_path))
            self.assertEqual(evidence["evidence_manifest_sha256"], sha256(evidence_path))
            self.assertEqual(evidence["source_artifact"]["sha256"], sha256(artifact_path))

            verification = self.run_verification(
                capture_path, evidence_path, artifact_path, output_path
            )
            self.assertEqual(verification.returncode, 0, verification.stderr)
            self.assertIn("VRAM EVIDENCE VERIFIED", verification.stdout)

            second_capture_path = directory / "capture-second.json"
            second_evidence_path = directory / "evidence-second.json"
            second_artifact_path = directory / "vendor-capture-second.bin"
            second_path = directory / "enriched-second.json"
            second_capture = raw_capture_fixture()
            second_capture["measurements"]["frame_interval"]["p95_ms"] = 17.0
            second_capture_path.write_text(json.dumps(second_capture), encoding="utf-8")
            second_artifact_path.write_bytes(b"second raw vendor profiler capture fixture")
            second_evidence_path.write_text(
                json.dumps(evidence_fixture(second_capture_path, second_artifact_path)),
                encoding="utf-8",
            )
            second_binding = self.run_binding(
                second_capture_path,
                second_evidence_path,
                second_artifact_path,
                second_path,
            )
            self.assertEqual(second_binding.returncode, 0, second_binding.stderr)
            report = subprocess.run(
                [
                    sys.executable,
                    str(REPORT_SCRIPT),
                    "--hardware",
                    "Evidence GPU",
                    "--qualifying-hardware",
                    "--vram-bundle",
                    str(capture_path),
                    str(evidence_path),
                    str(artifact_path),
                    "--vram-bundle",
                    str(second_capture_path),
                    str(second_evidence_path),
                    str(second_artifact_path),
                    str(output_path),
                    str(second_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(report.returncode, 0, report.stderr)
            self.assertIn("Overall status: **PASS**", report.stdout)

    def test_verification_refuses_semantically_tampered_enriched_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(
                Path(temporary)
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 0, result.stderr)
            enriched = json.loads(output_path.read_text(encoding="utf-8"))
            enriched["measurements"]["render_cpu"]["p95_ms"] = 7.5
            output_path.write_text(json.dumps(enriched), encoding="utf-8")

            verification = self.run_verification(
                capture_path, evidence_path, artifact_path, output_path
            )
            self.assertEqual(verification.returncode, 2)
            self.assertIn("does not exactly match", verification.stderr)

    def test_capture_hash_mismatch_is_refused_without_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(Path(temporary))
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            evidence["profile_capture_sha256"] = "0" * 64
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("does not match the input capture", result.stderr)
            self.assertFalse(output_path.exists())

    def test_wrong_scope_and_backwards_time_are_refused(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(Path(temporary))
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            evidence["measurement_scope"] = "global_free_vram"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("measurement_scope must be complete_process_gpu_residency_peak", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            evidence["measurement"]["ended_utc"] = "2026-08-24T09:59:00Z"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("must follow started_utc", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            evidence["measurement"]["ended_utc"] = evidence["measurement"]["started_utc"]
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("must follow started_utc", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            evidence["process"]["executable"] = "unrelated_process"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("must identify iron_gang", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            evidence["process"]["pid"] = 4243
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("PID does not match capture_session", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            evidence["measurement"]["started_utc"] = "2026-08-24T10:00:10Z"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("interval must enclose", result.stderr)

            capture = raw_capture_fixture()
            capture.pop("capture_session")
            capture_path.write_text(json.dumps(capture), encoding="utf-8")
            evidence_path.write_text(
                json.dumps(evidence_fixture(capture_path, artifact_path)), encoding="utf-8"
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("requires capture_session metadata", result.stderr)

            evidence = evidence_fixture(capture_path, artifact_path)
            serialized = json.dumps(evidence)
            evidence_path.write_text(
                serialized[:-1] + ', "schema_version": 1}', encoding="utf-8"
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("duplicate JSON object key 'schema_version'", result.stderr)

    def test_mutated_raw_artifact_is_refused(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(
                Path(temporary)
            )
            artifact_path.write_bytes(b"mutated vendor profiler capture fixture")
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("source_artifact.sha256 does not match", result.stderr)
            self.assertFalse(output_path.exists())

            artifact_path.write_bytes(b"")
            evidence_path.write_text(
                json.dumps(evidence_fixture(capture_path, artifact_path)), encoding="utf-8"
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("raw evidence artifact must be a non-empty regular file", result.stderr)
            self.assertFalse(output_path.exists())

            artifact_path.unlink()
            artifact_path.hardlink_to(capture_path)
            evidence_path.write_text(
                json.dumps(evidence_fixture(capture_path, artifact_path)), encoding="utf-8"
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("original capture and raw evidence artifact must be distinct", result.stderr)
            self.assertFalse(output_path.exists())

    def test_logical_total_is_conservative_floor_and_input_is_never_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            capture_path, evidence_path, artifact_path, output_path = self.write_inputs(directory)
            evidence_path.write_text(
                json.dumps(
                    evidence_fixture(capture_path, artifact_path, peak_bytes=32 * 1024 * 1024)
                ),
                encoding="utf-8",
            )
            result = self.run_binding(capture_path, evidence_path, artifact_path, output_path)
            self.assertEqual(result.returncode, 0, result.stderr)
            enriched = json.loads(output_path.read_text(encoding="utf-8"))
            self.assertEqual(enriched["video_memory"]["tracked_bytes"], 64 * 1024 * 1024)

            result = self.run_binding(capture_path, evidence_path, artifact_path, capture_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ", result.stderr)

            result = self.run_binding(capture_path, evidence_path, artifact_path, evidence_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from the evidence manifest", result.stderr)

            artifact_before = artifact_path.read_bytes()
            result = self.run_binding(capture_path, evidence_path, artifact_path, artifact_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from the raw evidence artifact", result.stderr)
            self.assertEqual(artifact_path.read_bytes(), artifact_before)

            hardlink_output = directory / "capture-output-hardlink.json"
            capture_before = capture_path.read_bytes()
            hardlink_output.hardlink_to(capture_path)
            result = self.run_binding(
                capture_path,
                evidence_path,
                artifact_path,
                hardlink_output,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("output must differ from the original capture", result.stderr)
            self.assertEqual(capture_path.read_bytes(), capture_before)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
