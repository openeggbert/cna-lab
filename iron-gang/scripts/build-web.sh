#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
emsdk_root="${EMSDK:-}"
if [[ -z "$emsdk_root" ]]; then
  for candidate in "$project_root/../emsdk" "$HOME/emsdk" "/home/robertvokac/emsdk"; do
    if [[ -f "$candidate/upstream/emscripten/emcc" ]]; then
      emsdk_root="$candidate"
      break
    fi
  done
fi

if [[ -z "$emsdk_root" || ! -f "$emsdk_root/upstream/emscripten/emcmake" ]]; then
  echo "Emscripten SDK not found. Set EMSDK=/path/to/emsdk and retry." >&2
  exit 1
fi

export EMSDK="$emsdk_root"
export PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
export EM_CACHE="${IRON_GANG_EM_CACHE:-/tmp/iron-gang-emscripten-cache}"
mkdir -p "$EM_CACHE"

cd "$project_root"
cmake --preset web-emscripten
cmake --build --preset web-emscripten

cat <<EOF

Web build ready in:
  $project_root/web/build/iron_gang.js
  $project_root/web/build/iron_gang.wasm
  $project_root/web/build/iron_gang.data

Serve the web/ directory over HTTP, for example:
  python3 -m http.server 8080 --directory web
EOF
