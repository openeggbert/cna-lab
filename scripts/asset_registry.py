#!/usr/bin/env python3
"""Validate approved production-asset provenance and generate third-party notices."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import os
import posixpath
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from urllib.parse import urlsplit


SCHEMA_VERSION = "1"
REGISTRY_COLUMNS = (
    "schema_version",
    "asset_id",
    "relative_path",
    "sha256",
    "source_url",
    "author",
    "license",
    "download_date",
    "modified",
    "attribution_required",
    "commercial_use_allowed",
    "redistribution_allowed",
    "ai_processing_allowed",
    "license_evidence",
    "license_evidence_sha256",
    "reviewer",
    "approval_status",
    "quality_state",
    "notes",
)
ALLOWED_LICENSES = frozenset(("MIT", "CC0", "Public Domain"))
PRODUCTION_DIRECTORIES = ("audio", "config", "cutscenes", "dialogues", "districts", "missions", "vehicles")
EMBEDDED_ASSET_PATHS = frozenset(("src/UI/BitmapFont.cpp",))
ASSET_ID_PATTERN = re.compile(r"[a-z0-9][a-z0-9_]*")
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")


class RegistryError(ValueError):
    pass


@dataclass(frozen=True)
class AssetRecord:
    asset_id: str
    relative_path: str
    resolved_path: Path
    sha256: str
    source_url: str
    author: str
    license_name: str
    download_date: str
    modified: bool
    attribution_required: bool
    commercial_use_allowed: bool
    redistribution_allowed: bool
    ai_processing_allowed: bool
    license_evidence: str
    evidence_path: Path
    license_evidence_sha256: str
    reviewer: str
    approval_status: str
    quality_state: str
    notes: str

    @property
    def external(self) -> bool:
        return bool(self.source_url)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        raise RegistryError(f"could not hash {path}: {error}") from error
    return digest.hexdigest()


def _canonical_text(value: str | None, label: str, *, allow_empty: bool = False) -> str:
    if value is None or value != value.strip() or (not value and not allow_empty):
        requirement = "a canonical string" if allow_empty else "a non-empty canonical string"
        raise RegistryError(f"{label} must be {requirement}")
    if value and not value.isprintable():
        raise RegistryError(f"{label} must be a single printable line")
    return value


def _boolean(value: str | None, label: str) -> bool:
    if value not in ("true", "false"):
        raise RegistryError(f"{label} must be exactly true or false")
    return value == "true"


def _canonical_path(value: str | None, label: str) -> str:
    path = _canonical_text(value, label)
    if "\\" in path or PurePosixPath(path).is_absolute() or posixpath.normpath(path) != path:
        raise RegistryError(f"{label} must be a normalized relative POSIX path")
    return path


def _resolve_inside(
    root: Path, relative: str, label: str, *, boundary: Path | None = None
) -> Path:
    candidate = root / PurePosixPath(relative)
    if candidate.is_symlink():
        raise RegistryError(f"{label} must not be a symlink: {relative}")
    resolved = candidate.resolve()
    boundary = root if boundary is None else boundary
    try:
        resolved.relative_to(boundary)
    except ValueError as error:
        raise RegistryError(f"{label} escapes the project root") from error
    if not resolved.is_file():
        raise RegistryError(f"{label} does not name a file: {relative}")
    return resolved


def _https_url(value: str, label: str) -> None:
    parsed = urlsplit(value)
    if (
        parsed.scheme != "https"
        or not parsed.netloc
        or parsed.username
        or parsed.password
        or any(character.isspace() for character in value)
    ):
        raise RegistryError(f"{label} must be an absolute HTTPS URL without credentials")


def _download_date(value: str, label: str) -> None:
    try:
        parsed = dt.date.fromisoformat(value)
    except ValueError as error:
        raise RegistryError(f"{label} must use YYYY-MM-DD") from error
    if parsed > dt.date.today():
        raise RegistryError(f"{label} must not be in the future")


def _production_paths(project_root: Path) -> set[Path]:
    asset_root = project_root / "assets"
    result: set[Path] = set()

    def register(path: Path) -> None:
        if path.is_symlink():
            raise RegistryError(f"production asset must not be a symlink: {path}")
        resolved = path.resolve()
        try:
            resolved.relative_to(project_root)
        except ValueError as error:
            raise RegistryError(f"production asset escapes the project root: {path}") from error
        if not resolved.is_file():
            raise RegistryError(f"production asset is missing: {path}")
        result.add(resolved)

    for directory in PRODUCTION_DIRECTORIES:
        root = asset_root / directory
        if not root.is_dir():
            raise RegistryError(f"missing production asset directory: assets/{directory}")
        for path in root.rglob("*"):
            if path.is_file():
                register(path)
    source_root = asset_root / "source"
    if not source_root.is_dir():
        raise RegistryError("missing production asset directory: assets/source")
    for pattern in ("*.gltf", "*.mc3.xml"):
        for path in source_root.rglob(pattern):
            if path.is_file():
                register(path)
    for path in EMBEDDED_ASSET_PATHS:
        register(project_root / path)
    folded: dict[str, Path] = {}
    for path in result:
        relative = path.relative_to(project_root).as_posix()
        collision = folded.setdefault(relative.casefold(), path)
        if collision != path:
            raise RegistryError(f"case-colliding production asset paths: {collision} and {path}")
    return result


def load_registry(registry_path: Path, project_root: Path) -> list[AssetRecord]:
    project_root = project_root.resolve()
    asset_root = (project_root / "assets").resolve()
    try:
        with registry_path.open("r", encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source, strict=True)
            if tuple(reader.fieldnames or ()) != REGISTRY_COLUMNS:
                raise RegistryError(
                    "registry header must be exactly: " + ",".join(REGISTRY_COLUMNS)
                )
            rows = list(reader)
    except (OSError, csv.Error) as error:
        raise RegistryError(f"could not read registry {registry_path}: {error}") from error
    if not rows:
        raise RegistryError("asset registry must contain at least one record")

    records: list[AssetRecord] = []
    identifiers: set[str] = set()
    paths: set[Path] = set()
    asset_hashes: dict[str, str] = {}
    casefolded_paths: dict[str, str] = {}
    for index, row in enumerate(rows, start=2):
        label = f"registry row {index}"
        if None in row or any(value is None for value in row.values()):
            raise RegistryError(f"{label} has an unexpected or missing CSV field")
        if _canonical_text(row["schema_version"], f"{label}.schema_version") != SCHEMA_VERSION:
            raise RegistryError(f"{label}.schema_version must be {SCHEMA_VERSION}")
        asset_id = _canonical_text(row["asset_id"], f"{label}.asset_id")
        if ASSET_ID_PATTERN.fullmatch(asset_id) is None:
            raise RegistryError(f"{label}.asset_id must match {ASSET_ID_PATTERN.pattern}")
        if asset_id in identifiers:
            raise RegistryError(f"duplicate asset_id: {asset_id}")
        identifiers.add(asset_id)

        relative_path = _canonical_path(row["relative_path"], f"{label}.relative_path")
        resolved_path = _resolve_inside(
            asset_root,
            relative_path,
            f"{label}.relative_path",
            boundary=project_root,
        )
        if resolved_path in paths:
            raise RegistryError(f"duplicate registered asset path: {relative_path}")
        paths.add(resolved_path)
        project_relative = resolved_path.relative_to(project_root).as_posix()
        prior_case = casefolded_paths.setdefault(project_relative.casefold(), project_relative)
        if prior_case != project_relative:
            raise RegistryError(
                f"case-colliding registered asset paths: {prior_case} and {project_relative}"
            )

        digest = _canonical_text(row["sha256"], f"{label}.sha256")
        if SHA256_PATTERN.fullmatch(digest) is None:
            raise RegistryError(f"{label}.sha256 must be lowercase SHA-256")
        actual_digest = sha256(resolved_path)
        if digest != actual_digest:
            raise RegistryError(
                f"{asset_id}: asset SHA-256 mismatch: registry={digest}, actual={actual_digest}"
            )
        duplicate_hash = asset_hashes.setdefault(digest, asset_id)
        if duplicate_hash != asset_id:
            raise RegistryError(
                f"duplicate asset content: {duplicate_hash} and {asset_id} share {digest}"
            )

        source_url = _canonical_text(
            row["source_url"], f"{label}.source_url", allow_empty=True
        )
        download_date = _canonical_text(
            row["download_date"], f"{label}.download_date", allow_empty=True
        )
        license_name = _canonical_text(row["license"], f"{label}.license")
        if license_name not in ALLOWED_LICENSES:
            raise RegistryError(
                f"{asset_id}: license {license_name!r} is not in the shipping allow-list"
            )
        if source_url:
            _https_url(source_url, f"{label}.source_url")
            if not download_date:
                raise RegistryError(f"{asset_id}: external asset requires download_date")
            _download_date(download_date, f"{label}.download_date")
        elif download_date:
            raise RegistryError(f"{asset_id}: original asset must not have download_date")
        elif license_name != "MIT":
            raise RegistryError(f"{asset_id}: non-MIT asset requires a primary source URL")

        evidence_relative = _canonical_path(
            row["license_evidence"], f"{label}.license_evidence"
        )
        evidence_path = _resolve_inside(
            project_root, evidence_relative, f"{label}.license_evidence"
        )
        evidence_digest = _canonical_text(
            row["license_evidence_sha256"], f"{label}.license_evidence_sha256"
        )
        if SHA256_PATTERN.fullmatch(evidence_digest) is None:
            raise RegistryError(f"{label}.license_evidence_sha256 must be lowercase SHA-256")
        actual_evidence_digest = sha256(evidence_path)
        if evidence_digest != actual_evidence_digest:
            raise RegistryError(
                f"{asset_id}: license-evidence SHA-256 mismatch: "
                f"registry={evidence_digest}, actual={actual_evidence_digest}"
            )

        commercial = _boolean(
            row["commercial_use_allowed"], f"{label}.commercial_use_allowed"
        )
        redistribution = _boolean(
            row["redistribution_allowed"], f"{label}.redistribution_allowed"
        )
        if not commercial or not redistribution:
            raise RegistryError(
                f"{asset_id}: shipping requires commercial use and redistribution approval"
            )
        approval = _canonical_text(row["approval_status"], f"{label}.approval_status")
        quality = _canonical_text(row["quality_state"], f"{label}.quality_state")
        if approval != "approved" or quality != "shipping":
            raise RegistryError(
                f"{asset_id}: release registry requires approval_status=approved and "
                "quality_state=shipping"
            )

        records.append(
            AssetRecord(
                asset_id=asset_id,
                relative_path=relative_path,
                resolved_path=resolved_path,
                sha256=digest,
                source_url=source_url,
                author=_canonical_text(row["author"], f"{label}.author"),
                license_name=license_name,
                download_date=download_date,
                modified=_boolean(row["modified"], f"{label}.modified"),
                attribution_required=_boolean(
                    row["attribution_required"], f"{label}.attribution_required"
                ),
                commercial_use_allowed=commercial,
                redistribution_allowed=redistribution,
                ai_processing_allowed=_boolean(
                    row["ai_processing_allowed"], f"{label}.ai_processing_allowed"
                ),
                license_evidence=evidence_relative,
                evidence_path=evidence_path,
                license_evidence_sha256=evidence_digest,
                reviewer=_canonical_text(row["reviewer"], f"{label}.reviewer"),
                approval_status=approval,
                quality_state=quality,
                notes=_canonical_text(row["notes"], f"{label}.notes"),
            )
        )

    production_paths = _production_paths(project_root)
    missing = production_paths - paths
    unexpected = paths - production_paths
    if missing:
        names = ", ".join(sorted(path.relative_to(project_root).as_posix() for path in missing))
        raise RegistryError(f"unregistered production assets: {names}")
    if unexpected:
        names = ", ".join(sorted(path.relative_to(project_root).as_posix() for path in unexpected))
        raise RegistryError(f"registry contains non-production asset paths: {names}")
    return records


def _escape(value: str) -> str:
    return (
        value.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace("|", "&#124;")
        .replace("`", "&#96;")
    )


def generate_notice(records: list[AssetRecord], project_root: Path) -> str:
    external = sorted((record for record in records if record.external), key=lambda item: item.asset_id)
    lines = [
        "# Third-party assets",
        "",
        "<!-- Generated by scripts/asset_registry.py; edit assets/licenses/asset-registry.csv instead. -->",
        "",
        "This notice lists every approved external content asset in the current production registry.",
        "Dependency notices remain in `THIRD_PARTY.md`.",
        "",
    ]
    if external:
        lines.extend(
            [
                "| Asset | Bundled path | Author | License | Source | Modified | Attribution required | SHA-256 |",
                "| --- | --- | --- | --- | --- | :---: | :---: | --- |",
            ]
        )
        for record in external:
            relative = record.resolved_path.relative_to(project_root).as_posix()
            lines.append(
                f"| {_escape(record.asset_id)} | `{_escape(relative)}` | "
                f"{_escape(record.author)} | {_escape(record.license_name)} | "
                f"{_escape(record.source_url)} | {'yes' if record.modified else 'no'} | "
                f"{'yes' if record.attribution_required else 'no'} | `{record.sha256}` |"
            )
    else:
        lines.append("No approved external content assets are currently registered.")
    lines.extend(
        [
            "",
            "## Review evidence",
            "",
            "| Asset | Local review record | Evidence SHA-256 | Reviewer |",
            "| --- | --- | --- | --- |",
        ]
    )
    for record in external:
        lines.append(
            f"| {_escape(record.asset_id)} | `{_escape(record.license_evidence)}` | "
            f"`{record.license_evidence_sha256}` | {_escape(record.reviewer)} |"
        )
    lines.extend(
        [
            "",
            "All listed rows are approved for commercial use and redistribution. The notice is",
            "informational even where the license does not require attribution.",
            "",
        ]
    )
    return "\n".join(lines)


def _same_file(left: Path, right: Path) -> bool:
    try:
        return left.resolve() == right.resolve() or (
            left.exists() and right.exists() and os.path.samefile(left, right)
        )
    except OSError:
        return left.resolve() == right.resolve()


def _protect_output(output: Path, registry: Path, records: list[AssetRecord]) -> None:
    protected = [registry]
    protected.extend(record.resolved_path for record in records)
    protected.extend(record.evidence_path for record in records)
    for path in protected:
        if _same_file(output, path):
            raise RegistryError(f"notice output must not overwrite registry evidence: {path}")


def _verify_records_unchanged(records: list[AssetRecord]) -> None:
    for record in records:
        if sha256(record.resolved_path) != record.sha256:
            raise RegistryError(f"{record.asset_id}: asset changed during registry validation")
        if sha256(record.evidence_path) != record.license_evidence_sha256:
            raise RegistryError(
                f"{record.asset_id}: license evidence changed during registry validation"
            )


def _write_atomic(path: Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary.write(contents)
            temporary_path = Path(temporary.name)
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def _path_from_root(value: str, project_root: Path) -> Path:
    path = Path(value)
    return path.resolve() if path.is_absolute() else (project_root / path).resolve()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", default=".")
    parser.add_argument("--registry", default="assets/licenses/asset-registry.csv")
    notice = parser.add_mutually_exclusive_group()
    notice.add_argument("--check-notice", metavar="PATH")
    notice.add_argument("--write-notice", metavar="PATH")
    arguments = parser.parse_args()

    try:
        project_root = Path(arguments.project_root).resolve()
        if not project_root.is_dir():
            raise RegistryError(f"project root is not a directory: {project_root}")
        registry_path = _path_from_root(arguments.registry, project_root)
        records = load_registry(registry_path, project_root)
        external_count = sum(record.external for record in records)
        action = "validated"
        if arguments.check_notice or arguments.write_notice:
            option = arguments.check_notice or arguments.write_notice
            assert option is not None
            output_path = _path_from_root(option, project_root)
            _protect_output(output_path, registry_path, records)
            expected = generate_notice(records, project_root)
            if arguments.check_notice:
                try:
                    actual = output_path.read_text(encoding="utf-8")
                except OSError as error:
                    raise RegistryError(f"could not read generated notice {output_path}: {error}") from error
                if actual != expected:
                    raise RegistryError(
                        f"generated notice is stale: run {Path(__file__).name} "
                        f"--write-notice {option}"
                    )
                action = "notice verified"
            else:
                _verify_records_unchanged(records)
                _write_atomic(output_path, expected)
                action = f"notice written to {output_path}"
        _verify_records_unchanged(records)
        print(
            f"asset-registry: {len(records)} approved shipping assets; "
            f"{external_count} external assets; {action}"
        )
        return 0
    except RegistryError as error:
        print(f"asset-registry: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
