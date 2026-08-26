#!/usr/bin/env python3
"""plan_39 IG-39-021: verify the vehicle controls, by measurement.

The same argument as the on-foot gate: a screenshot of a car does not say the throttle accelerated
it, and a log line does not say the handbrake did anything. Each control is driven by a committed
input script keyed on simulation updates, and checked against the positions and speeds the run
recorded.
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


def run_traced(script: str, work: Path) -> list[dict]:
    environment = dict(os.environ)
    environment["SDL_VIDEODRIVER"] = "offscreen"
    environment["SDL_AUDIODRIVER"] = "dummy"
    trace = work / f"{script}.jsonl"
    if trace.exists():
        trace.unlink()
    result = subprocess.run(
        [str(GAME),
         "--play-input", str(PROJECT_ROOT / "tests" / "input-scripts" / f"{script}.inputscript.json"),
         "--trace-state", str(trace), "--trace-state-every", "10"],
        cwd=work, env=environment, capture_output=True, text=True, timeout=900, check=False)
    if result.returncode != 0:
        raise AssertionError(f"{script} exited {result.returncode}:\n{result.stderr}")
    rows = [json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
    if not rows:
        raise AssertionError(f"{script} produced an empty state trace")
    return rows


class VehicleControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._directory = TemporaryDirectory()
        cls.rows = run_traced("vehicle_controls", Path(cls._directory.name))

    @classmethod
    def tearDownClass(cls):
        cls._directory.cleanup()

    def at(self, update: int) -> dict:
        return min(self.rows, key=lambda row: abs(row["update"] - update))

    def window(self, start: int, end: int) -> list[dict]:
        return [row for row in self.rows if start <= row["update"] <= end]

    def test_the_player_actually_gets_into_the_car(self):
        # Every other assertion is about a vehicle. If Interact failed they would measure a
        # pedestrian, and most of them would still pass.
        driving = [row for row in self.rows if row["driving"]]
        self.assertTrue(driving, "the run must reach the driving state")
        self.assertLess(driving[0]["update"], 500, "the player should be driving early in the run")

    def test_the_car_never_leaves_the_ground(self):
        # A car that drives off the district's ground plane falls forever; an earlier version of
        # this script reached y = -814. Nothing in the game stops it, so the script must stay on
        # the road and the test must say so if it ever does not.
        lowest = min(row["y"] for row in self.rows)
        self.assertGreater(lowest, -1.0, f"the car fell off the world: lowest y = {lowest}")

    def test_throttle_accelerates_from_rest(self):
        start = self.at(430)["speedKph"]
        end = self.at(560)["speedKph"]
        self.assertLess(start, 3.0, "the throttle segment must start from near rest")
        self.assertGreater(end, 12.0, f"holding the throttle must accelerate the car, got {end} km/h")

    def test_the_handbrake_slows_the_car_far_faster_than_coasting(self):
        braked = self.at(660)["speedKph"] - self.at(720)["speedKph"]
        coasted = self.at(720)["speedKph"] - self.at(780)["speedKph"]
        self.assertGreater(braked, 4.0,
                           f"the handbrake must shed real speed over 60 updates, got {braked} km/h")
        self.assertGreater(braked, coasted * 3.0,
                           f"the handbrake must be clearly more than coasting: braked {braked:.1f} "
                           f"km/h vs coasted {coasted:.1f} km/h over the same window")

    def test_steering_turns_the_car_both_ways(self):
        left = self.at(830)["yaw"] - self.at(780)["yaw"]
        right = self.at(900)["yaw"] - self.at(830)["yaw"]
        self.assertLess(left, -0.1, f"steering left must decrease yaw, got {left}")
        self.assertGreater(right, 0.1, f"steering right must increase yaw, got {right}")

    def test_reverse_drives_the_car_backwards(self):
        slowest = min(row["speedKph"] for row in self.window(930, 1140))
        self.assertLess(slowest, -3.0,
                        f"holding back must eventually drive the car backwards, got {slowest} km/h")

    def test_releasing_the_throttle_does_not_brake(self):
        """A finding, pinned so a future tuning change is noticed rather than assumed.

        Releasing the throttle does not slow the car for about a second: it keeps accelerating,
        from 17.8 to 27.8 km/h in the measured run, on Jolt's default vehicle tuning. That is
        recorded in docs/validation.md as a handling defect. This asserts the behaviour as it is,
        so that fixing it fails here and forces the note to be updated rather than silently going
        stale.
        """
        released = self.at(560)["speedKph"]
        one_second_later = self.at(620)["speedKph"]
        self.assertGreater(one_second_later, released,
                           "known defect: the car still accelerates a second after the throttle is "
                           "released. If this now fails, the tuning was fixed -- update the note in "
                           "docs/validation.md and turn this into a deceleration assertion.")


if __name__ == "__main__":
    if GAME is None or not GAME.is_file() or PROJECT_ROOT is None:
        print("usage: test_gate_vehicle.py <path-to-iron_gang> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
