#!/usr/bin/env bash
set -euo pipefail

# Regression test for IS-39-035: the game must not crash or hang when the generated
# warehouse.cnj / vehicle_*.cnj assets (produced by scripts/build-assets.sh) have not been built.
# It should log a warning and fall back to procedural geometry instead.

executable="${1:?Usage: test-missing-asset-fallback.sh <path-to-iron_shadows>}"
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

# Minimal asset root: dialogues only, deliberately no generated/ subdirectory.
mkdir -p "$work_dir/dialogues"
cp "$project_root/assets/dialogues/prologue.dialogue.txt" "$work_dir/dialogues/"

set +e
output="$("$executable" --assets "$work_dir" --smoke 5 2>&1)"
status=$?
set -e

echo "$output"

if [[ $status -ne 0 ]]; then
  echo "FAIL: iron_shadows exited with status $status when the generated asset was missing" >&2
  exit 1
fi

if ! grep -q "using procedural warehouse box" <<<"$output"; then
  echo "FAIL: expected warehouse fallback log message was not printed" >&2
  exit 1
fi

if ! grep -q "using procedural sedan" <<<"$output"; then
  echo "FAIL: expected sedan fallback log message was not printed" >&2
  exit 1
fi

echo "PASS: missing-asset fallback exited cleanly and logged the expected warning"
