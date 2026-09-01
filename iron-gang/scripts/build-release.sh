#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$project_root/cmake-build-release-easygl"
export CCACHE_DIR="${IRON_GANG_CCACHE_DIR:-${CCACHE_DIR:-$build_dir/ccache}}"

cd "$project_root"
./scripts/preflight.sh release-easygl
./scripts/asset_registry.py --project-root . --check-notice THIRD_PARTY_ASSETS.md
cmake --preset release-easygl
cmake --build --preset release-easygl
./scripts/release_archive.py \
  --project-root "$project_root" \
  --build-dir "$build_dir"
