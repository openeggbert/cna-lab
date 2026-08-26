#!/usr/bin/env python3
"""plan_14 IG-14-011/012: extract collision proxies from a generated glTF/GLB.

MC3 authors a collision *role* on every object (see docs/mc3-conventions.md), and `mc3togltf`
carries it into the glTF verbatim as `node.extras.collision`. The `.cnj` runtime model deliberately
does not: it is render data, and a physics world should not be reconstructed by walking render
meshes.

So the proxies travel separately. This walks the glTF scene graph, accumulates each node's
transform, and writes a world-space axis-aligned box for every node whose role blocks movement. The
game loads that sidecar and registers the boxes with the physics world -- independent of whether the
render model loaded at all, which is the point of IG-14-012.

Boxes rather than hulls because every collider in this game is an AABB (`PrototypeWorld`'s `Aabb`).
A rotated proxy is handled by transforming the local bounding box's eight corners and taking their
extent: exact for the quarter-turns props are actually placed at, and conservative -- never smaller
than the geometry -- for anything else.
"""

import argparse
import json
import math
import struct
import sys
from pathlib import Path

# Roles that produce a solid collider. "trigger" deliberately does not: a trigger reports overlap
# and must not block, so registering it as a static body would wall off the volume it watches.
BLOCKING_ROLES = ("static",)
COLLISION_FILE_VERSION = 1


class CollisionError(ValueError):
    pass


def load_gltf(path: Path) -> dict:
    data = path.read_bytes()
    if data[:4] == b"glTF":
        if len(data) < 20:
            raise CollisionError(f"{path}: truncated GLB")
        length = struct.unpack("<I", data[12:16])[0]
        return json.loads(data[20:20 + length])
    try:
        return json.loads(data)
    except json.JSONDecodeError as error:
        raise CollisionError(f"{path}: not a GLB or a JSON glTF: {error}") from error


def _matrix_multiply(a: list[float], b: list[float]) -> list[float]:
    """Column-major 4x4, glTF's own convention."""
    out = [0.0] * 16
    for column in range(4):
        for row in range(4):
            out[column * 4 + row] = sum(a[k * 4 + row] * b[column * 4 + k] for k in range(4))
    return out


def _node_matrix(node: dict) -> list[float]:
    if "matrix" in node:
        return list(node["matrix"])
    tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
    x, y, z, w = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    sx, sy, sz = node.get("scale", (1.0, 1.0, 1.0))
    # Quaternion to column-major rotation, then scale columns and set the translation column.
    rotation = [
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0.0,
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0.0,
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]
    for index, scale in enumerate((sx, sy, sz)):
        for row in range(3):
            rotation[index * 4 + row] *= scale
    rotation[12], rotation[13], rotation[14] = tx, ty, tz
    return rotation


def _mesh_bounds(gltf: dict, mesh_index: int) -> tuple[list[float], list[float]] | None:
    mesh = gltf["meshes"][mesh_index]
    minimum = [math.inf] * 3
    maximum = [-math.inf] * 3
    for primitive in mesh.get("primitives", []):
        position = primitive.get("attributes", {}).get("POSITION")
        if position is None:
            continue
        accessor = gltf["accessors"][position]
        low, high = accessor.get("min"), accessor.get("max")
        if low is None or high is None:
            raise CollisionError(
                f"mesh '{mesh.get('name', mesh_index)}' has a POSITION accessor without min/max; "
                "a collision proxy cannot be sized without it")
        for axis in range(3):
            minimum[axis] = min(minimum[axis], low[axis])
            maximum[axis] = max(maximum[axis], high[axis])
    if any(math.isinf(value) for value in minimum + maximum):
        return None
    return minimum, maximum


def extract(gltf: dict) -> list[dict]:
    proxies: list[dict] = []
    nodes = gltf.get("nodes", [])
    scene = gltf.get("scenes", [{}])[gltf.get("scene", 0)]

    def visit(index: int, parent: list[float], inherited_role: str | None) -> None:
        node = nodes[index]
        world = _matrix_multiply(parent, _node_matrix(node))
        # A role set on a group applies to the geometry beneath it unless a child overrides it.
        role = node.get("extras", {}).get("collision", inherited_role)
        if "mesh" in node and role in BLOCKING_ROLES:
            bounds = _mesh_bounds(gltf, node["mesh"])
            if bounds is not None:
                low, high = bounds
                corners = [
                    (low[0] if i & 1 else high[0], low[1] if i & 2 else high[1],
                     low[2] if i & 4 else high[2]) for i in range(8)
                ]
                transformed = []
                for cx, cy, cz in corners:
                    transformed.append(tuple(
                        world[0 * 4 + r] * cx + world[1 * 4 + r] * cy + world[2 * 4 + r] * cz
                        + world[12 + r] for r in range(3)))
                axis_min = [min(p[a] for p in transformed) for a in range(3)]
                axis_max = [max(p[a] for p in transformed) for a in range(3)]
                proxies.append({
                    "name": node.get("name", f"node{index}"),
                    "center": [round((axis_min[a] + axis_max[a]) * 0.5, 5) for a in range(3)],
                    "halfExtents": [round((axis_max[a] - axis_min[a]) * 0.5, 5) for a in range(3)],
                })
        for child in node.get("children", []):
            visit(child, world, role)

    identity = [1.0 if i % 5 == 0 else 0.0 for i in range(16)]
    for root in scene.get("nodes", []):
        visit(root, identity, None)
    return proxies


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="generated .glb or .gltf")
    parser.add_argument("output", help="collision sidecar to write")
    parser.add_argument("--id", default=None, help="identifier recorded in the sidecar")
    options = parser.parse_args(argv)

    source = Path(options.input)
    try:
        proxies = extract(load_gltf(source))
    except CollisionError as error:
        print(f"extract-collision: {error}", file=sys.stderr)
        return 2

    document = {
        "id": options.id or source.stem,
        "version": COLLISION_FILE_VERSION,
        "proxies": proxies,
    }
    target = Path(options.output)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(document, indent=2) + "\n")
    print(f"extract-collision: {len(proxies)} proxy box(es) -> {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
