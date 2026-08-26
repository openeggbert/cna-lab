#!/usr/bin/env python3
"""End-to-end check for plan_30 IG-30-013: the game can capture a frame headlessly.

Every visual claim in this repository has been made without anyone seeing the game -- there is no
display in the environment it is built in. This test is the first thing that looks at a frame: it
runs the real binary with the software renderer, captures a draw frame, and checks the PNG and the
summary sidecar the capture writes beside it.

It deliberately does NOT compare against a golden image. `--smoke N` is not frame-deterministic
(CNA drives Update() from the wall clock), so which moment frame N lands on varies between runs and
machines. What is stable is that the frame is a rendered scene rather than an empty buffer.
"""

import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GAME = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None


def read_png_header(path: Path) -> tuple[int, int, int, int]:
    """Returns (width, height, bit_depth, colour_type) from a PNG's IHDR, or raises."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: signature mismatch")
    # IHDR is required to be the first chunk: 4-byte length, 4-byte type, then the data.
    length, chunk_type = struct.unpack(">I4s", data[8:16])
    if chunk_type != b"IHDR" or length != 13:
        raise ValueError(f"first chunk is {chunk_type!r} (length {length}), expected a 13-byte IHDR")
    width, height, bit_depth, colour_type = struct.unpack(">IIBB", data[16:26])
    if data[-8:-4] != b"IEND":
        raise ValueError("the file does not end with an IEND chunk -- it is truncated")
    return width, height, bit_depth, colour_type


class ScreenshotCaptureTests(unittest.TestCase):
    def run_game(self, work: Path, *extra: str) -> subprocess.CompletedProcess:
        environment = dict(os.environ)
        environment["SDL_VIDEODRIVER"] = "offscreen"
        environment["SDL_AUDIODRIVER"] = "dummy"
        return subprocess.run(
            [str(GAME), "--smoke", "3", *extra],
            cwd=work,
            env=environment,
            capture_output=True,
            text=True,
            timeout=300,
            check=False,
        )

    def test_capture_writes_a_real_png_of_a_rendered_scene(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            target = work / "frame.png"
            result = self.run_game(work, "--screenshot", str(target), "--screenshot-frame", "2")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(target.is_file(), f"no screenshot was written:\n{result.stderr}")

            width, height, bit_depth, colour_type = read_png_header(target)
            self.assertGreater(width, 0)
            self.assertGreater(height, 0)
            self.assertEqual(bit_depth, 8, "the capture must be 8 bits per channel")
            self.assertEqual(colour_type, 6, "the capture must be RGBA (colour type 6)")

            sidecar = json.loads((work / "frame.png.summary.json").read_text())
            self.assertEqual(sidecar["width"], width, "the sidecar must describe the PNG beside it")
            self.assertEqual(sidecar["height"], height)
            self.assertEqual(sidecar["pixelCount"], width * height)

            # The check this whole test exists for: the frame is not an empty buffer.
            self.assertGreater(
                sidecar["nonSkyFraction"], 0.01,
                "the frame is nothing but the clear colour -- the renderer drew nothing")
            self.assertGreaterEqual(
                sidecar["distinctColours"], 8,
                "the frame is a flat fill rather than a rendered scene")
            self.assertNotIn(
                "does not look like a rendered scene", result.stderr,
                "the game itself reported the captured frame as suspect")

    def test_capture_failure_is_never_fatal_and_frame_choice_is_honoured(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            # A frame number past the end of the run: nothing is captured, and the game still exits
            # cleanly. A diagnostic must not be able to fail the run.
            target = work / "never.png"
            result = self.run_game(work, "--screenshot", str(target), "--screenshot-frame", "999")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(target.exists(), "a frame that never arrives must not produce a file")

    def test_argument_errors_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            for arguments in (
                ["--screenshot"],
                ["--screenshot", str(work / "a.png"), "--screenshot-frame", "0"],
                ["--screenshot", str(work / "a.png"), "--screenshot-frame", "later"],
                ["--screenshot-frame", "4"],
            ):
                with self.subTest(arguments=arguments):
                    result = self.run_game(work, *arguments)
                    self.assertNotEqual(result.returncode, 0,
                                        f"{arguments} should have been rejected")
                    self.assertIn("screenshot", result.stderr.lower())


if __name__ == "__main__":
    if GAME is None or not GAME.is_file():
        print("usage: test_screenshot_capture.py <path-to-iron_gang>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
