#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev-easygl}"
cmake --build --preset "$preset" --parallel 4
