#!/usr/bin/env python3
"""Validate committed MC3/glTF source assets against explicit bootstrap budgets."""

from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Any


POLICY_SCHEMA_VERSION = 1
METRICS = ("triangles", "materials", "textures")


class BudgetError(ValueError):
    pass


@dataclass(frozen=True)
class AssetMetrics:
    triangles: int = 0
    materials: int = 0
    textures: int = 0

    def __add__(self, other: "AssetMetrics") -> "AssetMetrics":
        return AssetMetrics(
            self.triangles + other.triangles,
            self.materials + other.materials,
            self.textures + other.textures,
        )


@dataclass(frozen=True)
class BudgetAsset:
    identifier: str
    category: str
    sources: tuple[Path, ...]
    baseline: AssetMetrics
    limits: AssetMetrics


def _object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise BudgetError(f"{label} must be an object")
    return value


def _non_negative_integer(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise BudgetError(f"{label} must be a non-negative integer")
    return value


def _metrics(value: Any, label: str) -> AssetMetrics:
    source = _object(value, label)
    unknown = set(source) - set(METRICS)
    missing = set(METRICS) - set(source)
    if unknown or missing:
        raise BudgetError(
            f"{label} must contain exactly {', '.join(METRICS)}; "
            f"missing={sorted(missing)}, unknown={sorted(unknown)}"
        )
    return AssetMetrics(
        *(_non_negative_integer(source[name], f"{label}.{name}") for name in METRICS)
    )


def load_policy(path: Path, project_root: Path) -> list[BudgetAsset]:
    try:
        policy = _object(json.loads(path.read_text(encoding="utf-8")), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise BudgetError(f"could not read policy {path}: {error}") from error
    if policy.get("schema_version") != POLICY_SCHEMA_VERSION:
        raise BudgetError(
            f"{path}: schema_version must be {POLICY_SCHEMA_VERSION}, "
            f"got {policy.get('schema_version')!r}"
        )
    raw_assets = policy.get("assets")
    if not isinstance(raw_assets, list) or not raw_assets:
        raise BudgetError(f"{path}: assets must be a non-empty array")

    assets: list[BudgetAsset] = []
    identifiers: set[str] = set()
    claimed_sources: set[Path] = set()
    for index, raw_asset in enumerate(raw_assets):
        label = f"assets[{index}]"
        asset = _object(raw_asset, label)
        identifier = asset.get("id")
        category = asset.get("category")
        if not isinstance(identifier, str) or not identifier:
            raise BudgetError(f"{label}.id must be a non-empty string")
        if identifier in identifiers:
            raise BudgetError(f"duplicate asset id: {identifier}")
        identifiers.add(identifier)
        if not isinstance(category, str) or not category:
            raise BudgetError(f"{label}.category must be a non-empty string")
        raw_sources = asset.get("sources")
        if not isinstance(raw_sources, list) or not raw_sources or not all(
            isinstance(source, str) and source for source in raw_sources
        ):
            raise BudgetError(f"{label}.sources must be a non-empty string array")
        sources = tuple((project_root / source).resolve() for source in raw_sources)
        for source in sources:
            if source in claimed_sources:
                raise BudgetError(f"source appears in more than one budget asset: {source}")
            claimed_sources.add(source)
            if not source.is_file():
                raise BudgetError(f"budget source does not exist: {source}")
        baseline = _metrics(asset.get("baseline"), f"{label}.baseline")
        limits = _metrics(asset.get("limits"), f"{label}.limits")
        for metric in METRICS:
            if getattr(baseline, metric) > getattr(limits, metric):
                raise BudgetError(f"{identifier}: baseline {metric} exceeds its limit")
        assets.append(BudgetAsset(identifier, category, sources, baseline, limits))
    return assets


def inspect_mc3(path: Path) -> AssetMetrics:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        raise BudgetError(f"could not parse MC3 {path}: {error}") from error
    if root.tag != "mc3":
        raise BudgetError(f"{path}: expected an mc3 root element")
    objects = root.find("objects")
    if objects is None:
        raise BudgetError(f"{path}: missing objects element")
    triangles = 0
    for primitive in objects:
        if primitive.tag in ("box", "cube"):
            triangles += 12
        else:
            raise BudgetError(
                f"{path}: unsupported MC3 primitive <{primitive.tag}>; "
                "add an exact triangulation rule before budgeting it"
            )
    materials = root.find("materials")
    textures = root.find("textures")
    return AssetMetrics(
        triangles=triangles,
        materials=0 if materials is None else len(materials.findall("material")),
        textures=0 if textures is None else len(textures.findall("texture")),
    )


def inspect_gltf(path: Path) -> AssetMetrics:
    try:
        gltf = _object(json.loads(path.read_text(encoding="utf-8")), str(path))
    except (OSError, json.JSONDecodeError) as error:
        raise BudgetError(f"could not parse glTF {path}: {error}") from error
    accessors = gltf.get("accessors", [])
    meshes = gltf.get("meshes", [])
    materials = gltf.get("materials", [])
    textures = gltf.get("textures", [])
    if not all(isinstance(value, list) for value in (accessors, meshes, materials, textures)):
        raise BudgetError(f"{path}: accessors/meshes/materials/textures must be arrays")

    triangles = 0
    for mesh_index, mesh_value in enumerate(meshes):
        mesh = _object(mesh_value, f"{path}: meshes[{mesh_index}]")
        primitives = mesh.get("primitives")
        if not isinstance(primitives, list) or not primitives:
            raise BudgetError(f"{path}: meshes[{mesh_index}].primitives must be non-empty")
        for primitive_index, primitive_value in enumerate(primitives):
            label = f"{path}: meshes[{mesh_index}].primitives[{primitive_index}]"
            primitive = _object(primitive_value, label)
            mode = primitive.get("mode", 4)
            if mode not in (4, 5, 6):
                raise BudgetError(f"{label}: unsupported non-triangle glTF mode {mode!r}")
            if "indices" in primitive:
                accessor_index = _non_negative_integer(primitive["indices"], f"{label}.indices")
            else:
                attributes = _object(primitive.get("attributes"), f"{label}.attributes")
                accessor_index = _non_negative_integer(attributes.get("POSITION"), f"{label}.POSITION")
            if accessor_index >= len(accessors):
                raise BudgetError(f"{label}: accessor index {accessor_index} is out of range")
            accessor = _object(accessors[accessor_index], f"{path}: accessors[{accessor_index}]")
            count = _non_negative_integer(accessor.get("count"), f"{path}: accessors[{accessor_index}].count")
            if mode == 4:
                if count % 3 != 0:
                    raise BudgetError(f"{label}: TRIANGLES element count {count} is not divisible by 3")
                triangles += count // 3
            else:
                if count < 3:
                    raise BudgetError(f"{label}: triangle strip/fan needs at least 3 elements")
                triangles += count - 2
    return AssetMetrics(triangles, len(materials), len(textures))


def inspect_source(path: Path) -> AssetMetrics:
    if path.name.endswith(".mc3.xml"):
        return inspect_mc3(path)
    if path.suffix.lower() == ".gltf":
        return inspect_gltf(path)
    raise BudgetError(f"unsupported budget source format: {path}")


def select_assets(assets: list[BudgetAsset], selected_sources: list[Path]) -> list[BudgetAsset]:
    if not selected_sources:
        return assets
    requested = {source.resolve() for source in selected_sources}
    known = {source for asset in assets for source in asset.sources}
    unknown = sorted(requested - known)
    if unknown:
        raise BudgetError(
            "no content-budget entry covers: " + ", ".join(str(source) for source in unknown)
        )
    return [asset for asset in assets if requested.intersection(asset.sources)]


def validate_asset(asset: BudgetAsset) -> tuple[AssetMetrics, list[str]]:
    actual = AssetMetrics()
    for source in asset.sources:
        actual += inspect_source(source)
    failures = [
        f"{metric} {getattr(actual, metric)} exceeds limit {getattr(asset.limits, metric)}"
        for metric in METRICS
        if getattr(actual, metric) > getattr(asset.limits, metric)
    ]
    return actual, failures


def parse_args(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--policy",
        type=Path,
        default=Path("assets/content-budgets.json"),
        help="versioned budget policy (default: assets/content-budgets.json)",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        type=Path,
        help="validate the budget group containing this source (repeatable)",
    )
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_args(sys.argv[1:] if arguments is None else arguments)
    project_root = options.project_root.resolve()
    policy_path = options.policy
    if not policy_path.is_absolute():
        policy_path = project_root / policy_path
    selected_sources = [
        source if source.is_absolute() else project_root / source for source in options.source
    ]
    try:
        assets = select_assets(load_policy(policy_path.resolve(), project_root), selected_sources)
        failed = False
        for asset in assets:
            actual, failures = validate_asset(asset)
            state = "FAIL" if failures else "PASS"
            print(
                f"{state} {asset.identifier} [{asset.category}]: "
                f"triangles {actual.triangles}/{asset.limits.triangles}, "
                f"materials {actual.materials}/{asset.limits.materials}, "
                f"textures {actual.textures}/{asset.limits.textures}"
            )
            for failure in failures:
                print(f"  - {failure}")
            failed = failed or bool(failures)
        return 1 if failed else 0
    except BudgetError as error:
        print(f"content-budget: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
