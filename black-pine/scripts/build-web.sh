#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
emsdk_root="${EMSDK:-}"

if [[ -z "$emsdk_root" ]]; then
  for candidate in "$project_root/../emsdk" "/home/robertvokac/emsdk"; do
    if [[ -x "$candidate/upstream/emscripten/emcmake" ]]; then
      emsdk_root="$candidate"
      break
    fi
  done
fi

if [[ -z "$emsdk_root" || ! -x "$emsdk_root/upstream/emscripten/emcmake" ]]; then
  echo "Emscripten SDK not found. Set EMSDK=/path/to/emsdk and retry." >&2
  exit 1
fi

export EMSDK="$emsdk_root"
export EMSCRIPTEN="$EMSDK/upstream/emscripten"
export PATH="$EMSDK:$EMSCRIPTEN:$PATH"
export EM_CACHE="${BLACK_PINE_EM_CACHE:-/tmp/black-pine-emscripten-cache}"
export CCACHE_DISABLE=1
mkdir -p "$EM_CACHE"

cd "$project_root"
cmake --preset web-emscripten
cmake --build --preset web-emscripten

for artifact in black-pine.html black-pine.js black-pine.wasm; do
  if [[ ! -s "$project_root/build-web-emscripten/$artifact" ]]; then
    echo "Missing or empty web artifact: $artifact" >&2
    exit 1
  fi
done

printf '%s\n' \
  "Web build ready in $project_root/build-web-emscripten" \
  "Serve it with:" \
  "  python3 -m http.server 8080 --directory $project_root/build-web-emscripten" \
  "Then open http://localhost:8080/black-pine.html"
