#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"

preset="${1:-dev-easygl}"
case "$preset" in
  dev-easygl) build_dir="cmake-build-dev-easygl" ;;
  dev-vulkan) build_dir="cmake-build-dev-vulkan" ;;
  compile-software) build_dir="cmake-build-compile-software" ;;
  release-easygl) build_dir="cmake-build-release-easygl" ;;
  *) echo "Unknown preset: $preset" >&2; exit 2 ;;
esac

exec "$project_root/${build_dir}/iron_gang" --assets "$project_root/assets" "${@:2}"
