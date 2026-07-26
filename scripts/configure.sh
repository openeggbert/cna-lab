#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev-easygl}"
"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/preflight.sh" "$preset"
cmake --preset "$preset"
