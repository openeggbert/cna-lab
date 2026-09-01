#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev-easygl}"
ctest --preset "$preset"
