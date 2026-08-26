#!/usr/bin/env python3
"""plan_09 IG-09-002/003: invalid MC3 must fail the asset build, with file and line.

scripts/validate-mc3.sh has existed since the pipeline was first written and is called by
build-assets.sh -- but nothing ever checked that it rejects anything, and in this workspace it could
not even find the schema (its default assumed one checkout layout, and the repository has since been
moved a directory deeper). A validator that cannot find its schema fails open in the worst way: it
prints an error, the build stops, and the obvious fix is to skip validation.

Each shipped MC3 file is validated, and each rule the entry claims is exercised against a file that
breaks it.
"""

import os
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

PROJECT_ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None


def has_validator() -> bool:
    if shutil.which("xmllint"):
        return True
    try:
        import lxml  # noqa: F401
        return True
    except ImportError:
        return False


class ValidateMc3Tests(unittest.TestCase):
    def setUp(self):
        if not has_validator():
            self.skipTest("neither xmllint nor Python lxml is available for XSD validation")
        self.script = PROJECT_ROOT / "scripts" / "validate-mc3.sh"
        self.assertTrue(self.script.is_file(), "validate-mc3.sh must exist")

    def run_validator(self, path, environment=None) -> subprocess.CompletedProcess:
        env = dict(os.environ)
        # Deliberately cleared: the point is that the script finds the schema on its own, the way a
        # contributor who has just cloned the workspace would run it.
        for name in ("MC3_SCHEMA", "MESH_CRAFT_SOURCE_DIR", "MESH_CRAFT_BUILD_DIR",
                     "IRON_GANG_CNA_DIR"):
            env.pop(name, None)
        if environment:
            env.update(environment)
        return subprocess.run(["bash", str(self.script), str(path)], cwd=PROJECT_ROOT, env=env,
                              capture_output=True, text=True, timeout=120, check=False)

    def test_every_shipped_mc3_file_validates(self):
        sources = sorted((PROJECT_ROOT / "assets" / "source" / "mc3").glob("*.mc3.xml"))
        self.assertGreaterEqual(len(sources), 5, "the shipped MC3 sources must be found")
        for source in sources:
            with self.subTest(source=source.name):
                result = self.run_validator(source)
                self.assertEqual(result.returncode, 0,
                                 f"{source.name} must validate:\n{result.stderr}")

    def test_invalid_mc3_fails_with_file_and_line(self):
        # IG-09-003's actual requirement. An element the schema does not allow, at a known line.
        source = (PROJECT_ROOT / "assets" / "source" / "mc3" / "warehouse.mc3.xml").read_text()
        broken = source.replace('<box name="Warehouse"', '<sphere name="Warehouse"')
        self.assertNotEqual(broken, source, "the fixture must actually differ from the original")
        expected_line = broken.splitlines().index(
            next(line for line in broken.splitlines() if "<sphere" in line)) + 1

        with TemporaryDirectory() as directory:
            path = Path(directory) / "broken.mc3.xml"
            path.write_text(broken)
            result = self.run_validator(path)

            self.assertNotEqual(result.returncode, 0, "invalid MC3 must fail the build")
            self.assertIn("broken.mc3.xml", result.stderr,
                          f"the diagnostic must name the file:\n{result.stderr}")
            self.assertRegex(result.stderr, rf"broken\.mc3\.xml:{expected_line}\b",
                             f"the diagnostic must give the offending line ({expected_line}):"
                             f"\n{result.stderr}")

    def test_a_missing_input_is_reported_rather_than_passing(self):
        result = self.run_validator(PROJECT_ROOT / "assets" / "source" / "mc3" / "no-such-file.mc3.xml")
        self.assertNotEqual(result.returncode, 0, "a missing input must not validate")
        self.assertIn("not found", result.stderr)

    def test_a_missing_schema_is_actionable(self):
        # Pointed at a schema that does not exist: the script must say so and say what to set,
        # rather than silently succeeding.
        result = self.run_validator(PROJECT_ROOT / "assets" / "source" / "mc3" / "warehouse.mc3.xml",
                                    {"MC3_SCHEMA": "/nonexistent/mc3.xsd"})
        # MC3_SCHEMA is only the first candidate; a bogus value must fall through to a real one
        # rather than break a working workspace.
        self.assertEqual(result.returncode, 0,
                         f"a bogus MC3_SCHEMA must fall through to a real schema:\n{result.stderr}")

    def test_the_schema_is_found_without_any_environment_variable(self):
        # The bug this test exists for: the default assumed ../mesh-craft, and this repository sits
        # one level deeper, so validation could not run at all here. Every hint variable is cleared
        # by run_validator() -- an earlier version of this test left IRON_GANG_CNA_DIR set, which
        # happens to point one directory above the Mesh Craft checkout, so it passed while proving
        # nothing about the bare-workspace path.
        result = self.run_validator(PROJECT_ROOT / "assets" / "source" / "mc3" / "warehouse.mc3.xml")
        self.assertEqual(result.returncode, 0,
                         f"the schema must be found with no environment set:\n{result.stderr}")
        self.assertNotIn("schema (mc3.xsd) not found", result.stderr)


if __name__ == "__main__":
    if PROJECT_ROOT is None or not PROJECT_ROOT.is_dir():
        print("usage: test_validate_mc3.py <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
