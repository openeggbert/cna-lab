#!/usr/bin/env python3
"""Compare compatible Iron Gang schema-8 captures with explicit regression tolerances."""

from __future__ import annotations

import argparse
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from performance_report import (
    DIAGNOSTIC_HARDWARE_TERMS,
    ReportError,
    VramBundleFingerprint,
    _boolean,
    _escape,
    _file_sha256,
    _integer,
    _markdown_code,
    _number,
    _path,
    _require_distinct_bundle_sources,
    _require_unchanged,
    _same_file,
    _single_line_text,
    _write_text_atomic,
    _verify_report_inputs_unchanged,
    load_capture,
    swap_interval_acknowledged,
    validate_complete_vram_evidence,
)


@dataclass(frozen=True)
class MetricSpec:
    name: str
    path: tuple[str, ...]
    absolute_tolerance_name: str
    unit: str
    sample_metric: str | None = None
    scale: float = 1.0


@dataclass(frozen=True)
class MetricResult:
    spec: MetricSpec
    baseline: float
    candidate: float
    delta: float
    allowed_increase: float
    regressed: bool


METRICS = (
    MetricSpec("Frame interval p95", ("measurements", "frame_interval", "p95_ms"), "frame_ms", "ms"),
    MetricSpec("Update CPU p95", ("measurements", "update_cpu", "p95_ms"), "cpu_ms", "ms"),
    MetricSpec("Physics CPU p95", ("measurements", "physics_cpu", "p95_ms"), "cpu_ms", "ms"),
    MetricSpec("AI CPU p95", ("measurements", "ai_cpu", "p95_ms"), "cpu_ms", "ms"),
    MetricSpec("Audio CPU p95", ("measurements", "audio_cpu", "p95_ms"), "cpu_ms", "ms"),
    MetricSpec("Render CPU p95", ("measurements", "render_cpu", "p95_ms"), "cpu_ms", "ms"),
    MetricSpec("Present CPU p95", ("measurements", "present_cpu", "p95_ms"), "frame_ms", "ms"),
    MetricSpec(
        "GPU Draw-range p95",
        ("measurements", "gpu_render", "p95_ms"),
        "frame_ms",
        "ms",
        sample_metric="gpu_render",
    ),
    MetricSpec(
        "District load p95",
        ("measurements", "district_load_cpu", "p95_ms"),
        "district_ms",
        "ms",
        sample_metric="district_load_cpu",
    ),
    MetricSpec(
        "Minimum-budget misses",
        ("frame_pacing", "minimum_budget_misses", "percent"),
        "rate_percent",
        "%",
    ),
    MetricSpec("Hitches", ("frame_pacing", "hitches", "percent"), "rate_percent", "%"),
    MetricSpec(
        "Severe hitches",
        ("frame_pacing", "severe_hitches", "percent"),
        "rate_percent",
        "%",
    ),
    MetricSpec(
        "Peak resident RAM",
        ("memory", "peak_resident_bytes"),
        "memory_mib",
        "MiB",
        scale=1.0 / (1024.0 * 1024.0),
    ),
    MetricSpec(
        "Tracked video memory",
        ("video_memory", "tracked_bytes"),
        "memory_mib",
        "MiB",
        scale=1.0 / (1024.0 * 1024.0),
    ),
    MetricSpec(
        "District-transition frame",
        ("frame_pacing", "district_transition_boundaries", "maximum_ms"),
        "frame_ms",
        "ms",
    ),
)


COMPATIBILITY_PATHS = (
    ("backend",),
    ("build_configuration",),
    ("scenario",),
    ("resolution", "width"),
    ("resolution", "height"),
    ("timing", "vertical_sync_requested"),
    ("timing", "fixed_timestep"),
    ("timing", "target_frame_ms"),
    ("budgets", "minimum_frame_p95_ms"),
    ("budgets", "recommended_frame_p95_ms"),
    ("budgets", "update_cpu_p95_ms"),
    ("budgets", "physics_cpu_p95_ms"),
    ("budgets", "ai_cpu_p95_ms"),
    ("budgets", "audio_cpu_p95_ms"),
    ("budgets", "render_cpu_p95_ms"),
    ("budgets", "district_load_p95_ms"),
    ("budgets", "ram_bytes"),
    ("budgets", "vram_bytes"),
    ("swap_interval", "requested"),
    ("swap_interval", "apply_result_known"),
    ("swap_interval", "apply_succeeded"),
    ("swap_interval", "applied"),
    ("gpu_timing", "supported"),
    ("gpu_timing", "scope"),
    ("frame_pacing", "minimum_budget_misses", "threshold_ms"),
    ("frame_pacing", "minimum_budget_misses", "comparison"),
    ("frame_pacing", "hitches", "threshold_ms"),
    ("frame_pacing", "hitches", "comparison"),
    ("frame_pacing", "severe_hitches", "threshold_ms"),
    ("frame_pacing", "severe_hitches", "comparison"),
    ("memory", "known"),
    ("video_memory", "tracking_complete"),
    ("video_memory", "coverage"),
)


WORKLOAD_PATHS = (
    ("Physics bodies", ("workload", "physics_bodies")),
    ("Traffic vehicles", ("workload", "traffic_vehicles")),
    ("Pedestrians", ("workload", "pedestrians")),
    ("Police vehicles", ("workload", "police_vehicles")),
)


def _format_path(path: tuple[str, ...]) -> str:
    return ".".join(path)


def require_compatible(
    baseline: dict[str, Any],
    candidate: dict[str, Any],
    baseline_hardware: str,
    candidate_hardware: str,
    baseline_kind: str,
    candidate_kind: str,
) -> None:
    if not baseline_hardware.strip() or not candidate_hardware.strip():
        raise ReportError("both hardware identities must be non-empty")
    if baseline_hardware.strip() != candidate_hardware.strip():
        raise ReportError("hardware identities differ; cross-hardware comparison is refused")
    if baseline_kind != candidate_kind:
        raise ReportError("capture kinds differ; diagnostic-vs-qualifying comparison is refused")
    for capture in (baseline, candidate):
        validate_complete_vram_evidence(capture, baseline_hardware.strip())
    if baseline_kind == "qualifying":
        lowered = baseline_hardware.lower()
        if any(term in lowered for term in DIAGNOSTIC_HARDWARE_TERMS):
            raise ReportError("virtual/software display cannot be labelled qualifying")
        for label, capture in (("baseline", baseline), ("candidate", candidate)):
            if not swap_interval_acknowledged(capture):
                raise ReportError(f"{label} qualifying capture lacks acknowledged presentation")
            if not _boolean(capture, "memory", "known"):
                raise ReportError(f"{label} qualifying capture has unknown RAM")
            if not _boolean(capture, "video_memory", "tracking_complete"):
                raise ReportError(f"{label} qualifying capture has incomplete VRAM tracking")

    for path in COMPATIBILITY_PATHS:
        baseline_value = _path(baseline, *path)
        candidate_value = _path(candidate, *path)
        if baseline_value != candidate_value:
            raise ReportError(
                f"incompatible {_format_path(path)}: baseline={baseline_value!r}, "
                f"candidate={candidate_value!r}"
            )

    for metric in ("gpu_render", "district_load_cpu"):
        baseline_available = _integer(baseline, "measurements", metric, "samples") > 0
        candidate_available = _integer(candidate, "measurements", metric, "samples") > 0
        if baseline_available != candidate_available:
            raise ReportError(f"incompatible {metric} sample availability")

    if _boolean(baseline, "video_memory", "tracking_complete"):
        for path in (
            ("video_memory", "complete_evidence", "source"),
            ("video_memory", "complete_evidence", "measurement_scope"),
            ("video_memory", "complete_evidence", "tool", "name"),
            ("video_memory", "complete_evidence", "tool", "version"),
        ):
            if _path(baseline, *path) != _path(candidate, *path):
                raise ReportError(f"incompatible {_format_path(path)}")

    baseline_boundary = _path(
        baseline, "frame_pacing", "district_transition_boundaries", "maximum_ms"
    )
    candidate_boundary = _path(
        candidate, "frame_pacing", "district_transition_boundaries", "maximum_ms"
    )
    if (baseline_boundary is None) != (candidate_boundary is None):
        raise ReportError("incompatible district-transition boundary availability")


def verify_vram_bundle(
    role: str,
    capture_path: Path,
    capture_sha256: str,
    bundle: list[Path],
) -> VramBundleFingerprint:
    fingerprint: VramBundleFingerprint = (
        (bundle[0], _file_sha256(bundle[0])),
        (bundle[1], _file_sha256(bundle[1])),
        (bundle[2], _file_sha256(bundle[2])),
    )
    verifier = Path(__file__).resolve().with_name("vram_evidence.py")
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
        raise ReportError(f"{role} VRAM bundle verification failed: {detail}")
    _require_unchanged(capture_path, capture_sha256, f"{role} enriched capture")
    for label, (path, expected_sha256) in zip(
        ("original capture", "evidence manifest", "raw evidence artifact"),
        fingerprint,
    ):
        _require_unchanged(path, expected_sha256, f"{role} {label}")
    return fingerprint


def compare_metrics(
    baseline: dict[str, Any],
    candidate: dict[str, Any],
    relative_tolerance_percent: float,
    tolerances: dict[str, float],
) -> list[MetricResult]:
    results: list[MetricResult] = []
    for spec in METRICS:
        if spec.sample_metric is not None and _integer(
            baseline, "measurements", spec.sample_metric, "samples"
        ) == 0:
            continue
        raw_baseline = _path(baseline, *spec.path)
        raw_candidate = _path(candidate, *spec.path)
        if raw_baseline is None and raw_candidate is None:
            continue
        baseline_value = _number(baseline, *spec.path) * spec.scale
        candidate_value = _number(candidate, *spec.path) * spec.scale
        absolute_tolerance = tolerances[spec.absolute_tolerance_name]
        relative_tolerance = baseline_value * relative_tolerance_percent / 100.0
        allowed_increase = max(absolute_tolerance, relative_tolerance)
        delta = candidate_value - baseline_value
        results.append(
            MetricResult(
                spec,
                baseline_value,
                candidate_value,
                delta,
                allowed_increase,
                delta > allowed_increase and not math.isclose(delta, allowed_increase),
            )
        )
    return results


def build_markdown(
    baseline_path: Path,
    candidate_path: Path,
    baseline_sha256: str,
    candidate_sha256: str,
    baseline_bundle: VramBundleFingerprint | None,
    candidate_bundle: VramBundleFingerprint | None,
    baseline: dict[str, Any],
    candidate: dict[str, Any],
    hardware: str,
    kind: str,
    relative_tolerance_percent: float,
    tolerances: dict[str, float],
    results: list[MetricResult],
    title: str,
) -> str:
    regressions = sum(result.regressed for result in results)
    bundle_cells: list[tuple[str, str, str]] = []
    for bundle in (baseline_bundle, candidate_bundle):
        if bundle is None:
            bundle_cells.append(("—", "—", "—"))
        else:
            bundle_cells.append(
                tuple(
                    f"{_markdown_code(path.name)}<br>{_markdown_code(sha256)}"
                    for path, sha256 in bundle
                )
            )
    lines = [
        f"# {_escape(title)}",
        "",
        f"- Baseline: {_markdown_code(baseline_path.name)}",
        f"- Candidate: {_markdown_code(candidate_path.name)}",
        f"- Hardware: {_markdown_code(hardware)}",
        f"- Capture kind: `{kind}`",
        f"- Scenario: {_markdown_code(_path(candidate, 'scenario'))}",
        f"- Overall result: **{'REGRESSION' if regressions else 'NO REGRESSION'}**",
        "",
        "A metric regresses only when its increase is greater than both the absolute tolerance "
        "and the relative tolerance from the baseline.",
        "",
        "## Evidence provenance",
        "",
        "| Role | Capture | SHA-256 | Original profile | Evidence manifest | Raw profiler artifact |",
        "| --- | --- | --- | --- | --- | --- |",
        f"| Baseline | {_markdown_code(baseline_path.name)} | {_markdown_code(baseline_sha256)} | {bundle_cells[0][0]} | {bundle_cells[0][1]} | {bundle_cells[0][2]} |",
        f"| Candidate | {_markdown_code(candidate_path.name)} | {_markdown_code(candidate_sha256)} | {bundle_cells[1][0]} | {bundle_cells[1][1]} | {bundle_cells[1][2]} |",
        "",
        "## Tolerances",
        "",
        f"- Relative: `{relative_tolerance_percent:.3f}%`",
        f"- Frame/GPU/Present/transition frame: `{tolerances['frame_ms']:.3f} ms`",
        f"- CPU subsystem: `{tolerances['cpu_ms']:.3f} ms`",
        f"- District load: `{tolerances['district_ms']:.3f} ms`",
        f"- RAM/VRAM: `{tolerances['memory_mib']:.3f} MiB`",
        f"- Frame miss/hitch rate: `{tolerances['rate_percent']:.3f} percentage points`",
        "",
        "## Metric comparison",
        "",
        "| Metric | Baseline | Candidate | Delta | Allowed increase | Result |",
        "| --- | ---: | ---: | ---: | ---: | :---: |",
    ]
    for result in results:
        lines.append(
            f"| {_escape(result.spec.name)} | {result.baseline:.3f} {result.spec.unit} | "
            f"{result.candidate:.3f} {result.spec.unit} | {result.delta:+.3f} {result.spec.unit} | "
            f"{result.allowed_increase:.3f} {result.spec.unit} | "
            f"{'REGRESSION' if result.regressed else 'pass'} |"
        )

    lines.extend(
        [
            "",
            "## Workload context",
            "",
            "Workload changes are reported but do not independently fail the comparison.",
            "",
            "| Workload | Baseline | Candidate |",
            "| --- | ---: | ---: |",
        ]
    )
    for label, path in WORKLOAD_PATHS:
        lines.append(f"| {label} | {_path(baseline, *path)} | {_path(candidate, *path)} |")
    lines.append("")
    return "\n".join(lines)


def non_negative_float(value: str) -> float:
    try:
        converted = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be numeric") from error
    if not math.isfinite(converted) or converted < 0.0:
        raise argparse.ArgumentTypeError("must be finite and non-negative")
    return converted


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--baseline-hardware", required=True)
    parser.add_argument("--candidate-hardware", required=True)
    parser.add_argument("--baseline-kind", required=True, choices=("diagnostic", "qualifying"))
    parser.add_argument("--candidate-kind", required=True, choices=("diagnostic", "qualifying"))
    parser.add_argument(
        "--baseline-vram-bundle",
        nargs=3,
        type=Path,
        metavar=("ORIGINAL", "EVIDENCE", "ARTIFACT"),
    )
    parser.add_argument(
        "--candidate-vram-bundle",
        nargs=3,
        type=Path,
        metavar=("ORIGINAL", "EVIDENCE", "ARTIFACT"),
    )
    parser.add_argument("--relative-tolerance-percent", type=non_negative_float, default=10.0)
    parser.add_argument("--frame-absolute-ms", type=non_negative_float, default=0.5)
    parser.add_argument("--cpu-absolute-ms", type=non_negative_float, default=0.1)
    parser.add_argument("--district-absolute-ms", type=non_negative_float, default=1.0)
    parser.add_argument("--memory-absolute-mib", type=non_negative_float, default=8.0)
    parser.add_argument("--rate-absolute-percent", type=non_negative_float, default=0.25)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--title", default="Iron Gang M12 performance regression comparison")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_args(sys.argv[1:] if arguments is None else arguments)
    try:
        baseline_hardware = _single_line_text(
            options.baseline_hardware, "baseline hardware identity"
        )
        candidate_hardware = _single_line_text(
            options.candidate_hardware, "candidate hardware identity"
        )
        title = _single_line_text(options.title, "comparison title")
        if options.baseline_kind != options.candidate_kind:
            raise ReportError("capture kinds differ; diagnostic-vs-qualifying comparison is refused")
        if options.baseline_kind == "qualifying" and options.baseline_vram_bundle is None:
            raise ReportError("qualifying baseline requires --baseline-vram-bundle")
        if options.candidate_kind == "qualifying" and options.candidate_vram_bundle is None:
            raise ReportError("qualifying candidate requires --candidate-vram-bundle")
        if options.baseline_kind == "qualifying":
            assert options.baseline_vram_bundle is not None
            assert options.candidate_vram_bundle is not None
            _require_distinct_bundle_sources(
                [options.baseline_vram_bundle, options.candidate_vram_bundle],
                "qualifying comparison",
            )
        if options.output is not None:
            protected_inputs = [
                ("baseline", options.baseline),
                ("candidate", options.candidate),
            ]
            for role, bundle in (
                ("baseline", options.baseline_vram_bundle),
                ("candidate", options.candidate_vram_bundle),
            ):
                if bundle is not None:
                    protected_inputs.extend((f"{role} VRAM source", path) for path in bundle)
            for input_label, input_path in protected_inputs:
                if _same_file(options.output, input_path):
                    raise ReportError(f"output must differ from the {input_label} input")
        baseline_sha256 = _file_sha256(options.baseline)
        candidate_sha256 = _file_sha256(options.candidate)
        baseline_bundle = (
            verify_vram_bundle(
                "baseline",
                options.baseline,
                baseline_sha256,
                options.baseline_vram_bundle,
            )
            if options.baseline_vram_bundle is not None
            else None
        )
        candidate_bundle = (
            verify_vram_bundle(
                "candidate",
                options.candidate,
                candidate_sha256,
                options.candidate_vram_bundle,
            )
            if options.candidate_vram_bundle is not None
            else None
        )
        baseline = load_capture(options.baseline)
        candidate = load_capture(options.candidate)
        _verify_report_inputs_unchanged(
            [options.baseline, options.candidate],
            [baseline_sha256, candidate_sha256],
            [baseline_bundle, candidate_bundle],
        )
        require_compatible(
            baseline,
            candidate,
            baseline_hardware,
            candidate_hardware,
            options.baseline_kind,
            options.candidate_kind,
        )
        tolerances = {
            "frame_ms": options.frame_absolute_ms,
            "cpu_ms": options.cpu_absolute_ms,
            "district_ms": options.district_absolute_ms,
            "memory_mib": options.memory_absolute_mib,
            "rate_percent": options.rate_absolute_percent,
        }
        results = compare_metrics(
            baseline,
            candidate,
            options.relative_tolerance_percent,
            tolerances,
        )
        report = build_markdown(
            options.baseline,
            options.candidate,
            baseline_sha256,
            candidate_sha256,
            baseline_bundle,
            candidate_bundle,
            baseline,
            candidate,
            candidate_hardware,
            options.candidate_kind,
            options.relative_tolerance_percent,
            tolerances,
            results,
            title,
        )
        _verify_report_inputs_unchanged(
            [options.baseline, options.candidate],
            [baseline_sha256, candidate_sha256],
            [baseline_bundle, candidate_bundle],
        )
        if options.output:
            _write_text_atomic(options.output, report)
        else:
            sys.stdout.write(report)
        return 1 if any(result.regressed for result in results) else 0
    except (OSError, ReportError) as error:
        sys.stderr.write(f"performance-compare: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
