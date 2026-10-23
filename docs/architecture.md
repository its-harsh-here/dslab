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
| `Frame.h`, `Trace.h`, `Counters.h` | `Frame` (one animation step, with a copy of the counters so far). `Trace` collects the frames of one operation, enforces the frame cap and holds the algorithm `Counters`. |
| `NodePool.h` | Small class template that **owns** every node of one structure (section 2.2). |
| `Stack.h/.cpp`, `Queue.h/.cpp`, `SinglyLinkedList.h/.cpp`, `BinaryTree.h/.cpp`, `Bst.h/.cpp`, `AvlTree.h/.cpp`, `RedBlackTree.h/.cpp` | One class per teaching structure, each with its own `Node` struct. Mutating algorithms are never shared between structures, so each file reads on its own in a viva. |
| `TreeTraversal.h` | The only shared algorithm code: the four read-only traversals as function templates over the node type (pre-, in-, post-order recursive; level order with a plain array used as a queue). |
| `History.h/.cpp` | Bounded undo and redo lists for one structure: each entry is a `Snapshot` (ID counter included) and a label such as "insert 25" (section 2.9). |
| `Validate.h/.cpp` | `validateSnapshot()`: rules 3 to 10 of the import checks (operation-contract.md 5.7), on the decoded snapshot, before anything changes. Independent of the test checkers. |
| `Catalog.h/.cpp` | For each structure and operation: Big-O and the pseudocode listing with its step IDs, including the helper procedures it calls. The JSON layer adds argument names and whether it mutates, which it already knows. Tests check every frame's step ID against it. |
| `Request.h`, `Response.h` | Plain C++ request and response types (no JSON). |
| `Engine.h/.cpp` | Owns one instance of each structure plus its `History`. `Response apply(const Request&)` dispatches with plain `switch` statements. |

**Status after milestone 7:** `Limits`, `Types`, `Result`, `Snapshot`, `Frame`,
`Counters`, `Trace`, `NodePool`, `History`, `Validate`, `Catalog`, `Request`,
`Response`, `Engine`, `TreeTraversal` and all seven structures (`Stack`, `Queue`,
`SinglyLinkedList`, `BinaryTree`, `Bst`, `AvlTree`, `RedBlackTree`) exist. `AvlTree` is
the BST's code plus heights, `fixUp`, `rebalance` and CLRS rotations (design:
[avl.md](avl.md)). `RedBlackTree` is CLRS chapter 13 with one black NIL sentinel owned by
the tree as a member (design: [red-black.md](red-black.md)); `TreeTraversal` is told what
NIL is (nullptr, or that sentinel). The enums list only what is used so far.
`Engine::apply` takes the snapshot for history, then dispatches with one
`run<Structure>()` function per structure.

Every structure class has the same shape:

- one public method per operation; each takes its arguments and a `Trace&`, and returns a
  `Status` or `Result<Data>`;
- `Snapshot snapshot() const` copies the current state (all nodes, ascending ID order);
- `void restore(const Snapshot&)` rebuilds the structure exactly as the snapshot
  describes it (same IDs, values, links, heights or colors, and ID counter). It never
  fails: undo and redo give it snapshots the engine made itself, and import calls it only
  after `validateSnapshot()` accepted the file (section 2.9);
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
| `web/src/main.tsx`, `App.tsx` | Mount the app in `<StrictMode>`; loading, load-error and ready states; the structure selector (`#queue` etc. in the URL selects one); creates the seven players once; one keyboard listener for the structure on screen. Only that structure's page is rendered; leaving a structure cancels its playback. |
| `web/src/engine/` | `loadEngine.ts` (loads Wasm once; section 4), `client.ts` (`send(...) → EngineResponse`, stringify/parse, schema-version check), `protocol.ts` (hand-written read-only TypeScript types that mirror operation-contract.md). |
| `web/src/generated/` | Emscripten output: `dslab_engine.mjs`, `dslab_engine.wasm`, `dslab_engine.d.mts`. Build output: gitignored, never edited, never committed. |
| `web/src/state/` | Not created: each structure's state lives in its player. |
| `web/src/playback/` | `player.ts`: one `Player` per structure, outside React (read through `useSyncExternalStore` in `usePlayer.ts`). It keeps the authoritative snapshot (`latest`) apart from the frame on screen, owns the only `requestAnimationFrame` loop (600 ms per frame at 1×, nodes move in the first 360 ms, no movement under reduced motion), sends batch steps one at a time in order, and keeps the operation history. `animation.ts`: one response's frames laid out, sampled at a frame index plus movement progress. `players.ts` creates the seven players. |
| `web/src/layout/` | `layout.ts`: the layout rule of section 3.2, interpolation, the viewBox, and the "focus" box around the nodes an operation highlights. `linear.ts`: stack, queue and list strategies. `tree.ts`: every tree (x = in-order rank, y = depth), with extra room below for red-black NIL leaves. |
| `web/src/render/` | `parts.tsx` (per-drawing arrow markers, box nodes, selectable nodes, pointer labels), `StackSvg.tsx`, `ChainSvg.tsx` (queue and list: FRONT/REAR, HEAD/TAIL, index labels), `TreeSvg.tsx` (L/R edge labels, ROOT marker, AVL height/balance labels, red-black fills with R/B badges and optional NIL leaves). Nodes are keyed by node ID (`data-node-id`), edges by edge ID; nodes not reachable from the entry pointer are dashed. |
| `web/src/panels/` | One page per structure, defining only its controls, examples and drawing (`SearchTreePanel.tsx` is shared by the BST, AVL and red-black pages, which add their presets; red-black adds the "Show NIL leaves" toggle), `examples.ts` (set-up and random input), `common.tsx` (integer input), `TraversalButtons.tsx`, `EngineLoadErrorView.tsx`. Selection is always by node ID. |
| `web/src/ui/` | The shared workspace: `Workspace.tsx` (the layout), `OperationBar.tsx` (Undo, Redo, Export, Import and Reset; status line with Jump to result; examples: batch, seeded random, presets; `OpButton` is disabled while busy; Export saves C++'s text as a file, Import sends a file's text unread), `Stage.tsx` (pan, zoom, fit; readable automatic view), `PlayerBar.tsx`, `SidePanels.tsx` (explanation, pseudocode, node properties, cost, text view), `HistoryLog.tsx`, `StructureNav.tsx`, `useKeyboardShortcuts.ts`, `describe.ts`, `random.ts` (seeded generator for input values only). |
| `web/e2e/` | The Playwright suite (`npm run test:e2e`, section 7). `support.ts` fails any test on a console error or a failed `.wasm` request and compares the screen with frames; the frames come from `window.__dslab.createEngine()`, a test hook in `loadEngine.ts` that creates a separate C++ engine from the same Wasm module (it never touches the page's own engine). |

Two React rules matter here, both from the React docs:

1. **Never call the engine during render or inside a reducer or state-updater function.**
   In development `<StrictMode>` calls those twice, and a second `push` would really push
   twice in C++. Engine calls happen only in event handlers and in the player's own
   animation loop (the next batch step), always through the player.
2. **Load the engine through one module-level promise**, not inside a component's effect.
   StrictMode runs effects setup → cleanup → setup in development, and a module-level
   promise guarantees a single Wasm instance and a single `Engine` handle.

### 2.8 Scripts and docs

`scripts/` holds `check-env.sh`, `setup-system.sh` (privileged; the owner runs it) and,
from milestone 2, `build-wasm.sh`. `docs/` holds these documents and `toolchain.md`.

---

### 2.9 Undo and redo; export and import

**Undo is snapshot-based.** Before every successful change, `Engine::apply` stores the
structure's whole `Snapshot` (ID counter included) in that structure's `History`, with a
label for the explanations ("insert 25"). Undo moves the current state onto the redo list
and calls `restore()` with the newest undo entry; redo is the mirror image. A new change
empties the redo list, because its states no longer follow from the current one.

| | Snapshot-based (chosen) | Command-based |
| --- | --- | --- |
| What one entry holds | A copy of the whole structure: at most 200 nodes | The operation and enough data to reverse it |
| Undoing | `restore(entry)`: one function per structure, about 15 lines each, shared by undo, redo and import | A hand-written inverse for every operation of every structure, and AVL and red-black inverses would have to replay rotations and recolors backwards |
| Exactness | Exact by construction: IDs, links, heights, colors and the ID counter are all in the copy | Exact only if every inverse is right; each one needs its own tests |
| Memory | At most 50 copies of at most 200 nodes per structure (below) | Very small |
| Explaining it in a viva | "We keep a photo of the structure before each change and put the photo back" | One inverse per operation to justify |

At 200 nodes the copies are cheap, and exactness and simplicity matter more here than
memory, so the snapshot design wins. (It was already decision D23 of the contract.)

**The bound: 50 entries per structure, undo and redo together** (`kMaxHistory`). A new
change beyond 50 drops the oldest undo entry. Undo and redo only move entries between the
two lists, so together they never exceed 50. Worst case: 7 structures × 50 × 200 nodes =
70,000 node copies of about 40 bytes each, under 3 MB. Fifty steps is far more than a
teaching session undoes, and the bound keeps memory fixed however long the page stays
open.

**The ID counter is part of the state.** Undo, redo and import restore it exactly
(contract decision D30), so after undoing an insert the next node gets the same ID the
undone one had. Within any one state IDs stay unique: the undone node exists only on the
redo list, and the next change clears that list.

**Export and import.** Export writes the snapshot and the ID counter as a versioned JSON
document (`"format": "dslab-structure"`, `"formatVersion": 1`); the page only saves the
text. Import is checked in two layers before anything changes:

1. the JSON layer (`json/Protocol.cpp`) checks the document itself: valid JSON, format,
   version, structure, and exactly the right fields with the right types (rules 1 and 2
   of contract 5.7), and decodes it into a `Snapshot`;
2. `validateSnapshot()` checks that the snapshot is one the engine could have produced:
   sizes and IDs, links, chains or tree shape, BST order, AVL heights and balances,
   red-black properties, and the ID counter (rules 3 to 10).

Only then does `restore()` replace the structure, and `restore()` cannot fail, so an
invalid file is rejected atomically: the response is `begin` + `reject` with the first
broken rule in one sentence, and the structure, its ID counter and its history are
untouched. A successful import is one undoable operation.

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
10. **State.** The BST's player stores the response: its snapshot becomes `latest` (the
    authoritative state), its frames are laid out, it is added to the history, and
    playback starts at frame 0 (set-up steps of a preset or random example show their last
    frame only). New operations wait until the result is shown again.

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
12. **Playback.** The player advances the frame index in its `requestAnimationFrame`
    loop (speed setting) or on step, scrub, replay and jump to result. Layouts are
    computed once per response, so stepping backwards is free.
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
| Native sanitizers | `build/asan` | `npm run test:cpp:asan` | `-O1 -fsanitize=address,undefined -fno-omit-frame-pointer`. `-O1` is what the ASan documentation recommends: at `-O0`, the default stack-use-after-return check (a "fake stack" frame on every call) made the suite hundreds of times slower than native and it never finished. All checks stay on. |
| Native command runner | `build/native` | `cmake --build --preset native --target dslab_cli` (part of `npm run test:e2e`) | `engine/tools/dslab_cli.cpp`: one JSON request per input line through `handleJson`, one response per output line; the native side of the native-versus-Wasm comparison |
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
- each structure keeps at most 50 history snapshots (undo and redo together) of at most
  200 nodes. All seven structures together hold at most 70,000 node views (under 3 MB);
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
  `restore()` exists since milestone 7, but the rollback (`Engine::apply` restoring the
  pre-operation snapshot after an unexpected exception) is not wired in yet (milestone
  8). Until then, state after an `INTERNAL_ERROR` may be partly changed. Tests treat any
  `INTERNAL_ERROR` as a failure.
- The Wasm module never aborts on bad input. A JSON parse failure returns a discarded
  value instead of throwing, and argument checks use type tests (`is_number_integer()`)
  rather than throwing accessors.

---

## 7. Testing architecture

| Layer | Where | What it proves |
| --- | --- | --- |
| Unit tests | `engine/tests/test_<structure>.cpp` | Each operation's result, data and final snapshot; every error code it can return. |
| Invariant checkers | `engine/tests/support/` | Independent code that checks a `Snapshot` only, never the structure's internals: chain shape, tree shape, parent links, every node reachable, BST order, AVL heights and balance, the red-black properties. `test::run()` applies them after **every** operation in every test, together with the frame contract, the catalog step IDs, and that the response's snapshot is the engine's real state afterwards (ID counter included). |
| Rejection tests | same, and `test_audit.cpp` | For each error code and for malformed input of every kind: the state, history counts and ID counter are unchanged. |
| Oracle tests | `test_stack.cpp`, `test_avl.cpp`, `test_red_black.cpp`, `test_random.cpp` | Seeded random sequences for every structure, compared after each operation with standard containers (allowed as test oracles): `std::vector` (stack, list), `std::deque` (queue), a `std::map` model (binary tree, exact IDs and links), `std::set` (BST, AVL, red-black); traversals compared with traversals computed from the snapshot. |
| Trace contract | `support/Checkers.cpp` (`checkFrames`) | Frame 0 is `begin`; the last frame is `done` or `reject` and equals the response snapshot; a rejected operation's frames all equal the begin snapshot; frame count ≤ 2n + 16; highlights and pointers refer to listed nodes. |
| Aliasing and stress | `test_audit.cpp` | A response's frames stay unchanged after later operations free and replace every node; engines are created, filled, reset, imported and destroyed repeatedly with structure switching (ASan/LSan watch for use-after-free and leaks); ID exhaustion on every structure. |
| Undo/redo and import | `test_history.cpp`, `test_import.cpp`, `engine/tests/fixtures/` | Section 2.9 and contract 5.7. |
| Golden protocol tests | `engine/tests/golden/*.json` | Exact request text produces the exact response (compared as ordered JSON, ignoring whitespace). The two worked examples in operation-contract.md are golden files. |
| Sanitizers | `npm run test:cpp:asan` | All of the above under ASan + UBSan (section 4). |
| Native versus Wasm | `web/e2e/parity.spec.ts`, `web/e2e/fixtures/parity-commands.jsonl` (written by `scripts/make-parity-commands.py`) | The same 1,298 requests (every structure and operation, invalid input, the C++ import fixtures, undo/redo, a 200-node red-black traversal, a degenerate 200-node BST that recurses 200 deep) through `dslab_cli` and through the Wasm engine of the production build: every normalized response is equal. Invalid requests leave every structure unchanged; 300 Wasm engines are created and destroyed. |
| End-to-end | `web/e2e/*.spec.ts` | Real clicks and keys on the production build (`npm run preview`) in bundled Chromium: every step on screen matches its frame (nodes, values, IDs, colors, AVL labels, edges drawn between the drawn nodes, highlights, pointers, explanation, step line, traversal output, pseudocode line) for all seven structures; pause/resume, rapid clicking, scrubbing, keyboard, batch, random examples, reset and structure switching during playback, undo/redo, export/import and invalid import, reduced motion, pan/zoom, the phone layout. Any console error or failed `.wasm` request fails the test. |

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
├── playwright.config.ts          bundled Chromium, preview server         (milestone 8)
├── CMakeLists.txt                native + Emscripten builds               (milestone 2)
├── CMakePresets.json             native, asan, wasm presets               (milestone 2)
├── engine/
│   ├── include/dslab/            public headers: Limits, Types, Result, Snapshot, Frame,
│   │                             Trace, NodePool, the seven structures, TreeTraversal,
│   │                             History, Catalog, Request, Response, Engine
│   ├── src/                      .cpp for the above
│   │   └── json/                 protocol + JSON conversions (only place using nlohmann)
│   ├── bindings/                 wasm_bindings.cpp (Emscripten only)
│   ├── tools/                    dslab_cli.cpp: native command runner (tests)
│   ├── tests/                    doctest tests
│   │   ├── support/              invariant checkers, oracle helpers
│   │   ├── golden/               request/response golden files
│   │   └── fixtures/             import files: one valid, several invalid per tree type
│   └── third_party/              nlohmann/json.hpp, doctest/doctest.h (+ licences, VERSION)
├── web/
│   ├── index.html
│   ├── public/                   static files (favicon)
│   ├── src/
│   │   ├── engine/  state/  playback/  layout/  render/  panels/
│   │   └── generated/            Emscripten output: gitignored, never edited
│   └── e2e/                      Playwright specs; fixtures/parity-commands.jsonl
├── scripts/                      check-env.sh, setup-system.sh, build-wasm.sh,
│                                 make-parity-commands.py
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
