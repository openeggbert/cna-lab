#!/usr/bin/env python3
"""plan_39 IG-39-022/023: complete the dialogue and mission, and save and load.

Both gates were plausibly true -- the owner played the game and the log showed mission transitions.
A log line is not a measurement, though: it says the mission changed state, not that the dialogue
was read through first, and certainly not that loading put the player back where they saved. These
drive committed input scripts and check the recorded state trace.
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

# The prologue's three lines, in the order assets/dialogues/prologue.dialogue.json authors them.
PROLOGUE_LINES = [
    "prologue.mara.quiet_tonight",
    "prologue.elias.take_the_sedan",
    "prologue.mara.no_heroics",
]


def run_traced(script: str, work: Path) -> list[dict]:
    environment = dict(os.environ)
    environment["SDL_VIDEODRIVER"] = "offscreen"
    environment["SDL_AUDIODRIVER"] = "dummy"
    trace = work / f"{script}.jsonl"
    if trace.exists():
        trace.unlink()
    result = subprocess.run(
        [str(GAME), "--assets", str(PROJECT_ROOT / "assets"),
         "--play-input", str(PROJECT_ROOT / "tests" / "input-scripts" / f"{script}.inputscript.json"),
         "--trace-state", str(trace), "--trace-state-every", "10"],
        cwd=work, env=environment, capture_output=True, text=True, timeout=900, check=False)
    if result.returncode != 0:
        raise AssertionError(f"{script} exited {result.returncode}:\n{result.stderr}")
    rows = [json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
    if not rows:
        raise AssertionError(f"{script} produced an empty state trace")
    return rows


def transitions(rows: list[dict], key) -> list:
    """The sequence of distinct values of key(row), in the order they first ran."""
    out = []
    for row in rows:
        value = key(row)
        if not out or out[-1] != value:
            out.append(value)
    return out


class DialogueAndMissionTests(unittest.TestCase):
    """IG-39-022."""

    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.rows = run_traced("mission_completion", Path(cls._directory.name))

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def test_every_prologue_line_is_shown_in_the_authored_order(self):
        shown = [line for line in transitions(self.rows, lambda row: row["dialogueLine"]) if line]
        self.assertEqual(shown, PROLOGUE_LINES,
                         f"the conversation must play through in order, got {shown}")

    def test_the_dialogue_finishes_before_the_mission_leaves_its_first_state(self):
        # The prologue's own rule: introduction -> reach_vehicle is gated on dialogue_finished.
        left_introduction = next(row["update"] for row in self.rows
                                 if row["missionState"] != "introduction")
        still_talking = [row["update"] for row in self.rows
                         if row["dialogueLine"] and row["update"] >= left_introduction]
        self.assertFalse(still_talking,
                         f"dialogue was still showing after the mission advanced, at {still_talking[:3]}")

    def test_the_mission_runs_through_its_states_in_order(self):
        states = transitions([r for r in self.rows if r["mission"] == "prototype_delivery"],
                             lambda row: row["missionState"])
        self.assertEqual(states,
                         ["introduction", "reach_vehicle", "enter_vehicle", "drive_to_warehouse"],
                         f"the prologue must progress through its states in order, got {states}")

    def test_completing_the_prologue_starts_the_next_mission(self):
        # The prologue never appears as "completed" in a trace because the campaign starts the next
        # mission on the same update. That handover IS the completion.
        missions = transitions(self.rows, lambda row: row["mission"])
        self.assertEqual(missions[:2], ["prototype_delivery", "countryside_run"],
                         f"finishing the delivery must start the next mission, got {missions}")


class SaveAndLoadTests(unittest.TestCase):
    """IG-39-023."""

    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.work = Path(cls._directory.name)
        cls.rows = run_traced("save_and_load", cls.work)

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def at(self, update: int) -> dict:
        return min(self.rows, key=lambda row: abs(row["update"] - update))

    def test_quick_save_writes_a_save_file(self):
        saves = list((self.work / "runtime").glob("*.save"))
        self.assertTrue(saves, "quick save must write a save file")
        self.assertGreater(saves[0].stat().st_size, 0, "the save file must not be empty")

    def test_the_run_actually_went_somewhere_between_saving_and_loading(self):
        # Without this the load could restore nothing and every assertion below would still pass.
        saved = self.at(760)
        before_load = self.at(1140)
        self.assertGreater(abs(before_load["z"] - saved["z"]), 25.0,
                           "the car must be far from the save point when the load happens")
        self.assertNotEqual(before_load["district"], saved["district"],
                            "the run should have crossed into another district before loading")

    def test_loading_restores_the_saved_position_district_and_speed(self):
        saved = self.at(760)
        restored = self.at(1150)
        self.assertLess(abs(restored["z"] - saved["z"]), 5.0,
                        f"loading must put the player back near the save point: saved z="
                        f"{saved['z']:.2f}, restored z={restored['z']:.2f}")
        self.assertLess(abs(restored["x"] - saved["x"]), 5.0, "the x position must be restored too")
        self.assertEqual(restored["district"], saved["district"],
                         "loading must return to the saved district")
        self.assertEqual(restored["driving"], saved["driving"],
                         "loading must restore whether the player was driving")
        self.assertLess(abs(restored["speedKph"] - saved["speedKph"]), 3.0,
                        f"loading must restore the vehicle's speed: saved "
                        f"{saved['speedKph']:.1f} km/h, restored {restored['speedKph']:.1f} km/h")


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_gate_mission.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
