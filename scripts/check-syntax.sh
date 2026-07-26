#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cna_dir="${IRON_SHADOWS_CNA_DIR:-$project_root/../cna}"
dependency_root="$(cd "$(dirname "$cna_dir")" 2>/dev/null && pwd || dirname "$cna_dir")"
sharp_dir="$dependency_root/sharp-runtime"
compiler="${CXX:-c++}"

if [[ ! -d "$cna_dir/include" || ! -d "$sharp_dir/include" ]]; then
  echo "CNA/sharp-runtime headers were not found. Run scripts/preflight.sh first." >&2
  exit 1
fi

mapfile -t sources < <(find "$project_root/src" "$project_root/tests" -type f -name '*.cpp' | sort)
for source in "${sources[@]}"; do
  echo "syntax: ${source#$project_root/}"
  "$compiler" -std=c++23 -fsyntax-only \
    -DSOUND_ENABLED -DXNA5 -DCNA_BACKEND_SOFTWARE \
    -I"$project_root/include" \
    -I"$cna_dir/include" \
    -I"$sharp_dir/include" \
    -I"$sharp_dir/vendor" \
    "$source"
done
