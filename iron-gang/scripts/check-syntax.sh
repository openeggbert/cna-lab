#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cna_dir="${IRON_GANG_CNA_DIR:-$project_root/../cnanext}"
dependency_root="$(cd "$(dirname "$cna_dir")" 2>/dev/null && pwd || dirname "$cna_dir")"
sharp_dir="${IRON_GANG_SHARP_RUNTIME_DIR:-$dependency_root/sharp-runtimenext}"
if [[ ! -f "$sharp_dir/CMakeLists.txt" && -f "$dependency_root/sharp-runtime/CMakeLists.txt" ]]; then
  sharp_dir="$dependency_root/sharp-runtime"
fi
jolt_dir="${IRON_GANG_JOLT_DIR:-$HOME/deps/jolt}"
compiler="${CXX:-c++}"

if [[ ! -d "$cna_dir/modules/graphics/include" || ! -d "$sharp_dir/modules/core/include" ]]; then
  echo "Modular CNA/sharp-runtime headers were not found. Run scripts/preflight.sh first." >&2
  exit 1
fi
if [[ ! -d "$jolt_dir/Jolt" ]]; then
  echo "Jolt Physics headers were not found at $jolt_dir. Clone it once:" >&2
  echo "  git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt" >&2
  exit 1
fi

cna_modules=(core math platform graphics input audio media content runtime)
sharp_modules=(core uri time-zone io collections runtime threading buffers text text-json)
include_args=(-I"$project_root/include")
for module in "${cna_modules[@]}"; do
  include_args+=(-I"$cna_dir/modules/$module/include")
done
for module in "${sharp_modules[@]}"; do
  include_args+=(-I"$sharp_dir/modules/$module/include")
done
include_args+=(-isystem "$sharp_dir/vendor" -isystem "$jolt_dir")

mapfile -t sources < <(find "$project_root/src" "$project_root/tests" -type f -name '*.cpp' | sort)
for source in "${sources[@]}"; do
  echo "syntax: ${source#$project_root/}"
  "$compiler" -std=c++23 -fsyntax-only \
    -DSOUND_ENABLED -DXNA5 -DCNA_RENDERER_SOFTWARE -DCNA_FFMPEG_AVAILABLE \
    -DIRON_GANG_SOURCE_ASSET_DIR="\"$project_root/assets\"" \
    -DIRON_GANG_SOURCE_ROOT="\"$project_root\"" \
    "${include_args[@]}" \
    "$source"
done
