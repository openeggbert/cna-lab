#!/usr/bin/env python3
"""plan_39 IG-39-024: reset the prototype, and quit from the pause menu.

Both are one keypress, which is exactly why they are worth measuring: a reset that reloads the
district but leaves the mission mid-flight, or a quit that closes the window without ending the
process, both look fine from outside.
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

# PrototypeWorld's authored player spawn for the warehouse block.
SPAWN_X = 0.0
SPAWN_Z = 20.0


def run_traced(script: str, work: Path) -> tuple[list[dict], subprocess.CompletedProcess]:
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
    rows = [json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
    return rows, result


def script_last_update(script: str) -> int:
    document = json.loads(
        (PROJECT_ROOT / "tests" / "input-scripts" / f"{script}.inputscript.json").read_text())
    return max(step["update"] for step in document["steps"])


class ResetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.rows, cls.result = run_traced("reset_prototype", Path(cls._directory.name))

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def at(self, update: int) -> dict:
        return min(self.rows, key=lambda row: abs(row["update"] - update))

    def test_the_run_exits_cleanly(self):
        self.assertEqual(self.result.returncode, 0, self.result.stderr)

    def test_there_was_something_to_reset(self):
        # Without this, a reset that did nothing would satisfy every assertion below on a run that
        # had never left the spawn.
        before = self.at(770)
        self.assertTrue(before["driving"], "the player must be driving before the reset")
        self.assertGreater(abs(before["z"] - SPAWN_Z), 20.0,
                           "the player must be well away from the spawn before the reset")
        self.assertEqual(before["missionState"], "drive_to_warehouse",
                         "the mission must have progressed before the reset")

    def test_reset_returns_the_player_to_the_spawn_on_foot(self):
        after = self.at(800)
        self.assertAlmostEqual(after["x"], SPAWN_X, places=1,
                               msg=f"reset must return the player to the spawn, got x={after['x']}")
        self.assertAlmostEqual(after["z"], SPAWN_Z, places=1,
                               msg=f"reset must return the player to the spawn, got z={after['z']}")
        self.assertFalse(after["driving"], "reset must put the player back on foot")

    def test_reset_restarts_the_mission_rather_than_only_moving_the_player(self):
        self.assertEqual(self.at(800)["missionState"], "introduction",
                         "reset must take the mission back to its first state")
        self.assertEqual(self.at(800)["district"], "warehouse_block",
                         "reset must return to the first district")

    def test_the_reset_state_persists(self):
        # A reset that is immediately undone by the still-running input would be worse than none.
        end = self.at(900)
        self.assertEqual(end["missionState"], "introduction")
        self.assertAlmostEqual(end["z"], SPAWN_Z, places=1)


class QuitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.rows, cls.result = run_traced("pause_and_quit", Path(cls._directory.name))

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def test_quitting_ends_the_process_cleanly(self):
        self.assertEqual(self.result.returncode, 0, self.result.stderr)

    def test_the_game_stops_before_the_script_does(self):
        # The script runs to update 600; Quit is activated around 400. If the game had ignored the
        # menu the run would simply have played out to the end, so "stopped early" is the evidence.
        last_traced = max(row["update"] for row in self.rows)
        last_scripted = script_last_update("pause_and_quit")
        self.assertLess(last_traced, last_scripted - 100,
                        f"the game must stop when Quit is chosen: traced to {last_traced}, script "
                        f"runs to {last_scripted}")
        self.assertGreater(last_traced, 390, "the run must at least reach the menu activation")

    def test_the_run_ended_by_quitting_and_not_by_the_script_running_out(self):
        # The two exits are indistinguishable by return code, so distinguish them by the log line
        # the script-finished path prints.
        output = self.result.stdout + self.result.stderr
        self.assertNotIn("finished; exiting", output,
                         "the run ended because the script ran out, not because Quit was chosen")


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_gate_reset_quit.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
