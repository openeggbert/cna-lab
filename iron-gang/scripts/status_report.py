#!/usr/bin/env python3
"""Generate the plan-progress half of docs/status.md from plan/plan_*.md.

plan_38 IG-38-014. The counts in a status document are exactly the part that goes stale
silently: nobody notices that "36/76" became wrong, and a dashboard nobody trusts is worse
than none. So they are generated from the plan files themselves and verified in CI
(--check), while the prose around them -- what is playable, what is blocked -- stays
hand-written between the same markers, because no script can know that.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

BEGIN_MARKER = "<!-- BEGIN GENERATED PLAN PROGRESS -->"
END_MARKER = "<!-- END GENERATED PLAN PROGRESS -->"

TASK_PATTERN = re.compile(r"^- \[(x| )\] \*\*(IG-\d\d-\d\d\d) (P\d)\*\*")
TITLE_PATTERN = re.compile(r"^# (\d\d)\. (.+)$")


class StatusError(Exception):
    pass


def scan_group(path: Path) -> tuple[str, str, int, int, dict[str, int]]:
    """Returns (group number, title, done, total, open-by-priority)."""
    number = ""
    title = ""
    done = 0
    total = 0
    open_by_priority: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        title_match = TITLE_PATTERN.match(line)
        if title_match and not number:
            number, title = title_match.group(1), title_match.group(2)
            continue
        task = TASK_PATTERN.match(line)
        if not task:
            continue
        total += 1
        if task.group(1) == "x":
            done += 1
        else:
            open_by_priority[task.group(3)] = open_by_priority.get(task.group(3), 0) + 1
    if not number:
        raise StatusError(f"no '# NN. Title' heading in {path}")
    if total == 0:
        raise StatusError(f"no tasks found in {path}")
    return number, title, done, total, open_by_priority


def render(plan_directory: Path) -> str:
    groups = sorted(plan_directory.glob("plan_*.md"))
    if not groups:
        raise StatusError(f"no plan group files under {plan_directory}")

    rows = []
    done_total = 0
    task_total = 0
    priority_total: dict[str, int] = {}
    for path in groups:
        number, title, done, total, open_by_priority = scan_group(path)
        done_total += done
        task_total += total
        for priority, count in open_by_priority.items():
            priority_total[priority] = priority_total.get(priority, 0) + count
        percent = (done * 100) // total
        rows.append(
            f"| {number} | [{title}](../plan/{path.name}) | {done}/{total} | {percent}% |"
        )

    priorities = " · ".join(
        f"{priority} {priority_total[priority]}" for priority in sorted(priority_total)
    )
    lines = [
        BEGIN_MARKER,
        "",
        f"**{done_total} of {task_total} tasks done** ({(done_total * 100) // task_total}%). "
        f"Open by priority: {priorities}.",
        "",
        "| # | Group | Done | |",
        "| --- | --- | --- | --- |",
        *rows,
        "",
        "Regenerate with `python3 scripts/status_report.py --write docs/status.md`; "
        "`--check` fails when it is stale.",
        "",
        END_MARKER,
    ]
    return "\n".join(lines)


def splice(document: str, generated: str) -> str:
    begin = document.find(BEGIN_MARKER)
    end = document.find(END_MARKER)
    if begin < 0 or end < 0 or end < begin:
        raise StatusError(
            "the target document must contain both generated-section markers exactly once"
        )
    return document[:begin] + generated + document[end + len(END_MARKER) :]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan-directory", default="plan")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--write", metavar="PATH", help="rewrite the generated section in PATH")
    action.add_argument("--check", metavar="PATH", help="fail if PATH's generated section is stale")
    arguments = parser.parse_args()

    try:
        generated = render(Path(arguments.plan_directory))
        target = Path(arguments.write or arguments.check)
        if not target.is_file():
            raise StatusError(f"status document not found: {target}")
        document = target.read_text(encoding="utf-8")
        updated = splice(document, generated)
        if arguments.write:
            if updated != document:
                target.write_text(updated, encoding="utf-8")
                print(f"status-report: updated {target}")
            else:
                print(f"status-report: {target} already current")
            return 0
        if updated != document:
            print(
                f"status-report: {target} is stale; run "
                f"python3 scripts/status_report.py --write {target}",
                file=sys.stderr,
            )
            return 2
        print(f"status-report: {target} matches the plan files")
        return 0
    except StatusError as error:
        print(f"status-report: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
