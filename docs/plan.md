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

Design and pseudocode with step IDs: [avl.md](avl.md) (approved before any code). At the
owner's request to reuse the BST's structure, the AVL tree uses parent pointers and a
fix-up walk to the root instead of the recursive design first planned (contract D10).

- [x] `AvlTree`: the BST's `insert` and `delete` (successor node relinked) followed by
      `fixUp`, `rebalance` with the LL, RR, LR and RL cases, CLRS rotations, `search`
      (path highlight), traversals, `clear`. (`restore()` moved to milestone 7.)
- [x] Snapshot includes `parent`, `height` and the derived `balance`. Frames for each
      comparison, insertion and removal, each height update, imbalance detection, the case
      (named in the explanation), every rotation, and the end of rebalancing. Any
      intermediate frame with a balance of ±2 is labelled "temporarily unbalanced".
      (Catalog listings follow with the catalog in milestone 6.)
- [x] AVL invariant checker that recomputes heights from the snapshot itself, checks BST
      order, stored heights and balances, and every balance in −1..+1, plus a frame
      check that every temporary imbalance is labelled.
- [x] Tests: the four insert cases (LL 30,20,10 · RR 10,20,30 · LR 30,10,20 ·
      RL 10,30,20) asserting the final shape, heights, case names, rotation steps and ID
      survival; a delete that rebalances the root, and one that rebalances two ancestors
      (LL at 30, then RL at 50); rejected duplicates and absent keys change nothing.
- [x] Randomized: 2,000 seeded mixed inserts and deletes compared with `std::set`, the
      checker after every operation, height ≤ 1.44 · log2(n + 2), and all four cases
      exercised (LL 86, LR 76, RR 98, RL 84 times). 200 increasing inserts give height 8.
- [x] UI: height and balance shown under each node, out-of-range nodes in red; a
      rotation is one consistent frame, so nodes and edges move together; presets for the
      four cases.
- [x] Checked in the browser (Playwright MCP): each preset's imbalance frame and final
      tree, `docs/screenshots/m4-avl-*.png`, no console errors.

**Definition of done:** all four rotation cases are demonstrable from the UI, and every
test above passes.

---

## Milestone 5: red-black tree

Design first: [red-black.md](red-black.md) (the sentinel and its owner, the five
properties, both fix-ups with their mirrors, the extra black, step IDs).

- [x] `RedBlackTree` (CLRS): NIL sentinel as a member, `insert` + insert fix-up
      (cases 1–3 and mirrors), `delete` + transplant + delete fix-up (cases 1–4 and
      mirrors), `search` (path highlight), traversals (the shared `TreeTraversal`, now
      told what NIL is), `clear`. (`restore()` moved to milestone 7, as for the AVL tree.)
- [x] Frames for each comparison, insertion and removal, each recolor (one frame per
      color assignment), each rotation, and each named fix-up case (`fixupCase` event);
      mirror cases have their own `…Mirror` step IDs (contract D24). Frames that break
      property 2, 4 or 5 say "temporary violation: …".
- [x] Red-black invariant checker for the properties in contract 2.7, working on the
      snapshot only: BST order, valid parent/child links, every node reachable, red or
      black, black NIL, black root, no red-red, equal black heights. For every frame it
      requires the "temporary violation" label exactly when a property is broken.
      Checked against three deliberately broken engines (a missing recolor, no labels,
      the BST's transplant): each one fails the tests.
- [x] Case coverage: one scenario per case and side (14) asserts the exact case sequence
      and the final shape and colors; a coverage test fails if any of the 14 case step
      IDs is never seen.
- [x] Delete tests: the root (leaf, one child, two children), a red leaf (no fix-up), a
      black leaf (fix-up), two-child deletes (deeper successor; successor with a NIL x
      found through `nil.parent`), and every key until the tree is empty.
- [x] Oracle against `std::set`: 5,000 seeded operations (the milestone request; this
      list first said 10,000), membership and in-order output after every one, the engine's
      own in-order traversal every 100; height ≤ 2 · log2(n + 1). All 14 case IDs occur.
      ID survival across rotations; 200 increasing inserts; 11-character values keep
      every explanation within 140 characters.
- [x] UI: red and black nodes, also marked with an "R"/"B" badge (and in the accessible
      name) so color is not the only signal; highlighting changes only the outline; a
      "Show NIL leaves" toggle; seven presets, one per fix-up case.
- [x] Checked in the browser (Playwright MCP): every preset's frames, a recolor and a
      rotation frame per preset in `docs/screenshots/m5-rb-*.png`; typing 10, 20, 30
      shows a recolor and a rotation and the root ends black; no console errors. (The
      automated end-to-end suite, `npm run test:e2e`, is milestone 6.)

**Definition of done:** every fix-up case can be reached from the UI and is covered by a
test; all checks pass.

---

## Milestone 6: the teaching UI (playback, layout, panels)

Rewritten to match the milestone request. No engine algorithm changed.

- [x] Engine data the UI needed: the `Catalog` (pseudocode, step IDs, Big-O for every
      operation, helper procedures included) and the `catalog` operation (contract 2.8,
      5.6); `counters` in every frame (contract 6.1). Every step ID of every frame of every
      test is now checked against its listing (contract 6.6 item 4), and a deliberately
      wrong listing line makes the tests fail.
- [x] Layout: structure selector on the left, operations above, the drawing in the
      centre, explanation / pseudocode / node properties / cost / text view on the right,
      playback and operation history below; one column below 900 px.
- [x] Player (`web/src/playback/player.ts`), one per structure, outside React: play,
      pause, step forward and back, replay, speed 0.5×–4×, timeline scrubber, jump to
      result. The authoritative state (`latest`) is kept apart from the frame on screen;
      operations wait while a frame other than the result is shown; batches are sent one
      at a time, in order; reset and switching structure cancel playback (one
      requestAnimationFrame loop per structure, one keyboard listener).
- [x] Pseudocode panel highlighting `frame.stepId`; node properties by ID; traversal output
      building up; counters "so far" and "whole operation" beside the catalog's Big-O and
      the tracing cost, labelled separately (rule 11).
- [x] Pan (drag), zoom (wheel, buttons, keys) and fit; the automatic view keeps labels
      readable (scale 0.7–1.5) and centres on where the operation acts when everything
      would be too small.
- [x] Examples: batch insert from a comma-separated list, seeded random examples, named
      presets (AVL rotations, red-black fix-up cases, two BST cases).
- [x] Keyboard shortcuts, accessible names, one visible focus style, reduced motion, and a
      text view of the frame on screen.
- [x] Checked with the Playwright MCP browser: every structure mid-animation at 1440×900
      and 390×844 (`docs/screenshots/m6-*.png`), a keyboard-only run, no console errors;
      `npm run test:cpp` and `npm run build` pass.

Moved to milestone 8: the automated `npm run test:e2e` suite (`@playwright/test`,
`playwright.config.ts`, the `window.__dslab.handle` hook and the Wasm-equals-native golden
check) and the click-to-first-frame / frame-to-frame timing measurements.

**Definition of done:** each structure can be played, paused, stepped and scrubbed with
pseudocode, properties and counters in step; screenshots at both sizes, a keyboard-only
run and a clean console are recorded; all checks pass.

---

## Milestone 7: undo/redo and import/export

Rewritten to match the milestone request. Design: architecture.md 2.9.

- [x] Engine operations `undo`, `redo`, `export`, `import` (contract 2.8 and 5.7), with
      `restore` frames that name the operation undone or redone.
- [x] Snapshot-based history (decision D23): at most 50 entries per structure, undo and
      redo together, oldest dropped first; a new change clears the redo list. Each entry
      holds the ID counter, which undo, redo and import restore exactly (decision D30,
      replacing "never lower the counter").
- [x] `restore()` for all seven structures; `validateSnapshot()` for rules 3–10 and the
      JSON layer for rules 1–2, both before anything changes, so an invalid file is
      rejected atomically.
- [x] Tests: undo/redo round trips for every structure (every field and the ID counter);
      the redo branch cleared; the 50-entry bound; one or more malformed-import tests per
      rule (1–10) with the message checked and state, counter and history unchanged;
      export → import → export byte for byte for all seven structures; an import undone
      and redone. Fixtures in `engine/tests/fixtures`: one valid and four to six invalid
      files per tree type. Checked against two deliberately broken engines.
- [x] UI: Undo / Redo enabled from the `history` counts (Ctrl+Z / Ctrl+Y); Export saves
      `dslab-<structure>.json`; Import opens a file picker and sends the text unread;
      errors appear in the explanation, the result line and the history log.
- [x] Checked with the Playwright MCP browser: an AVL tree built, undone three times,
      redone twice, exported, reset and imported (the export afterwards is byte-identical);
      a broken file shows `IMPORT_INVALID` and leaves the tree and its export unchanged
      (`docs/screenshots/m7-*.png`).

Moved to milestone 8: the `INTERNAL_ERROR` rollback in `Engine::apply` (it can now use
`restore()`).

**Definition of done:** history and files work on all seven structures, and all checks
pass.

---

## Milestone 8: audit of algorithm and UI correctness

Rewritten to match the milestone request.

- [x] All native tests run (`npm run test:cpp`).
- [x] `npm run test:cpp:asan` made practical and run: the sanitizer preset now builds
      with `-O1` (architecture.md section 4); every check stays on, including
      stack-use-after-return.
- [x] Seeded random sequences with standard-library oracles for every structure (queue,
      list, binary tree and BST added in `test_random.cpp`), with the invariant checkers
      after every operation (parent pointers, reachability, heights, colors, ordering);
      `test::run()` now also checks that each response's snapshot is the engine's real
      state; stored frames never alias live nodes; ID exhaustion on every structure;
      repeated create/fill/reset/import/destroy across all structures (`test_audit.cpp`).
- [x] Native versus Wasm: the same 1,298 command fixtures through `dslab_cli` and the
      Wasm engine of the production build, normalized and compared; invalid input never
      changes state; 300 Wasm engines created and destroyed.
- [x] `npm run test:e2e` with `@playwright/test` 1.64.1 against `npm run preview`
      (bundled Chromium): every step on screen matches its frame for all seven
      structures; pause/resume, rapid clicking, scrubbing, batch insert, reset, undo/redo,
      invalid import, structure switching, reduced motion, pan/zoom, the phone layout.
      Any console error or failed `.wasm` request fails a test.
- [x] Bugs found are fixed and covered by tests (the table in the milestone report).

Still open, not part of this request:

- [ ] Moved here from milestone 7: the `INTERNAL_ERROR` rollback in `Engine::apply`
      (restore the pre-operation snapshot after an unexpected exception), with a test.
- [ ] Measured click-to-first-frame and frame-to-frame times for the worst response
      (a 200-node red-black traversal).
- [ ] The rule-by-rule evidence of the original audit plan, in `docs/audit.md`: rule 1
      (no standard containers in the engine's structures; no algorithm in `web/src`),
      rule 2 (readonly snapshot types), rule 3 (ID survival for every relink), rule 4 (no
      `new`/`delete` outside `NodePool`; every non-owning pointer commented), rules 5–8
      (trace and rejection coverage per operation and error code), rule 9 (every bound
      has a test; largest response measured), rule 10 (viva readiness), rule 11 (test
      count never went down), rule 12 (catalog Big-O and counters), zero warnings in the
      Emscripten build, bundle sizes, an accessibility pass.

**Definition of done:** the outputs of `npm run test:cpp`, `npm run test:cpp:asan` and
`npm run test:e2e`, and a table of bugs found, root causes, fixes and covering tests.

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
