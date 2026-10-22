# DSLab plan

The milestone checklist. Work on one milestone at a time, in order. Stop when its
definition of done is met, and do not start the next milestone without the owner's go-ahead.
The design is in [architecture.md](architecture.md) and
[operation-contract.md](operation-contract.md); this file says only **what** is delivered
**when**.

## Rules for every milestone

A milestone is done only when all of these hold, in addition to its own definition of done:

- [ ] `npm run test:cpp` and `npm run test:cpp:asan` pass; the commands and their output
      are shown in the hand-over message (from milestone 2).
- [ ] `npm run build:wasm`, `npm run build` and `npm run test:e2e` pass, with output shown
      (from milestone 2).
- [ ] Native tests for a structure were written and passing **before** it was wired to
      the UI.
- [ ] The independent invariant checker runs after every operation in every structure test.
- [ ] No test was weakened, skipped or deleted. A test that looks wrong was reported to
      the owner instead of being changed.
- [ ] Every new error path has a rejection test: snapshot, ID counter and history are
      unchanged afterwards.
- [ ] architecture.md (file list), operation-contract.md (if behaviour changed) and the
      CLAUDE.md command list match the code.
- [ ] No build output (`build/`, `dist/`, `web/src/generated/`, `*.wasm`) is committed.
- [ ] The hand-over lists the files changed and any failing or flaky test with the exact
      command that shows it.

---

## Milestone 1: documents (this session)

Goal: agree on the architecture and contract before writing code.

- [x] Context7 checked for current Emscripten/Embind, Vite 8 and React 19 APIs
      (architecture.md, section 10).
- [x] Five open contract questions asked and answered (operation-contract.md, decisions
      D1, D2, D4, D5, D6).
- [x] docs/architecture.md
- [x] docs/operation-contract.md (with two worked examples, both validated as JSON
      and against the frame invariants)
- [x] docs/plan.md
- [x] CLAUDE.md checked against the rules; layout, conventions and third-party lines updated.
- [ ] Owner has read the four documents and accepted or amended the decisions list.

**Definition of done:** the four documents exist and agree with each other, and the owner
accepts them. No source code, CMake or package.json was written.

---

## Milestone 2: stack vertical slice

Goal: the thinnest path through every layer. Push, pop, peek and clear on a stack, from a
button click through C++ and back to SVG, animated from the frames C++ returns.

**2026-10-22: the owner narrowed this milestone** to the stack slice with automatic
animation. Playback controls, the pseudocode/metrics panels and the Playwright suite are
explicitly out of scope, and the definition of done is five items: the `test:cpp`
output, `npm run build`, a Playwright MCP check (push 10, push 20, pop), a README
section, and a 10-line click-to-SVG explanation. Items marked *deferred* move to the
milestone named after them.

Project skeleton:
- [x] `package.json` with the scripts in CLAUDE.md; React 19.3.1, Vite 8.3.5, TypeScript
      7.0.2, `@vitejs/plugin-react` 6.1.2 pinned exactly; versions in toolchain.md.
      `@playwright/test` *deferred* (with `test:e2e`).
- [x] `CMakeLists.txt` and `CMakePresets.json` (`native` → `build/native`, `asan` →
      `build/asan`); the Wasm build uses `emcmake` into `build/wasm`.
- [x] `scripts/build-wasm.sh`: finds Emscripten through `$EMSDK` or `em++` on PATH, fails
      with a clear message if neither (all three cases checked), refuses a version other
      than the pin, builds, and copies `.mjs`, `.wasm` and `.d.mts` into
      `web/src/generated/`.
- [x] `.gitignore` gains `web/src/generated/`.
- [x] `engine/third_party/`: nlohmann/json v3.12.0 and doctest v2.5.3, pinned, with
      licences and `VERSION.md` (sources and SHA-256).
- [x] `vite.config.ts` (`root: "web"`, `base: "./"`, `outDir: "../dist"`), `tsconfig.json`.
      `playwright.config.ts` *deferred* (with `test:e2e`).
- [x] Every Emscripten flag in architecture.md section 4 confirmed with emsdk 6.0.13.

Engine:
- [x] `Limits`, `Types`, `Result`, `Snapshot`, `Frame`, `Trace` (with the 512-frame cap),
      `NodePool`, `History` (recording and bounds), `Request`, `Response`, `Engine`.
      `Catalog` *deferred* (milestone 6, with the pseudocode panel).
- [x] `Stack`: `push`, `pop`, `peek`, `clear`, `snapshot()`. `restore()` *deferred*
      (milestone 7, with undo/redo and import; it also enables the `INTERNAL_ERROR` rollback).
- [x] Shared operation `snapshot`. Other structures answer `UNKNOWN_STRUCTURE`; `catalog`,
      `undo`, `redo`, `export` and `import` answer `UNKNOWN_OPERATION`.
- [x] Protocol layer (`handleJson`, in `engine/src/json/`) and `wasm_bindings.cpp`.

Tests (written before the UI):
- [x] Stack invariant checker and frame-contract checker, working only on plain data
      (`engine/tests/support/Checkers.*`), run after every operation.
- [x] Unit tests: LIFO order, repeated values, pop and peek on an empty stack (typed
      error, state, history and ID counter unchanged), 1,000 create/destroy cycles,
      `CAPACITY_EXCEEDED` at 200, clear, counters, both ends of the 32-bit range.
- [ ] Oracle test: 10,000 seeded random operations compared with a `std::vector`
      (*deferred* to milestone 3, where the queue and list need the same harness).
- [x] Trace-contract tests (operation-contract.md 6.6) for every stack operation.
- [x] Protocol tests for every protocol error code, unknown fields, non-integer values,
      requests over 1 MiB, a `schemaVersion` other than 1, invalid UTF-8, echo rules.
- [x] History tests: one entry per successful mutation, none for reads or rejections,
      at most 50 entries.
- [ ] `restore()` tests (*deferred* with `restore()`).
- [x] Golden files generated from contract 7.1 (stack push) and 5.3 (protocol error);
      the engine's output matches them exactly.

Web:
- [x] `loadEngine.ts` (one module-level promise; explicit `?url` URL; fetch + compile +
      `instantiateWasm`; actionable load errors), `client.ts`, `protocol.ts`.
- [x] Stack view: value box (syntax check only), Push / Pop / Peek / Clear buttons,
      loading state, load-error view (file, URL, problem, fix).
- [x] SVG stack drawn vertically, nodes keyed by ID, TOP marker, NULL links, pointer
      labels, highlights; animated automatically from the C++ frames.
- [ ] Frame stepper, pseudocode panel and metrics panel (*deferred* to milestone 6).
- [ ] Test hook `window.__dslab.handle(text)` and the Wasm-equals-native golden check in
      the browser (*deferred* to the first Playwright suite).

Checked by hand with the Playwright MCP browser (2026-10-22):
- [x] Dev site: push 10, push 20, pop leaves only node #1 (10); 20 is gone; no console
      errors or warnings. Screenshot: `docs/screenshots/stack-after-push10-push20-pop.png`.
- [x] Pop on an empty stack shows `EMPTY_STRUCTURE` "Cannot pop: the stack is empty." and
      the picture does not change; typing `abc` sends no request.
- [x] The production build served under a `/dslab/` sub-path works; with the `.wasm`
      removed it shows the load error naming the file, the full URL and HTTP 404.

**Definition of done (as narrowed):** the five items above are shown with evidence. The
deferred items stay open and are carried by the milestones named.

---

## Milestone 3: queue, singly linked list, binary tree and BST

Goal: the remaining non-balancing structures, with the same depth of testing as the stack.
Each structure was committed on its own: `M3: queue`, `M3: linked list`,
`M3: binary tree`, `M3: BST`.

Engine:
- [x] `Queue`: `enqueue`, `dequeue`, `front`, `clear`. (`restore()` moved to milestone 7.)
- [x] `SinglyLinkedList`: `insertAt`, `deleteNode`, `search`, `reverse`, `clear`.
- [x] `BinaryTree`: `addRoot`, `addChild`, `deleteSubtree`, the four traversals, `clear`.
- [x] `Bst`: `insert`, `delete` (CLRS transplant, successor relinked), `search`, the four
      traversals, `clear`.
- [x] `TreeTraversal.h` shared by the binary tree and the BST; level order uses a plain
      array as its queue and fills `pending`.
- [ ] Catalog listings for every new operation (moved to milestone 6 with the catalog;
      the listings are in operation-contract.md section 2 and in source comments).

Tests:
- [x] Tree invariant checker (shape, single root, one parent each, parent fields, no
      cycles) and a BST-order checker, both independent of the structure classes.
- [x] Unit and rejection tests for every operation and error code listed in contract 2.2–2.5.
- [x] Oracles: stack, queue and list against `std::vector`, BST against `std::set`
      (10,000 seeded operations each).
- [x] BST delete: leaf, one child (each side), two children with the successor as the
      right child, two children with a deeper successor (with and without a right child),
      and deleting the root in each case. Surviving values keep their IDs.
- [x] A degenerate 200-node BST and binary tree: traversals, `deleteSubtree`/`clear`
      natively. [ ] The same under ASan was not run in this milestone (the ASan suite is slow
      on this machine; see the note in architecture.md section 10).
- [x] Frame-count bound ≤ 2n + 16 checked after every operation in every test, including
      200-node cases for inserts, traversals, clears and a degenerate BST delete.
- [x] Golden files: contract 7.2 (BST insert 25) and 5.2 (BST snapshot); the rejected
      duplicate described after 7.2 is a unit test. (7.2 printed `"undo": 3`; the rule in
      contract section 1 gives 4, and the example was corrected.)

Web:
- [x] Queue and list tabs (horizontal), binary-tree and BST tabs.
- [x] Clicking a node selects it by ID (list `deleteNode`; binary-tree `addChild` and
      `deleteSubtree`). In the BST tab a click fills the value box.
- [x] Layout: one rule for every structure (architecture.md 3.2): re-pack when every node
      is reachable, otherwise freeze positions; new nodes wait in a staging spot.
- [x] Traversal output strip (`output`) and level-order queue (`pending`).

End-to-end (by hand with the Playwright MCP browser; the scripted suite is milestone 6):
- [x] Each tab: its main operations plus at least one typed error, with a screenshot in
      `docs/screenshots/m3-*.png` and no console errors.
- [x] 200 increasing values inserted into the BST, then all four traversals: they complete
      in the browser (Wasm stack depth) with 200 values each and 402 frames for the
      depth-first orders.

**Definition of done:** every structure in this milestone passes its tests, the golden
files match, and the demo works: build the BST 30, 20, 40, insert 25 and step through the
six frames of contract 7.2.

---

## Milestone 4: AVL tree

- [ ] `AvlTree`: recursive `insert` and `delete` (`removeMin` detaches the successor
      node), `rebalance` with the LL, RR, LR and RL cases, `search`, traversals, `clear`,
      `restore()` (heights and balances validated).
- [ ] Snapshot includes `height` and the derived `balance`; catalog listings include
      `rebalance` steps.
- [ ] AVL invariant checker that recomputes heights from the snapshot itself.
- [ ] Tests for each rotation case on insert **and** on delete, a delete that rotates
      more than once, a rejected duplicate (no height or rotation frames, nothing
      changed), and **ID survival**: every surviving value keeps its ID across every
      rotation.
- [ ] Oracle against `std::set` (10,000 seeded operations); after each operation,
      height ≤ 1.44 · log2(n + 2).
- [ ] UI: height and balance shown on each node; rotation frames highlight the pivot
      and the edges that change.
- [ ] End-to-end: inserting 30, 20, 10 shows an LL rotation and ends with 20 at the root.

**Definition of done:** all four rotation cases are demonstrable from the UI, and every
test above passes.

---

## Milestone 5: red-black tree

- [ ] `RedBlackTree` (CLRS): NIL sentinel as a member, `insert` + insert fix-up
      (cases 1–3 and mirrors), `delete` + transplant + delete fix-up (cases 1–4 and
      mirrors), `search`, traversals, `clear`, `restore()` (colors and black-heights
      validated).
- [ ] Red-black invariant checker for the properties in contract 2.7, working on the
      snapshot only.
- [ ] Case coverage: a curated test sequence must emit every insert and delete case step
      ID in both mirror directions. The test fails if any case step ID is never seen.
- [ ] Oracle against `std::set` (10,000 seeded operations); after each operation,
      height ≤ 2 · log2(n + 1). ID survival across rotations.
- [ ] UI: red and black nodes, also marked with "R"/"B" text so color is not the only
      signal; recolor frames highlight the nodes that changed.
- [ ] End-to-end: inserting 10, 20, 30 shows a rotation and recolor, and the root ends
      black.

**Definition of done:** every fix-up case can be reached from the UI and is covered by a
test; all checks pass.

---

## Milestone 6: playback and layout

- [ ] Player: play / pause, step forward and back, first / last, scrub slider, speed
      0.5× to 4×, "skip animation", and keyboard shortcuts for each.
- [ ] `Catalog` and the `catalog` operation, the pseudocode panel that highlights
      `stepId`, and the metrics panel (deferred from milestone 2).
- [ ] `npm run test:e2e`: `@playwright/test`, `playwright.config.ts` (bundled Chromium),
      the `window.__dslab.handle` hook and the Wasm-equals-native golden check (deferred
      from milestone 2), plus the stack checks done by hand in milestone 2.
- [ ] Layout implements the full rule of architecture.md 3.2: reachable nodes in normal
      places, unreachable nodes keep their previous position, new nodes in the staging
      spot, positions cached per response and continued from the previous response.
- [ ] Rendering: CSS transform transitions keyed by node ID (rotations and transplants
      slide), edge highlights, pointer labels (`curr`, `parent`, `x`…), entry labels, and
      no transitions under `prefers-reduced-motion`.
- [ ] Zoom-to-fit so that a 200-node tree fits the view; horizontal scrolling for long
      lists.
- [ ] Metrics panel separates algorithm counters and catalog Big-O from trace cost
      (`frames`, `snapshotNodes`).
- [ ] Explanation bar in an `aria-live` region; every control reachable by keyboard.
- [ ] End-to-end: stepping changes the explanation and pseudocode line together; scrubbing
      to the end equals the final snapshot; a rotated node keeps the same SVG element
      (checked through a `data-node-id` attribute); with reduced motion on, nothing animates.
- [ ] Measured and recorded in the hand-over: time from click to first frame, and
      frame-to-frame render time, for the worst response (a 200-node red-black
      traversal).

**Definition of done:** the list reverse, BST delete with a deeper successor, an AVL LR
rotation and a red-black delete fix-up all play smoothly, with nodes keeping their identity
on screen; all checks pass.

---

## Milestone 7: undo/redo and import/export

- [ ] Engine operations `undo`, `redo`, `export`, `import` (contract 2.8 and 5.7), with
      `restore` frames.
- [ ] `restore()` for every structure, with its tests (the stack's was deferred from
      milestone 2), and the `INTERNAL_ERROR` rollback in `Engine::apply`.
- [ ] Tests: the 50-entry bound per list; redo cleared by a new mutation; undo/redo never
      lower the ID counter; one rejection test per import rule (5.7, rules 1–9) for every
      structure it applies to; importing a file of another structure is rejected;
      export → import → export gives identical text for all seven structures.
- [ ] UI: Undo / Redo buttons enabled from `history` counts (Ctrl+Z / Ctrl+Y); Export
      downloads `dslab-<structure>.json`; Import opens a file picker and sends the text
      unread; errors appear in the banner.
- [ ] End-to-end: build a tree, export, clear, import → the same picture and IDs; undo
      after import brings back the cleared state; a broken file shows `IMPORT_INVALID`.

**Definition of done:** history and files work on all seven tabs, and all checks pass.

---

## Milestone 8: audit

Goal: prove each non-negotiable rule with evidence, fix what fails, and record it in
`docs/audit.md`.

- [ ] Rule 1: no `std::stack`, `std::queue`, `std::list`, `std::set`, `std::map`,
      `std::forward_list`, `std::deque` or `std::priority_queue` in `engine/include`,
      `engine/src` or `engine/bindings` (search output pasted); no algorithm logic in
      `web/src` beyond layout (reviewed file by file).
- [ ] Rule 2: TypeScript snapshot types are `readonly`; no code path sends a snapshot back
      except export text.
- [ ] Rule 3: ID-survival tests exist for every rotation, transplant and relink.
- [ ] Rule 4: no `new`/`delete` outside `NodePool`; every raw-pointer field and local
      carries a non-owning comment (search output pasted).
- [ ] Rules 5–7: trace-contract tests cover every operation of every structure (list the
      operations against the tests).
- [ ] Rule 8: a rejection test exists for every error code on every structure that can
      raise it.
- [ ] Rule 9: every bound in contract section 8 has a test; the largest response size is
      measured and recorded.
- [ ] Rule 10 / viva readiness: functions short enough to explain (aim ≤ 40 lines), a
      header comment on every engine file, no template code beyond `NodePool` and
      `TreeTraversal`; the owner can walk through each structure file line by line.
- [ ] Rule 11: test count never went down (compare with the git history).
- [ ] Rule 12: catalog Big-O matches contract section 2; counters checked by tests on
      known cases (for example a degenerate BST insert uses n comparisons).
- [ ] Zero compiler warnings natively and with Emscripten; ASan/UBSan clean; Wasm and JS
      bundle sizes recorded.
- [ ] Accessibility pass: keyboard-only use, `aria-live` explanations, color never the
      only signal.

**Definition of done:** `docs/audit.md` lists every check with its evidence and every
fix made; no open findings except ones the owner explicitly accepts.

---

## Milestone 9: deployment

- [ ] The owner creates or approves the GitHub repository. Nothing is pushed without
      asking.
- [ ] `.github/workflows/ci.yml`: Ubuntu runner, emsdk pinned to the version in
      toolchain.md (cached), Node 24, `npm ci`, `test:cpp`, `test:cpp:asan`, `build:wasm`,
      `build`, Playwright Chromium install, `test:e2e`.
- [ ] Deploy job on `main`: upload `dist/` and publish with GitHub Pages actions.
- [ ] The live site loads the Wasm file with no console errors, at a sub-path (relative
      `base`).
- [ ] The Playwright smoke suite passes against the live URL.
- [ ] `README.md`: what DSLab is, the live link, how to run locally, the command list, and
      links to the docs.

**Definition of done:** CI is green on `main`, the public URL works in Chromium and
Firefox, and the owner has the link.
