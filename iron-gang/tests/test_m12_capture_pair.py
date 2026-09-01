#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


SCRIPT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("scripts/m12_capture_pair.py")


FAKE_TOOL = r"""
import json
import os
import sys
from pathlib import Path

name = Path(sys.argv[0]).name
arguments = sys.argv[1:]
with Path(os.environ["M12_PAIR_FAKE_LOG"]).open("a", encoding="utf-8") as log:
    log.write(json.dumps({"tool": name, "arguments": arguments}) + "\n")

def value(option):
    return Path(arguments[arguments.index(option) + 1])

if name == "fake-drm.py":
    value("--capture").write_text("{}\n", encoding="utf-8")
    value("--evidence").write_text("{}\n", encoding="utf-8")
    value("--artifact").write_text("{}\n", encoding="utf-8")
elif name == "fake-binder.py":
    value("--output").write_text("{}\n", encoding="utf-8")
elif name == "fake-report.py":
    status = os.environ.get("M12_PAIR_FAKE_STATUS", "PASS")
    value("--output").write_text(
        "# Fake report\n\n- Overall status: **" + status + "**\n",
        encoding="utf-8",
    )
elif name == "fake-compare.py":
    value("--output").write_text("# Fake comparison\n", encoding="utf-8")
    raise SystemExit(int(os.environ.get("M12_PAIR_FAKE_COMPARISON_EXIT", "0")))
else:
    raise SystemExit(9)
"""


class M12CapturePairTests(unittest.TestCase):
    def prepare(self, directory: Path) -> tuple[Path, dict[str, Path], Path]:
        game = directory / "iron_gang"
        game.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        game.chmod(0o755)
        tools: dict[str, Path] = {}
        for role in ("drm", "binder", "report", "compare"):
            path = directory / f"fake-{role}.py"
            path.write_text(textwrap.dedent(FAKE_TOOL), encoding="utf-8")
            tools[role] = path
        return game, tools, directory / "calls.jsonl"

    def run_pair(
        self,
        directory: Path,
        environment_overrides: dict[str, str] | None = None,
    ) -> tuple[subprocess.CompletedProcess[str], Path]:
        game, tools, log = self.prepare(directory)
        environment = os.environ.copy()
        environment["M12_PAIR_FAKE_LOG"] = str(log)
        if environment_overrides:
            environment.update(environment_overrides)
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--output-dir",
                str(directory / "output"),
                "--prefix",
                "test-pair",
                "--hardware",
                "Test CPU / GPU / physical display",
                "--game",
                str(game),
                "--vsync",
                "on",
                "--drm-capture-tool",
                str(tools["drm"]),
                "--binder-tool",
                str(tools["binder"]),
                "--report-tool",
                str(tools["report"]),
                "--compare-tool",
                str(tools["compare"]),
            ],
            check=False,
            capture_output=True,
            text=True,
            env=environment,
        )
        return result, log

    def read_log(self, path: Path) -> list[dict[str, object]]:
        return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]

    def test_complete_pass_uses_locked_workload_and_qualifying_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            result, log = self.run_pair(directory)
            self.assertEqual(result.returncode, 0, result.stderr)
            calls = self.read_log(log)
            self.assertEqual([call["tool"] for call in calls], [
                "fake-drm.py",
                "fake-binder.py",
                "fake-drm.py",
                "fake-binder.py",
                "fake-report.py",
                "fake-compare.py",
            ])
            drm_calls = [call["arguments"] for call in calls if call["tool"] == "fake-drm.py"]
            self.assertEqual(len(drm_calls), 2)
            for arguments in drm_calls:
                self.assertIn("--smoke", arguments)
                self.assertEqual(arguments[arguments.index("--smoke") + 1], "900")
                self.assertEqual(arguments[arguments.index("--profile-scenario") + 1], "mixed")
                self.assertEqual(arguments[arguments.index("--vsync") + 1], "on")
            comparison = calls[-1]["arguments"]
            self.assertEqual(comparison[comparison.index("--baseline-kind") + 1], "qualifying")
            self.assertEqual(comparison[comparison.index("--candidate-kind") + 1], "qualifying")
            output = directory / "output"
            self.assertTrue((output / "test-pair-qualification.md").is_file())
            self.assertTrue((output / "test-pair-comparison.md").is_file())
            self.assertEqual(len(list(output.iterdir())), 10)
            self.assertIn("qualification: PASS", result.stdout)

    def test_failed_gate_is_preserved_and_compared_as_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result, log = self.run_pair(
                Path(temporary), {"M12_PAIR_FAKE_STATUS": "FAIL"}
            )
            self.assertEqual(result.returncode, 1, result.stderr)
            calls = self.read_log(log)
            comparison = calls[-1]["arguments"]
            self.assertEqual(comparison[comparison.index("--baseline-kind") + 1], "diagnostic")
            self.assertEqual(comparison[comparison.index("--candidate-kind") + 1], "diagnostic")
            self.assertIn("qualification: FAIL", result.stdout)

    def test_regression_returns_one_without_discarding_reports(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            result, _ = self.run_pair(
                directory, {"M12_PAIR_FAKE_COMPARISON_EXIT": "1"}
            )
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertTrue((directory / "output/test-pair-qualification.md").is_file())
            self.assertTrue((directory / "output/test-pair-comparison.md").is_file())
            self.assertIn("comparison: REGRESSION", result.stdout)

    def test_preexisting_output_blocks_every_tool_before_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            game, tools, log = self.prepare(directory)
            output = directory / "output"
            output.mkdir()
            protected = output / "test-pair-02-evidence.json"
            protected.write_text("do not replace", encoding="utf-8")
            environment = os.environ.copy()
            environment["M12_PAIR_FAKE_LOG"] = str(log)
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--output-dir",
                    str(output),
                    "--prefix",
                    "test-pair",
                    "--hardware",
                    "Test hardware",
                    "--game",
                    str(game),
                    "--drm-capture-tool",
                    str(tools["drm"]),
                    "--binder-tool",
                    str(tools["binder"]),
                    "--report-tool",
                    str(tools["report"]),
                    "--compare-tool",
                    str(tools["compare"]),
                ],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("refusing to overwrite existing pair output", result.stderr)
            self.assertEqual(protected.read_text(encoding="utf-8"), "do not replace")
            self.assertFalse(log.exists())


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
