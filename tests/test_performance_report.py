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
LOGICAL_VRAM_COVERAGE = (
    "Iron Gang-owned meshes, lightmaps, and HUD/map textures plus imported CNA model buffers and "
    "effect-bound textures; backend effect programs, swapchain/depth/render-target/transient "
    "allocations, driver padding, and physical residency are not reported"
)
COMPLETE_VRAM_COVERAGE = (
    "complete external peak process GPU residency bound to this profile capture; tracked_bytes is "
    "the conservative maximum of external residency and Iron Gang's logical resource total"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def workload_summary(samples: int, value: float) -> dict:
    return {"samples": samples, "average": value, "p95": value, "maximum": value}


def workload_fixtures(samples: int) -> dict:
    def summaries(values: dict[str, float]) -> dict:
        return {key: workload_summary(samples, value) for key, value in values.items()}

    return {
        "render_workload": {
            "scope": (
                "Iron Gang 3D front-end submissions; excludes Clear, HUD SpriteBatch internal "
                "batching, Present, and backend state deduplication"
            ),
            "visibility_policy": (
                "no frustum or occlusion culling; every submitted scene object counts as visible"
            ),
            **summaries(
                {
                    "draw_calls": 18,
                    "state_change_calls": 56,
                    "vertices": 1768,
                    "triangles": 948,
                    "geometry_instances": 16,
                    "visible_objects": 67,
                }
            ),
        },
        "physics_workload": {
            "scope": (
                "per game Update; body/contact fields are current state and step/query fields are "
                "operations consumed since the previous sample"
            ),
            "contact_scope": (
                "Jolt rigid-body/subshape contact manifolds plus actual CharacterVirtual contacts; "
                "contact points within a manifold are not counted separately"
            ),
            "query_scope": (
                "public PhysicsWorld raycasts, actual vehicle suspension raycasts, and "
                "CharacterVirtual collision-update batches are separate because their "
                "granularities differ"
            ),
            **summaries(
                {
                    "bodies": 9,
                    "active_rigid_bodies": 1,
                    "rigid_body_contact_manifolds": 0,
                    "character_contacts": 1,
                    "fixed_steps": 1,
                    "public_raycasts": 0,
                    "character_collision_updates": 1,
                    "vehicle_wheel_raycasts": 4,
                }
            ),
        },
        "ai_workload": {
            "scope": (
                "per game Update; state counts are current after the Update (including "
                "AI-suspended transition frames) and operation counts are exact loop work for "
                "that update"
            ),
            "cpu_scope": (
                "ai_cpu covers traffic, pedestrian, witness, and police updates; mission state "
                "progression is excluded"
            ),
            "route_scope": (
                "traffic and pedestrians follow fixed WaypointPaths; no road graph or "
                "path-request queue exists yet"
            ),
            **summaries(
                {
                    "traffic_vehicles": 2,
                    "pedestrians": 2,
                    "fleeing_pedestrians": 0,
                    "police_patrols": 0,
                    "traffic_updates": 2,
                    "traffic_obstacle_checks": 2,
                    "pedestrian_updates": 2,
                    "pedestrian_threat_checks": 0,
                    "police_witness_checks": 0,
                    "police_patrol_updates": 0,
                }
            ),
        },
        "audio_workload": {
            "scope": (
                "per game Update; exact Iron Gang-owned SoundEffect assets, tracked loop state, "
                "and playback/control commands"
            ),
            "voice_scope": (
                "tracked_playing_loop_voices covers only retained SoundEffectInstances; CNA "
                "exposes no lifetime query for fire-and-forget SoundEffect::Play voices"
            ),
            "backend_scope": (
                "decoder time, mixer callback time, active backend channels, and bus cost are "
                "unavailable through CNA and are not reported as zero"
            ),
            **summaries(
                {
                    "loaded_sound_assets": 3,
                    "tracked_loop_instances": 1,
                    "tracked_playing_loop_voices": 0,
                    "streamed_audio_assets": 0,
                    "one_shot_play_requests": 0,
                    "one_shot_play_successes": 0,
                    "loop_play_commands": 0,
                    "loop_stop_commands": 0,
                    "loop_parameter_updates": 0,
                }
            ),
        },
    }


def capture_fixture() -> dict:
    def measurement(samples: int, p95: float, maximum: float | None = None) -> dict:
        return {
            "samples": samples,
            "average_ms": p95,
            "p95_ms": p95,
            "maximum_ms": p95 if maximum is None else maximum,
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
            "proof": (
                "platform SetSwapInterval acknowledgement; not physical vblank or compositor proof"
            ),
            "unavailable_reason": "",
        },
        "gpu_timing": {
            "supported": True,
            "non_blocking": True,
            "scope": "draw_commands_excluding_present",
            "discarded_samples": 1,
            "unsupported_reason": "",
        },
        "measurements": {
            "frame_interval": measurement(4, 16.8, 20.0),
            "update_cpu": measurement(4, 0.3),
            "physics_cpu": measurement(4, 0.2),
            "ai_cpu": measurement(4, 0.01),
            "audio_cpu": measurement(4, 0.02),
            "render_cpu": measurement(4, 1.0),
            "present_cpu": measurement(4, 2.0),
            "gpu_render": measurement(4, 4.0),
            "district_world_physics_cpu": measurement(1, 0.1),
            "district_renderer_upload_cpu": measurement(1, 0.2),
            "district_load_cpu": measurement(1, 0.3),
            "startup_cpu": measurement(1, 10.0),
        },
        "frame_pacing": {
            "scope": (
                "wall-clock intervals between consecutive BeginFrame calls; the first frame "
                "establishes a baseline and has no sample"
            ),
            "boundary_scope": (
                "a district-transition boundary is the first frame-interval sample recorded "
                "after RecordDistrictLoad"
            ),
            "samples": 4,
            "histogram": {
                "at_or_below_recommended_budget": {"upper_bound_ms": 16.667, "count": 1},
                "above_recommended_at_or_below_minimum_budget": {
                    "lower_bound_exclusive_ms": 16.667,
                    "upper_bound_ms": 33.333,
                    "count": 3,
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
        "district_load": {
            "content_path": (
                "procedural in-memory PrototypeWorld; no district file/package is read during a "
                "transition"
            ),
            "unload_activation_scope": (
                "destroy old static physics bodies, construct target world, and build target "
                "static physics bodies; exit-trigger samples also include player/vehicle arrival "
                "placement"
            ),
            "renderer_upload_scope": (
                "CPU time to rebuild target static geometry/lightmap and issue resource uploads; "
                "not GPU-completion time"
            ),
            "io_ms": None,
            "decompression_ms": None,
            "parse_ms": None,
            "unavailable_reason": (
                "districts have no serialized runtime package yet; null means not applicable, not "
                "measured zero"
            ),
            "samples": [
                {
                    "reason": "exit_transition",
                    "source": "warehouse_block",
                    "target": "countryside",
                    "world_physics_ms": 0.1,
                    "renderer_upload_ms": 0.2,
                    "total_ms": 0.3,
                    "asset_counts": {
                        "district_files": 0,
                        "procedural_world_objects": 17,
                        "static_physics_bodies": 9,
                    },
                    "memory": {
                        "resident_known": True,
                        "resident_before_bytes": 1000,
                        "resident_after_bytes": 900,
                        "resident_delta_bytes": -100,
                        "tracked_video_memory_before_bytes": 200,
                        "tracked_video_memory_after_bytes": 250,
                        "tracked_video_memory_delta_bytes": 50,
                    },
                }
            ],
        },
        **workload_fixtures(4),
        "memory": {
            "peak_resident_bytes": 128 * 1024 * 1024,
            "known": True,
            "budget_pass": True,
        },
        "video_memory": {
            "tracked_bytes": 64 * 1024 * 1024,
            "game_owned_bytes": 64 * 1024 * 1024,
            "imported_model_buffer_bytes": 0,
            "imported_model_texture_bytes": 0,
            "logical_tracked_bytes": 64 * 1024 * 1024,
            "tracking_complete": True,
            "tracked_budget_pass": True,
            "coverage": COMPLETE_VRAM_COVERAGE,
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
        "workload": {
            "physics_bodies": 9,
            "traffic_vehicles": 2,
            "pedestrians": 2,
            "police_vehicles": 0,
        },
        "checks": {
            "minimum_frame_rate_pass": True,
            "recommended_frame_rate_pass": False,
            "cpu_subsystems_pass": True,
            "district_load_pass": True,
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


def independent_capture_fixture() -> dict:
    capture = capture_fixture()
    capture["capture_session"] = {
        "process": {"executable": "iron_gang", "pid_known": True, "pid": 124},
        "started_utc": "2026-08-24T11:00:05Z",
        "ended_utc": "2026-08-24T11:00:55Z",
    }
    capture["video_memory"]["complete_evidence"]["process"]["pid"] = 124
    capture["video_memory"]["complete_evidence"]["measurement"] = {
        "peak_resident_bytes": 64 * 1024 * 1024,
        "started_utc": "2026-08-24T11:00:00Z",
        "ended_utc": "2026-08-24T11:01:00Z",
    }
    capture["measurements"]["frame_interval"]["p95_ms"] = 17.1
    capture["measurements"]["frame_interval"]["maximum_ms"] = 17.1
    return capture


class PerformanceReportTests(unittest.TestCase):
    def bind_capture(
        self,
        directory: Path,
        index: int,
        capture: dict,
        shared_artifact_path: Path | None = None,
    ) -> tuple[Path, tuple[Path, Path, Path]]:
        original_path = directory / f"original-{index}.json"
        evidence_path = directory / f"evidence-{index}.json"
        artifact_path = shared_artifact_path or directory / f"artifact-{index}.bin"
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
        video["coverage"] = LOGICAL_VRAM_COVERAGE
        original_path.write_text(json.dumps(original), encoding="utf-8")
        if shared_artifact_path is None:
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

        result = self.run_report([capture_fixture()], "   ")
        self.assertEqual(result.returncode, 2)
        self.assertIn("hardware identity must be non-empty", result.stderr)

        result = self.run_report(
            [capture_fixture()],
            "Test hardware",
            "--title",
            "Release report\n# Injected heading",
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("report title must be a single printable line", result.stderr)

    def test_two_complete_physical_captures_pass(self) -> None:
        first = capture_fixture()
        second = independent_capture_fixture()
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

        overlapping = deepcopy(first)
        overlapping["measurements"]["frame_interval"]["p95_ms"] = 17.1
        overlapping["measurements"]["frame_interval"]["maximum_ms"] = 17.1
        result = self.run_report(
            [first, overlapping],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("capture sessions overlap", result.stdout)

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

        different_resolution = deepcopy(second)
        different_resolution["resolution"]["width"] = 1920
        result = self.run_report(
            [first, different_resolution],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("repeatability policy does not match", result.stdout)
        self.assertIn("resolution.width", result.stdout)

        different_tool = deepcopy(second)
        different_tool["video_memory"]["complete_evidence"]["tool"]["version"] = "2.0"
        result = self.run_report(
            [first, different_tool],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("complete\\_evidence.tool.version", result.stdout)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first_path, first_bundle = self.bind_capture(root, 0, first)
            second_path, second_bundle = self.bind_capture(
                root,
                1,
                second,
                shared_artifact_path=first_bundle[2],
            )
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
                    str(first_path),
                    str(second_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("must not share source files or hardlinks", result.stderr)

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
        second = independent_capture_fixture()
        first["swap_interval"]["apply_succeeded"] = False
        first["swap_interval"]["applied"] = None
        first["swap_interval"]["unavailable_reason"] = (
            "the platform declined the requested swap interval"
        )
        result = self.run_report(
            [first, second],
            "Minimum Linux EasyGL GPU",
            "--qualifying-hardware",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **FAIL**", result.stdout)
        self.assertIn("lacks a successful platform acknowledgement", result.stdout)

        incomplete = capture_fixture()
        incomplete_video = incomplete["video_memory"]
        incomplete_video["tracked_bytes"] = incomplete_video.pop("logical_tracked_bytes")
        incomplete_video["tracking_complete"] = False
        incomplete_video["coverage"] = LOGICAL_VRAM_COVERAGE
        incomplete_video.pop("complete_evidence")
        result = self.run_report([incomplete], "Test diagnostic hardware")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Overall status: **DIAGNOSTIC**", result.stdout)
        self.assertIn("VRAM tracking is incomplete", result.stdout)

    def test_complete_evidence_must_match_hardware_label(self) -> None:
        first = capture_fixture()
        second = independent_capture_fixture()
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

        multiline_session_executable = capture_fixture()
        multiline_session_executable["capture_session"]["process"]["executable"] = (
            "spoofed\n/iron_gang"
        )
        result = self.run_report([multiline_session_executable], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn(
            "capture_session.process.executable must be a single printable line",
            result.stderr,
        )

    def test_schema_and_histogram_mismatch_is_rejected(self) -> None:
        bad_schema = capture_fixture()
        bad_schema["schema_version"] = 7
        result = self.run_report([bad_schema], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("schema_version must be integer 8", result.stderr)

        floating_schema = capture_fixture()
        floating_schema["schema_version"] = 8.0
        result = self.run_report([floating_schema], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("schema_version must be integer 8, got 8.0", result.stderr)

        missing_startup = capture_fixture()
        missing_startup["measurements"].pop("startup_cpu")
        result = self.run_report([missing_startup], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing measurements.startup_cpu", result.stderr)

        multiline_backend = capture_fixture()
        multiline_backend["backend"] = "OPENGLES3\nspoofed"
        result = self.run_report([multiline_backend], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("backend must be a single printable line", result.stderr)

        blank_build = capture_fixture()
        blank_build["build_configuration"] = "   "
        result = self.run_report([blank_build], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("build_configuration must be a non-empty string", result.stderr)

        multiline_scenario = capture_fixture()
        multiline_scenario["scenario"] = "mixed\nidle"
        result = self.run_report([multiline_scenario], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("scenario must be a single printable line", result.stderr)

        zero_resolution = capture_fixture()
        zero_resolution["resolution"]["width"] = 0
        result = self.run_report([zero_resolution], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("resolution width and height must be positive", result.stderr)

        zero_target_frame = capture_fixture()
        zero_target_frame["timing"]["target_frame_ms"] = 0.0
        result = self.run_report([zero_target_frame], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("timing.target_frame_ms must be positive", result.stderr)

        bad_budget = capture_fixture()
        bad_budget["budgets"]["minimum_frame_p95_ms"] = 40.0
        result = self.run_report([bad_budget], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("budgets.minimum_frame_p95_ms must be 33.333", result.stderr)

        bad_maximum = capture_fixture()
        bad_maximum["measurements"]["render_cpu"]["p95_ms"] = 2.0
        result = self.run_report([bad_maximum], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("p95_ms must not exceed maximum_ms", result.stderr)

        bad_percentile_bucket = capture_fixture()
        bad_percentile_bucket["measurements"]["frame_interval"]["p95_ms"] = 10.0
        result = self.run_report([bad_percentile_bucket], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("histogram bucket containing the nearest-rank p95", result.stderr)

        bad_maximum_bucket = capture_fixture()
        bad_maximum_bucket["measurements"]["frame_interval"]["maximum_ms"] = 60.0
        result = self.run_report([bad_maximum_bucket], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("maximum_ms does not fall in the highest non-empty", result.stderr)

        bad_pacing_scope = capture_fixture()
        bad_pacing_scope["frame_pacing"]["scope"] = "Draw CPU time"
        result = self.run_report([bad_pacing_scope], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("frame_pacing.scope does not match", result.stderr)

        bad_boundary_scope = capture_fixture()
        bad_boundary_scope["frame_pacing"]["boundary_scope"] = "last frame before transition"
        result = self.run_report([bad_boundary_scope], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("frame_pacing.boundary_scope does not match", result.stderr)

        bad_zero_sample_summary = capture_fixture()
        bad_zero_sample_summary["measurements"]["gpu_render"]["samples"] = 0
        result = self.run_report([bad_zero_sample_summary], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("with zero samples must have zero statistics", result.stderr)

        bad_histogram = capture_fixture()
        bad_histogram["frame_pacing"]["histogram"]["at_or_below_recommended_budget"]["count"] = 2
        result = self.run_report([bad_histogram], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("do not match frame_interval samples", result.stderr)

        bad_hitch_count = capture_fixture()
        bad_hitch_count["frame_pacing"]["hitches"]["count"] = 1
        bad_hitch_count["frame_pacing"]["hitches"]["percent"] = 25.0
        result = self.run_report([bad_hitch_count], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("hitches.count does not match", result.stderr)

        bad_hitch_threshold = capture_fixture()
        bad_hitch_threshold["frame_pacing"]["hitches"]["threshold_ms"] = 49.0
        result = self.run_report([bad_hitch_threshold], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("hitches.threshold_ms must be 50.000", result.stderr)

        bad_boundary_count = capture_fixture()
        bad_boundary_count["frame_pacing"]["district_transition_boundaries"]["transitions"] = 0
        result = self.run_report([bad_boundary_count], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("transitions must match district_load_cpu samples", result.stderr)

        bad_boundary_hitches = capture_fixture()
        boundaries = bad_boundary_hitches["frame_pacing"]["district_transition_boundaries"]
        boundaries["hitch_count"] = 1
        boundaries["maximum_ms"] = 50.0
        result = self.run_report([bad_boundary_hitches], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("hitch_count cannot exceed total frame hitches", result.stderr)

        bad_boundary_maximum = capture_fixture()
        bad_boundary_maximum["frame_pacing"]["district_transition_boundaries"][
            "maximum_ms"
        ] = 21.0
        result = self.run_report([bad_boundary_maximum], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("maximum_ms cannot exceed frame_interval.maximum_ms", result.stderr)

        empty_boundary_bucket = capture_fixture()
        pacing = empty_boundary_bucket["frame_pacing"]
        pacing["histogram"]["above_recommended_at_or_below_minimum_budget"]["count"] = 2
        pacing["histogram"]["above_hitch_at_or_below_severe_hitch"]["count"] = 1
        pacing["minimum_budget_misses"]["count"] = 1
        pacing["minimum_budget_misses"]["percent"] = 25.0
        pacing["hitches"]["count"] = 1
        pacing["hitches"]["percent"] = 25.0
        pacing["district_transition_boundaries"]["maximum_ms"] = 40.0
        frame = empty_boundary_bucket["measurements"]["frame_interval"]
        frame["p95_ms"] = 60.0
        frame["maximum_ms"] = 60.0
        empty_boundary_bucket["checks"]["minimum_frame_rate_pass"] = False
        result = self.run_report([empty_boundary_bucket], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("has no matching non-empty", result.stderr)

        rounded_boundary_hitch = capture_fixture()
        pacing = rounded_boundary_hitch["frame_pacing"]
        pacing["histogram"]["above_recommended_at_or_below_minimum_budget"]["count"] = 2
        pacing["histogram"]["above_hitch_at_or_below_severe_hitch"]["count"] = 1
        pacing["minimum_budget_misses"]["count"] = 1
        pacing["minimum_budget_misses"]["percent"] = 25.0
        pacing["hitches"]["count"] = 1
        pacing["hitches"]["percent"] = 25.0
        pacing["district_transition_boundaries"]["hitch_count"] = 1
        pacing["district_transition_boundaries"]["maximum_ms"] = 50.0
        frame = rounded_boundary_hitch["measurements"]["frame_interval"]
        frame["p95_ms"] = 50.0
        frame["maximum_ms"] = 50.0
        rounded_boundary_hitch["checks"]["minimum_frame_rate_pass"] = False
        result = self.run_report([rounded_boundary_hitch], "Test hardware")
        self.assertEqual(result.returncode, 0, result.stderr)

        bad_memory_known = capture_fixture()
        bad_memory_known["memory"]["known"] = False
        result = self.run_report([bad_memory_known], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("memory.known must equal", result.stderr)

        bad_memory_budget = capture_fixture()
        bad_memory_budget["memory"]["budget_pass"] = False
        result = self.run_report([bad_memory_budget], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("memory.budget_pass must match", result.stderr)

        bad_vram_categories = capture_fixture()
        bad_vram_categories["video_memory"]["game_owned_bytes"] -= 1
        result = self.run_report([bad_vram_categories], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("logical_tracked_bytes must equal", result.stderr)

        bad_vram_budget = capture_fixture()
        bad_vram_budget["video_memory"]["tracked_budget_pass"] = False
        result = self.run_report([bad_vram_budget], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("tracked_budget_pass must match", result.stderr)

        bad_complete_vram_coverage = capture_fixture()
        bad_complete_vram_coverage["video_memory"]["coverage"] = (
            "complete physical residency including every driver allocation"
        )
        result = self.run_report([bad_complete_vram_coverage], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("coverage does not match schema-8 complete", result.stderr)

        bad_logical_vram_coverage = capture_fixture()
        logical_video = bad_logical_vram_coverage["video_memory"]
        logical_video["tracked_bytes"] = logical_video.pop("logical_tracked_bytes")
        logical_video["tracking_complete"] = False
        logical_video.pop("complete_evidence")
        logical_video["coverage"] = "all process GPU allocations"
        result = self.run_report([bad_logical_vram_coverage], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("coverage does not match schema-8 logical", result.stderr)

        stale_complete_evidence = capture_fixture()
        stale_video = stale_complete_evidence["video_memory"]
        stale_video["tracked_bytes"] = stale_video.pop("logical_tracked_bytes")
        stale_video["tracking_complete"] = False
        stale_video["coverage"] = LOGICAL_VRAM_COVERAGE
        result = self.run_report([stale_complete_evidence], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("complete_evidence is only permitted", result.stderr)

        stale_logical_total = capture_fixture()
        stale_video = stale_logical_total["video_memory"]
        stale_video["tracking_complete"] = False
        stale_video["coverage"] = LOGICAL_VRAM_COVERAGE
        stale_video.pop("complete_evidence")
        result = self.run_report([stale_logical_total], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("logical_tracked_bytes is only permitted", result.stderr)

        bad_frame_check = capture_fixture()
        bad_frame_check["checks"]["minimum_frame_rate_pass"] = False
        result = self.run_report([bad_frame_check], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("minimum_frame_rate_pass must match", result.stderr)

        bad_recommended_check = capture_fixture()
        bad_recommended_check["checks"]["recommended_frame_rate_pass"] = True
        result = self.run_report([bad_recommended_check], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("recommended_frame_rate_pass must match", result.stderr)

        bad_cpu_check = capture_fixture()
        bad_cpu_check["checks"]["cpu_subsystems_pass"] = False
        result = self.run_report([bad_cpu_check], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("cpu_subsystems_pass must match", result.stderr)

        bad_district_check = capture_fixture()
        bad_district_check["checks"]["district_load_pass"] = None
        result = self.run_report([bad_district_check], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("district_load_pass must be boolean", result.stderr)

        bad_district_total = capture_fixture()
        bad_district_total["district_load"]["samples"][0]["total_ms"] = 0.4
        result = self.run_report([bad_district_total], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("total_ms must equal its two phase durations", result.stderr)

        bad_district_resident_delta = capture_fixture()
        bad_district_resident_delta["district_load"]["samples"][0]["memory"][
            "resident_delta_bytes"
        ] = -99
        result = self.run_report([bad_district_resident_delta], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("resident_delta_bytes is inconsistent", result.stderr)

        bad_district_tracked_delta = capture_fixture()
        bad_district_tracked_delta["district_load"]["samples"][0]["memory"][
            "tracked_video_memory_delta_bytes"
        ] = 49
        result = self.run_report([bad_district_tracked_delta], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("tracked_video_memory_delta_bytes is inconsistent", result.stderr)

        bad_district_resident_known = capture_fixture()
        bad_district_resident_known["district_load"]["samples"][0]["memory"][
            "resident_known"
        ] = False
        result = self.run_report([bad_district_resident_known], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("resident_known must match", result.stderr)

        missing_district_sample = capture_fixture()
        missing_district_sample["district_load"]["samples"] = []
        result = self.run_report([missing_district_sample], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("district_world_physics_cpu.samples must match", result.stderr)

        bad_district_phase_summary = capture_fixture()
        phase_summary = bad_district_phase_summary["measurements"][
            "district_world_physics_cpu"
        ]
        phase_summary["average_ms"] = 0.2
        phase_summary["p95_ms"] = 0.2
        phase_summary["maximum_ms"] = 0.2
        result = self.run_report([bad_district_phase_summary], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("district_world_physics_cpu.average_ms must match", result.stderr)

        rounded_district_total = capture_fixture()
        rounded_district_total["district_load"]["samples"][0]["total_ms"] = 0.301
        result = self.run_report([rounded_district_total], "Test hardware")
        self.assertEqual(result.returncode, 0, result.stderr)

        missing_workload = capture_fixture()
        missing_workload.pop("audio_workload")
        result = self.run_report([missing_workload], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing audio_workload", result.stderr)

        bad_workload_scope = capture_fixture()
        bad_workload_scope["render_workload"]["scope"] = "backend draw calls"
        result = self.run_report([bad_workload_scope], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("render_workload.scope does not match schema-8 scope", result.stderr)

        bad_zero_workload = capture_fixture()
        bad_zero_workload["physics_workload"]["bodies"]["samples"] = 0
        result = self.run_report([bad_zero_workload], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("with zero samples must have zero statistics", result.stderr)

        bad_workload_maximum = capture_fixture()
        bad_workload_maximum["ai_workload"]["traffic_updates"]["maximum"] = 1
        result = self.run_report([bad_workload_maximum], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("average and p95 must not exceed maximum", result.stderr)

        fractional_workload_count = capture_fixture()
        fractional_workload_count["audio_workload"]["loaded_sound_assets"]["p95"] = 2.5
        result = self.run_report([fractional_workload_count], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("p95 must be an integer-valued count", result.stderr)

        bad_one_sample_workload = capture_fixture()
        one_sample = bad_one_sample_workload["render_workload"]["draw_calls"]
        one_sample["samples"] = 1
        one_sample["average"] = 17
        result = self.run_report([bad_one_sample_workload], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("with one sample must have identical statistics", result.stderr)

        blocking_gpu_timing = capture_fixture()
        blocking_gpu_timing["gpu_timing"]["non_blocking"] = False
        result = self.run_report([blocking_gpu_timing], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("gpu_timing.non_blocking must be true", result.stderr)

        bad_gpu_scope = capture_fixture()
        bad_gpu_scope["gpu_timing"]["scope"] = "including_present"
        result = self.run_report([bad_gpu_scope], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("gpu_timing.scope must be draw_commands_excluding_present", result.stderr)

        supported_with_reason = capture_fixture()
        supported_with_reason["gpu_timing"]["unsupported_reason"] = "contradiction"
        result = self.run_report([supported_with_reason], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("must be empty when GPU timing is supported", result.stderr)

        unsupported_without_reason = capture_fixture()
        unsupported_without_reason["gpu_timing"]["supported"] = False
        result = self.run_report([unsupported_without_reason], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("gpu_timing.unsupported_reason must be non-empty", result.stderr)

        rounded_cpu_boundary = capture_fixture()
        rounded_cpu_boundary["measurements"]["render_cpu"]["p95_ms"] = 8.0
        rounded_cpu_boundary["measurements"]["render_cpu"]["maximum_ms"] = 8.0
        rounded_cpu_boundary["checks"]["cpu_subsystems_pass"] = False
        result = self.run_report([rounded_cpu_boundary], "Test hardware")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            "full-precision producer CPU subsystem check does not pass",
            result.stdout,
        )

        rounded_frame_boundary = capture_fixture()
        rounded_frame = rounded_frame_boundary["measurements"]["frame_interval"]
        rounded_frame["p95_ms"] = 33.333
        rounded_frame["maximum_ms"] = 33.333
        rounded_frame_boundary["checks"]["minimum_frame_rate_pass"] = False
        result = self.run_report([rounded_frame_boundary], "Test hardware")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            "full-precision producer check does not pass 33.333 ms",
            result.stdout,
        )
        self.assertIn("| 33.333 ms | no | no |", result.stdout)

        rounded_district_boundary = capture_fixture()
        rounded_district_sample = rounded_district_boundary["district_load"]["samples"][0]
        rounded_district_sample["world_physics_ms"] = 400.0
        rounded_district_sample["renderer_upload_ms"] = 600.0
        rounded_district_sample["total_ms"] = 1000.0
        for metric, value in (
            ("district_world_physics_cpu", 400.0),
            ("district_renderer_upload_cpu", 600.0),
            ("district_load_cpu", 1000.0),
        ):
            summary = rounded_district_boundary["measurements"][metric]
            summary["average_ms"] = value
            summary["p95_ms"] = value
            summary["maximum_ms"] = value
        rounded_district_boundary["checks"]["district_load_pass"] = False
        result = self.run_report([rounded_district_boundary], "Test hardware")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            "mixed capture lacks a passing real district transition",
            result.stdout,
        )

        bad_session_time = capture_fixture()
        bad_session_time["capture_session"]["started_utc"] = (
            "2026-08-24T10:00:05.1234567Z"
        )
        result = self.run_report([bad_session_time], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("must use YYYY-MM-DDTHH:MM:SS[.ffffff]Z", result.stderr)

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

        bad_swap_proof = capture_fixture()
        bad_swap_proof["swap_interval"]["proof"] = "physical vblank proven"
        result = self.run_report([bad_swap_proof], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("swap_interval.proof does not match", result.stderr)

        successful_swap_with_reason = capture_fixture()
        successful_swap_with_reason["swap_interval"]["unavailable_reason"] = "contradiction"
        result = self.run_report([successful_swap_with_reason], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("must be empty after successful apply", result.stderr)

        failed_swap_without_reason = capture_fixture()
        failed_swap_without_reason["swap_interval"]["apply_succeeded"] = False
        failed_swap_without_reason["swap_interval"]["applied"] = None
        result = self.run_report([failed_swap_without_reason], "Test hardware")
        self.assertEqual(result.returncode, 2)
        self.assertIn("swap_interval.unavailable_reason must be non-empty", result.stderr)

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

            non_standard_path = Path(directory) / "non-standard-number.json"
            non_standard = capture_fixture()
            non_standard["ignored_extension"] = float("nan")
            non_standard_path.write_text(json.dumps(non_standard), encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--hardware",
                    "Test hardware",
                    str(non_standard_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("non-standard JSON numeric constant 'NaN'", result.stderr)

    def test_output_is_atomic_and_never_overwrites_evidence_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture_path = root / "diagnostic`<b>|line\nbreak.json"
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
            second = independent_capture_fixture()
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
                    "Test `<b>| diagnostic hardware",
                    "--title",
                    "Report <b>*unsafe*",
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
            report_text = report_path.read_text(encoding="utf-8")
            self.assertIn("Overall status: **DIAGNOSTIC**", report_text)
            self.assertIn("# Report &lt;b&gt;\\*unsafe\\*", report_text)
            self.assertIn(
                "<code>Test `&lt;b&gt;&#124; diagnostic hardware</code>",
                report_text,
            )
            self.assertIn(
                "<code>diagnostic`&lt;b&gt;&#124;line\\u000abreak.json</code>",
                report_text,
            )
            self.assertEqual(list(report_path.parent.glob("release.md.*.tmp")), [])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
