# DSLab architecture

Milestone 1 document. It covers the components, who owns what, and the path from a
button click to C++ and back to SVG. Read it together with:

- [operation-contract.md](operation-contract.md): operations, error codes, the JSON wire
  schema, the frame schema, limits, node ownership and the list of decisions;
- [plan.md](plan.md): milestones and their definitions of done;
- [toolchain.md](toolchain.md): pinned tool versions.

If code and this document disagree, fix one of them in the same change. Do not leave
them out of step.

---

## 1. The big picture

```
┌──────────────────────────── one browser tab, no backend ────────────────────────────┐
│                                                                                     │
│   React + TypeScript (web/src)                    C++17 engine compiled to Wasm      │
│  ┌──────────────────────────────┐   request JSON  ┌───────────────────────────────┐  │
│  │ panels   collect input       │ ──────────────▶ │ bindings  Engine.handle()     │  │
│  │ state    last response       │                 │ protocol  parse + validate    │  │
│  │ playback current frame index │                 │ Engine    dispatch + history  │  │
│  │ layout   x,y for each node ID│ ◀────────────── │ structures run, emit frames   │  │
│  │ render   SVG                 │  response JSON  │ protocol  write response JSON │  │
│  └──────────────────────────────┘                 └───────────────────────────────┘  │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

Native build (CMake + g++): the same engine sources, linked with doctest tests. No Wasm.
```

There is exactly one boundary between the two languages: a string of JSON goes in and
a string of JSON comes out. No C++ pointer, Embind vector or C++ object other than the
single `Engine` handle ever reaches JavaScript.

### Who owns what

| Concern | Owner | TypeScript may | TypeScript must never |
| --- | --- | --- | --- |
| Structure state (nodes, links, IDs) | C++ structure classes | read snapshots | keep its own copy as truth, edit a snapshot and send it back |
| Algorithms (insert, rotate, traverse…) | C++ | display their frames | re-run, simulate or "fix up" any algorithm step |
| Validation of values and preconditions | C++ | check that the input box holds digits | decide whether an operation is legal |
| Undo/redo history, import/export | C++ | trigger them, save/load the text file | interpret or edit the exported text |
| Layout (x, y per node) | TypeScript | compute from links in the snapshot | change a link, decide an outcome |
| Playback (which frame is shown) | TypeScript | step, play, pause, scrub | create or merge frames |
| Rendering | TypeScript | draw SVG, animate between frames | — |

---

## 2. Components

### 2.1 Engine core: `engine/include/dslab/` and `engine/src/`

Plain C++17. No Emscripten headers, no JSON library. Every file below compiles natively.
The file list is the plan for milestone 2 onward; update it when files change.

| File(s) | Responsibility |
| --- | --- |
| `Limits.h` | All numeric bounds in one place (see operation-contract.md, "Limits"). |
| `Types.h` | `NodeId` and `Value` (both `std::int32_t`; `NodeId` 0 means "no node"), `StructureKind`, `Event`, `Color`. |
| `Result.h` | `ErrorCode` enum, `Status` (code + one-sentence message), `Result<T>` (status + data). |
| `Snapshot.h` | Plain data: `NodeView` (every stored field of one node) and `Snapshot` (one whole structure). Also the format of history entries and export files. |
| `Frame.h`, `Trace.h` | `Frame` (one animation step). `Trace` collects the frames of one operation, enforces the frame cap and holds the algorithm `Counters`. |
| `NodePool.h` | Small class template that **owns** every node of one structure (section 2.2). |
| `Stack.h/.cpp`, `Queue.h/.cpp`, `SinglyLinkedList.h/.cpp`, `BinaryTree.h/.cpp`, `Bst.h/.cpp`, `AvlTree.h/.cpp`, `RedBlackTree.h/.cpp` | One class per teaching structure, each with its own `Node` struct. Mutating algorithms are never shared between structures, so each file reads on its own in a viva. |
| `TreeTraversal.h` | The only shared algorithm code: the four read-only traversals as function templates over the node type (pre-, in-, post-order recursive; level order with a plain array used as a queue). |
| `History.h/.cpp` | Bounded undo and redo lists of `Snapshot`s for one structure. |
| `Catalog.h/.cpp` | For each structure and operation: argument names, whether it mutates, Big-O, and the pseudocode listing with its step IDs. |
| `Request.h`, `Response.h` | Plain C++ request and response types (no JSON). |
| `Engine.h/.cpp` | Owns one instance of each structure plus its `History`. `Response apply(const Request&)` dispatches with plain `switch` statements. |

**Status after milestone 2:** `Limits`, `Types`, `Result`, `Snapshot`, `Frame`, `Trace`,
`NodePool`, `History` (recording only), `Request`, `Response`, `Engine` and `Stack` exist.
`NodeView` and `Snapshot` hold only the stack's fields so far, and the enums list only
what the stack uses. Each later structure adds its own. `Catalog` and `TreeTraversal` do
not exist yet.

Every structure class has the same shape:

- one public method per operation; each takes its arguments and a `Trace&`, and returns a
  `Status` or `Result<Data>`;
- `Snapshot snapshot() const` copies the current state (all nodes, ascending ID order);
- `Status restore(const Snapshot&)` validates a snapshot completely and only then replaces
  the contents (used by undo, redo, import and internal-error rollback). Not written yet;
  it arrives with the first operation that needs it;
- copy and move are deleted. The `Engine` holds each structure as a plain member for the
  whole page lifetime, so raw links into the pool never dangle.

Inside an algorithm, a frame is recorded with one call at the exact point the step
happens, for example (conceptually) `trace.frame(snapshot(), Event::Compare,
"bst.insert.goLeft", highlights, pointers, explanation)`. Nothing is recorded afterwards
or reconstructed (rule 7).

### 2.2 Node ownership in one paragraph

Each structure owns a `NodePool<Node>`: a `std::vector<std::unique_ptr<Node>>` kept in
ascending ID order. The pool is the only owner of nodes; `pool.create(value)` plays the
role of `new` and `pool.destroy(node)` the role of `delete`. Every link (`next`, `left`,
`right`, `parent`, `top`, `front`, `rear`, `head`, `root`) is a raw **non-owning**
pointer, and each such field carries a `// non-owning: the pool owns every node`
comment. Because the pool owns everything, a snapshot taken half-way through an
operation (a list half reversed, a node detached during BST delete) still contains every
node, and rotations read exactly like textbook pseudocode. Full rules are in
operation-contract.md, "Node ownership".

### 2.3 Protocol and serialization: `engine/src/json/`

The only engine code that includes `nlohmann/json.hpp` (as `nlohmann::ordered_json`, so
keys come out in the documented order). Algorithm classes never see JSON.

- `Protocol.h/.cpp`: `std::string handleJson(Engine&, const std::string& requestText)`.
  Parses with exceptions turned off (`parse(text, nullptr, false)` then `is_discarded()`),
  validates the request shape, builds a `Request`, calls `Engine::apply`, writes the
  response. A `try/catch (...)` around the whole call turns any unexpected C++ exception
  into an `INTERNAL_ERROR` response instead of aborting the Wasm module.
- For now `Protocol.cpp` holds both halves: reading requests and writing responses.
  When export arrives it splits into `SnapshotJson.cpp`, `FrameJson.cpp` and
  `ExportJson.cpp`.
- Responses are written frame by frame: one frame's JSON object is built, dumped and
  appended to the output string, then discarded. A JSON tree of the whole response is
  never held in memory.

Because the protocol is plain C++, native tests send exactly the same request text as the
browser and compare against golden files.

### 2.4 Bindings: `engine/bindings/`

One file, `wasm_bindings.cpp`, compiled only by Emscripten:

- class `WasmEngine` holds an `Engine` and has one method
  `std::string handle(const std::string& requestJson)` that calls `handleJson`;
- `EMSCRIPTEN_BINDINGS(dslab)` exposes it as JavaScript class `Engine` with
  `.constructor<>()` and `.function("handle", &WasmEngine::handle)`.

Embind converts JavaScript strings to and from UTF-8 `std::string`. All protocol text is
ASCII, so no encoding questions arise. JavaScript owns the one `Engine` handle and must
call `.delete()` on it when finished; the app keeps it for the page lifetime and deletes
it only on Vite hot-module dispose.

### 2.5 Native tests: `engine/tests/`

doctest, vendored. See section 7 for the test layers.

### 2.6 Third-party code: `engine/third_party/`

Exactly two pinned single headers, each with its licence file. Versions, sources and
SHA-256 hashes are in `engine/third_party/VERSION.md`: `nlohmann/json.hpp` v3.12.0
(protocol layer and native tests only) and `doctest/doctest.h` v2.5.3 (tests only). They are added as `SYSTEM` include directories so their warnings do not fail our
build. Nothing else may be added without asking the owner.

### 2.7 Web app: `web/`

React 19, TypeScript, Vite 8. `web/index.html` is the Vite root page.

| Folder | Responsibility |
| --- | --- |
| `web/src/main.tsx`, `App.tsx` | Mount the app in `<StrictMode>`; loading, load-error and ready states. Tabs (one per structure) come with the second structure. |
| `web/src/engine/` | `loadEngine.ts` (loads Wasm once; section 4), `client.ts` (`send(...) → EngineResponse`, stringify/parse, schema-version check), `protocol.ts` (hand-written read-only TypeScript types that mirror operation-contract.md). |
| `web/src/generated/` | Emscripten output: `dslab_engine.mjs`, `dslab_engine.wasm`, `dslab_engine.d.mts`. Build output: gitignored, never edited, never committed. |
| `web/src/state/` | Not created yet: `StackPanel` keeps its own state with `useState`. A reducer for several tabs arrives with them. |
| `web/src/playback/` | `animation.ts` turns one response's frames into a timed animation (one frame every ≤ 550 ms, about 3 s per operation; moves are skipped under reduced motion). `useAnimationClock.ts` drives it with `requestAnimationFrame`. Play/pause/step/scrub controls come in milestone 6. |
| `web/src/layout/` | `stackLayout.ts`: positions from a frame and the previous positions (section 3.2), plus interpolation between two layouts. |
| `web/src/render/` | `StackSvg.tsx`: nodes keyed by node ID (`data-node-id`), `next` edges keyed by edge ID, the TOP marker, pointer labels, highlights, and dashed outlines for nodes not reachable from `top`. |
| `web/src/panels/` | `StackPanel.tsx` (value box, Push/Pop/Peek/Clear, explanation, step counter, result or typed error) and `EngineLoadErrorView.tsx`. Pseudocode, metrics, undo/redo and import/export panels come later. |
| `web/e2e/` | Not created yet. Milestone 2 was checked by hand with the Playwright MCP browser (bundled Chromium only). |

Two React rules matter here, both from the React docs:

1. **Never call the engine during render or inside a reducer or state-updater function.**
   In development `<StrictMode>` calls those twice, and a second `push` would really push
   twice in C++. Engine calls happen only in event handlers; the handler then dispatches
   the response it got.
2. **Load the engine through one module-level promise**, not inside a component's effect.
   StrictMode runs effects setup → cleanup → setup in development, and a module-level
   promise guarantees a single Wasm instance and a single `Engine` handle.

### 2.8 Scripts and docs

`scripts/` holds `check-env.sh`, `setup-system.sh` (privileged; the owner runs it) and,
from milestone 2, `build-wasm.sh`. `docs/` holds these documents and `toolchain.md`.

---

## 3. From a button click to C++ and back to SVG

Running example: the BST tab holds 30, 20 and 40 and the user inserts 25. The full JSON
of this exchange is worked example 2 in operation-contract.md.

### 3.1 The request path

1. **Input.** The user types `25` and clicks **Insert**. The form checks only the
   syntax: optional minus sign, 1 to 10 digits. Otherwise it shows "enter a whole number"
   and sends nothing. Range and every other rule are checked in C++.
2. **Request.** The click handler builds
   `{schemaVersion: 1, requestId: 4, structure: "bst", operation: "insert", args: {value: 25}}`
   and calls `client.send(request)`.
3. **Across the boundary.** `client.ts` runs `JSON.stringify` and calls
   `engine.handle(text)`. The call is synchronous: Embind copies the string into Wasm
   memory as a `std::string`.
4. **Protocol.** `handleJson` parses the text, checks `schemaVersion`, structure,
   operation and argument types and ranges. A problem here returns a protocol error with
   no frames (operation-contract.md, "Errors").
5. **Dispatch.** `Engine::apply` takes the BST's current snapshot (for history), creates
   an empty `Trace` and calls `bst_.insert(25, trace)`.
6. **Algorithm and frames.** `Bst::insert` runs the real CLRS insert. At each pseudocode
   step it records a frame: begin, compare with 30 (go left), compare with 20 (go right),
   create node 4, link it as 20's right child, done. Each frame copies the full snapshot
   at that moment. The algorithm also increments its counters (3 comparisons, 2 nodes
   visited, 2 links changed).
7. **Commit or reject.** On success, the pre-operation snapshot is pushed onto the BST's
   undo list and the redo list is cleared. On a rejection (say 25 was already present)
   nothing was modified, by construction: every check happens before the first write. No
   history entry is made and no node ID is used up.
8. **Response.** `handleJson` writes result, final snapshot, frames, metrics and history
   counts as compact JSON and returns the string.
9. **Back in TypeScript.** `client.ts` runs `JSON.parse`, checks
   `schemaVersion === 1` (a mismatch means a stale cached Wasm file and is shown as such)
   and returns a typed `Response`.
10. **State.** The handler dispatches `responseReceived`. The reducer stores the response
    for the BST tab, clears the selection if the selected ID no longer exists, and sets
    the player to frame 0 (or to the last frame when "skip animation" is on).

### 3.2 The rendering path

11. **Layout, once per response.** Positions are computed for every frame in order,
    because each frame's positions depend on the previous frame's positions:
    - when **every** node is reachable from the entry pointer (`top`, `front`, `head` or
      `root`), the structure is consistent and all nodes get their normal places.
      Linear structures use their index along the `next` chain (the stack bottom-up;
      the queue and list left to right from front/head). Trees use x = in-order rank ×
      column width and y = depth × row height, computed from `left`/`right`;
    - when some node is not reachable (mid-operation: just created, being unlinked, or
      the not-yet-reversed part of a list), **every** node keeps its position from the
      previous frame. Nothing jumps while links are being changed, and the next
      consistent frame slides everything into place. (Placing only the reachable nodes
      would make a dequeued node and the new front overlap.);
    - a node with no previous position goes in its structure's staging spot: for the
      stack, right of the slot above the top; for the queue, below the slot after the
      rear; for the list and trees, below the first node named in the frame's
      `pointers` that has a position.

    Code: `web/src/layout/layout.ts` (the rule above, interpolation, viewBox) and one
    `LayoutStrategy` per structure (`linear.ts`, `tree.ts`).

    Frame 0 starts from the final positions of the previous response. Layout reads links
    only to place nodes. It never changes a link and never decides an outcome.
12. **Playback.** The player advances the frame index on a timer (speed setting) or on
    step/scrub. Layouts are cached, so stepping backwards is free.
13. **SVG.** For the current frame, `render/` draws one `<g>` per node keyed by node ID
    and one edge per link keyed by edge ID (`"from->to"`). Positions are interpolated
    between the previous and the current frame's layout on every `requestAnimationFrame`.
    Edges are recomputed from their nodes' positions, so arrows follow the moving nodes,
    which CSS transitions cannot do for SVG lines. Because React keys are node IDs, a
    rotation or transplant animates as the same nodes sliding to new places.
    `prefers-reduced-motion` turns the movement off; frames still advance.
    Highlighted node and edge IDs get the highlight style; `pointers` become labels such as
    `curr` and `parent` under their nodes.
14. **Panels.** The pseudocode panel highlights the line whose step ID equals
    `frame.stepId`. The explanation bar shows `frame.explanation` in an `aria-live` region.
    The metrics panel shows the algorithm counters next to the trace cost, labelled
    separately (rule 11). An error result shows `result.message` in the error banner.

```
click ─▶ form (syntax) ─▶ client.send ─▶ engine.handle(JSON) ═══ Wasm ═══▶ handleJson
                                                                           │ parse/validate
                                                                           ▼
SVG ◀─ render ◀─ layout ◀─ player ◀─ reducer ◀─ JSON.parse ◀═══ response ◀─ Engine::apply
                                                                           │ Bst::insert
                                                                           │  └─ trace.frame(...) × 6
```

---

## 4. Builds

| Build | Directory | Command (from milestone 2) | Notes |
| --- | --- | --- | --- |
| Native debug + tests | `build/native` | `npm run test:cpp` | g++, C++17, `-Wall -Wextra -Wpedantic -Werror` on our code |
| Native sanitizers | `build/asan` | `npm run test:cpp:asan` | `-fsanitize=address,undefined -fno-omit-frame-pointer` |
| WebAssembly | `build/wasm` | `npm run build:wasm` | `scripts/build-wasm.sh` sources emsdk, runs `emcmake cmake` and copies outputs to `web/src/generated/` |
| Website | `dist/` | `npm run build` | Vite, `root: "web"`, `build.outDir: "../dist"`, `base: "./"` |

The native and Wasm build directories never share a cache (CLAUDE.md). All of them are
gitignored already (`build/`, `dist/`, `*.wasm`). Milestone 2 adds `web/src/generated/`
to `.gitignore`.

Emscripten flags (all confirmed with the pinned emsdk 6.0.13 in milestone 2, in
`CMakeLists.txt`):

| Flag | Why |
| --- | --- |
| `-std=c++17 -O2` (Release) / `-g -sASSERTIONS=1` (Debug) | Same language level as native. |
| `-fwasm-exceptions` at compile **and** link | Lets the protocol layer catch any stray exception (for example `std::bad_alloc`) instead of aborting. Algorithms never throw for expected errors; those are `Status` values. Default `WASM_LEGACY_EXCEPTIONS` works in every browser that Vite 8's default target supports. |
| `-lembind` | Embind runtime. |
| `-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createDslabEngine` | Output is an ES module whose default export is a factory returning a Promise of a module instance. |
| `-sENVIRONMENT=web` | Browser only; keeps Node-specific code out of the bundle. |
| `-sSTACK_SIZE=1048576` | Recursive traversals and subtree deletion on a degenerate 200-node tree recurse 200 deep. The 64 KiB default is too close for comfort. |
| `-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=134217728` | Worst-case responses are several MB (see section 5). Growth is allowed and capped at 128 MiB. |
| `--emit-tsd dslab_engine.d.ts` | TypeScript declarations for the `Engine` class, generated from the bindings. `scripts/build-wasm.sh` copies the file as `dslab_engine.d.mts`, the name TypeScript looks for next to `dslab_engine.mjs`. |

Loading in the browser (`web/src/engine/loadEngine.ts`):

1. Import the factory from `../generated/dslab_engine.mjs`, and the Wasm file's URL with
   Vite's `?url` suffix. With `base: "./"` Vite compiles that URL to
   `new URL("assets/dslab_engine-<hash>.wasm", import.meta.url)`, which is relative to
   the bundle itself. It therefore works at `/` and under a sub-path such as
   `/dslab/`, which was checked by serving `dist/` under `/dslab/`.
2. `fetch` the URL and `WebAssembly.compile` the bytes ourselves, so every failure can
   name the file, the full URL and the cause (network error, HTTP status, not valid
   WebAssembly). `EngineLoadErrorView` shows these with a fix.
3. Start Emscripten's runtime through its `instantiateWasm(imports, callback)` hook with
   that compiled module. The 6.0.13 glue reads `locateFile` and `instantiateWasm` but
   not `wasmBinary`. Its promise never settles if instantiation fails, so the loader
   races it against its own failure signal.
4. Construct one `new module.Engine()` and send a read-only `snapshot` request for the
   initial picture.

Supported browsers are Vite 8's default build target: Chrome/Edge 111+, Firefox 114+,
Safari 16.4+. All of them support WebAssembly exception handling.

---

## 5. Cost model: algorithm versus tracing versus rendering (rule 11)

Three costs are always reported and documented separately:

| Cost | What it measures | Bound |
| --- | --- | --- |
| **Algorithm** | The data-structure work itself: key comparisons, nodes visited, links changed, rotations, recolors. Shown as Big-O in the catalog and as counters in `metrics.algorithm`. | Per operation, for example BST insert O(h), AVL insert O(log n). Table in operation-contract.md. |
| **Tracing** | Copying a snapshot into every frame and serializing it. Shown in `metrics.trace` (`frames`, `snapshotNodes`). | O(F × n) time and memory per operation, F ≤ 512 frames, n ≤ 200 nodes. |
| **Support** | Pool bookkeeping (vector insert/erase, O(n)), ID lookup (binary search, O(log n)), history snapshot (O(n)). These stand in for `new`, `delete` and "the caller already has the pointer". | Not counted in algorithm counters; documented here. |
| **Rendering** | Layout O(n) per frame (O(F × n) per response, cached); SVG with at most 200 nodes, 199 edges and a few labels per frame. | TypeScript side only. |

Memory bounds (exact numbers in operation-contract.md, "Limits"):

- one response holds at most 512 frames × 200 nodes = 102,400 node views (a node view is
  9 integers, about 36 bytes, so about 3.7 MB natively; at most about 8 MB of JSON, about
  6.5 MB for the worst real operation, a traversal of a 200-node red-black tree);
- each structure keeps at most 50 undo and 50 redo snapshots of at most 200 nodes. All
  seven structures together hold at most 140,000 node views (about 5 MB);
- TypeScript keeps only the latest response per structure. Older frames are dropped.

---

## 6. Errors and exceptions

- **Expected failures are values.** Every operation returns `Status`/`Result` with an
  `ErrorCode`. Algorithms check every precondition and detect duplicates or missing values
  *before* their first write, so a rejected operation has changed nothing (rule 8). Tests
  compare the snapshot and the next-ID counter before and after every rejection.
- **Protocol errors** (malformed JSON, wrong version, unknown structure or operation, bad
  argument type or range) are caught in `handleJson` before the structure is touched.
  Their responses have no frames.
- **Unexpected exceptions** (which should never happen) are caught by a `catch (...)` in
  `handleJson`, which answers `INTERNAL_ERROR` instead of letting the Wasm module abort.
  The planned rollback (`Engine::apply` restoring the pre-operation snapshot through
  `restore()`) arrives with `restore()`. Until then, state after an `INTERNAL_ERROR` may
  be partly changed. Tests treat any `INTERNAL_ERROR` as a failure.
- The Wasm module never aborts on bad input. A JSON parse failure returns a discarded
  value instead of throwing, and argument checks use type tests (`is_number_integer()`)
  rather than throwing accessors.

---

## 7. Testing architecture

| Layer | Where | What it proves |
| --- | --- | --- |
| Unit tests | `engine/tests/test_<structure>.cpp` | Each operation's result, data and final snapshot; every error code it can return. |
| Invariant checkers | `engine/tests/support/` | Independent code that checks a `Snapshot` only, never the structure's internals: chain shape, tree shape, parent links, BST order, AVL heights and balance, the red-black properties. It runs after **every** operation in every test. |
| Rejection tests | same | For each error code: the snapshot, history counts and next-ID counter are unchanged. |
| Oracle tests | same | Seeded random sequences of 10,000 operations compared with `std::vector` (linear) or `std::set` (search trees), which are allowed as test oracles. |
| Trace-contract tests | `engine/tests/test_trace_contract.cpp` | Frame 0 is `begin`; the last frame is `done` or `reject`; the last frame's snapshot equals the response snapshot; every snapshot of a rejected operation equals the begin snapshot; every step ID is in the catalog for that operation; frame count ≤ 2n + 16; highlights refer to nodes that exist in that frame or were removed in this operation. |
| Golden protocol tests | `engine/tests/golden/*.json` | Exact request text produces the exact response (compared as ordered JSON, ignoring whitespace). The two worked examples in operation-contract.md are golden files. |
| Sanitizers | `npm run test:cpp:asan` | All of the above under ASan + UBSan. |
| End-to-end | `web/e2e/*.spec.ts` | Real clicks on the preview build in bundled Chromium. Includes a 200-node degenerate BST traversal (Wasm stack depth) and a golden request sent through the in-page engine, which must match the native golden file. |

Milestone 2 has the unit, invariant-checker, rejection, trace-contract, golden and
sanitizer layers (`engine/tests/`; the checkers are in `engine/tests/support/Checkers.*`).
Seeded oracle tests and the Playwright suite are still to come.

---

## 8. Versioning

- Wire schema: `schemaVersion` 1. Adding an optional field, an operation, an error code
  or an event type is compatible and keeps version 1. Renaming or removing a field,
  changing a field's meaning, or renaming a step ID or event type requires version 2 and an
  update of operation-contract.md in the same change.
- Export files: `format: "dslab-structure"`, `formatVersion` 1, versioned separately
  from the wire schema.
- The emsdk version is pinned in toolchain.md, and `check-env.sh` enforces it.

---

## 9. Folder layout

```
dslab/
├── CLAUDE.md                     project instructions (keep under ~80 lines)
├── package.json                  npm scripts listed in CLAUDE.md          (milestone 2)
├── vite.config.ts                root "web", outDir "../dist", base "./"  (milestone 2)
├── playwright.config.ts          bundled Chromium, preview server         (milestone 2)
├── CMakeLists.txt                native + Emscripten builds               (milestone 2)
├── CMakePresets.json             native, asan, wasm presets               (milestone 2)
├── engine/
│   ├── include/dslab/            public headers: Limits, Types, Result, Snapshot, Frame,
│   │                             Trace, NodePool, the seven structures, TreeTraversal,
│   │                             History, Catalog, Request, Response, Engine
│   ├── src/                      .cpp for the above
│   │   └── json/                 protocol + JSON conversions (only place using nlohmann)
│   ├── bindings/                 wasm_bindings.cpp (Emscripten only)
│   ├── tests/                    doctest tests
│   │   ├── support/              invariant checkers, oracle helpers
│   │   └── golden/               request/response golden files
│   └── third_party/              nlohmann/json.hpp, doctest/doctest.h (+ licences, VERSION)
├── web/
│   ├── index.html
│   ├── public/                   static files (favicon)
│   ├── src/
│   │   ├── engine/  state/  playback/  layout/  render/  panels/
│   │   └── generated/            Emscripten output: gitignored, never edited
│   └── e2e/                      Playwright specs
├── scripts/                      check-env.sh, setup-system.sh, build-wasm.sh
└── docs/                         architecture.md, operation-contract.md, plan.md, toolchain.md
```

---

## 10. API facts checked against current docs (Context7, 2026-10-22)

- Embind: `class_<T>("Name").constructor<>().function(...)`; JavaScript must call
  `.delete()` on every C++ object handle it holds; `std::string` maps to a JavaScript
  string as UTF-8 (`EMBIND_STD_STRING_IS_UTF8` defaults to true).
- `-sMODULARIZE` + `-sEXPORT_ES6`: default export is an async factory, named with
  `-sEXPORT_NAME`. `Module.locateFile(path, prefix)` overrides where the `.wasm` file is
  loaded from. `--emit-tsd` generates `.d.ts` from the bindings.
- `STACK_SIZE` defaults to 64 KiB and cannot grow. `ALLOW_MEMORY_GROWTH` defaults to off.
- Exceptions: catching is off by default (a `throw` aborts). `-fwasm-exceptions` is
  needed at compile and link time. `WASM_LEGACY_EXCEPTIONS` defaults to true.
- Vite 8: needs Node `^20.19.0 || >=22.12.0`; `?url` imports give an asset URL; `base`
  accepts `"./"` for relative deployment; default `build.target` is Chrome 111, Edge 111,
  Firefox 114, Safari 16.4.
- React: StrictMode runs effects setup → cleanup → setup, and calls updater and reducer
  functions twice in development. Hence the two rules in section 2.7.
- nlohmann/json: `parse(text, nullptr, false)` returns a value for which
  `is_discarded()` is true instead of throwing; `ordered_json` keeps insertion order.

Found by inspecting the generated output in milestone 2:

- The 6.0.13 glue calls `instantiateWasm(imports, callback)` with a callback that takes
  only the instance, and resolves only when that callback runs. It reads `locateFile`
  and `instantiateWasm`, but not `wasmBinary`.
- With `--emit-tsd`, the declarations include the default-exported factory, the
  `Engine` class and `ClassHandle.delete()`.
- `requestAnimationFrame` timestamps can be slightly earlier than a `performance.now()`
  taken just before scheduling. Elapsed time is clamped at 0, because this caused a crash
  on first render.
- ASan's default stack-use-after-return detection makes the native suite about 20×
  slower on this ARM64 machine (about 3 minutes in total). It is left on.
