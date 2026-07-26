#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev-easygl}"
case "$preset" in
  dev-easygl) build_dir="cmake-build-dev-easygl" ;;
  dev-vulkan) build_dir="cmake-build-dev-vulkan" ;;
  compile-software) build_dir="cmake-build-compile-software" ;;
  release-easygl) build_dir="cmake-build-release-easygl" ;;
  *) echo "Unknown preset: $preset" >&2; exit 2 ;;
esac

exec "./${build_dir}/iron_shadows" --assets "$(pwd)/assets" "${@:2}"
