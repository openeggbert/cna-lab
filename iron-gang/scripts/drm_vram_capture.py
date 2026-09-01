#!/usr/bin/env python3
"""Capture Linux DRM per-process resident GPU memory around an Iron Gang run."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from performance_report import (
    COMPLETE_VRAM_SCOPE,
    ReportError,
    _array,
    _canonical_single_line_string,
    _file_sha256,
    _integer,
    _iron_gang_executable_name,
    _mapping,
    _path,
    _same_file,
    _sha256_string,
    _single_line_string,
    _single_line_text,
    _strict_json_load,
    _utc_timestamp,
    _write_text_atomic,
    load_capture,
    validate_capture_session,
)


ARTIFACT_SCHEMA_VERSION = 1
EVIDENCE_SCHEMA_VERSION = 1
TOOL_NAME = "Iron Gang Linux DRM fdinfo sampler"
TOOL_VERSION = "1.0"
UINT64_MAX = (1 << 64) - 1
PCI_DEVICE_PATTERN = re.compile(r"[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]")
REGION_PATTERN = re.compile(r"[A-Za-z0-9_.-]+")
SIZE_PATTERN = re.compile(r"([0-9]+)(?:\s+(KiB|MiB))?")
ACCOUNTING_POLICY = {
    "source": "/proc/<pid>/fdinfo DRM client usage statistics",
    "resident_keys": "drm-resident-<region>; amdgpu drm-memory-<region> alias when needed",
    "client_deduplication": "drm-client-id globally, or drm-pdev plus drm-client-id",
    "multiple_descriptor_policy": "maximum per region for duplicate client descriptors",
    "multiple_client_policy": "sum all process DRM clients; shared cross-client buffers may be conservatively counted more than once",
    "region_policy": "sum every reported resident buffer-object region, including VRAM and GPU-accessible system memory",
}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace(
        "+00:00", "Z"
    )


def parse_size(value: str, key: str) -> int:
    match = SIZE_PATTERN.fullmatch(value.strip())
    if match is None:
        raise ReportError(f"{key} has an unsupported DRM memory size: {value!r}")
    amount = int(match.group(1))
    multiplier = {None: 1, "KiB": 1024, "MiB": 1024 * 1024}[match.group(2)]
    result = amount * multiplier
    if result > UINT64_MAX:
        raise ReportError(f"{key} exceeds the unsigned 64-bit byte range")
    return result


def parse_resident_fields(
    driver: str,
    fd_number: str,
    fields: dict[str, str],
) -> tuple[dict[str, int], dict[str, str]]:
    standard: dict[str, int] = {}
    legacy: dict[str, int] = {}
    source_fields: dict[str, str] = {}
    for key, value in fields.items():
        if not isinstance(key, str) or not isinstance(value, str):
            raise ReportError(f"fd {fd_number} DRM source fields must be strings")
        if key.startswith("drm-resident-"):
            region = key.removeprefix("drm-resident-")
            if REGION_PATTERN.fullmatch(region) is None:
                raise ReportError(f"fd {fd_number} has an invalid DRM resident region")
            standard[region] = parse_size(value, key)
            source_fields[key] = value
        elif key.startswith("drm-memory-"):
            if driver != "amdgpu":
                raise ReportError(
                    f"fd {fd_number} uses the deprecated drm-memory alias outside amdgpu"
                )
            region = key.removeprefix("drm-memory-")
            if REGION_PATTERN.fullmatch(region) is None:
                raise ReportError(f"fd {fd_number} has an invalid DRM memory region")
            legacy[region] = parse_size(value, key)
            source_fields[key] = value

    regions: dict[str, int] = {}
    for region in sorted(set(standard) | set(legacy)):
        if region in standard and region in legacy and standard[region] != legacy[region]:
            raise ReportError(
                f"fd {fd_number} has conflicting resident and deprecated-alias values "
                f"for DRM region {region}"
            )
        regions[region] = standard[region] if region in standard else legacy[region]
    if not regions:
        raise ReportError(f"fd {fd_number} DRM client exposes no resident-memory regions")
    return regions, source_fields


def parse_fdinfo(fd_number: str, contents: str) -> dict[str, Any] | None:
    fields: dict[str, str] = {}
    for line in contents.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        if key in fields:
            raise ReportError(f"fd {fd_number} repeats fdinfo key {key!r}")
        fields[key] = value.strip()

    drm_keys = [key for key in fields if key.startswith("drm-")]
    if not drm_keys:
        return None
    driver = fields.get("drm-driver", "")
    if not driver or not driver.isprintable() or any(character.isspace() for character in driver):
        raise ReportError(f"fd {fd_number} has no valid drm-driver")
    raw_client_id = fields.get("drm-client-id")
    if raw_client_id is None or not raw_client_id.isdecimal():
        raise ReportError(f"fd {fd_number} has no valid drm-client-id")
    client_id = int(raw_client_id)
    if client_id > UINT64_MAX:
        raise ReportError(f"fd {fd_number} drm-client-id exceeds unsigned 64-bit range")

    pdev = fields.get("drm-pdev")
    if pdev is not None:
        if PCI_DEVICE_PATTERN.fullmatch(pdev) is None:
            raise ReportError(f"fd {fd_number} has invalid drm-pdev {pdev!r}")
        pdev = pdev.lower()

    regions, source_fields = parse_resident_fields(driver, fd_number, fields)

    identity = f"{pdev}/{client_id}" if pdev is not None else f"global/{client_id}"
    return {
        "identity": identity,
        "driver": driver,
        "client_id": client_id,
        "pdev": pdev,
        "fd": int(fd_number),
        "resident_regions_bytes": regions,
        "source_fields": source_fields,
    }


def parse_fdinfo_snapshot(fdinfo_by_number: dict[str, str]) -> dict[str, Any] | None:
    grouped: dict[str, dict[str, Any]] = {}
    for fd_number in sorted(fdinfo_by_number, key=int):
        observation = parse_fdinfo(fd_number, fdinfo_by_number[fd_number])
        if observation is None:
            continue
        identity = observation["identity"]
        client = grouped.get(identity)
        if client is None:
            client = {
                "identity": identity,
                "driver": observation["driver"],
                "client_id": observation["client_id"],
                "pdev": observation["pdev"],
                "file_descriptors": [],
                "resident_regions_bytes": {},
                "observations": [],
            }
            grouped[identity] = client
        elif (
            client["driver"] != observation["driver"]
            or client["client_id"] != observation["client_id"]
            or client["pdev"] != observation["pdev"]
        ):
            raise ReportError(f"DRM client identity {identity} is internally inconsistent")

        client["file_descriptors"].append(observation["fd"])
        client["observations"].append(
            {
                "fd": observation["fd"],
                "resident_regions_bytes": observation["resident_regions_bytes"],
                "source_fields": observation["source_fields"],
            }
        )
        for region, byte_count in observation["resident_regions_bytes"].items():
            client["resident_regions_bytes"][region] = max(
                client["resident_regions_bytes"].get(region, 0), byte_count
            )

    if not grouped:
        return None

    clients: list[dict[str, Any]] = []
    total = 0
    for identity in sorted(grouped):
        client = grouped[identity]
        client["resident_bytes"] = sum(client["resident_regions_bytes"].values())
        total += client["resident_bytes"]
        if total > UINT64_MAX:
            raise ReportError("summed DRM client residency exceeds unsigned 64-bit range")
        clients.append(client)
    return {"clients": clients, "resident_bytes": total}


def read_fdinfo_snapshot(pid: int) -> tuple[dict[str, Any] | None, int]:
    root = Path("/proc") / str(pid) / "fdinfo"
    try:
        entries = list(root.iterdir())
    except OSError:
        return None, 1
    fdinfo: dict[str, str] = {}
    read_errors = 0
    for entry in entries:
        if not entry.name.isdecimal():
            continue
        try:
            fdinfo[entry.name] = entry.read_text(encoding="utf-8")
        except OSError:
            read_errors += 1
    if read_errors:
        # A disappearing descriptor may be the one that owned the current peak. Keep the attempt
        # visible in the artifact counters, but never accept a partially read process snapshot.
        return None, read_errors
    return parse_fdinfo_snapshot(fdinfo), read_errors


def non_negative_integer(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ReportError(f"{label} must be a non-negative integer")
    return value


def region_bytes(value: Any, label: str) -> dict[str, int]:
    regions = _mapping(value, label)
    if not regions:
        raise ReportError(f"{label} must not be empty")
    result: dict[str, int] = {}
    for region, byte_count in regions.items():
        if not isinstance(region, str) or REGION_PATTERN.fullmatch(region) is None:
            raise ReportError(f"{label} has an invalid region name")
        result[region] = non_negative_integer(byte_count, f"{label}.{region}")
    return result


def load_and_validate_artifact(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as source:
            artifact = _mapping(_strict_json_load(source), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"could not read built-in DRM artifact {path}: {error}") from error

    if _integer(artifact, "schema_version") != ARTIFACT_SCHEMA_VERSION:
        raise ReportError(f"DRM artifact schema_version must be {ARTIFACT_SCHEMA_VERSION}")
    if _canonical_single_line_string(artifact, "measurement_scope") != COMPLETE_VRAM_SCOPE:
        raise ReportError(f"DRM artifact measurement_scope must be {COMPLETE_VRAM_SCOPE}")
    if _single_line_string(artifact, "tool", "name") != TOOL_NAME:
        raise ReportError(f"DRM artifact tool.name must be {TOOL_NAME}")
    if _single_line_string(artifact, "tool", "version") != TOOL_VERSION:
        raise ReportError(f"DRM artifact tool.version must be {TOOL_VERSION}")
    _single_line_string(artifact, "hardware_identity")
    _single_line_string(artifact, "kernel_release")
    _iron_gang_executable_name(
        artifact,
        "process",
        "executable",
        label="DRM artifact process.executable",
    )
    process_pid = _integer(artifact, "process", "pid")
    if process_pid == 0:
        raise ReportError("DRM artifact process.pid must be positive")
    command = _array(_path(artifact, "process", "command"), "DRM artifact process.command")
    if not command:
        raise ReportError("DRM artifact process.command must not be empty")
    for index, argument in enumerate(command):
        if not isinstance(argument, str) or not argument or not argument.isprintable():
            raise ReportError(
                f"DRM artifact process.command[{index}] must be a printable string"
            )

    started = _utc_timestamp(artifact, "measurement", "started_utc")
    ended = _utc_timestamp(artifact, "measurement", "ended_utc")
    if ended <= started:
        raise ReportError("DRM artifact measurement interval must be positive")
    peak = _integer(artifact, "measurement", "peak_resident_bytes")
    if peak == 0:
        raise ReportError("DRM artifact peak_resident_bytes must be positive")
    poll_ms = _integer(artifact, "measurement", "poll_interval_ms")
    if not 1 <= poll_ms <= 1000:
        raise ReportError("DRM artifact poll_interval_ms must be between 1 and 1000")
    attempts = _integer(artifact, "measurement", "sampling_attempts")
    successful = _integer(artifact, "measurement", "successful_samples")
    _integer(artifact, "measurement", "fdinfo_read_errors")
    if successful == 0 or attempts < successful:
        raise ReportError("DRM artifact sampling counts are inconsistent")
    if _mapping(_path(artifact, "accounting"), "DRM artifact accounting") != ACCOUNTING_POLICY:
        raise ReportError("DRM artifact accounting policy does not match the built-in sampler")
    _sha256_string(artifact, "profile_capture_sha256")

    samples = _array(_path(artifact, "samples"), "DRM artifact samples")
    if len(samples) != successful:
        raise ReportError("DRM artifact successful_samples does not match samples")
    derived_peak = 0
    previous_sampled = started
    for sample_index, raw_sample in enumerate(samples):
        label = f"DRM artifact samples[{sample_index}]"
        sample = _mapping(raw_sample, label)
        sampled = _utc_timestamp(sample, "sampled_utc")
        if sampled < started or sampled > ended or sampled < previous_sampled:
            raise ReportError(f"{label}.sampled_utc is outside or reverses the measurement interval")
        previous_sampled = sampled
        clients = _array(_path(sample, "clients"), f"{label}.clients")
        if not clients:
            raise ReportError(f"{label}.clients must not be empty")
        identities: set[str] = set()
        sample_total = 0
        for client_index, raw_client in enumerate(clients):
            client_label = f"{label}.clients[{client_index}]"
            client = _mapping(raw_client, client_label)
            identity = _canonical_single_line_string(client, "identity")
            driver = _canonical_single_line_string(client, "driver")
            client_id = _integer(client, "client_id")
            pdev = _path(client, "pdev")
            if pdev is not None:
                if not isinstance(pdev, str) or PCI_DEVICE_PATTERN.fullmatch(pdev) is None:
                    raise ReportError(f"{client_label}.pdev is invalid")
                pdev = pdev.lower()
            expected_identity = f"{pdev}/{client_id}" if pdev is not None else f"global/{client_id}"
            if identity != expected_identity or identity in identities:
                raise ReportError(f"{client_label}.identity is inconsistent or duplicated")
            identities.add(identity)

            descriptors = _array(
                _path(client, "file_descriptors"), f"{client_label}.file_descriptors"
            )
            descriptor_numbers = [
                non_negative_integer(value, f"{client_label}.file_descriptors")
                for value in descriptors
            ]
            if not descriptor_numbers or len(descriptor_numbers) != len(set(descriptor_numbers)):
                raise ReportError(f"{client_label}.file_descriptors is empty or duplicated")
            observations = _array(
                _path(client, "observations"), f"{client_label}.observations"
            )
            if len(observations) != len(descriptor_numbers):
                raise ReportError(f"{client_label}.observations does not match descriptors")
            derived_regions: dict[str, int] = {}
            observed_descriptors: list[int] = []
            for observation_index, raw_observation in enumerate(observations):
                observation_label = (
                    f"{client_label}.observations[{observation_index}]"
                )
                observation = _mapping(raw_observation, observation_label)
                fd_number = _integer(observation, "fd")
                source_fields = _mapping(
                    _path(observation, "source_fields"),
                    f"{observation_label}.source_fields",
                )
                parsed_regions, canonical_source_fields = parse_resident_fields(
                    driver, str(fd_number), source_fields
                )
                if canonical_source_fields != source_fields:
                    raise ReportError(f"{observation_label}.source_fields has unrelated keys")
                stored_regions = region_bytes(
                    _path(observation, "resident_regions_bytes"),
                    f"{observation_label}.resident_regions_bytes",
                )
                if stored_regions != parsed_regions:
                    raise ReportError(f"{observation_label} does not match its source fields")
                for region, byte_count in parsed_regions.items():
                    derived_regions[region] = max(derived_regions.get(region, 0), byte_count)
                observed_descriptors.append(fd_number)
            if observed_descriptors != descriptor_numbers:
                raise ReportError(f"{client_label}.observations are not in descriptor order")
            stored_client_regions = region_bytes(
                _path(client, "resident_regions_bytes"),
                f"{client_label}.resident_regions_bytes",
            )
            if stored_client_regions != derived_regions:
                raise ReportError(f"{client_label} resident regions do not match observations")
            client_total = _integer(client, "resident_bytes")
            if client_total != sum(derived_regions.values()):
                raise ReportError(f"{client_label}.resident_bytes is inconsistent")
            sample_total += client_total
        if _integer(sample, "resident_bytes") != sample_total:
            raise ReportError(f"{label}.resident_bytes is inconsistent")
        derived_peak = max(derived_peak, sample_total)
    if peak != derived_peak:
        raise ReportError("DRM artifact peak_resident_bytes does not match its samples")
    return artifact


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, type=Path, help="profile JSON written by the game")
    parser.add_argument("--evidence", required=True, type=Path, help="evidence manifest to create")
    parser.add_argument("--artifact", required=True, type=Path, help="raw DRM sample JSON to create")
    parser.add_argument("--hardware", required=True, help="physical CPU/GPU/driver/display identity")
    parser.add_argument("--poll-ms", type=int, default=20, help="DRM sampling interval (default: 20)")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="-- iron_gang command and arguments")
    options = parser.parse_args(arguments)
    if options.command and options.command[0] == "--":
        options.command = options.command[1:]
    return options


def serialize_json(value: dict[str, Any]) -> str:
    return json.dumps(value, indent=2, sort_keys=True) + "\n"


def run_capture(options: argparse.Namespace) -> None:
    if not options.command:
        raise ReportError("an iron_gang command is required after --")
    if not 1 <= options.poll_ms <= 1000:
        raise ReportError("--poll-ms must be between 1 and 1000")
    hardware = _single_line_text(options.hardware, "hardware identity")

    outputs = (
        ("profile capture", options.capture),
        ("evidence manifest", options.evidence),
        ("raw DRM artifact", options.artifact),
    )
    for index, (left_label, left_path) in enumerate(outputs):
        if left_path.exists():
            raise ReportError(f"{left_label} already exists: {left_path}")
        for right_label, right_path in outputs[index + 1 :]:
            if _same_file(left_path, right_path):
                raise ReportError(f"{left_label} and {right_label} must be distinct files")
    for _, output_path in outputs:
        output_path.parent.mkdir(parents=True, exist_ok=True)

    measurement_started = utc_now()
    process = subprocess.Popen(options.command)
    samples: list[dict[str, Any]] = []
    attempts = 0
    read_errors = 0
    try:
        while process.poll() is None:
            attempts += 1
            snapshot, snapshot_read_errors = read_fdinfo_snapshot(process.pid)
            read_errors += snapshot_read_errors
            if snapshot is not None:
                snapshot["sampled_utc"] = utc_now()
                samples.append(snapshot)
            time.sleep(options.poll_ms / 1000.0)
        return_code = process.wait()
    except BaseException:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        raise
    measurement_ended = utc_now()

    if return_code != 0:
        raise ReportError(f"profiled command exited with status {return_code}")
    if not samples:
        raise ReportError("the process exposed no DRM client resident-memory samples")
    peak_resident_bytes = max(sample["resident_bytes"] for sample in samples)
    if peak_resident_bytes == 0:
        raise ReportError("the process DRM resident-memory peak is zero")
    if not options.capture.is_file():
        raise ReportError(f"profiled command did not create {options.capture}")

    capture = load_capture(options.capture)
    session = validate_capture_session(capture, required=True)
    assert session is not None
    executable_name, capture_pid, capture_started, capture_ended = session
    if capture_pid != process.pid:
        raise ReportError(
            f"profile capture PID {capture_pid} does not match profiled child PID {process.pid}"
        )
    measured_started = datetime.fromisoformat(measurement_started[:-1] + "+00:00")
    measured_ended = datetime.fromisoformat(measurement_ended[:-1] + "+00:00")
    if measured_started > capture_started or measured_ended < capture_ended:
        raise ReportError("DRM measurement interval does not enclose the capture session")

    capture_sha256 = _file_sha256(options.capture)
    artifact = {
        "schema_version": ARTIFACT_SCHEMA_VERSION,
        "measurement_scope": COMPLETE_VRAM_SCOPE,
        "hardware_identity": hardware,
        "tool": {"name": TOOL_NAME, "version": TOOL_VERSION},
        "kernel_release": os.uname().release,
        "process": {
            "executable": _path(capture, "capture_session", "process", "executable"),
            "pid": process.pid,
            "command": options.command,
        },
        "measurement": {
            "peak_resident_bytes": peak_resident_bytes,
            "started_utc": measurement_started,
            "ended_utc": measurement_ended,
            "poll_interval_ms": options.poll_ms,
            "sampling_attempts": attempts,
            "successful_samples": len(samples),
            "fdinfo_read_errors": read_errors,
        },
        "accounting": ACCOUNTING_POLICY,
        "profile_capture_sha256": capture_sha256,
        "samples": samples,
    }
    _write_text_atomic(options.artifact, serialize_json(artifact))
    artifact_sha256 = _file_sha256(options.artifact)

    evidence = {
        "schema_version": EVIDENCE_SCHEMA_VERSION,
        "measurement_scope": COMPLETE_VRAM_SCOPE,
        "hardware_identity": hardware,
        "tool": {"name": TOOL_NAME, "version": TOOL_VERSION},
        "process": {"executable": executable_name, "pid": process.pid},
        "measurement": {
            "peak_resident_bytes": peak_resident_bytes,
            "started_utc": measurement_started,
            "ended_utc": measurement_ended,
        },
        "profile_capture_sha256": capture_sha256,
        "source_artifact": {
            "file_name": options.artifact.name,
            "sha256": artifact_sha256,
        },
    }
    _write_text_atomic(options.evidence, serialize_json(evidence))
    if _file_sha256(options.capture) != capture_sha256:
        raise ReportError("profile capture changed while DRM evidence was being written")


def main(arguments: list[str] | None = None) -> int:
    try:
        run_capture(parse_args(sys.argv[1:] if arguments is None else arguments))
        return 0
    except (OSError, ReportError, subprocess.SubprocessError) as error:
        sys.stderr.write(f"drm-vram-capture: {error}\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
