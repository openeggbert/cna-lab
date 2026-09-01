#!/usr/bin/env python3

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/content_budget.py")
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else Path.cwd()


def policy(source: str, triangles: int = 48) -> dict:
    return {
        "schema_version": 1,
        "assets": [
            {
                "id": "fixture",
                "category": "building",
                "sources": [source],
                "baseline": {"triangles": 12, "materials": 1, "textures": 0},
                "limits": {"triangles": triangles, "materials": 4, "textures": 4},
            }
        ],
    }


class ContentBudgetTests(unittest.TestCase):
    def run_validator(
        self,
        project_root: Path,
        policy_path: Path,
        *arguments: str,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--project-root",
                str(project_root),
                "--policy",
                str(policy_path),
                *arguments,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_committed_policy_matches_real_sources(self) -> None:
        result = self.run_validator(
            PROJECT_ROOT,
            PROJECT_ROOT / "assets/content-budgets.json",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PASS warehouse [building]: triangles 12/48", result.stdout)
        self.assertIn("PASS sedan [vehicle]: triangles 48/192", result.stdout)
        self.assertIn("PASS test_character [character]: triangles 36/144", result.stdout)

    def test_mc3_budget_failure_is_actionable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "box.mc3.xml"
            source.write_text(
                "<mc3><materials><material id='m'/></materials>"
                "<objects><box material='m'/><box material='m'/></objects></mc3>",
                encoding="utf-8",
            )
            policy_path = root / "policy.json"
            policy_path.write_text(json.dumps(policy("box.mc3.xml", triangles=12)), encoding="utf-8")
            result = self.run_validator(root, policy_path)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("FAIL fixture [building]: triangles 24/12", result.stdout)
            self.assertIn("triangles 24 exceeds limit 12", result.stdout)

    def test_unknown_mc3_primitive_is_never_guessed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "sphere.mc3.xml"
            source.write_text("<mc3><objects><sphere/></objects></mc3>", encoding="utf-8")
            policy_path = root / "policy.json"
            policy_path.write_text(json.dumps(policy("sphere.mc3.xml")), encoding="utf-8")
            result = self.run_validator(root, policy_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("unsupported MC3 primitive <sphere>", result.stderr)
            self.assertIn("add an exact triangulation rule", result.stderr)

    def test_gltf_triangle_count_and_source_coverage_are_validated(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "mesh.gltf"
            source.write_text(
                json.dumps(
                    {
                        "accessors": [{"count": 4}],
                        "meshes": [
                            {"primitives": [{"indices": 0, "mode": 4}]}
                        ],
                        "materials": [],
                        "textures": [],
                    }
                ),
                encoding="utf-8",
            )
            policy_path = root / "policy.json"
            policy_path.write_text(json.dumps(policy("mesh.gltf")), encoding="utf-8")
            result = self.run_validator(root, policy_path)
            self.assertEqual(result.returncode, 2)
            self.assertIn("TRIANGLES element count 4 is not divisible by 3", result.stderr)

            unknown = root / "unbudgeted.gltf"
            unknown.write_text("{}", encoding="utf-8")
            result = self.run_validator(root, policy_path, "--source", str(unknown))
            self.assertEqual(result.returncode, 2)
            self.assertIn("no content-budget entry covers", result.stderr)



    def test_every_shipped_mc3_source_has_a_budget_entry(self):
        """plan_09 IG-09-005: a new MC3 prop with no budget entry must not slip in silently.

        content_budget.py only checks the group containing a source it is *given*, and
        build-assets.sh only gives it the file being built. So an MC3 file nobody has built yet is
        unbudgeted and nothing says so -- which is how the street lamp arrived.
        """
        root = pathlib.Path(__file__).resolve().parent.parent
        policy = json.loads((root / "assets" / "content-budgets.json").read_text())
        covered = {source for asset in policy["assets"] for source in asset["sources"]}
        sources = sorted((root / "assets" / "source" / "mc3").glob("*.mc3.xml"))
        self.assertGreaterEqual(len(sources), 6, "the shipped MC3 sources must be found")
        for source in sources:
            relative = source.relative_to(root).as_posix()
            with self.subTest(source=relative):
                self.assertIn(relative, covered,
                              f"{relative} has no entry in assets/content-budgets.json")



    def test_measured_metrics_match_the_recorded_baseline(self):
        """A ceiling alone cannot catch a counter that stopped counting.

        Budgets only assert measured <= limit, so a bug that reports zero triangles passes every
        budget in the file -- which is exactly what happened when the <instance> expansion was
        stubbed out during review. The baseline is the *committed measurement*, so requiring
        measurement == baseline turns it into a two-sided check and makes a content edit update the
        number it claims.
        """
        root = pathlib.Path(__file__).resolve().parent.parent
        sys.path.insert(0, str(root / "scripts"))
        import content_budget

        policy = json.loads((root / "assets" / "content-budgets.json").read_text())
        for asset in policy["assets"]:
            mc3 = [s for s in asset["sources"] if s.endswith(".mc3.xml")]
            if not mc3:
                continue
            with self.subTest(asset=asset["id"]):
                measured = content_budget.AssetMetrics(0, 0, 0)
                for source in mc3:
                    measured = measured + content_budget.inspect_mc3(root / source)
                self.assertEqual(measured.triangles, asset["baseline"]["triangles"],
                                 f"{asset['id']}: measured triangles differ from the recorded "
                                 f"baseline; update assets/content-budgets.json if the change is "
                                 f"intended")
                self.assertEqual(measured.materials, asset["baseline"]["materials"],
                                 f"{asset['id']}: measured materials differ from the baseline")

    def test_a_definition_that_instances_itself_is_refused(self):
        """Without the cycle guard this recurses until the stack ends."""
        root = pathlib.Path(__file__).resolve().parent.parent
        sys.path.insert(0, str(root / "scripts"))
        import content_budget

        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "cycle.mc3.xml"
            path.write_text(
                '<?xml version="1.0"?>\n'
                '<mc3 version="0.3" model="Cycle">'
                '<definitions>'
                '<definition id="a"><group name="A"><instance name="I" definition="a"/></group></definition>'
                '</definitions>'
                '<objects><instance name="Root" definition="a"/></objects>'
                '</mc3>')
            with self.assertRaises(content_budget.BudgetError) as caught:
                content_budget.inspect_mc3(path)
            self.assertIn("instances itself", str(caught.exception))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
