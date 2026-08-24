#!/usr/bin/env python3
"""Generate a concise M12 release summary from Iron Gang schema-8 captures."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path
from typing import Any, TextIO


SCHEMA_VERSION = 8
MINIMUM_FRAME_MS = 1000.0 / 30.0
RECOMMENDED_FRAME_MS = 1000.0 / 60.0
MINIMUM_WIDTH = 1280
MINIMUM_HEIGHT = 720
RECOMMENDED_WIDTH = 1920
RECOMMENDED_HEIGHT = 1080
RAM_BUDGET_BYTES = 2 * 1024 * 1024 * 1024
VRAM_BUDGET_BYTES = 512 * 1024 * 1024
DISTRICT_LOAD_BUDGET_MS = 1000.0
FRAME_HITCH_MS = 50.0
SEVERE_FRAME_HITCH_MS = 100.0
SCHEMA_MINIMUM_FRAME_MS = 33.333
SCHEMA_RECOMMENDED_FRAME_MS = 16.667
CPU_BUDGETS_MS = {
    "update_cpu": 8.0,
    "physics_cpu": 3.0,
    "ai_cpu": 2.0,
    "audio_cpu": 1.0,
    "render_cpu": 8.0,
}
LOCKED_FLOAT_BUDGETS = {
    "minimum_frame_p95_ms": SCHEMA_MINIMUM_FRAME_MS,
    "recommended_frame_p95_ms": SCHEMA_RECOMMENDED_FRAME_MS,
    "update_cpu_p95_ms": CPU_BUDGETS_MS["update_cpu"],
    "physics_cpu_p95_ms": CPU_BUDGETS_MS["physics_cpu"],
    "ai_cpu_p95_ms": CPU_BUDGETS_MS["ai_cpu"],
    "audio_cpu_p95_ms": CPU_BUDGETS_MS["audio_cpu"],
    "render_cpu_p95_ms": CPU_BUDGETS_MS["render_cpu"],
    "district_load_p95_ms": DISTRICT_LOAD_BUDGET_MS,
}
LOCKED_INTEGER_BUDGETS = {
    "ram_bytes": RAM_BUDGET_BYTES,
    "vram_bytes": VRAM_BUDGET_BYTES,
}
QUALIFICATION_REPEATABILITY_PATHS = (
    ("resolution", "width"),
    ("resolution", "height"),
    ("timing", "vertical_sync_requested"),
    ("timing", "fixed_timestep"),
    ("timing", "target_frame_ms"),
    ("swap_interval", "requested"),
    ("gpu_timing", "supported"),
    ("gpu_timing", "scope"),
    ("workload", "physics_bodies"),
    ("workload", "traffic_vehicles"),
    ("workload", "pedestrians"),
    ("workload", "police_vehicles"),
    ("video_memory", "coverage"),
    ("video_memory", "complete_evidence", "source"),
    ("video_memory", "complete_evidence", "measurement_scope"),
    ("video_memory", "complete_evidence", "tool", "name"),
    ("video_memory", "complete_evidence", "tool", "version"),
)
DIAGNOSTIC_HARDWARE_TERMS = ("xvfb", "llvmpipe", "software rasterizer")
COMPLETE_VRAM_SCOPE = "complete_process_gpu_residency_peak"
GPU_TIMING_SCOPE = "draw_commands_excluding_present"
SWAP_INTERVAL_PROOF = (
    "platform SetSwapInterval acknowledgement; not physical vblank or compositor proof"
)
FRAME_PACING_SCOPE = (
    "wall-clock intervals between consecutive BeginFrame calls; the first frame establishes a "
    "baseline and has no sample"
)
FRAME_BOUNDARY_SCOPE = (
    "a district-transition boundary is the first frame-interval sample recorded after "
    "RecordDistrictLoad"
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
DISTRICT_CONTENT_PATH = (
    "procedural in-memory PrototypeWorld; no district file/package is read during a transition"
)
DISTRICT_UNLOAD_SCOPE = (
    "destroy old static physics bodies, construct target world, and build target static physics "
    "bodies; exit-trigger samples also include player/vehicle arrival placement"
)
DISTRICT_RENDERER_SCOPE = (
    "CPU time to rebuild target static geometry/lightmap and issue resource uploads; not "
    "GPU-completion time"
)
DISTRICT_UNAVAILABLE_REASON = (
    "districts have no serialized runtime package yet; null means not applicable, not measured zero"
)
WORKLOAD_SCHEMAS = {
    "render_workload": (
        {
            "scope": (
                "Iron Gang 3D front-end submissions; excludes Clear, HUD SpriteBatch internal "
                "batching, Present, and backend state deduplication"
            ),
            "visibility_policy": (
                "no frustum or occlusion culling; every submitted scene object counts as visible"
            ),
        },
        (
            "draw_calls",
            "state_change_calls",
            "vertices",
            "triangles",
            "geometry_instances",
            "visible_objects",
        ),
    ),
    "physics_workload": (
        {
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
        },
        (
            "bodies",
            "active_rigid_bodies",
            "rigid_body_contact_manifolds",
            "character_contacts",
            "fixed_steps",
            "public_raycasts",
            "character_collision_updates",
            "vehicle_wheel_raycasts",
        ),
    ),
    "ai_workload": (
        {
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
        },
        (
            "traffic_vehicles",
            "pedestrians",
            "fleeing_pedestrians",
            "police_patrols",
            "traffic_updates",
            "traffic_obstacle_checks",
            "pedestrian_updates",
            "pedestrian_threat_checks",
            "police_witness_checks",
            "police_patrol_updates",
        ),
    ),
    "audio_workload": (
        {
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
        },
        (
            "loaded_sound_assets",
            "tracked_loop_instances",
            "tracked_playing_loop_voices",
            "streamed_audio_assets",
            "one_shot_play_requests",
            "one_shot_play_successes",
            "loop_play_commands",
            "loop_stop_commands",
            "loop_parameter_updates",
        ),
    ),
}
IRON_GANG_EXECUTABLES = frozenset(("iron_gang", "iron_gang.exe"))
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
UTC_TIMESTAMP_PATTERN = re.compile(
    r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z"
)
HISTOGRAM_BUCKETS = (
    "at_or_below_recommended_budget",
    "above_recommended_at_or_below_minimum_budget",
    "above_minimum_at_or_below_hitch",
    "above_hitch_at_or_below_severe_hitch",
    "above_severe_hitch",
)
FileFingerprint = tuple[Path, str]
VramBundleFingerprint = tuple[FileFingerprint, FileFingerprint, FileFingerprint]


class ReportError(ValueError):
    pass


def _unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ReportError(f"duplicate JSON object key {key!r}")
        result[key] = value
    return result


def _reject_json_constant(value: str) -> None:
    raise ReportError(f"non-standard JSON numeric constant {value!r} is not allowed")


def _strict_json_load(source: TextIO) -> Any:
    return json.load(
        source,
        object_pairs_hook=_unique_json_object,
        parse_constant=_reject_json_constant,
    )


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _same_file(left: Path, right: Path) -> bool:
    if left.resolve() == right.resolve():
        return True
    try:
        return left.samefile(right)
    except OSError:
        return False


def _require_distinct_bundle_sources(bundles: list[list[Path]], context: str) -> None:
    for bundle_index, bundle in enumerate(bundles):
        for earlier_index, earlier in enumerate(bundles[:bundle_index]):
            if any(_same_file(left, right) for left in bundle for right in earlier):
                raise ReportError(
                    f"{context} VRAM bundles {earlier_index + 1} and "
                    f"{bundle_index + 1} must not share source files or hardlinks"
                )


def _write_text_atomic(path: Path, contents: str) -> None:
    temporary_path: Path | None = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=path.parent,
            prefix=path.name + ".",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary.write(contents)
            temporary_path = Path(temporary.name)
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass


def _require_unchanged(path: Path, expected_sha256: str, label: str) -> None:
    if _file_sha256(path) != expected_sha256:
        raise ReportError(f"{label} changed while the report was being generated")


def _verify_report_inputs_unchanged(
    capture_paths: list[Path],
    capture_sha256s: list[str],
    bundle_fingerprints: list[VramBundleFingerprint | None],
) -> None:
    for capture_path, expected_sha256 in zip(capture_paths, capture_sha256s):
        _require_unchanged(capture_path, expected_sha256, str(capture_path))
    bundle_labels = ("original capture", "evidence manifest", "raw evidence artifact")
    for bundle in bundle_fingerprints:
        if bundle is None:
            continue
        for label, (path, expected_sha256) in zip(bundle_labels, bundle):
            _require_unchanged(path, expected_sha256, label)


def _mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ReportError(f"{label} must be a JSON object")
    return value


def _array(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise ReportError(f"{label} must be a JSON array")
    return value


def _path(value: dict[str, Any], *keys: str) -> Any:
    current: Any = value
    traversed: list[str] = []
    for key in keys:
        traversed.append(key)
        if not isinstance(current, dict) or key not in current:
            raise ReportError(f"missing {'.'.join(traversed)}")
        current = current[key]
    return current


def _number(value: dict[str, Any], *keys: str) -> float:
    result = _path(value, *keys)
    if isinstance(result, bool) or not isinstance(result, (int, float)):
        raise ReportError(f"{'.'.join(keys)} must be numeric")
    converted = float(result)
    if not math.isfinite(converted) or converted < 0.0:
        raise ReportError(f"{'.'.join(keys)} must be finite and non-negative")
    return converted


def _integer(value: dict[str, Any], *keys: str) -> int:
    result = _path(value, *keys)
    if isinstance(result, bool) or not isinstance(result, int) or result < 0:
        raise ReportError(f"{'.'.join(keys)} must be a non-negative integer")
    return result


def _signed_integer(value: dict[str, Any], *keys: str) -> int:
    result = _path(value, *keys)
    if isinstance(result, bool) or not isinstance(result, int):
        raise ReportError(f"{'.'.join(keys)} must be an integer")
    return result


def _boolean(value: dict[str, Any], *keys: str) -> bool:
    result = _path(value, *keys)
    if not isinstance(result, bool):
        raise ReportError(f"{'.'.join(keys)} must be boolean")
    return result


def _non_empty_string(value: dict[str, Any], *keys: str) -> str:
    result = _path(value, *keys)
    if not isinstance(result, str) or not result.strip():
        raise ReportError(f"{'.'.join(keys)} must be a non-empty string")
    return result.strip()


def _single_line_text(text: str, label: str) -> str:
    normalized = text.strip()
    if not normalized:
        raise ReportError(f"{label} must be non-empty")
    if not normalized.isprintable():
        raise ReportError(f"{label} must be a single printable line")
    return normalized


def _single_line_string(value: dict[str, Any], *keys: str) -> str:
    return _single_line_text(_non_empty_string(value, *keys), ".".join(keys))


def _utc_timestamp(value: dict[str, Any], *keys: str) -> datetime:
    text = _non_empty_string(value, *keys)
    if UTC_TIMESTAMP_PATTERN.fullmatch(text) is None:
        raise ReportError(
            f"{'.'.join(keys)} must use YYYY-MM-DDTHH:MM:SS[.ffffff]Z UTC format"
        )
    try:
        return datetime.fromisoformat(text[:-1] + "+00:00")
    except ValueError as error:
        raise ReportError(f"{'.'.join(keys)} must be an ISO-8601 UTC timestamp") from error


def _iron_gang_executable_name(value: dict[str, Any], *keys: str, label: str) -> str:
    executable = _single_line_text(_non_empty_string(value, *keys), label)
    executable_name = executable.replace("\\", "/").rsplit("/", 1)[-1].casefold()
    if executable_name not in IRON_GANG_EXECUTABLES:
        raise ReportError(f"{label} must identify iron_gang")
    return executable_name


def validate_capture_session(
    capture: dict[str, Any],
    required: bool,
) -> tuple[str, int | None, datetime, datetime] | None:
    raw_session = capture.get("capture_session")
    if raw_session is None:
        if required:
            raise ReportError("complete VRAM evidence requires capture_session metadata")
        return None
    session = _mapping(raw_session, "capture_session")
    executable_name = _iron_gang_executable_name(
        session,
        "process",
        "executable",
        label="capture_session.process.executable",
    )
    pid_known = _boolean(session, "process", "pid_known")
    raw_pid = _path(session, "process", "pid")
    if pid_known:
        pid = _integer(session, "process", "pid")
        if pid == 0:
            raise ReportError("capture_session.process.pid must be positive when known")
    else:
        if raw_pid is not None:
            raise ReportError("capture_session.process.pid must be null when pid_known is false")
        pid = None
    started = _utc_timestamp(session, "started_utc")
    ended = _utc_timestamp(session, "ended_utc")
    if ended <= started:
        raise ReportError("capture_session.ended_utc must follow started_utc")
    return executable_name, pid, started, ended


def _capture_session_interval(capture: dict[str, Any]) -> tuple[datetime, datetime]:
    session = validate_capture_session(capture, required=True)
    assert session is not None
    return session[2], session[3]


def _capture_sessions_overlap(
    left: dict[str, Any],
    right: dict[str, Any],
) -> bool:
    left_started, left_ended = _capture_session_interval(left)
    right_started, right_ended = _capture_session_interval(right)
    return left_started < right_ended and right_started < left_ended


def validate_external_vram_measurement(
    evidence: dict[str, Any],
    label: str,
) -> int:
    _iron_gang_executable_name(
        evidence,
        "process",
        "executable",
        label=f"{label}.process.executable",
    )
    if _integer(evidence, "process", "pid") == 0:
        raise ReportError(f"{label}.process.pid must be positive")
    started = _utc_timestamp(evidence, "measurement", "started_utc")
    ended = _utc_timestamp(evidence, "measurement", "ended_utc")
    if ended <= started:
        raise ReportError(f"{label}.measurement.ended_utc must follow started_utc")
    peak_resident_bytes = _integer(evidence, "measurement", "peak_resident_bytes")
    if peak_resident_bytes == 0:
        raise ReportError(f"{label}.measurement.peak_resident_bytes must be positive")
    return peak_resident_bytes


def swap_interval_acknowledged(capture: dict[str, Any]) -> bool:
    requested = _integer(capture, "swap_interval", "requested")
    if requested not in (0, 1):
        raise ReportError("swap_interval.requested must be 0 or 1")
    vertical_sync_requested = _boolean(capture, "timing", "vertical_sync_requested")
    if vertical_sync_requested != (requested == 1):
        raise ReportError(
            "timing.vertical_sync_requested must agree with swap_interval.requested"
        )

    result_known = _boolean(capture, "swap_interval", "apply_result_known")
    apply_succeeded = _path(capture, "swap_interval", "apply_succeeded")
    applied = _path(capture, "swap_interval", "applied")
    if result_known:
        if not isinstance(apply_succeeded, bool):
            raise ReportError(
                "swap_interval.apply_succeeded must be boolean when apply_result_known is true"
            )
    elif apply_succeeded is not None:
        raise ReportError(
            "swap_interval.apply_succeeded must be null when apply_result_known is false"
        )

    if apply_succeeded is True:
        if isinstance(applied, bool) or not isinstance(applied, int):
            raise ReportError("swap_interval.applied must be an integer after successful apply")
        if applied != requested:
            raise ReportError("swap_interval.applied must equal swap_interval.requested")
    elif applied is not None:
        raise ReportError("swap_interval.applied must be null unless apply succeeded")

    if _single_line_string(capture, "swap_interval", "proof") != SWAP_INTERVAL_PROOF:
        raise ReportError("swap_interval.proof does not match schema-8 presentation scope")
    reason = _path(capture, "swap_interval", "unavailable_reason")
    if not isinstance(reason, str):
        raise ReportError("swap_interval.unavailable_reason must be a string")
    if apply_succeeded is True:
        if reason != "":
            raise ReportError(
                "swap_interval.unavailable_reason must be empty after successful apply"
            )
    else:
        _single_line_text(reason, "swap_interval.unavailable_reason")
    return result_known and apply_succeeded is True


def _require_schema_number(
    value: dict[str, Any],
    keys: tuple[str, ...],
    expected: float,
) -> None:
    actual = _number(value, *keys)
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.000001):
        raise ReportError(f"{'.'.join(keys)} must be {expected:.3f}")


def validate_locked_budgets(capture: dict[str, Any]) -> None:
    for key, expected in LOCKED_FLOAT_BUDGETS.items():
        _require_schema_number(capture, ("budgets", key), expected)
    for key, expected in LOCKED_INTEGER_BUDGETS.items():
        actual = _integer(capture, "budgets", key)
        if actual != expected:
            raise ReportError(f"budgets.{key} must be {expected}")


def validate_capture_metadata(capture: dict[str, Any]) -> None:
    for key in ("backend", "build_configuration", "scenario"):
        _single_line_string(capture, key)
    width = _integer(capture, "resolution", "width")
    height = _integer(capture, "resolution", "height")
    if width == 0 or height == 0:
        raise ReportError("resolution width and height must be positive")
    _boolean(capture, "timing", "vertical_sync_requested")
    _boolean(capture, "timing", "fixed_timestep")
    if _number(capture, "timing", "target_frame_ms") == 0.0:
        raise ReportError("timing.target_frame_ms must be positive")


def validate_gpu_timing(capture: dict[str, Any]) -> None:
    supported = _boolean(capture, "gpu_timing", "supported")
    if not _boolean(capture, "gpu_timing", "non_blocking"):
        raise ReportError("gpu_timing.non_blocking must be true")
    if _single_line_string(capture, "gpu_timing", "scope") != GPU_TIMING_SCOPE:
        raise ReportError(f"gpu_timing.scope must be {GPU_TIMING_SCOPE}")
    _integer(capture, "gpu_timing", "discarded_samples")
    reason = _path(capture, "gpu_timing", "unsupported_reason")
    if not isinstance(reason, str):
        raise ReportError("gpu_timing.unsupported_reason must be a string")
    if supported:
        if reason != "":
            raise ReportError(
                "gpu_timing.unsupported_reason must be empty when GPU timing is supported"
            )
    else:
        _single_line_text(reason, "gpu_timing.unsupported_reason")


def validate_measurement_summary(capture: dict[str, Any], metric: str) -> None:
    samples = _integer(capture, "measurements", metric, "samples")
    average = _number(capture, "measurements", metric, "average_ms")
    percentile = _number(capture, "measurements", metric, "p95_ms")
    maximum = _number(capture, "measurements", metric, "maximum_ms")
    if samples == 0:
        if any(
            not math.isclose(value, 0.0, rel_tol=0.0, abs_tol=0.000001)
            for value in (average, percentile, maximum)
        ):
            raise ReportError(
                f"measurements.{metric} with zero samples must have zero statistics"
            )
        return
    if average > maximum or percentile > maximum:
        raise ReportError(
            f"measurements.{metric} average_ms and p95_ms must not exceed maximum_ms"
        )
    if samples == 1 and not (
        math.isclose(average, percentile, rel_tol=0.0, abs_tol=0.000001)
        and math.isclose(percentile, maximum, rel_tol=0.0, abs_tol=0.000001)
    ):
        raise ReportError(
            f"measurements.{metric} with one sample must have identical statistics"
        )


def _validate_stored_pass(
    capture: dict[str, Any],
    check: str,
    metric: str,
    budget_ms: float,
) -> None:
    actual = _boolean(capture, "checks", check)
    samples = _integer(capture, "measurements", metric, "samples")
    percentile = _number(capture, "measurements", metric, "p95_ms")
    if samples == 0:
        expected = False
    elif percentile == budget_ms:
        # The producer evaluates full-precision samples but serializes p95 to three decimals.
        # At the rounded boundary either stored result can be faithful to the hidden value.
        return
    else:
        expected = percentile < budget_ms
    if actual != expected:
        raise ReportError(
            f"checks.{check} must match measurements.{metric} availability and p95 budget"
        )


def validate_derived_checks(capture: dict[str, Any]) -> None:
    _validate_stored_pass(
        capture,
        "minimum_frame_rate_pass",
        "frame_interval",
        SCHEMA_MINIMUM_FRAME_MS,
    )
    _validate_stored_pass(
        capture,
        "recommended_frame_rate_pass",
        "frame_interval",
        SCHEMA_RECOMMENDED_FRAME_MS,
    )

    cpu_pass = _boolean(capture, "checks", "cpu_subsystems_pass")
    cpu_definitely_fails = False
    cpu_has_rounded_boundary = False
    for metric, budget_ms in CPU_BUDGETS_MS.items():
        samples = _integer(capture, "measurements", metric, "samples")
        percentile = _number(capture, "measurements", metric, "p95_ms")
        cpu_definitely_fails |= samples == 0 or percentile > budget_ms
        cpu_has_rounded_boundary |= samples > 0 and percentile == budget_ms
    if cpu_definitely_fails:
        expected_cpu_pass: bool | None = False
    elif cpu_has_rounded_boundary:
        expected_cpu_pass = None
    else:
        expected_cpu_pass = True
    if expected_cpu_pass is not None and cpu_pass != expected_cpu_pass:
        raise ReportError(
            "checks.cpu_subsystems_pass must match CPU measurement availability and p95 budgets"
        )

    district_samples = _integer(
        capture, "measurements", "district_load_cpu", "samples"
    )
    district_check = _path(capture, "checks", "district_load_pass")
    if district_samples == 0:
        if district_check is not None:
            raise ReportError(
                "checks.district_load_pass must be null when district_load_cpu has no samples"
            )
    else:
        if not isinstance(district_check, bool):
            raise ReportError(
                "checks.district_load_pass must be boolean when district_load_cpu has samples"
            )
        district_p95 = _number(
            capture, "measurements", "district_load_cpu", "p95_ms"
        )
        if district_p95 != DISTRICT_LOAD_BUDGET_MS:
            expected_district_pass = district_p95 < DISTRICT_LOAD_BUDGET_MS
            if district_check != expected_district_pass:
                raise ReportError(
                    "checks.district_load_pass must match district_load_cpu p95 budget"
                )


def _validate_district_measurement(
    capture: dict[str, Any],
    metric: str,
    samples: list[float],
) -> None:
    sample_count = _integer(capture, "measurements", metric, "samples")
    if sample_count != len(samples):
        raise ReportError(
            f"measurements.{metric}.samples must match district_load.samples"
        )
    if not samples:
        return
    sorted_samples = sorted(samples)
    percentile_index = math.ceil(0.95 * len(sorted_samples)) - 1
    expected = {
        "average_ms": sum(samples) / len(samples),
        "p95_ms": sorted_samples[percentile_index],
        "maximum_ms": max(samples),
    }
    for key, expected_value in expected.items():
        actual = _number(capture, "measurements", metric, key)
        if not math.isclose(actual, expected_value, rel_tol=0.0, abs_tol=0.001001):
            raise ReportError(
                f"measurements.{metric}.{key} must match district_load.samples"
            )


def validate_district_load(capture: dict[str, Any]) -> None:
    district_load = _mapping(_path(capture, "district_load"), "district_load")
    fixed_metadata = {
        "content_path": DISTRICT_CONTENT_PATH,
        "unload_activation_scope": DISTRICT_UNLOAD_SCOPE,
        "renderer_upload_scope": DISTRICT_RENDERER_SCOPE,
        "unavailable_reason": DISTRICT_UNAVAILABLE_REASON,
    }
    for key, expected in fixed_metadata.items():
        if _single_line_string(district_load, key) != expected:
            raise ReportError(f"district_load.{key} does not match schema-8 scope")
    for key in ("io_ms", "decompression_ms", "parse_ms"):
        if _path(district_load, key) is not None:
            raise ReportError(f"district_load.{key} must be null for procedural districts")

    samples = _array(_path(district_load, "samples"), "district_load.samples")
    world_samples: list[float] = []
    renderer_samples: list[float] = []
    total_samples: list[float] = []
    for index, raw_sample in enumerate(samples):
        label = f"district_load.samples[{index}]"
        sample = _mapping(raw_sample, label)
        for key in ("reason", "source", "target"):
            _single_line_string(sample, key)
        world_ms = _number(sample, "world_physics_ms")
        renderer_ms = _number(sample, "renderer_upload_ms")
        total_ms = _number(sample, "total_ms")
        if not math.isclose(
            total_ms,
            world_ms + renderer_ms,
            rel_tol=0.0,
            abs_tol=0.001001,
        ):
            raise ReportError(f"{label}.total_ms must equal its two phase durations")
        world_samples.append(world_ms)
        renderer_samples.append(renderer_ms)
        total_samples.append(total_ms)

        if _integer(sample, "asset_counts", "district_files") != 0:
            raise ReportError(f"{label}.asset_counts.district_files must be zero")
        _integer(sample, "asset_counts", "procedural_world_objects")
        _integer(sample, "asset_counts", "static_physics_bodies")

        resident_known = _boolean(sample, "memory", "resident_known")
        resident_before = _integer(sample, "memory", "resident_before_bytes")
        resident_after = _integer(sample, "memory", "resident_after_bytes")
        if resident_known != (resident_before > 0 and resident_after > 0):
            raise ReportError(
                f"{label}.memory.resident_known must match its before/after byte counts"
            )
        resident_delta = _path(sample, "memory", "resident_delta_bytes")
        if resident_known:
            if _signed_integer(sample, "memory", "resident_delta_bytes") != (
                resident_after - resident_before
            ):
                raise ReportError(f"{label}.memory.resident_delta_bytes is inconsistent")
        elif resident_delta is not None:
            raise ReportError(
                f"{label}.memory.resident_delta_bytes must be null when resident memory is unknown"
            )

        tracked_before = _integer(
            sample, "memory", "tracked_video_memory_before_bytes"
        )
        tracked_after = _integer(
            sample, "memory", "tracked_video_memory_after_bytes"
        )
        if _signed_integer(
            sample, "memory", "tracked_video_memory_delta_bytes"
        ) != (tracked_after - tracked_before):
            raise ReportError(
                f"{label}.memory.tracked_video_memory_delta_bytes is inconsistent"
            )

    _validate_district_measurement(
        capture, "district_world_physics_cpu", world_samples
    )
    _validate_district_measurement(
        capture, "district_renderer_upload_cpu", renderer_samples
    )
    _validate_district_measurement(capture, "district_load_cpu", total_samples)


def _validate_workload_summary(
    workload: dict[str, Any],
    section: str,
    metric: str,
) -> None:
    summary = _mapping(_path(workload, metric), f"{section}.{metric}")
    samples = _integer(summary, "samples")
    average = _number(summary, "average")
    percentile = _number(summary, "p95")
    maximum = _number(summary, "maximum")
    if samples == 0:
        if any(
            not math.isclose(value, 0.0, rel_tol=0.0, abs_tol=0.000001)
            for value in (average, percentile, maximum)
        ):
            raise ReportError(
                f"{section}.{metric} with zero samples must have zero statistics"
            )
        return
    if average > maximum or percentile > maximum:
        raise ReportError(
            f"{section}.{metric} average and p95 must not exceed maximum"
        )
    if samples == 1 and not (
        math.isclose(average, percentile, rel_tol=0.0, abs_tol=0.000001)
        and math.isclose(percentile, maximum, rel_tol=0.0, abs_tol=0.000001)
    ):
        raise ReportError(
            f"{section}.{metric} with one sample must have identical statistics"
        )
    for key, value in (("p95", percentile), ("maximum", maximum)):
        if not math.isclose(value, round(value), rel_tol=0.0, abs_tol=0.000001):
            raise ReportError(
                f"{section}.{metric}.{key} must be an integer-valued count"
            )


def validate_workloads(capture: dict[str, Any]) -> None:
    for section, (metadata, metrics) in WORKLOAD_SCHEMAS.items():
        workload = _mapping(_path(capture, section), section)
        for key, expected in metadata.items():
            if _single_line_string(workload, key) != expected:
                raise ReportError(f"{section}.{key} does not match schema-8 scope")
        for metric in metrics:
            _validate_workload_summary(workload, section, metric)


def _validate_pacing_counter(
    capture: dict[str, Any],
    key: str,
    expected_count: int,
    threshold_ms: float,
    sample_count: int,
) -> None:
    _require_schema_number(capture, ("frame_pacing", key, "threshold_ms"), threshold_ms)
    if _non_empty_string(capture, "frame_pacing", key, "comparison") != "greater_than":
        raise ReportError(f"frame_pacing.{key}.comparison must be greater_than")
    actual_count = _integer(capture, "frame_pacing", key, "count")
    if actual_count != expected_count:
        raise ReportError(
            f"frame_pacing.{key}.count does not match the frame-pacing histogram"
        )
    expected_percent = 0.0 if sample_count == 0 else 100.0 * expected_count / sample_count
    actual_percent = _number(capture, "frame_pacing", key, "percent")
    if not math.isclose(actual_percent, expected_percent, rel_tol=0.0, abs_tol=0.0005):
        raise ReportError(
            f"frame_pacing.{key}.percent does not match count/samples"
        )


def validate_frame_pacing(capture: dict[str, Any], path: Path) -> None:
    if _single_line_string(capture, "frame_pacing", "scope") != FRAME_PACING_SCOPE:
        raise ReportError("frame_pacing.scope does not match schema-8 sampling scope")
    if (
        _single_line_string(capture, "frame_pacing", "boundary_scope")
        != FRAME_BOUNDARY_SCOPE
    ):
        raise ReportError(
            "frame_pacing.boundary_scope does not match schema-8 transition scope"
        )
    frame_samples = _integer(capture, "measurements", "frame_interval", "samples")
    pacing_samples = _integer(capture, "frame_pacing", "samples")
    histogram = _mapping(_path(capture, "frame_pacing", "histogram"), "frame_pacing.histogram")
    counts = {
        bucket: _integer(histogram, bucket, "count") for bucket in HISTOGRAM_BUCKETS
    }
    histogram_total = sum(counts.values())
    if pacing_samples != frame_samples or histogram_total != frame_samples:
        raise ReportError(
            f"{path}: frame-pacing samples/histogram ({pacing_samples}/{histogram_total}) "
            f"do not match frame_interval samples ({frame_samples})"
        )

    _require_schema_number(
        histogram,
        ("at_or_below_recommended_budget", "upper_bound_ms"),
        SCHEMA_RECOMMENDED_FRAME_MS,
    )
    _require_schema_number(
        histogram,
        ("above_recommended_at_or_below_minimum_budget", "lower_bound_exclusive_ms"),
        SCHEMA_RECOMMENDED_FRAME_MS,
    )
    _require_schema_number(
        histogram,
        ("above_recommended_at_or_below_minimum_budget", "upper_bound_ms"),
        SCHEMA_MINIMUM_FRAME_MS,
    )
    _require_schema_number(
        histogram,
        ("above_minimum_at_or_below_hitch", "lower_bound_exclusive_ms"),
        SCHEMA_MINIMUM_FRAME_MS,
    )
    _require_schema_number(
        histogram,
        ("above_minimum_at_or_below_hitch", "upper_bound_ms"),
        FRAME_HITCH_MS,
    )
    _require_schema_number(
        histogram,
        ("above_hitch_at_or_below_severe_hitch", "lower_bound_exclusive_ms"),
        FRAME_HITCH_MS,
    )
    _require_schema_number(
        histogram,
        ("above_hitch_at_or_below_severe_hitch", "upper_bound_ms"),
        SEVERE_FRAME_HITCH_MS,
    )
    _require_schema_number(
        histogram,
        ("above_severe_hitch", "lower_bound_exclusive_ms"),
        SEVERE_FRAME_HITCH_MS,
    )

    if frame_samples > 0:
        percentile_rank = math.ceil(0.95 * frame_samples)
        cumulative = 0
        percentile_bucket = -1
        for index, bucket in enumerate(HISTOGRAM_BUCKETS):
            cumulative += counts[bucket]
            if cumulative >= percentile_rank:
                percentile_bucket = index
                break
        bounds = (
            (None, SCHEMA_RECOMMENDED_FRAME_MS),
            (SCHEMA_RECOMMENDED_FRAME_MS, SCHEMA_MINIMUM_FRAME_MS),
            (SCHEMA_MINIMUM_FRAME_MS, FRAME_HITCH_MS),
            (FRAME_HITCH_MS, SEVERE_FRAME_HITCH_MS),
            (SEVERE_FRAME_HITCH_MS, None),
        )
        lower, upper = bounds[percentile_bucket]
        percentile = _number(capture, "measurements", "frame_interval", "p95_ms")
        rounding_tolerance = 0.0005
        if (lower is not None and percentile < lower - rounding_tolerance) or (
            upper is not None and percentile > upper + rounding_tolerance
        ):
            raise ReportError(
                "measurements.frame_interval.p95_ms does not fall in the frame-pacing "
                "histogram bucket containing the nearest-rank p95 sample"
            )

    minimum_misses = (
        counts["above_minimum_at_or_below_hitch"]
        + counts["above_hitch_at_or_below_severe_hitch"]
        + counts["above_severe_hitch"]
    )
    hitches = (
        counts["above_hitch_at_or_below_severe_hitch"]
        + counts["above_severe_hitch"]
    )
    severe_hitches = counts["above_severe_hitch"]
    _validate_pacing_counter(
        capture,
        "minimum_budget_misses",
        minimum_misses,
        SCHEMA_MINIMUM_FRAME_MS,
        frame_samples,
    )
    _validate_pacing_counter(capture, "hitches", hitches, FRAME_HITCH_MS, frame_samples)
    _validate_pacing_counter(
        capture,
        "severe_hitches",
        severe_hitches,
        SEVERE_FRAME_HITCH_MS,
        frame_samples,
    )

    transitions = _integer(capture, "frame_pacing", "district_transition_boundaries", "transitions")
    measured = _integer(
        capture, "frame_pacing", "district_transition_boundaries", "measured_samples"
    )
    boundary_hitches = _integer(
        capture, "frame_pacing", "district_transition_boundaries", "hitch_count"
    )
    load_samples = _integer(capture, "measurements", "district_load_cpu", "samples")
    if transitions != load_samples:
        raise ReportError(
            "frame_pacing.district_transition_boundaries.transitions must match "
            "district_load_cpu samples"
        )
    if measured > transitions or boundary_hitches > measured:
        raise ReportError("district-transition boundary counts are inconsistent")
    raw_maximum = _path(capture, "frame_pacing", "district_transition_boundaries", "maximum_ms")
    if measured == 0:
        if raw_maximum is not None:
            raise ReportError("unmeasured district-transition boundaries require maximum_ms null")
    else:
        maximum = _number(
            capture, "frame_pacing", "district_transition_boundaries", "maximum_ms"
        )
        if (maximum > FRAME_HITCH_MS) != (boundary_hitches > 0):
            raise ReportError(
                "district-transition boundary hitch_count does not match maximum_ms"
            )


def validate_complete_vram_evidence(
    capture: dict[str, Any],
    hardware: str | None = None,
) -> None:
    if not _boolean(capture, "video_memory", "tracking_complete"):
        return

    capture_session = validate_capture_session(capture, required=True)
    assert capture_session is not None
    _, capture_pid, capture_started, capture_ended = capture_session
    if capture_pid is None:
        raise ReportError("complete VRAM evidence requires a known capture_session process PID")

    evidence = _mapping(
        _path(capture, "video_memory", "complete_evidence"),
        "video_memory.complete_evidence",
    )
    if _integer(evidence, "schema_version") != 1:
        raise ReportError("video_memory.complete_evidence.schema_version must be 1")
    if _non_empty_string(evidence, "source") != "external_capture":
        raise ReportError("video_memory.complete_evidence.source must be external_capture")
    if _non_empty_string(evidence, "measurement_scope") != COMPLETE_VRAM_SCOPE:
        raise ReportError(
            "video_memory.complete_evidence.measurement_scope must be " + COMPLETE_VRAM_SCOPE
        )
    evidence_hardware = _single_line_string(evidence, "hardware_identity")
    _single_line_string(evidence, "tool", "name")
    _single_line_string(evidence, "tool", "version")
    peak_resident_bytes = validate_external_vram_measurement(
        evidence, "video_memory.complete_evidence"
    )
    evidence_pid = _integer(evidence, "process", "pid")
    if evidence_pid != capture_pid:
        raise ReportError("external VRAM evidence PID does not match capture_session process PID")
    evidence_started = _utc_timestamp(evidence, "measurement", "started_utc")
    evidence_ended = _utc_timestamp(evidence, "measurement", "ended_utc")
    if evidence_started > capture_started or evidence_ended < capture_ended:
        raise ReportError(
            "external VRAM evidence interval must enclose the complete capture_session interval"
        )
    logical_tracked_bytes = _integer(capture, "video_memory", "logical_tracked_bytes")
    tracked_bytes = _integer(capture, "video_memory", "tracked_bytes")
    if tracked_bytes != max(logical_tracked_bytes, peak_resident_bytes):
        raise ReportError(
            "video_memory.tracked_bytes must equal the conservative maximum of logical and "
            "externally measured complete residency"
        )
    for key in ("profile_capture_sha256", "evidence_manifest_sha256"):
        digest = _non_empty_string(evidence, key)
        if SHA256_PATTERN.fullmatch(digest) is None:
            raise ReportError(f"video_memory.complete_evidence.{key} must be lowercase SHA-256")
    _non_empty_string(evidence, "source_artifact", "file_name")
    source_digest = _non_empty_string(evidence, "source_artifact", "sha256")
    if SHA256_PATTERN.fullmatch(source_digest) is None:
        raise ReportError(
            "video_memory.complete_evidence.source_artifact.sha256 must be lowercase SHA-256"
        )
    if hardware is not None and evidence_hardware != hardware.strip():
        raise ReportError(
            "external VRAM evidence hardware identity does not match the report hardware label"
        )


def validate_memory_summary(capture: dict[str, Any]) -> None:
    peak_resident_bytes = _integer(capture, "memory", "peak_resident_bytes")
    memory_known = _boolean(capture, "memory", "known")
    expected_known = peak_resident_bytes > 0
    if memory_known != expected_known:
        raise ReportError(
            "memory.known must equal whether memory.peak_resident_bytes is nonzero"
        )
    memory_budget_pass = _boolean(capture, "memory", "budget_pass")
    expected_memory_pass = expected_known and peak_resident_bytes <= RAM_BUDGET_BYTES
    if memory_budget_pass != expected_memory_pass:
        raise ReportError(
            "memory.budget_pass must match the derived peak-resident RAM budget result"
        )

    tracked_bytes = _integer(capture, "video_memory", "tracked_bytes")
    category_bytes = sum(
        _integer(capture, "video_memory", key)
        for key in (
            "game_owned_bytes",
            "imported_model_buffer_bytes",
            "imported_model_texture_bytes",
        )
    )
    tracking_complete = _boolean(capture, "video_memory", "tracking_complete")
    if tracking_complete:
        logical_tracked_bytes = _integer(
            capture, "video_memory", "logical_tracked_bytes"
        )
        if logical_tracked_bytes != category_bytes:
            raise ReportError(
                "video_memory.logical_tracked_bytes must equal the sum of the three "
                "logical VRAM categories"
            )
    elif tracked_bytes != category_bytes:
        raise ReportError(
            "incomplete video_memory.tracked_bytes must equal the sum of the three "
            "logical VRAM categories"
        )

    tracked_budget_pass = _boolean(capture, "video_memory", "tracked_budget_pass")
    if tracked_budget_pass != (tracked_bytes <= VRAM_BUDGET_BYTES):
        raise ReportError(
            "video_memory.tracked_budget_pass must match the derived tracked VRAM budget result"
        )
    coverage = _single_line_string(capture, "video_memory", "coverage")
    expected_coverage = (
        COMPLETE_VRAM_COVERAGE if tracking_complete else LOGICAL_VRAM_COVERAGE
    )
    if coverage != expected_coverage:
        state = "complete" if tracking_complete else "logical"
        raise ReportError(
            f"video_memory.coverage does not match schema-8 {state} measurement scope"
        )


def load_capture(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as source:
            capture = _mapping(_strict_json_load(source), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"could not read {path}: {error}") from error

    schema_version = capture.get("schema_version")
    if (
        isinstance(schema_version, bool)
        or not isinstance(schema_version, int)
        or schema_version != SCHEMA_VERSION
    ):
        raise ReportError(
            f"{path}: schema_version must be integer {SCHEMA_VERSION}, got {schema_version!r}"
        )

    validate_capture_metadata(capture)
    validate_locked_budgets(capture)
    validate_gpu_timing(capture)
    for workload in (
        "physics_bodies",
        "traffic_vehicles",
        "pedestrians",
        "police_vehicles",
    ):
        _integer(capture, "workload", workload)
    required_metrics = (
        "frame_interval",
        *CPU_BUDGETS_MS,
        "present_cpu",
        "gpu_render",
        "district_world_physics_cpu",
        "district_renderer_upload_cpu",
        "district_load_cpu",
        "startup_cpu",
    )
    measurements = _mapping(_path(capture, "measurements"), "measurements")
    for metric in required_metrics:
        if metric not in measurements:
            raise ReportError(f"missing measurements.{metric}")
        _mapping(measurements[metric], f"measurements.{metric}")
    for metric in measurements:
        validate_measurement_summary(capture, metric)
    validate_frame_pacing(capture, path)
    validate_derived_checks(capture)
    validate_district_load(capture)
    validate_workloads(capture)
    validate_memory_summary(capture)
    swap_interval_acknowledged(capture)
    validate_capture_session(capture, required=False)
    validate_complete_vram_evidence(capture)
    return capture


def capture_blockers(path: Path, capture: dict[str, Any], hardware: str) -> list[str]:
    prefix = f"{path.name}: "
    blockers: list[str] = []
    if _path(capture, "backend") != "OPENGLES3":
        blockers.append(prefix + "backend is not the primary EasyGL/OPENGLES3 target")
    if _path(capture, "build_configuration") != "Release":
        blockers.append(prefix + "capture is not a Release build")

    fixed_timestep = _boolean(capture, "timing", "fixed_timestep")
    target_frame_ms = _number(capture, "timing", "target_frame_ms")
    if not fixed_timestep or not math.isclose(
        target_frame_ms,
        SCHEMA_RECOMMENDED_FRAME_MS,
        rel_tol=0.0,
        abs_tol=0.000001,
    ):
        blockers.append(prefix + "capture does not use the locked 16.667 ms fixed timestep")

    width = _integer(capture, "resolution", "width")
    height = _integer(capture, "resolution", "height")
    if width < MINIMUM_WIDTH or height < MINIMUM_HEIGHT:
        blockers.append(prefix + f"resolution {width}x{height} is below 1280x720")

    if not swap_interval_acknowledged(capture):
        blockers.append(prefix + "requested swap interval lacks a successful platform acknowledgement")

    frame_samples = _integer(capture, "measurements", "frame_interval", "samples")
    frame_p95 = _number(capture, "measurements", "frame_interval", "p95_ms")
    if frame_samples == 0 or frame_p95 > MINIMUM_FRAME_MS:
        blockers.append(prefix + f"frame p95 {frame_p95:.3f} ms does not pass 33.333 ms")

    for metric, budget in CPU_BUDGETS_MS.items():
        samples = _integer(capture, "measurements", metric, "samples")
        p95 = _number(capture, "measurements", metric, "p95_ms")
        if samples == 0 or p95 > budget:
            blockers.append(prefix + f"{metric} p95 {p95:.3f} ms does not pass {budget:.3f} ms")

    peak_ram = int(_number(capture, "memory", "peak_resident_bytes"))
    if not _boolean(capture, "memory", "known") or peak_ram > RAM_BUDGET_BYTES:
        blockers.append(prefix + "peak resident RAM is unknown or exceeds 2 GiB")

    tracked_vram = int(_number(capture, "video_memory", "tracked_bytes"))
    if not _boolean(capture, "video_memory", "tracking_complete"):
        blockers.append(prefix + "VRAM tracking is incomplete")
    else:
        try:
            validate_complete_vram_evidence(capture, hardware)
        except ReportError as error:
            blockers.append(prefix + str(error))
        if tracked_vram > VRAM_BUDGET_BYTES:
            blockers.append(prefix + "VRAM exceeds 512 MiB")

    if _path(capture, "scenario") == "mixed":
        load_samples = _integer(capture, "measurements", "district_load_cpu", "samples")
        load_p95 = _number(capture, "measurements", "district_load_cpu", "p95_ms")
        if load_samples == 0 or load_p95 > DISTRICT_LOAD_BUDGET_MS:
            blockers.append(prefix + "mixed capture lacks a passing real district transition")
    return blockers


def qualification_repeatability_blockers(
    capture_paths: list[Path],
    captures: list[dict[str, Any]],
) -> list[str]:
    mixed = [
        (path, capture)
        for path, capture in zip(capture_paths, captures)
        if _path(capture, "scenario") == "mixed"
    ]
    if len(mixed) < 2:
        return []

    reference_path, reference = mixed[0]
    blockers: list[str] = []
    for candidate_path, candidate in mixed[1:]:
        for policy_path in QUALIFICATION_REPEATABILITY_PATHS:
            if _path(reference, *policy_path) != _path(candidate, *policy_path):
                blockers.append(
                    f"{candidate_path.name}: repeatability policy does not match "
                    f"{reference_path.name} at {'.'.join(policy_path)}"
                )
    for left_index, (left_path, left) in enumerate(mixed):
        for right_path, right in mixed[left_index + 1 :]:
            if _capture_sessions_overlap(left, right):
                blockers.append(
                    f"{left_path.name} and {right_path.name}: qualifying capture sessions "
                    "overlap and are not independent runs"
                )
    return blockers


def _mib(byte_count: float) -> str:
    return f"{byte_count / (1024.0 * 1024.0):.1f}"


def _display_text(value: Any) -> str:
    return "".join(
        character if character.isprintable() else f"\\u{ord(character):04x}"
        for character in str(value)
    )


def _escape(value: Any) -> str:
    escaped = re.sub(r"([\\`*_{}\[\]!|~])", r"\\\1", _display_text(value))
    return html.escape(escaped, quote=False)


def _markdown_code(value: Any) -> str:
    escaped = html.escape(_display_text(value), quote=True).replace("|", "&#124;")
    return f"<code>{escaped}</code>"


def _capture_identity(capture: dict[str, Any]) -> str:
    identity_capture = dict(capture)
    video_memory = dict(_mapping(_path(capture, "video_memory"), "video_memory"))
    logical_bytes = (
        _integer(capture, "video_memory", "logical_tracked_bytes")
        if _boolean(capture, "video_memory", "tracking_complete")
        else _integer(capture, "video_memory", "tracked_bytes")
    )
    video_memory["tracked_bytes"] = logical_bytes
    video_memory["tracking_complete"] = False
    video_memory["tracked_budget_pass"] = logical_bytes <= VRAM_BUDGET_BYTES
    for external_key in ("complete_evidence", "logical_tracked_bytes", "coverage"):
        video_memory.pop(external_key, None)
    identity_capture["video_memory"] = video_memory
    return json.dumps(identity_capture, ensure_ascii=True, separators=(",", ":"), sort_keys=True)


def build_markdown(
    capture_paths: list[Path],
    captures: list[dict[str, Any]],
    capture_sha256s: list[str],
    bundle_fingerprints: list[VramBundleFingerprint | None],
    hardware: str,
    qualifying_hardware: bool,
    title: str,
) -> str:
    blockers: list[str] = []
    mixed_count = len(
        {
            _capture_identity(capture)
            for capture in captures
            if _path(capture, "scenario") == "mixed"
        }
    )
    if mixed_count < 2:
        blockers.append(
            "at least two mixed captures with distinct canonical contents are required for "
            "repeatable qualification"
        )
    if any(term in hardware.lower() for term in DIAGNOSTIC_HARDWARE_TERMS):
        blockers.append("the hardware label identifies a diagnostic software/virtual display")
    if qualifying_hardware:
        blockers.extend(qualification_repeatability_blockers(capture_paths, captures))
    per_capture_blockers: list[list[str]] = []
    for path, capture in zip(capture_paths, captures):
        current = capture_blockers(path, capture, hardware)
        per_capture_blockers.append(current)
        blockers.extend(current)

    if not qualifying_hardware:
        status = "DIAGNOSTIC"
        blockers.insert(0, "--qualifying-hardware was not supplied")
    else:
        status = "PASS" if not blockers else "FAIL"

    lines = [
        f"# {_escape(title)}",
        "",
        f"- Hardware: {_markdown_code(hardware)}",
        f"- Qualification intent: `{'qualifying' if qualifying_hardware else 'diagnostic'}`",
        f"- Schema: `{SCHEMA_VERSION}`",
        f"- Overall status: **{status}**",
        "",
        "## Qualification blockers",
        "",
    ]
    if blockers:
        lines.extend(f"- {_escape(blocker)}" for blocker in blockers)
    else:
        lines.append("- None.")

    lines.extend(
        [
            "",
            "## Evidence provenance",
            "",
            "| Evaluated capture | SHA-256 | Capture session | Original profile | Evidence manifest | Raw profiler artifact |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
    )
    for path, capture, capture_sha256, bundle in zip(
        capture_paths,
        captures,
        capture_sha256s,
        bundle_fingerprints,
    ):
        session = validate_capture_session(capture, required=False)
        if session is None:
            session_text = "—"
        else:
            _, process_id, _, _ = session
            session_text = (
                f"PID {process_id if process_id is not None else 'unknown'}; "
                f"{_non_empty_string(capture, 'capture_session', 'started_utc')} to "
                f"{_non_empty_string(capture, 'capture_session', 'ended_utc')}"
            )
        if bundle is None:
            bundle_cells = ("—", "—", "—")
        else:
            bundle_cells = tuple(
                f"{_markdown_code(source_path.name)}<br>{_markdown_code(source_sha256)}"
                for source_path, source_sha256 in bundle
            )
        lines.append(
            f"| {_markdown_code(path.name)} | {_markdown_code(capture_sha256)} | "
            f"{_escape(session_text)} | "
            f"{bundle_cells[0]} | {bundle_cells[1]} | {bundle_cells[2]} |"
        )

    lines.extend(
        [
            "",
            "## Capture summary",
            "",
            "| Capture | Scenario | Resolution | Frame p95 | 30 FPS | 60 FPS | CPU p95 U/P/AI/Au/R | GPU/Present p95 | Load p95 | Hitches | Severe | Transition frame | RAM MiB | Tracked VRAM MiB | VRAM complete | Swap ack | Local result |",
            "| --- | --- | ---: | ---: | :---: | :---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | :---: | :---: | :---: |",
        ]
    )
    for path, capture, local_blockers in zip(capture_paths, captures, per_capture_blockers):
        frame_p95 = _number(capture, "measurements", "frame_interval", "p95_ms")
        width = _integer(capture, "resolution", "width")
        height = _integer(capture, "resolution", "height")
        hitches = _integer(capture, "frame_pacing", "hitches", "count")
        severe = _integer(capture, "frame_pacing", "severe_hitches", "count")
        cpu_p95 = "/".join(
            f"{_number(capture, 'measurements', metric, 'p95_ms'):.3f}"
            for metric in CPU_BUDGETS_MS
        )
        gpu_samples = _integer(capture, "measurements", "gpu_render", "samples")
        gpu_p95 = _number(capture, "measurements", "gpu_render", "p95_ms")
        present_p95 = _number(capture, "measurements", "present_cpu", "p95_ms")
        gpu_present = f"{gpu_p95:.3f}/{present_p95:.3f} ms" if gpu_samples else f"—/{present_p95:.3f} ms"
        load_samples = _integer(capture, "measurements", "district_load_cpu", "samples")
        load_p95 = _number(capture, "measurements", "district_load_cpu", "p95_ms")
        load_text = f"{load_p95:.3f} ms" if load_samples else "—"
        boundary_max = _path(capture, "frame_pacing", "district_transition_boundaries", "maximum_ms")
        boundary_text = "—" if boundary_max is None else f"{float(boundary_max):.3f} ms"
        ram = _number(capture, "memory", "peak_resident_bytes")
        vram = _number(capture, "video_memory", "tracked_bytes")
        vram_complete = _boolean(capture, "video_memory", "tracking_complete")
        swap_ack = swap_interval_acknowledged(capture)
        recommended = (
            width >= RECOMMENDED_WIDTH
            and height >= RECOMMENDED_HEIGHT
            and frame_p95 <= RECOMMENDED_FRAME_MS
        )
        lines.append(
            f"| {_markdown_code(path.name)} | {_escape(_path(capture, 'scenario'))} | "
            f"{width}x{height} | "
            f"{frame_p95:.3f} ms | {'yes' if frame_p95 <= MINIMUM_FRAME_MS else 'no'} | "
            f"{'yes' if recommended else 'no'} | {cpu_p95} ms | {gpu_present} | {load_text} | "
            f"{hitches} | {severe} | {boundary_text} | "
            f"{_mib(ram)} | {_mib(vram)} | {'yes' if vram_complete else 'no'} | "
            f"{'yes' if swap_ack else 'no'} | {'PASS' if not local_blockers else 'FAIL'} |"
        )

    lines.extend(
        [
            "",
            "## Locked targets",
            "",
            "| Area | Minimum qualification | Recommended target |",
            "| --- | --- | --- |",
            "| Display cadence | 1280x720, frame p95 <=33.333 ms | 1920x1080, frame p95 <=16.667 ms |",
            "| CPU p95 | update 8 ms; physics 3 ms; AI 2 ms; audio 1 ms; render 8 ms | Same first-district guardrails |",
            "| Memory | <=2 GiB resident RAM; <=512 MiB complete VRAM | <=4 GiB RAM; <=1 GiB VRAM target hardware |",
            "| District transition | <=1000 ms p95 in at least two mixed captures | Same budget |",
            "| Presentation evidence | Successful platform swap-interval acknowledgement | Controlled physical vblank/compositor |",
            "",
            "The report generator never promotes an unlabelled capture, Xvfb, llvmpipe, or incomplete VRAM accounting to a qualifying pass.",
            "The provenance hashes identify the exact files read; qualifying bundle hashes were also reconstructed and verified before evaluation.",
            "",
        ]
    )
    return "\n".join(lines)


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path, help="schema-8 JSON capture(s)")
    parser.add_argument("--hardware", required=True, help="explicit hardware/display identity")
    parser.add_argument(
        "--qualifying-hardware",
        action="store_true",
        help="declare that captures came from controlled physical minimum-target hardware",
    )
    parser.add_argument(
        "--vram-bundle",
        action="append",
        default=[],
        nargs=3,
        type=Path,
        metavar=("ORIGINAL", "EVIDENCE", "ARTIFACT"),
        help="verify these three archived sources against the corresponding enriched capture",
    )
    parser.add_argument("--output", type=Path, help="write Markdown here instead of stdout")
    parser.add_argument("--title", default="Iron Gang M12 performance release report")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_args(sys.argv[1:] if arguments is None else arguments)
    try:
        hardware = _single_line_text(options.hardware, "hardware identity")
        title = _single_line_text(options.title, "report title")
        bundles: list[list[Path]] = options.vram_bundle
        if options.qualifying_hardware and len(bundles) != len(options.captures):
            raise ReportError(
                "qualifying reports require one --vram-bundle ORIGINAL EVIDENCE ARTIFACT "
                "for every enriched capture"
            )
        if bundles and len(bundles) != len(options.captures):
            raise ReportError("--vram-bundle count must match the capture count")
        if options.qualifying_hardware:
            _require_distinct_bundle_sources(bundles, "qualifying report")

        if options.output is not None:
            protected_inputs = [("capture", path) for path in options.captures]
            bundle_labels = ("original capture", "evidence manifest", "raw evidence artifact")
            for bundle in bundles:
                protected_inputs.extend(zip(bundle_labels, bundle))
            for input_label, input_path in protected_inputs:
                if _same_file(options.output, input_path):
                    raise ReportError(f"output must differ from every {input_label} input")

        capture_sha256s = [_file_sha256(path) for path in options.captures]
        bundle_fingerprints: list[VramBundleFingerprint | None] = [
            None for _ in options.captures
        ]
        verifier = Path(__file__).resolve().with_name("vram_evidence.py")
        for index, (capture_path, bundle) in enumerate(zip(options.captures, bundles)):
            bundle_fingerprint: VramBundleFingerprint = (
                (bundle[0], _file_sha256(bundle[0])),
                (bundle[1], _file_sha256(bundle[1])),
                (bundle[2], _file_sha256(bundle[2])),
            )
            verification = subprocess.run(
                [
                    sys.executable,
                    str(verifier),
                    "--capture",
                    str(bundle[0]),
                    "--evidence",
                    str(bundle[1]),
                    "--artifact",
                    str(bundle[2]),
                    "--verify-enriched",
                    str(capture_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            if verification.returncode != 0:
                detail = verification.stderr.strip() or "verification command failed"
                raise ReportError(f"VRAM bundle verification failed for {capture_path}: {detail}")
            _require_unchanged(
                capture_path,
                capture_sha256s[index],
                f"{capture_path} enriched capture",
            )
            for label, (path, expected_sha256) in zip(
                ("original capture", "evidence manifest", "raw evidence artifact"),
                bundle_fingerprint,
            ):
                _require_unchanged(path, expected_sha256, label)
            bundle_fingerprints[index] = bundle_fingerprint

        captures = [load_capture(path) for path in options.captures]
        _verify_report_inputs_unchanged(
            options.captures,
            capture_sha256s,
            bundle_fingerprints,
        )
        report = build_markdown(
            options.captures,
            captures,
            capture_sha256s,
            bundle_fingerprints,
            hardware,
            options.qualifying_hardware,
            title,
        )
        _verify_report_inputs_unchanged(
            options.captures,
            capture_sha256s,
            bundle_fingerprints,
        )
        if options.output:
            _write_text_atomic(options.output, report)
        else:
            sys.stdout.write(report)
        return 0
    except (OSError, ReportError) as error:
        sys.stderr.write(f"performance-report: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
