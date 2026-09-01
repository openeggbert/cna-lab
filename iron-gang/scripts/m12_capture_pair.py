#!/usr/bin/env python3
"""Capture, bind, report, and compare one complete Linux EasyGL M12 pair."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
DEFAULT_DRM_CAPTURE_TOOL = SCRIPT_DIRECTORY / "drm_vram_capture.py"
DEFAULT_BINDER_TOOL = SCRIPT_DIRECTORY / "vram_evidence.py"
DEFAULT_REPORT_TOOL = SCRIPT_DIRECTORY / "performance_report.py"
DEFAULT_COMPARE_TOOL = SCRIPT_DIRECTORY / "performance_compare.py"
PREFIX_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
REPORT_STATUS_PATTERN = re.compile(
    r"^- Overall status: \*\*(PASS|FAIL)\*\*$", re.MULTILINE
)


class PairCaptureError(RuntimeError):
    """A malformed request or failed capture stage."""


@dataclass(frozen=True)
class CaptureArtifacts:
    original: Path
    evidence: Path
    drm: Path
    complete: Path


@dataclass(frozen=True)
class PairArtifacts:
    first: CaptureArtifacts
    second: CaptureArtifacts
    report: Path
    comparison: Path

    def all_paths(self) -> tuple[Path, ...]:
        return (
            self.first.original,
            self.first.evidence,
            self.first.drm,
            self.first.complete,
            self.second.original,
            self.second.evidence,
            self.second.drm,
            self.second.complete,
            self.report,
            self.comparison,
        )


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--prefix", default="m12-physical")
    parser.add_argument("--hardware", required=True)
    parser.add_argument("--game", required=True, type=Path)
    parser.add_argument("--poll-ms", type=int, default=20)
    parser.add_argument("--vsync", choices=("on", "off"), default="off")
    parser.add_argument(
        "--report-title",
        default="Iron Gang M12 Linux EasyGL physical qualification",
    )
    parser.add_argument(
        "--drm-capture-tool",
        type=Path,
        default=DEFAULT_DRM_CAPTURE_TOOL,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--binder-tool",
        type=Path,
        default=DEFAULT_BINDER_TOOL,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--report-tool",
        type=Path,
        default=DEFAULT_REPORT_TOOL,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--compare-tool",
        type=Path,
        default=DEFAULT_COMPARE_TOOL,
        help=argparse.SUPPRESS,
    )
    return parser.parse_args(arguments)


def single_line(value: str, label: str) -> str:
    normalized = value.strip()
    if not normalized or not normalized.isprintable() or "\n" in normalized or "\r" in normalized:
        raise PairCaptureError(f"{label} must be a non-empty printable single line")
    return normalized


def output_exists(path: Path) -> bool:
    return path.exists() or path.is_symlink()


def artifact_paths(output_directory: Path, prefix: str) -> PairArtifacts:
    def capture(number: int) -> CaptureArtifacts:
        stem = f"{prefix}-{number:02d}"
        return CaptureArtifacts(
            original=output_directory / f"{stem}-original.json",
            evidence=output_directory / f"{stem}-evidence.json",
            drm=output_directory / f"{stem}-drm.json",
            complete=output_directory / f"{stem}-complete.json",
        )

    return PairArtifacts(
        first=capture(1),
        second=capture(2),
        report=output_directory / f"{prefix}-qualification.md",
        comparison=output_directory / f"{prefix}-comparison.md",
    )


def validate_options(options: argparse.Namespace) -> tuple[PairArtifacts, str, str]:
    if PREFIX_PATTERN.fullmatch(options.prefix) is None or ".." in options.prefix:
        raise PairCaptureError(
            "--prefix must be one safe filename component using letters, digits, '.', '_', or '-'"
        )
    if not 1 <= options.poll_ms <= 1000:
        raise PairCaptureError("--poll-ms must be between 1 and 1000")
    hardware = single_line(options.hardware, "hardware identity")
    report_title = single_line(options.report_title, "report title")

    game = options.game.resolve()
    if not game.is_file() or not os.access(game, os.X_OK):
        raise PairCaptureError(f"--game must name an executable regular file: {game}")
    options.game = game
    for label, tool in (
        ("DRM capture tool", options.drm_capture_tool),
        ("VRAM binder tool", options.binder_tool),
        ("report tool", options.report_tool),
        ("comparison tool", options.compare_tool),
    ):
        resolved = tool.resolve()
        if not resolved.is_file():
            raise PairCaptureError(f"{label} is missing: {resolved}")

    output_directory = options.output_dir.resolve()
    if output_exists(output_directory) and not output_directory.is_dir():
        raise PairCaptureError(f"--output-dir is not a directory: {output_directory}")
    output_directory.mkdir(parents=True, exist_ok=True)
    artifacts = artifact_paths(output_directory, options.prefix)
    for path in artifacts.all_paths():
        if output_exists(path):
            raise PairCaptureError(f"refusing to overwrite existing pair output: {path}")
    return artifacts, hardware, report_title


def run_stage(label: str, command: list[str], accepted: tuple[int, ...] = (0,)) -> int:
    print(f"[m12-pair] {label}", flush=True)
    try:
        result = subprocess.run(command, check=False)
    except OSError as error:
        raise PairCaptureError(f"{label} could not start: {error}") from error
    if result.returncode not in accepted:
        raise PairCaptureError(f"{label} exited with status {result.returncode}")
    return result.returncode


def python_tool(tool: Path, *arguments: object) -> list[str]:
    return [sys.executable, str(tool.resolve()), *(str(value) for value in arguments)]


def capture_and_bind(
    number: int,
    artifacts: CaptureArtifacts,
    options: argparse.Namespace,
    hardware: str,
) -> None:
    run_stage(
        f"capture {number}/2",
        python_tool(
            options.drm_capture_tool,
            "--capture",
            artifacts.original,
            "--evidence",
            artifacts.evidence,
            "--artifact",
            artifacts.drm,
            "--hardware",
            hardware,
            "--poll-ms",
            options.poll_ms,
            "--",
            options.game,
            "--smoke",
            900,
            "--profile",
            artifacts.original,
            "--profile-scenario",
            "mixed",
            "--vsync",
            options.vsync,
        ),
    )
    run_stage(
        f"bind capture {number}/2",
        python_tool(
            options.binder_tool,
            "--capture",
            artifacts.original,
            "--evidence",
            artifacts.evidence,
            "--artifact",
            artifacts.drm,
            "--output",
            artifacts.complete,
        ),
    )


def report_status(report_path: Path) -> str:
    try:
        contents = report_path.read_text(encoding="utf-8")
    except OSError as error:
        raise PairCaptureError(f"qualification report was not created: {error}") from error
    matches = REPORT_STATUS_PATTERN.findall(contents)
    if len(matches) != 1:
        raise PairCaptureError("qualification report has no unique PASS/FAIL status")
    return matches[0]


def run_pair(options: argparse.Namespace) -> int:
    artifacts, hardware, title = validate_options(options)
    capture_and_bind(1, artifacts.first, options, hardware)
    capture_and_bind(2, artifacts.second, options, hardware)

    run_stage(
        "generate qualification report",
        python_tool(
            options.report_tool,
            "--hardware",
            hardware,
            "--qualifying-hardware",
            "--vram-bundle",
            artifacts.first.original,
            artifacts.first.evidence,
            artifacts.first.drm,
            "--vram-bundle",
            artifacts.second.original,
            artifacts.second.evidence,
            artifacts.second.drm,
            "--output",
            artifacts.report,
            "--title",
            title,
            artifacts.first.complete,
            artifacts.second.complete,
        ),
    )
    status = report_status(artifacts.report)
    comparison_kind = "qualifying" if status == "PASS" else "diagnostic"
    comparison_result = run_stage(
        f"compare pair ({comparison_kind})",
        python_tool(
            options.compare_tool,
            "--baseline",
            artifacts.first.complete,
            "--candidate",
            artifacts.second.complete,
            "--baseline-hardware",
            hardware,
            "--candidate-hardware",
            hardware,
            "--baseline-kind",
            comparison_kind,
            "--candidate-kind",
            comparison_kind,
            "--baseline-vram-bundle",
            artifacts.first.original,
            artifacts.first.evidence,
            artifacts.first.drm,
            "--candidate-vram-bundle",
            artifacts.second.original,
            artifacts.second.evidence,
            artifacts.second.drm,
            "--output",
            artifacts.comparison,
            "--title",
            f"{title} repeatability",
        ),
        accepted=(0, 1),
    )

    print(f"[m12-pair] qualification: {status}")
    print(
        "[m12-pair] comparison: "
        + ("NO REGRESSION" if comparison_result == 0 else "REGRESSION")
    )
    print(f"[m12-pair] report: {artifacts.report}")
    print(f"[m12-pair] comparison report: {artifacts.comparison}")
    return 0 if status == "PASS" and comparison_result == 0 else 1


def main(arguments: list[str] | None = None) -> int:
    try:
        return run_pair(parse_args(sys.argv[1:] if arguments is None else arguments))
    except PairCaptureError as error:
        sys.stderr.write(f"m12-capture-pair: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
