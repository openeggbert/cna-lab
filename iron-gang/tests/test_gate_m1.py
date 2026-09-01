#!/usr/bin/env python3
"""plan_39 IG-39-002: gate M1 as one run.

"Builds, launches, completes its mission, saves, loads, and passes CTest." Each of those already
has its own gate script, but each ran in its own process. This is the claim the gate actually
makes: that they compose -- that you can play the mission to completion, save *after* the campaign
has moved on, drive away, load back, and reset, all in one session.

Composing them is not a formality. Doing so immediately found a save refused mid district-transition
that no isolated gate had hit, and that refusal was invisible outside the running HUD.
"""

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

GAME = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

SPAWN_Z = 20.0
PROLOGUE_LINES = [
    "prologue.mara.quiet_tonight",
    "prologue.elias.take_the_sedan",
    "prologue.mara.no_heroics",
]


class GateM1Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.work = Path(cls._directory.name)
        trace = cls.work / "gate_m1.jsonl"
        # A fresh file: --trace-state appends, and a stale trace from a previous run reads as a
        # perfectly plausible result. That has caught us twice.
        if trace.exists():
            trace.unlink()
        environment = dict(os.environ)
        environment["SDL_VIDEODRIVER"] = "offscreen"
        environment["SDL_AUDIODRIVER"] = "dummy"
        cls.result = subprocess.run(
            [str(GAME), "--assets", str(PROJECT_ROOT / "assets"),
             "--play-input", str(PROJECT_ROOT / "tests" / "input-scripts" / "gate_m1.inputscript.json"),
             "--trace-state", str(trace), "--trace-state-every", "5"],
            cwd=cls.work, env=environment, capture_output=True, text=True, timeout=900, check=False)
        cls.rows = [json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
        cls.output = cls.result.stdout + cls.result.stderr

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def at(self, update: int) -> dict:
        return min(self.rows, key=lambda row: abs(row["update"] - update))

    def test_the_session_runs_to_the_end_and_exits_cleanly(self):
        self.assertEqual(self.result.returncode, 0, self.result.stderr)
        self.assertIn('finished; exiting', self.output,
                      "the session must play the whole script, not stop early")
        self.assertGreater(len(self.rows), 100, "the run must produce a substantial trace")

    def test_nothing_was_refused_or_failed_along_the_way(self):
        # The composed run's own discovery: a save can be refused, and before this gate existed the
        # only sign was three seconds of on-screen text.
        for phrase in ("save refused", "Save failed", "load failed", "Load failed"):
            self.assertNotIn(phrase, self.output, f"the session reported: {phrase}")

    def test_the_only_missing_district_data_is_the_countryside(self):
        # Not a pass disguised as one: the countryside genuinely has no authored road, pavement, or
        # prop-collision data and falls back to built-in layouts. M1 is the warehouse block, so that
        # does not fail this gate -- but it is pinned here so that authoring the countryside, or a
        # *new* district quietly falling back, both show up as a failure of this test.
        fallbacks = sorted(line.split("File not found: ")[1].split(" -- ")[0].rsplit("/", 1)[-1]
                           for line in self.output.splitlines() if "File not found: " in line)
        self.assertEqual(fallbacks, ["countryside.roads.json", "countryside.sidewalks.json",
                                     "countryside_props.collision.json"],
                         f"unexpected missing district data: {fallbacks}")

    def test_the_prologue_is_played_and_completed(self):
        shown = []
        for row in self.rows:
            line = row["dialogueLine"]
            if line and (not shown or shown[-1] != line):
                shown.append(line)
        self.assertEqual(shown[:3], PROLOGUE_LINES,
                         f"the conversation must play in order, got {shown}")
        # The reset at the end restarts the mission, so the opening plays again. That is the
        # behaviour, not a duplicate: a reset that skipped the cutscene would leave the player in a
        # freshly started mission with no idea what it is.
        self.assertEqual(shown[3:], PROLOGUE_LINES[:1],
                         f"the reset must replay the opening, got {shown[3:]}")

        missions = []
        for row in self.rows:
            if not missions or missions[-1] != row["mission"]:
                missions.append(row["mission"])
        self.assertEqual(missions[:2], ["prototype_delivery", "countryside_run"],
                         f"the delivery must complete and hand over, got {missions}")

    def test_the_session_crosses_a_district_boundary(self):
        districts = {row["district"] for row in self.rows}
        self.assertEqual(districts, {"warehouse_block", "countryside"},
                         f"the session must visit both districts, saw {districts}")

    def test_saving_writes_a_file_after_the_campaign_has_moved_on(self):
        saves = list((self.work / "runtime").glob("*.save"))
        self.assertTrue(saves, "the session must leave a save file")
        self.assertIn("saved to", self.output, "the save must be recorded")
        saved = self.at(1020)
        self.assertEqual(saved["mission"], "countryside_run",
                         "the save must happen after the first mission completed, which is the "
                         "case an isolated save/load gate never reaches")

    def test_loading_returns_to_the_save_point(self):
        saved = self.at(1020)
        before = self.at(1410)
        after = self.at(1420)
        self.assertGreater(abs(before["z"] - saved["z"]), 20.0,
                           "the run must drive well away before loading")
        self.assertLess(abs(after["z"] - saved["z"]), 5.0,
                        f"loading must return to the save point: saved z={saved['z']:.2f}, "
                        f"before load z={before['z']:.2f}, after load z={after['z']:.2f}")
        self.assertIn("loaded ", self.output, "the load must be recorded")

    def test_resetting_returns_to_the_spawn_in_the_first_district(self):
        before = self.at(1580)
        after = self.at(1620)
        self.assertTrue(before["driving"], "the player must be driving before the reset")
        self.assertFalse(after["driving"], "reset must put the player back on foot")
        self.assertAlmostEqual(after["z"], SPAWN_Z, places=1,
                               msg=f"reset must return to the spawn, got z={after['z']}")
        self.assertEqual(after["district"], "warehouse_block",
                         "reset must return to the first district")


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_gate_m1.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
