#!/usr/bin/env python3
"""plan_14 IG-14-011/012: the collision extractor must respect roles and transforms.

A proxy in the wrong place is worse than no proxy: the player walks into nothing, or through
something. So each rule gets a fixture rather than being trusted to the one real asset.
"""

import json
import sys
import unittest
from pathlib import Path

EXTRACTOR = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
sys.path.insert(0, str(EXTRACTOR.parent) if EXTRACTOR else "scripts")
import extract_collision  # noqa: E402


def gltf(nodes, roots=None):
    """A minimal glTF whose single mesh is a 2x2x2 cube centred on its own origin."""
    return {
        "scene": 0,
        "scenes": [{"nodes": roots if roots is not None else list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": [{"name": "Cube", "primitives": [{"attributes": {"POSITION": 0}}]}],
        "accessors": [{"min": [-1.0, -1.0, -1.0], "max": [1.0, 1.0, 1.0]}],
    }


class ExtractCollisionTests(unittest.TestCase):
    def test_only_blocking_roles_become_proxies(self):
        proxies = extract_collision.extract(gltf([
            {"name": "Wall", "mesh": 0, "extras": {"collision": "static"}},
            {"name": "Paint", "mesh": 0, "extras": {"collision": "none"}},
            {"name": "Zone", "mesh": 0, "extras": {"collision": "trigger"}},
            {"name": "Unmarked", "mesh": 0},
        ]))
        self.assertEqual([p["name"] for p in proxies], ["Wall"])

    def test_a_trigger_never_becomes_a_solid_collider(self):
        # A trigger reports overlap and must not block; registering it would wall off the very
        # volume it exists to watch.
        self.assertNotIn("trigger", extract_collision.BLOCKING_ROLES)

    def test_translation_is_accumulated_through_the_hierarchy(self):
        proxies = extract_collision.extract(gltf([
            {"name": "Root", "translation": [10.0, 0.0, 5.0], "children": [1]},
            {"name": "Post", "translation": [0.0, 2.0, 0.0], "mesh": 0,
             "extras": {"collision": "static"}},
        ], roots=[0]))
        self.assertEqual(len(proxies), 1)
        self.assertEqual(proxies[0]["center"], [10.0, 2.0, 5.0])
        self.assertEqual(proxies[0]["halfExtents"], [1.0, 1.0, 1.0])

    def test_a_role_on_a_group_applies_to_the_geometry_beneath_it(self):
        proxies = extract_collision.extract(gltf([
            {"name": "Group", "extras": {"collision": "static"}, "children": [1]},
            {"name": "Child", "mesh": 0},
        ], roots=[0]))
        self.assertEqual([p["name"] for p in proxies], ["Child"])

    def test_a_child_overrides_an_inherited_role(self):
        proxies = extract_collision.extract(gltf([
            {"name": "Group", "extras": {"collision": "static"}, "children": [1]},
            {"name": "Child", "mesh": 0, "extras": {"collision": "none"}},
        ], roots=[0]))
        self.assertEqual(proxies, [])

    def test_a_quarter_turn_swaps_the_extents_exactly(self):
        # Y rotation by 90 degrees: quaternion (0, sin45, 0, cos45).
        half = 2 ** 0.5 / 2
        proxies = extract_collision.extract(gltf([
            {"name": "Bench", "rotation": [0.0, half, 0.0, half], "mesh": 0,
             "extras": {"collision": "static"}},
        ]))
        self.assertEqual(len(proxies), 1)
        # A cube is symmetric, so the useful check is that a *non*-cube swaps. Do that directly:
        rotated = extract_collision.extract({
            "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": [{"name": "Plank", "rotation": [0.0, half, 0.0, half], "mesh": 0,
                       "extras": {"collision": "static"}}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
            "accessors": [{"min": [-2.0, -0.1, -0.5], "max": [2.0, 0.1, 0.5]}],
        })
        self.assertAlmostEqual(rotated[0]["halfExtents"][0], 0.5, places=4)
        self.assertAlmostEqual(rotated[0]["halfExtents"][2], 2.0, places=4)
        self.assertAlmostEqual(rotated[0]["halfExtents"][1], 0.1, places=4)

    def test_scale_is_applied(self):
        proxies = extract_collision.extract(gltf([
            {"name": "Big", "scale": [3.0, 1.0, 1.0], "mesh": 0, "extras": {"collision": "static"}},
        ]))
        self.assertAlmostEqual(proxies[0]["halfExtents"][0], 3.0, places=4)

    def test_a_mesh_without_accessor_bounds_is_refused(self):
        # Without min/max a proxy cannot be sized, and guessing would put a collider in the wrong
        # place -- which is worse than having none.
        with self.assertRaises(extract_collision.CollisionError):
            extract_collision.extract({
                "scene": 0, "scenes": [{"nodes": [0]}],
                "nodes": [{"name": "N", "mesh": 0, "extras": {"collision": "static"}}],
                "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
                "accessors": [{}],
            })

    def test_a_node_without_a_mesh_produces_nothing(self):
        self.assertEqual(extract_collision.extract(gltf([
            {"name": "Empty", "extras": {"collision": "static"}},
        ])), [])


if __name__ == "__main__":
    if EXTRACTOR is None:
        print("usage: test_extract_collision.py <extract_collision.py>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
