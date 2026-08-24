#!/usr/bin/env python3
"""Generate a concise M12 release summary from Iron Gang schema-8 captures."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


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
CPU_BUDGETS_MS = {
    "update_cpu": 8.0,
    "physics_cpu": 3.0,
    "ai_cpu": 2.0,
    "audio_cpu": 1.0,
    "render_cpu": 8.0,
}
DIAGNOSTIC_HARDWARE_TERMS = ("xvfb", "llvmpipe", "software rasterizer")
COMPLETE_VRAM_SCOPE = "complete_process_gpu_residency_peak"
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
HISTOGRAM_BUCKETS = (
    "at_or_below_recommended_budget",
    "above_recommended_at_or_below_minimum_budget",
    "above_minimum_at_or_below_hitch",
    "above_hitch_at_or_below_severe_hitch",
    "above_severe_hitch",
)


class ReportError(ValueError):
    pass


def _mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ReportError(f"{label} must be a JSON object")
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


def _utc_timestamp(value: dict[str, Any], *keys: str) -> datetime:
    text = _non_empty_string(value, *keys)
    if not text.endswith("Z"):
        raise ReportError(f"{'.'.join(keys)} must be an ISO-8601 UTC timestamp ending in Z")
    try:
        return datetime.fromisoformat(text[:-1] + "+00:00")
    except ValueError as error:
        raise ReportError(f"{'.'.join(keys)} must be an ISO-8601 UTC timestamp") from error


def validate_complete_vram_evidence(
    capture: dict[str, Any],
    hardware: str | None = None,
) -> None:
    if not _boolean(capture, "video_memory", "tracking_complete"):
        return

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
    evidence_hardware = _non_empty_string(evidence, "hardware_identity")
    _non_empty_string(evidence, "tool", "name")
    _non_empty_string(evidence, "tool", "version")
    _non_empty_string(evidence, "process", "executable")
    if _integer(evidence, "process", "pid") == 0:
        raise ReportError("video_memory.complete_evidence.process.pid must be positive")
    started = _utc_timestamp(evidence, "measurement", "started_utc")
    ended = _utc_timestamp(evidence, "measurement", "ended_utc")
    if ended < started:
        raise ReportError(
            "video_memory.complete_evidence.measurement.ended_utc must not precede started_utc"
        )
    peak_resident_bytes = _integer(evidence, "measurement", "peak_resident_bytes")
    if peak_resident_bytes == 0:
        raise ReportError(
            "video_memory.complete_evidence.measurement.peak_resident_bytes must be positive"
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


def load_capture(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as source:
            capture = _mapping(json.load(source), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"could not read {path}: {error}") from error

    if capture.get("schema_version") != SCHEMA_VERSION:
        raise ReportError(
            f"{path}: schema_version must be {SCHEMA_VERSION}, got {capture.get('schema_version')!r}"
        )

    frame_samples = _integer(capture, "measurements", "frame_interval", "samples")
    pacing_samples = _integer(capture, "frame_pacing", "samples")
    histogram = _mapping(_path(capture, "frame_pacing", "histogram"), "frame_pacing.histogram")
    histogram_total = 0
    for bucket in HISTOGRAM_BUCKETS:
        histogram_total += _integer(histogram, bucket, "count")
    if pacing_samples != frame_samples or histogram_total != frame_samples:
        raise ReportError(
            f"{path}: frame-pacing samples/histogram ({pacing_samples}/{histogram_total}) "
            f"do not match frame_interval samples ({frame_samples})"
        )

    for metric in (
        "frame_interval",
        *CPU_BUDGETS_MS,
        "present_cpu",
        "gpu_render",
        "district_load_cpu",
    ):
        _integer(capture, "measurements", metric, "samples")
        _number(capture, "measurements", metric, "p95_ms")
    _number(capture, "memory", "peak_resident_bytes")
    _number(capture, "video_memory", "tracked_bytes")
    validate_complete_vram_evidence(capture)
    _integer(capture, "frame_pacing", "hitches", "count")
    _integer(capture, "frame_pacing", "severe_hitches", "count")
    return capture


def capture_blockers(path: Path, capture: dict[str, Any], hardware: str) -> list[str]:
    prefix = f"{path.name}: "
    blockers: list[str] = []
    if _path(capture, "backend") != "OPENGLES3":
        blockers.append(prefix + "backend is not the primary EasyGL/OPENGLES3 target")
    if _path(capture, "build_configuration") != "Release":
        blockers.append(prefix + "capture is not a Release build")

    width = _integer(capture, "resolution", "width")
    height = _integer(capture, "resolution", "height")
    if width < MINIMUM_WIDTH or height < MINIMUM_HEIGHT:
        blockers.append(prefix + f"resolution {width}x{height} is below 1280x720")

    swap_known = _boolean(capture, "swap_interval", "apply_result_known")
    swap_succeeded = _path(capture, "swap_interval", "apply_succeeded")
    swap_applied = _path(capture, "swap_interval", "applied")
    if (
        not swap_known
        or swap_succeeded is not True
        or isinstance(swap_applied, bool)
        or not isinstance(swap_applied, int)
    ):
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


def _mib(byte_count: float) -> str:
    return f"{byte_count / (1024.0 * 1024.0):.1f}"


def _escape(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


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
        f"# {title}",
        "",
        f"- Hardware: `{_escape(hardware)}`",
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
        swap_ack = (
            _boolean(capture, "swap_interval", "apply_result_known")
            and _path(capture, "swap_interval", "apply_succeeded") is True
            and not isinstance(_path(capture, "swap_interval", "applied"), bool)
            and isinstance(_path(capture, "swap_interval", "applied"), int)
        )
        recommended = (
            width >= RECOMMENDED_WIDTH
            and height >= RECOMMENDED_HEIGHT
            and frame_p95 <= RECOMMENDED_FRAME_MS
        )
        lines.append(
            f"| `{_escape(path.name)}` | {_escape(_path(capture, 'scenario'))} | {width}x{height} | "
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
    parser.add_argument("--output", type=Path, help="write Markdown here instead of stdout")
    parser.add_argument("--title", default="Iron Gang M12 performance release report")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_args(sys.argv[1:] if arguments is None else arguments)
    try:
        captures = [load_capture(path) for path in options.captures]
        report = build_markdown(
            options.captures,
            captures,
            options.hardware,
            options.qualifying_hardware,
            options.title,
        )
        if options.output:
            options.output.parent.mkdir(parents=True, exist_ok=True)
            options.output.write_text(report, encoding="utf-8")
        else:
            sys.stdout.write(report)
        return 0
    except (OSError, ReportError) as error:
        sys.stderr.write(f"performance-report: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
