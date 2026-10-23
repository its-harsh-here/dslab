# DSLab — Interactive Data Structure Simulator

College C++ data-structures project. A C++17 engine implements every structure and
algorithm. It compiles natively (CMake, for tests) and to WebAssembly (Emscripten +
Embind) for a React + TypeScript + Vite site that renders with SVG. No backend.

Before changing engine code, read docs/architecture.md and docs/operation-contract.md
(wire schema, error codes, limits, decisions). The milestone checklist is docs/plan.md.

## Commands
- `npm run test:cpp` — native CMake build (build/native) + doctest tests
- `npm run test:cpp:asan` — the same tests with ASan + UBSan at -O1 (build/asan, ~25 min)
- `npm run build:wasm` — Emscripten build (build/wasm), copied into web/src/generated/.
  Needs `EMSDK` set or em++ on PATH; in your shell: `export EMSDK=~/tools/emsdk`
- `npm run dev` — build:wasm, then the Vite dev server on http://localhost:5173/
- `npm run build` / `npm run preview` — build:wasm + tsc + Vite build into dist/ / serve it
- `bash scripts/check-env.sh` — verify the toolchain
- `npm run test:e2e` — build, native dslab_cli, then Playwright (web/e2e) against
  `npm run preview`, channel "chromium" (bundled). Keep this list current.

## Environment
- Ubuntu on WSL2, ARM64 (aarch64). Project lives in ~/projects/dslab, never /mnt/c.
- Node via nvm (`~/.nvm/nvm.sh`). Emscripten at `$EMSDK`, else `~/tools/emsdk`
  (`source ~/tools/emsdk/emsdk_env.sh`). Your shell may not load my profile:
  source these explicitly when a command needs them. Pinned emsdk version: docs/toolchain.md.
- Playwright: bundled Chromium only. Never `channel: "chrome"`; Chrome does not exist
  for Linux ARM64.
- Never run `sudo`; I have to type the password. Put privileged steps in a script and
  ask me to run it.

## Engineering rules (non-negotiable)
1. We implement the teaching structures (stack, queue, singly linked list, binary tree,
   BST, AVL, red-black) ourselves in C++. Never substitute std::stack, std::queue,
   std::list, std::set or std::map, and never implement algorithms in TypeScript.
   Standard containers are fine for trace storage, node registries and test oracles.
2. C++ owns the authoritative state. TypeScript only renders, lays out, plays back and
   collects input.
3. Every node has a stable integer ID, independent of its value, that survives rotations.
   The UI selects nodes by ID, never by value.
4. Explicit ownership with RAII. Comment every non-owning pointer.
5. Every operation returns a result (ok or a typed error), the final snapshot and an
   ordered list of frames.
6. A frame = full snapshot + highlighted node/edge IDs + event type + one-sentence
   explanation + stable pseudocode-step ID.
7. Frames are emitted from inside the real algorithm as it runs, never reconstructed.
8. A rejected operation leaves engine state unchanged.
9. Values are 32-bit signed ints; at most 200 nodes per structure; trace and history
   memory are bounded (bounds: docs/operation-contract.md, section 8).
10. Duplicates: allowed in linear structures and the ordinary binary tree; rejected in
    BST, AVL and red-black trees.
11. Keep algorithmic complexity separate from tracing and rendering cost.
12. Code must be simple enough for a student to explain line by line in a viva.
    Prefer clarity over cleverness.

## Testing rules
- IMPORTANT: Never weaken, skip or delete a test to make it pass. If a test looks
  wrong, explain why and wait for me.
- Write native tests before wiring a structure to the UI.
- Never report a check as passing unless you ran it. Show the command and its output.
- Tree structures need an independent invariant checker, run after every operation in tests.

## Course conventions (decided; see Decisions in docs/operation-contract.md)
- Stack and queue are linked nodes. Two-child delete uses the in-order successor node,
  relinked, never value-copied. AVL (docs/avl.md): height(NIL) 0, leaf 1, balance =
  h(left) − h(right), parent pointers + fix-up walk. Red-black (docs/red-black.md): CLRS,
  one black NIL sentinel owned by the tree; mirror fix-up cases have `…Mirror` step IDs.

## Layout
engine/include · engine/src · engine/tests · engine/bindings · engine/third_party ·
web/src · web/e2e · scripts · docs. web/src/generated/ is Emscripten output (gitignored
from milestone 2), never edited by hand.

## Workflow
- Stay inside the current milestone's scope; stop when its definition of done is met.
- Keep JSON serialization separate from algorithm classes.
- Third-party code: only vendored nlohmann/json (engine/src/json and tests) and
  doctest (tests) in engine/third_party (see VERSION.md). Ask before adding anything else.
- Keep native and Emscripten build directories separate. Never commit build output.
- When compacting, keep the list of modified files, failing tests and the commands
  used to run them.
