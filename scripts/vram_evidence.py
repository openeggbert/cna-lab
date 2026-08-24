#!/usr/bin/env python3
"""Bind or verify complete external VRAM evidence for an Iron Gang schema-8 capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

from performance_report import (
    COMPLETE_VRAM_COVERAGE,
    COMPLETE_VRAM_SCOPE,
    ReportError,
    _integer,
    _mapping,
    _non_empty_string,
    _same_file,
    _single_line_string,
    _strict_json_load,
    load_capture,
    validate_complete_vram_evidence,
    validate_external_vram_measurement,
    validate_memory_summary,
)


EVIDENCE_SCHEMA_VERSION = 1
VRAM_BUDGET_BYTES = 512 * 1024 * 1024
def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_evidence(
    path: Path,
    capture_sha256: str,
    artifact_path: Path,
    artifact_sha256: str,
) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as source:
            evidence = _mapping(_strict_json_load(source), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"could not read {path}: {error}") from error

    if _integer(evidence, "schema_version") != EVIDENCE_SCHEMA_VERSION:
        raise ReportError(f"evidence schema_version must be {EVIDENCE_SCHEMA_VERSION}")
    if _non_empty_string(evidence, "measurement_scope") != COMPLETE_VRAM_SCOPE:
        raise ReportError(f"measurement_scope must be {COMPLETE_VRAM_SCOPE}")
    _single_line_string(evidence, "hardware_identity")
    _single_line_string(evidence, "tool", "name")
    _single_line_string(evidence, "tool", "version")
    validate_external_vram_measurement(evidence, "evidence")
    bound_digest = _non_empty_string(evidence, "profile_capture_sha256")
    if bound_digest != capture_sha256:
        raise ReportError("profile_capture_sha256 does not match the input capture")
    artifact_name = _non_empty_string(evidence, "source_artifact", "file_name")
    if artifact_name != artifact_path.name:
        raise ReportError("source_artifact.file_name does not match --artifact")
    bound_artifact_digest = _non_empty_string(evidence, "source_artifact", "sha256")
    if bound_artifact_digest != artifact_sha256:
        raise ReportError("source_artifact.sha256 does not match --artifact")
    return evidence


def enrich_capture(
    capture: dict[str, Any],
    evidence: dict[str, Any],
    capture_sha256: str,
    evidence_manifest_sha256: str,
) -> dict[str, Any]:
    if capture["video_memory"]["tracking_complete"] is not False:
        raise ReportError("input capture already claims complete VRAM tracking")
    logical_tracked_bytes = _integer(capture, "video_memory", "tracked_bytes")
    peak_resident_bytes = _integer(evidence, "measurement", "peak_resident_bytes")
    tracked_bytes = max(logical_tracked_bytes, peak_resident_bytes)
    video_memory = capture["video_memory"]
    video_memory["logical_tracked_bytes"] = logical_tracked_bytes
    video_memory["tracked_bytes"] = tracked_bytes
    video_memory["tracking_complete"] = True
    video_memory["tracked_budget_pass"] = tracked_bytes <= VRAM_BUDGET_BYTES
    video_memory["coverage"] = COMPLETE_VRAM_COVERAGE
    video_memory["complete_evidence"] = {
        "schema_version": EVIDENCE_SCHEMA_VERSION,
        "source": "external_capture",
        "measurement_scope": evidence["measurement_scope"],
        "hardware_identity": evidence["hardware_identity"],
        "tool": evidence["tool"],
        "process": evidence["process"],
        "measurement": evidence["measurement"],
        "profile_capture_sha256": capture_sha256,
        "source_artifact": evidence["source_artifact"],
        "evidence_manifest_sha256": evidence_manifest_sha256,
    }
    validate_memory_summary(capture)
    validate_complete_vram_evidence(capture)
    return capture


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, type=Path, help="original schema-8 profile JSON")
    parser.add_argument("--evidence", required=True, type=Path, help="external evidence JSON")
    parser.add_argument("--artifact", required=True, type=Path, help="raw vendor/tool capture artifact")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--output", type=Path, help="bind and write enriched profile JSON here")
    action.add_argument(
        "--verify-enriched",
        type=Path,
        metavar="CAPTURE",
        help="verify an archived enriched profile against all three source files",
    )
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_args(sys.argv[1:] if arguments is None else arguments)
    temporary_path: Path | None = None
    try:
        input_paths = (
            ("original capture", options.capture),
            ("evidence manifest", options.evidence),
            ("raw evidence artifact", options.artifact),
        )
        for index, (left_label, left_path) in enumerate(input_paths):
            for right_label, right_path in input_paths[index + 1 :]:
                if _same_file(left_path, right_path):
                    raise ReportError(f"{left_label} and {right_label} must be distinct files")
        if not options.artifact.is_file() or options.artifact.stat().st_size == 0:
            raise ReportError("raw evidence artifact must be a non-empty regular file")
        if options.output is not None:
            for input_label, input_path in input_paths:
                if _same_file(options.output, input_path):
                    raise ReportError(f"output must differ from the {input_label}")
        capture_sha256 = file_sha256(options.capture)
        evidence_manifest_sha256 = file_sha256(options.evidence)
        artifact_sha256 = file_sha256(options.artifact)
        capture = load_capture(options.capture)
        evidence = load_evidence(
            options.evidence,
            capture_sha256,
            options.artifact,
            artifact_sha256,
        )
        enriched = enrich_capture(
            capture,
            evidence,
            capture_sha256,
            evidence_manifest_sha256,
        )
        stable_inputs = (
            ("original capture", options.capture, capture_sha256),
            ("evidence manifest", options.evidence, evidence_manifest_sha256),
            ("raw evidence artifact", options.artifact, artifact_sha256),
        )
        for input_label, input_path, expected_sha256 in stable_inputs:
            if file_sha256(input_path) != expected_sha256:
                raise ReportError(f"{input_label} changed while evidence was being processed")

        if options.verify_enriched is not None:
            enriched_sha256 = file_sha256(options.verify_enriched)
            archived_enriched = load_capture(options.verify_enriched)
            if file_sha256(options.verify_enriched) != enriched_sha256:
                raise ReportError("enriched capture changed while evidence was being verified")
            if archived_enriched != enriched:
                raise ReportError(
                    "enriched capture does not exactly match the original capture and bound evidence"
                )
            sys.stdout.write(f"VRAM EVIDENCE VERIFIED: {options.verify_enriched}\n")
            return 0

        assert options.output is not None
        options.output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=options.output.parent,
            prefix=options.output.name + ".",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            json.dump(enriched, temporary, indent=2)
            temporary.write("\n")
            temporary_path = Path(temporary.name)
        os.replace(temporary_path, options.output)
        temporary_path = None
        return 0
    except (OSError, ReportError) as error:
        sys.stderr.write(f"vram-evidence: {error}\n")
        return 2
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass


if __name__ == "__main__":
    raise SystemExit(main())
