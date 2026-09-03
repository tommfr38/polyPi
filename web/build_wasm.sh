#!/usr/bin/env bash
# Builds core/pi_engine.c + wasm_bridge.c into web/wasm/pi.js + pi.wasm
# Requires: emscripten (brew install emscripten), and a GMP built for wasm32
# at tools/wasm_build/gmp-6.3.0 (see README for the emconfigure/emmake steps).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
GMPDIR="$ROOT_DIR/tools/wasm_build/gmp-6.3.0"

if ! command -v emcc >/dev/null; then
  echo "emcc not found. Install with: brew install emscripten" >&2
  exit 1
fi

if [ ! -f "$GMPDIR/.libs/libgmp.a" ]; then
  echo "GMP wasm build not found at $GMPDIR/.libs/libgmp.a" >&2
  echo "Build it first (see README.md)." >&2
  exit 1
fi

mkdir -p "$SCRIPT_DIR/wasm"

emcc -O3 \
  -I"$GMPDIR" -I"$ROOT_DIR/core" \
  "$ROOT_DIR/core/pi_engine.c" "$ROOT_DIR/core/wasm_bridge.c" \
  "$GMPDIR/.libs/libgmp.a" \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=PiModule \
  -s EXPORT_ES6=0 \
  -s ENVIRONMENT=worker \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS=_wasm_compute_pi,_wasm_free,_malloc,_free \
  -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString \
  -s SINGLE_FILE=0 \
  -o "$SCRIPT_DIR/wasm/pi.js"

echo "Built $SCRIPT_DIR/wasm/pi.js + pi.wasm"
