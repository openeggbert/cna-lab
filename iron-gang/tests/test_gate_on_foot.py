#!/usr/bin/env python3
"""plan_39 IG-39-020: verify the on-foot controls, by measurement rather than by looking.

Gate M1 asks for the controls to be *verified*. Until there was a state trace the only observable
outputs were log lines and screenshots: a log line says a mission changed state, and a screenshot
says what one frame looked like. Neither says the player moved when a key was held, or that they
stopped at a lamp post rather than beside one.

So each control is driven by a committed input script, keyed on simulation updates, and checked
against the positions the run recorded.
"""

import json
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

GAME = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

# The player spawns facing yaw 0, which is -Z (ForwardFromYaw's convention).
FORWARD_AXIS = "z"


def run_traced(script: str, work: Path, assets: Path | None = None, label: str = "") -> list[dict]:
    environment = dict(os.environ)
    environment["SDL_VIDEODRIVER"] = "offscreen"
    environment["SDL_AUDIODRIVER"] = "dummy"
    # A distinct file per run: --trace-state APPENDS, so two runs sharing a path produce one file
    # holding both, and every "the final record" query silently answers about the first run.
    trace = work / f"{script}{label}.jsonl"
    if trace.exists():
        trace.unlink()
    command = [str(GAME),
               "--play-input", str(PROJECT_ROOT / "tests" / "input-scripts" / f"{script}.inputscript.json"),
               "--trace-state", str(trace), "--trace-state-every", "10"]
    if assets is not None:
        command[1:1] = ["--assets", str(assets)]
    result = subprocess.run(command, cwd=work, env=environment, capture_output=True, text=True,
                            timeout=600, check=False)
    if result.returncode != 0:
        raise AssertionError(f"{script} exited {result.returncode}:\n{result.stderr}")
    rows = [json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
    if not rows:
        raise AssertionError(f"{script} produced an empty state trace")
    return rows


def nearest(rows: list[dict], update: int) -> dict:
    return min(rows, key=lambda row: abs(row["update"] - update))


class OnFootControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.work = Path(cls._directory.name)
        cls.rows = run_traced("on_foot_controls", cls.work)

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def delta(self, start: int, end: int, key: str) -> float:
        return nearest(self.rows, end)[key] - nearest(self.rows, start)[key]

    def test_the_run_stayed_on_foot(self):
        # Every assertion below is about walking; if the player got into the car the segments would
        # measure the sedan instead.
        self.assertFalse(any(row["driving"] for row in self.rows), "the run must stay on foot")

    def test_forward_and_back_move_along_the_facing_axis(self):
        forward = self.delta(150, 240, FORWARD_AXIS)
        back = self.delta(300, 390, FORWARD_AXIS)
        # Facing yaw 0 is -Z, so forward decreases z and back increases it.
        self.assertLess(forward, -2.0, f"holding forward must move the player forward, got {forward}")
        self.assertGreater(back, 2.0, f"holding back must move the player backward, got {back}")

    def test_strafing_moves_sideways_without_turning(self):
        right = self.delta(450, 510, "x")
        left = self.delta(570, 600, "x")
        self.assertGreater(right, 1.0, f"strafe right must move +X while facing -Z, got {right}")
        self.assertLess(left, -0.5, f"strafe left must move -X, got {left}")
        # Strafing is not turning: the whole point of having both.
        self.assertAlmostEqual(self.delta(450, 510, "yaw"), 0.0, places=3,
                               msg="strafing must not change the facing")

    def test_turning_changes_the_facing_without_moving(self):
        left = self.delta(660, 750, "yaw")
        right = self.delta(810, 900, "yaw")
        self.assertLess(left, -1.0, f"turn left must decrease yaw, got {left}")
        self.assertGreater(right, 1.0, f"turn right must increase yaw, got {right}")
        for start, end in ((660, 750), (810, 900)):
            self.assertAlmostEqual(self.delta(start, end, "x"), 0.0, places=2,
                                   msg="turning on the spot must not translate the player")
            self.assertAlmostEqual(self.delta(start, end, FORWARD_AXIS), 0.0, places=2,
                                   msg="turning on the spot must not translate the player")

    def test_sprinting_covers_more_ground_than_walking(self):
        walk = abs(self.delta(1070, 1130, FORWARD_AXIS))
        sprint = abs(self.delta(1190, 1250, FORWARD_AXIS))
        self.assertGreater(walk, 1.0, "the walk segment must actually walk")
        self.assertGreater(sprint, walk * 1.25,
                           f"sprinting must be clearly faster than walking: walk {walk:.2f} m vs "
                           f"sprint {sprint:.2f} m over the same 60 updates")


class LampCollisionTests(unittest.TestCase):
    """plan_14 IG-14-012's missing evidence: somebody walks into a lamp post.

    The proxies were asserted to exist, to be the right boxes, and to reach the collider list. That
    the player then *stops* was inferred. This measures it, by running the same script twice against
    asset trees that differ only in whether the collision sidecar is present.
    """

    def test_the_proxy_stops_the_player_short_of_the_wall_behind_it(self):
        sidecar_directory = PROJECT_ROOT / "assets" / "generated" / "models" / "collision"
        if not any(sidecar_directory.glob("*.collision.json")):
            self.skipTest("no generated collision sidecar; run scripts/build-assets.sh")

        with TemporaryDirectory() as directory:
            work = Path(directory)
            stripped = work / "assets-without-proxies"
            shutil.copytree(PROJECT_ROOT / "assets", stripped)
            for sidecar in (stripped / "generated" / "models" / "collision").glob("*.collision.json"):
                sidecar.unlink()

            with_proxies = run_traced("lamp_collision", work, label="-with")
            without_proxies = run_traced("lamp_collision", work, assets=stripped, label="-without")

        final_with = max(with_proxies, key=lambda row: row["update"])
        final_without = max(without_proxies, key=lambda row: row["update"])

        # Both runs must end at the same point along the approach, or they are not comparable.
        self.assertAlmostEqual(final_with["z"], final_without["z"], places=2,
                               msg="the two runs must be aligned with the same lamp")

        # The lamp post stands one metre in front of the hotel wall. With the proxy the player stops
        # against the post; without it they walk through and stop against the building.
        self.assertGreater(final_with["x"], final_without["x"] + 0.8,
                           f"the collision proxy must stop the player short of the wall behind it: "
                           f"with proxies x={final_with['x']:.3f}, without x={final_without['x']:.3f}")
        self.assertLess(final_with["x"], -7.5, "the player must still have reached the lamp")


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_gate_on_foot.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
