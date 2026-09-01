#!/usr/bin/env python3
"""plan_09 IG-09-006/007: the MC3 conventions checker must catch each rule it claims.

The repository already follows every rule, so running the checker against the real tree proves only
that it does not crash. Each rule is therefore exercised against a fixture that breaks exactly that
rule -- otherwise a stale tag list or a typo'd allowed value would report "follow the conventions"
forever.
"""

import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

CHECKER = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

sys.path.insert(0, str(CHECKER.parent) if CHECKER else "scripts")
import check_mc3_conventions as conventions  # noqa: E402


def document(objects: str, definitions: str = "", attributes: str = "") -> str:
    definitions_block = f"<definitions>{definitions}</definitions>" if definitions else ""
    return (
        '<?xml version="1.0"?>\n'
        f'<mc3 version="0.3" model="Fixture" unit="meter" '
        f'coordinate_system="right_handed_y_up"{attributes}>'
        f"{definitions_block}"
        f"<objects>{objects}</objects>"
        "</mc3>"
    )


class Mc3ConventionTests(unittest.TestCase):
    def setUp(self):
        self._directory = TemporaryDirectory()
        self.path = Path(self._directory.name) / "fixture.mc3.xml"

    def tearDown(self):
        self._directory.cleanup()

    def check(self, text: str) -> list[str]:
        self.path.write_text(text)
        violations, examined = conventions.check_file(self.path)
        self.examined = examined
        return violations

    def test_a_conforming_document_passes(self):
        self.assertEqual(self.check(document('<box name="A" collision="static"/>')), [])
        self.assertEqual(self.examined, 1)

    def test_an_unstated_collision_is_caught(self):
        # MC3 defaults collision to "none", so silence and a deliberate "none" look identical.
        violations = self.check(document('<box name="A"/>'))
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("does not state a collision role", violations[0])

    def test_a_collision_value_outside_the_allowed_set_is_caught(self):
        # "box" is MC3's own vocabulary -- a SHAPE. Iron Gang's is a ROLE, and mixing them silently
        # is exactly what this rule exists to stop.
        violations = self.check(document('<box name="A" collision="box"/>'))
        self.assertEqual(len(violations), 1, violations)
        self.assertIn('collision="box"', violations[0])
        self.assertIn("static", violations[0], "the message must name the allowed values")

    def test_every_allowed_value_is_accepted(self):
        for value in conventions.ALLOWED_COLLISION:
            with self.subTest(value=value):
                self.assertEqual(self.check(document(f'<box name="A" collision="{value}"/>')), [])

    def test_geometry_inside_a_definition_is_checked(self):
        # The rule is worthless if a prop can hide its geometry in a definition.
        violations = self.check(document(
            '<instance name="I" definition="d"/>',
            definitions='<definition id="d"><group name="G"><box name="Inner"/></group></definition>'))
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("definition 'd'", violations[0])
        self.assertEqual(self.examined, 1)

    def test_geometry_inside_a_group_is_checked(self):
        violations = self.check(document('<group name="G"><box name="Inner" collision="box"/></group>'))
        self.assertEqual(len(violations), 1, violations)

    def test_a_definition_without_an_id_is_caught(self):
        violations = self.check(document(
            '<box name="A" collision="none"/>',
            definitions='<definition><group name="G"><box name="B" collision="none"/></group></definition>'))
        self.assertTrue(any("no id" in v for v in violations), violations)

    def test_wrong_units_or_axes_are_caught(self):
        self.path.write_text(
            '<?xml version="1.0"?><mc3 version="0.3" model="F" unit="centimeter" '
            'coordinate_system="right_handed_y_up"><objects>'
            '<box name="A" collision="none"/></objects></mc3>')
        violations, _ = conventions.check_file(self.path)
        self.assertTrue(any("centimeter" in v for v in violations), violations)

        self.path.write_text(
            '<?xml version="1.0"?><mc3 version="0.3" model="F" unit="meter" '
            'coordinate_system="left_handed_y_up"><objects>'
            '<box name="A" collision="none"/></objects></mc3>')
        violations, _ = conventions.check_file(self.path)
        self.assertTrue(any("left_handed_y_up" in v for v in violations), violations)

    def test_a_missing_model_name_is_caught(self):
        self.path.write_text(
            '<?xml version="1.0"?><mc3 version="0.3" unit="meter" '
            'coordinate_system="right_handed_y_up"><objects>'
            '<box name="A" collision="none"/></objects></mc3>')
        violations, _ = conventions.check_file(self.path)
        self.assertTrue(any("no model name" in v for v in violations), violations)

    def test_a_run_that_examines_nothing_is_refused(self):
        # The failure mode where GEOMETRY_TAGS goes stale and the checker quietly stops policing.
        self.path.write_text(document(""))
        with self.assertRaises(conventions.ConventionError):
            conventions.check([self.path])
        with self.assertRaises(conventions.ConventionError):
            conventions.check([])

    def test_the_real_repository_follows_the_conventions(self):
        sources = sorted((PROJECT_ROOT / "assets" / "source" / "mc3").glob("*.mc3.xml"))
        self.assertGreaterEqual(len(sources), 6, "the shipped MC3 sources must be found")
        self.assertEqual(conventions.check(sources), [])


if __name__ == "__main__":
    if CHECKER is None or PROJECT_ROOT is None:
        print("usage: test_mc3_conventions.py <check_mc3_conventions.py> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
