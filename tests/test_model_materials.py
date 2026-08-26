#!/usr/bin/env python3
"""plan_08 IG-08-014: keep assets/models/model-materials.json honest against the MC3 sources.

The MC3 -> glTF -> CNJ pipeline drops material data entirely: a generated .cnj holds only
vertices, indices, a vertex stride and an effect name, and the vertex layout (stride 32 =
position, normal, uv) has no colour channel. So Iron Gang ships the base colours separately and
applies them at draw time.

That makes the JSON a *copy* of numbers whose source of truth is the MC3 file, and a copy that
nothing checks is a copy that drifts. This test fails when someone retunes a colour in
assets/source/mc3/ without updating the shipped table -- which would silently leave the game
drawing the old colour.
"""

import json
import re
import sys
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path

PROJECT_ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

# Models imported as single-material CNJ props. prototype_city_block is deliberately absent: it
# has five materials and is not drawn as an imported model (the renderer builds the city
# procedurally, with its own baked per-face lighting).
SINGLE_MATERIAL_MODELS = (
    "warehouse",
    "vehicle_body",
    "vehicle_cabin",
    "vehicle_windshield",
    "vehicle_wheel",
)


def mc3_base_color(path: Path) -> tuple[float, float, float]:
    root = ElementTree.parse(path).getroot()
    colors = [element for element in root.iter() if element.tag.endswith("base_color")]
    if len(colors) != 1:
        raise AssertionError(f"{path.name} has {len(colors)} base_color elements, expected 1")
    parts = [float(value) for value in re.split(r"\s+", (colors[0].text or "").strip()) if value]
    if len(parts) not in (3, 4):
        raise AssertionError(f"{path.name} base_color has {len(parts)} components")
    return tuple(parts[:3])


class ModelMaterialsTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads(
            (PROJECT_ROOT / "assets" / "models" / "model-materials.json").read_text())
        self.by_id = {entry["modelId"]: entry for entry in self.document["models"]}

    def test_every_single_material_model_is_present_and_matches_its_mc3_source(self):
        for model_id in SINGLE_MATERIAL_MODELS:
            with self.subTest(model=model_id):
                source = PROJECT_ROOT / "assets" / "source" / "mc3" / f"{model_id}.mc3.xml"
                self.assertTrue(source.is_file(), f"missing MC3 source for {model_id}")
                expected = mc3_base_color(source)
                self.assertIn(model_id, self.by_id,
                              f"{model_id} has an MC3 base colour but no shipped entry")
                actual = tuple(self.by_id[model_id]["baseColor"])
                for channel, (want, got) in enumerate(zip(expected, actual)):
                    self.assertAlmostEqual(
                        want, got, places=4,
                        msg=f"{model_id} channel {channel}: MC3 says {want}, shipped table says {got}")

    def test_the_table_holds_nothing_it_cannot_justify(self):
        # An entry with no MC3 source behind it is either a typo or a colour someone invented; both
        # defeat the point of mirroring the source of truth.
        for model_id in self.by_id:
            with self.subTest(model=model_id):
                self.assertIn(model_id, SINGLE_MATERIAL_MODELS,
                              f"{model_id} is not a known single-material imported model")

    def test_the_file_is_well_formed(self):
        self.assertEqual(self.document["version"], 1)
        self.assertEqual(set(self.document), {"version", "models"})
        seen = set()
        for entry in self.document["models"]:
            self.assertEqual(set(entry), {"modelId", "baseColor"})
            self.assertNotIn(entry["modelId"], seen, "duplicate model id")
            seen.add(entry["modelId"])
            self.assertEqual(len(entry["baseColor"]), 3)
            for component in entry["baseColor"]:
                self.assertGreaterEqual(component, 0.0)
                self.assertLessEqual(component, 1.0)


if __name__ == "__main__":
    if PROJECT_ROOT is None or not PROJECT_ROOT.is_dir():
        print("usage: test_model_materials.py <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
