#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cna_dir="${IRON_GANG_CNA_DIR:-$project_root/../cna}"
dependency_root="$(cd "$(dirname "$cna_dir")" 2>/dev/null && pwd || dirname "$cna_dir")"
sharp_dir="$dependency_root/sharp-runtime"
cna_extended_dir="${IRON_GANG_CNA_EXTENDED_DIR:-$dependency_root/cna-extended}"
jolt_dir="${IRON_GANG_JOLT_DIR:-$HOME/deps/jolt}"
compiler="${CXX:-c++}"

if [[ ! -d "$cna_dir/include" || ! -d "$sharp_dir/include" ]]; then
  echo "CNA/sharp-runtime headers were not found. Run scripts/preflight.sh first." >&2
  exit 1
fi
if [[ ! -d "$jolt_dir/Jolt" ]]; then
  echo "Jolt Physics headers were not found at $jolt_dir. Clone it once:" >&2
  echo "  git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt" >&2
  exit 1
fi

mapfile -t sources < <(find "$project_root/src" "$project_root/tests" -type f -name '*.cpp' | sort)
for source in "${sources[@]}"; do
  echo "syntax: ${source#$project_root/}"
  "$compiler" -std=c++23 -fsyntax-only \
    -DSOUND_ENABLED -DXNA5 -DCNA_BACKEND_SOFTWARE \
    -DIRON_GANG_SOURCE_ASSET_DIR="\"$project_root/assets\"" \
    -I"$project_root/include" \
    -I"$cna_dir/include" \
    -I"$sharp_dir/include" \
    -I"$sharp_dir/vendor" \
    -I"$cna_extended_dir/include" \
    -I"$jolt_dir" \
    "$source"
done
