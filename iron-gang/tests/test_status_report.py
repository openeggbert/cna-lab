#!/usr/bin/env python3
"""Tests for scripts/status_report.py (plan_38 IG-38-014)."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY_ROOT / "scripts" / "status_report.py"

STATUS_TEMPLATE = """# Status

Prose a script must never touch.

<!-- BEGIN GENERATED PLAN PROGRESS -->
<!-- END GENERATED PLAN PROGRESS -->

Trailing prose.
"""

GROUP_ONE = """# 07. Rendering foundation

- [x] **IG-07-001 P0** — Done thing.
- [ ] **IG-07-002 P1** — Open thing.
- [ ] **IG-07-003 P2** — Another open thing.
"""

GROUP_TWO = """# 08. Materials

- [x] **IG-08-001 P0** — Done thing.
- [x] **IG-08-002 P1** — Also done.
"""


def run(*arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *arguments],
        capture_output=True,
        text=True,
        check=False,
    )


class StatusReportTests(unittest.TestCase):
    def create_project(self, root: Path) -> tuple[Path, Path]:
        plan = root / "plan"
        plan.mkdir()
        (plan / "plan_07-rendering.md").write_text(GROUP_ONE, encoding="utf-8")
        (plan / "plan_08-materials.md").write_text(GROUP_TWO, encoding="utf-8")
        status = root / "status.md"
        status.write_text(STATUS_TEMPLATE, encoding="utf-8")
        return plan, status

    def test_write_then_check_is_clean(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            plan, status = self.create_project(Path(directory))
            written = run("--plan-directory", str(plan), "--write", str(status))
            self.assertEqual(written.returncode, 0, written.stderr)

            body = status.read_text(encoding="utf-8")
            self.assertIn("**3 of 5 tasks done** (60%)", body)
            self.assertIn("P1 1", body)
            self.assertIn("P2 1", body)
            self.assertIn("| 2/2 | 100% |", body)
            self.assertIn("| 1/3 | 33% |", body)
            # The prose either side of the markers must survive regeneration untouched.
            self.assertIn("Prose a script must never touch.", body)
            self.assertIn("Trailing prose.", body)

            checked = run("--plan-directory", str(plan), "--check", str(status))
            self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_check_fails_when_the_plan_moves_on(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            plan, status = self.create_project(Path(directory))
            run("--plan-directory", str(plan), "--write", str(status))

            # Someone completes a task and forgets the dashboard: exactly the drift this exists
            # to catch.
            group = plan / "plan_07-rendering.md"
            group.write_text(
                group.read_text(encoding="utf-8").replace(
                    "- [ ] **IG-07-002 P1**", "- [x] **IG-07-002 P1**"
                ),
                encoding="utf-8",
            )
            stale = run("--plan-directory", str(plan), "--check", str(status))
            self.assertEqual(stale.returncode, 2)
            self.assertIn("stale", stale.stderr)

            refreshed = run("--plan-directory", str(plan), "--write", str(status))
            self.assertEqual(refreshed.returncode, 0, refreshed.stderr)
            self.assertEqual(run("--plan-directory", str(plan), "--check", str(status)).returncode, 0)

    def test_malformed_inputs_are_refused(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan, status = self.create_project(root)

            without_markers = root / "no-markers.md"
            without_markers.write_text("# Status\n\nNothing generated here.\n", encoding="utf-8")
            result = run("--plan-directory", str(plan), "--write", str(without_markers))
            self.assertEqual(result.returncode, 2)
            self.assertIn("markers", result.stderr)

            missing = run("--plan-directory", str(plan), "--check", str(root / "absent.md"))
            self.assertEqual(missing.returncode, 2)

            empty_plan = root / "empty-plan"
            empty_plan.mkdir()
            result = run("--plan-directory", str(empty_plan), "--write", str(status))
            self.assertEqual(result.returncode, 2)
            self.assertIn("no plan group files", result.stderr)

            headless = root / "headless-plan"
            headless.mkdir()
            (headless / "plan_09-nameless.md").write_text(
                "- [x] **IG-09-001 P0** — Task with no group heading.\n", encoding="utf-8"
            )
            result = run("--plan-directory", str(headless), "--write", str(status))
            self.assertEqual(result.returncode, 2)
            self.assertIn("heading", result.stderr)

            taskless = root / "taskless-plan"
            taskless.mkdir()
            (taskless / "plan_10-empty.md").write_text("# 10. Empty group\n\nProse only.\n", encoding="utf-8")
            result = run("--plan-directory", str(taskless), "--write", str(status))
            self.assertEqual(result.returncode, 2)
            self.assertIn("no tasks", result.stderr)

    def test_committed_status_document_is_current(self) -> None:
        result = run(
            "--plan-directory",
            str(REPOSITORY_ROOT / "plan"),
            "--check",
            str(REPOSITORY_ROOT / "docs" / "status.md"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
