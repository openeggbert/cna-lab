#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cna_dir="${IRON_GANG_CNA_DIR:-$project_root/../cnanext}"
dependency_root="$(cd "$(dirname "$cna_dir")" 2>/dev/null && pwd || dirname "$cna_dir")"
sharp_dir="$dependency_root/sharp-runtime"
easygl_dir="$dependency_root/easy-gl"
jolt_dir="${IRON_GANG_JOLT_DIR:-$HOME/deps/jolt}"
mesh_craft_dir="${MESH_CRAFT_SOURCE_DIR:-$dependency_root/mesh-craft}"
preset="${1:-dev-easygl}"
errors=0

ok() { printf 'OK: %s\n' "$1"; }
warn() { printf 'WARNING: %s\n' "$1" >&2; }
fail() { printf 'ERROR: %s\n' "$1" >&2; errors=$((errors + 1)); }
nonempty_dir() { [[ -d "$1" ]] && find "$1" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null | grep -q .; }

printf 'Iron Gang dependency preflight\n'
printf '  preset:        %s\n' "$preset"
printf '  project:       %s\n' "$project_root"
printf '  CNA (modular): %s\n' "$cna_dir"
printf '  sharp-runtime: %s\n' "$sharp_dir"
printf '  EasyGL:        %s\n' "$easygl_dir"
printf '  Jolt Physics:  %s\n' "$jolt_dir"
printf '  Mesh Craft:    %s\n' "$mesh_craft_dir"

if [[ -f "$cna_dir/CMakeLists.txt" && -d "$cna_dir/modules/graphics/include" && -d "$cna_dir/modules/runtime/include" ]]; then
  ok "modular CNA source tree found"
else
  fail "Modular CNA not found. Set IRON_GANG_CNA_DIR or place it at ../cnanext."
fi

if [[ -f "$sharp_dir/CMakeLists.txt" && -d "$sharp_dir/modules/io/include" && -d "$sharp_dir/modules/text-json/include" ]]; then
  ok "modular sharp-runtime sibling found"
else
  fail "Modular sharp-runtime must be a sibling of CNA at $sharp_dir."
fi

if [[ "$preset" == *easygl* ]]; then
  if [[ -f "$easygl_dir/CMakeLists.txt" ]]; then
    ok "EasyGL sibling found"
  else
    fail "EasyGL preset selected, but EasyGL was not found at $easygl_dir."
  fi
fi

if [[ -d "$cna_dir" ]]; then
  for submodule in SDL SDL_image SDL_mixer; do
    path="$cna_dir/third_party/$submodule"
    if nonempty_dir "$path"; then
      ok "CNA vendored $submodule is populated"
    else
      warn "CNA vendored $submodule is empty or absent at $path. A compatible system package may still satisfy CNA, but the supplied source archives did not."
    fi
  done
fi

if [[ -f "$jolt_dir/Build/CMakeLists.txt" ]]; then
  ok "Jolt Physics shared checkout found"
else
  fail "Jolt Physics not found at $jolt_dir. Clone it once: git clone --branch v5.6.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git ~/deps/jolt"
fi

if [[ -f "$mesh_craft_dir/CMakeLists.txt" ]]; then
  ok "Mesh Craft source tree found (optional for compiling the game)"
else
  warn "Mesh Craft not found; game compilation can continue, but MC3 conversion cannot."
fi

for command in cmake ninja c++; do
  if command -v "$command" >/dev/null 2>&1; then
    ok "$command is available"
  else
    fail "$command is not available in PATH"
  fi
done

if command -v ccache >/dev/null 2>&1; then
  ok "ccache is available"
else
  warn "ccache is unavailable; builds will still work but will rewrite more compiler output."
fi

if (( errors > 0 )); then
  printf '\nPreflight failed with %d blocking issue(s).\n' "$errors" >&2
  printf 'Recommended checkout pattern:\n' >&2
  printf '  git clone --recursive <CNA repository> cnanext\n' >&2
  printf '  git -C cnanext submodule update --init --recursive\n' >&2
  printf '  # then place sharp-runtime and easy-gl next to cnanext\n' >&2
  exit 1
fi

printf '\nPreflight completed. Warnings may still become CMake dependency errors.\n'
