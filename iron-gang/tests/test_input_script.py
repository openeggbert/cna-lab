#!/usr/bin/env python3
"""End-to-end check for plan_30 IG-30-012: a recorded repro case drives the real game.

Until this existed, no automated run ever pressed a key. `--smoke` renders frames and lets the
world tick, but every input-driven path -- advancing dialogue, skipping a cutscene, entering the
sedan -- was reachable only through unit tests calling the systems directly. This runs the real
binary against a committed repro script and checks the game reached the states that input causes.

The script is written in **simulation updates**, not draw frames, which is what makes it
reproducible: the fixed 60 Hz step is the same everywhere, while how many frames a run draws
depends entirely on how fast it renders.
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GAME = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None


class InputScriptTests(unittest.TestCase):
    def run_game(self, work: Path, *arguments: str, timeout: int = 300) -> subprocess.CompletedProcess:
        environment = dict(os.environ)
        environment["SDL_VIDEODRIVER"] = "offscreen"
        environment["SDL_AUDIODRIVER"] = "dummy"
        return subprocess.run(
            [str(GAME), *arguments],
            cwd=work,
            env=environment,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )

    def test_committed_repro_drives_the_prologue(self):
        script = PROJECT_ROOT / "tests" / "input-scripts" / "prologue_opening.inputscript.json"
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            result = self.run_game(work, "--play-input", str(script))
            self.assertEqual(result.returncode, 0, result.stderr)
            output = result.stdout + result.stderr

            # The run must end because the script ended, not because it hit some other limit --
            # and say so exactly once. Exit() only asks CNA to stop, so the updates already in
            # flight kept re-requesting it (and re-logging) until that was guarded.
            self.assertEqual(
                output.count('input script "prologue_opening" finished; exiting'), 1,
                "the end of the script must be requested once, not once per remaining update")

            # Every line of the conversation was reached -- the first two by the cutscene's own
            # dialogue track, the rest by the script's Confirm presses.
            for line in ("Iron City is quiet tonight",
                         "The sedan is outside",
                         "No heroics"):
                self.assertIn(line, output, f"the repro never reached the line {line!r}")

            # And the input-driven mission progression: dialogue finished, the player walked to the
            # sedan, and Interact put them in it.
            for transition in ("introduction -> reach_vehicle",
                               "reach_vehicle -> enter_vehicle",
                               "enter_vehicle -> drive_to_warehouse"):
                self.assertIn(transition, output,
                              f"the repro never produced the transition {transition!r}")

    def test_a_screenshot_can_be_pinned_to_a_scripted_moment(self):
        script = PROJECT_ROOT / "tests" / "input-scripts" / "prologue_opening.inputscript.json"
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            target = work / "moment.png"
            result = self.run_game(work, "--play-input", str(script),
                                   "--screenshot", str(target), "--screenshot-update", "480")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(target.is_file(), f"no screenshot at update 480:\n{result.stderr}")
            summary = json.loads((work / "moment.png.summary.json").read_text())
            self.assertGreater(summary["nonSkyFraction"], 0.01)
            # Update 480 is after Interact: the run must have been driving by then, which is what
            # makes this a reproducible moment rather than "whatever frame 40 happened to be".
            self.assertIn("enter_vehicle -> drive_to_warehouse", result.stdout + result.stderr)

    def test_recording_round_trips_through_the_real_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            recorded = work / "recorded.inputscript.json"
            result = self.run_game(work, "--smoke", "3", "--record-input", str(recorded),
                                   "--record-input-id", "smoke_repro")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(recorded.is_file(), f"nothing was recorded:\n{result.stderr}")

            document = json.loads(recorded.read_text())
            self.assertEqual(document["id"], "smoke_repro")
            self.assertEqual(document["version"], 1)
            self.assertGreaterEqual(len(document["steps"]), 1)
            # Nobody pressed anything, so a sparse recording is exactly one step.
            self.assertEqual(len(document["steps"]), 1,
                             "an input-free run must record one step, not one per update")

            # And the game must accept its own output.
            played = self.run_game(work, "--play-input", str(recorded))
            self.assertEqual(played.returncode, 0, played.stderr)
            self.assertIn('playing input script "smoke_repro"', played.stdout + played.stderr)

    def test_unusable_arguments_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "broken.json").write_text('{"id":"a","version":1,"steps":[]}')
            for arguments, expected in (
                (["--play-input"], "requires"),
                (["--play-input", str(work / "missing.json")], ""),
                (["--play-input", str(work / "broken.json")], "no steps"),
                (["--record-input"], "requires"),
                (["--play-input", str(work / "broken.json"),
                  "--record-input", str(work / "out.json")], "cannot be used together"),
                (["--screenshot", str(work / "a.png"), "--screenshot-update", "0"], "screenshot-update"),
                (["--screenshot-update", "5"], "screenshot"),
            ):
                with self.subTest(arguments=arguments):
                    result = self.run_game(work, "--smoke", "2", *arguments)
                    self.assertNotEqual(result.returncode, 0,
                                        f"{arguments} should have been rejected")
                    if expected:
                        self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_input_script.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
