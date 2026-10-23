#!/usr/bin/env bash
# DSLab: compile the C++ engine to WebAssembly and copy the result into
# web/src/generated/ (build output: gitignored, never edited by hand).
#
#     npm run build:wasm        (or: bash scripts/build-wasm.sh)
#
# Emscripten is found through the EMSDK environment variable (this script then
# sources $EMSDK/emsdk_env.sh) or through an em++ that is already on PATH.
# If neither exists, the script stops and says how to fix it.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/wasm"            # separate from the native build/native
OUT_DIR="$ROOT/web/src/generated"

fail() {
  printf '\nbuild:wasm failed: %s\n\n' "$1" >&2
  exit 1
}

# 1. Find Emscripten -----------------------------------------------------------
if [ -n "${EMSDK:-}" ]; then
  if [ ! -f "$EMSDK/emsdk_env.sh" ]; then
    fail "EMSDK is set to '$EMSDK', but $EMSDK/emsdk_env.sh does not exist.
Point EMSDK at your emsdk folder, for example:  export EMSDK=\$HOME/tools/emsdk"
  fi
  # emsdk_env.sh is not written for 'set -u'.
  set +u
  # shellcheck source=/dev/null
  source "$EMSDK/emsdk_env.sh" > /dev/null 2>&1
  set -u
  found_via="EMSDK=$EMSDK"
  command -v em++ > /dev/null || fail "Sourced $EMSDK/emsdk_env.sh, but em++ is still not on PATH.
Activate the pinned version (.emsdk-version):  $EMSDK/emsdk activate $(cat "$ROOT/.emsdk-version")"
elif command -v em++ > /dev/null; then
  found_via="em++ on PATH at $(command -v em++)"
else
  fail "Emscripten was not found: EMSDK is not set and em++ is not on PATH.
Do one of these, then run the command again:
  export EMSDK=\$HOME/tools/emsdk                 (where you installed emsdk)
  source \$HOME/tools/emsdk/emsdk_env.sh          (puts em++ on PATH)
Installing emsdk for the first time: see README.md, 'Prerequisites'."
fi
command -v emcmake > /dev/null || fail "em++ was found ($found_via), but emcmake was not. Is the emsdk installation complete?"

# 2. Check the pinned version (.emsdk-version: the one place it is written) -----
pinned="$(tr -d '[:space:]' < "$ROOT/.emsdk-version")"
found="$(em++ --version | head -n 1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)"
if [ "$found" != "$pinned" ]; then
  fail "em++ is version $found ($found_via), but .emsdk-version pins $pinned.
Install and activate the pinned version:  emsdk install $pinned && emsdk activate $pinned"
fi
echo "build:wasm: em++ $found ($found_via)"

# 3. Configure and build --------------------------------------------------------
emcmake cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release --log-level=WARNING
cmake --build "$BUILD_DIR"

# 4. Copy the outputs next to the web sources ----------------------------------
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/dslab_engine.mjs" "$BUILD_DIR/dslab_engine.wasm" "$OUT_DIR/"
# TypeScript looks for "<name>.d.mts" next to an imported "<name>.mjs".
cp "$BUILD_DIR/dslab_engine.d.ts" "$OUT_DIR/dslab_engine.d.mts"
echo "build:wasm: wrote web/src/generated/ (dslab_engine.mjs, dslab_engine.wasm, dslab_engine.d.mts)"
