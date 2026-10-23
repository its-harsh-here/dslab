# Traces, playback and undo/redo

How an operation becomes the steps you see, how the page plays them, and how undo and
redo work. The exact JSON schema is in
[operation-contract.md](operation-contract.md) (sections 5 and 6); this document explains
the design. Code: `engine/include/dslab/Frame.h`, `Trace.h`, `History.h`,
`engine/src/Engine.cpp` (C++) and `web/src/playback/` (TypeScript).

## 1. What a frame is

A **frame** is one step of a running algorithm: the whole structure at that moment, and
what the step was about. It is recorded **inside the C++ algorithm, at the moment the
step happens** (rule 7). Nothing is reconstructed afterwards, and TypeScript never creates
or merges frames.

Every frame has these fields (contract 6.1):

| Field | Meaning |
| --- | --- |
| `index` | 0, 1, 2… within one operation |
| `event` | one of a closed list: `begin`, `compare`, `move`, `visit`, `read`, `found`, `notFound`, `create`, `link`, `unlink`, `remove`, `update`, `imbalance`, `fixupCase`, `rotate`, `recolor`, `balanced`, `restore`, `reject`, `done` |
| `stepId` | the pseudocode line this step belongs to, for example `bst.insert.goRight`. Stable: the UI highlights the pseudocode line with this ID |
| `explanation` | one English sentence, written in C++, naming values ("25 > 20, so move to the right child, which is empty.") |
| `highlight` | the node IDs and edge IDs (`"from->to"`) the step touched |
| `pointers` | the algorithm's named cursors at this step, for example `{"parent": 2, "curr": null}` (`null` is NIL) |
| `output`, `pending` | traversals only: values output so far; the level-order queue |
| `counters` | the algorithm's counters so far (comparisons, nodes visited, links changed, rotations, recolors) |
| `snapshot` | the full structure: every node with its ID, value, links and (AVL) height and balance or (red-black) color |

A real frame: inserting 25 into a BST that holds 30 (ID 1) and 20 (ID 2), third of six
frames, as `engine/tools/dslab_cli` prints it:

```json
{
  "index": 2,
  "event": "compare",
  "stepId": "bst.insert.goRight",
  "explanation": "25 > 20, so move to the right child, which is empty.",
  "highlight": { "nodes": [2], "edges": [] },
  "pointers": { "parent": 2, "curr": null },
  "output": [],
  "pending": [],
  "counters": { "comparisons": 2, "nodesVisited": 2, "linksChanged": 0, "rotations": 0, "recolors": 0 },
  "snapshot": {
    "structure": "bst", "size": 2, "root": 1,
    "nodes": [
      { "id": 1, "value": 30, "left": 2, "right": null, "parent": null },
      { "id": 2, "value": 20, "left": null, "right": null, "parent": 1 }
    ]
  }
}
```

The six frames of that insert: `begin` (enter), `compare` (25 < 30, go left), `compare`
(25 > 20, go right: empty), `create` (new node 25, ID 3), `link` (attach as 20's right
child), `done`.

### How the C++ code records a frame

Each algorithm takes a `Trace&`. At each pseudocode step it calls

```cpp
trace.frame(snapshot(), Event::Compare, "bst.insert.goRight", sentence, highlight, pointers);
```

`Trace::frame` (`engine/src/Trace.cpp`) copies the counters, stores the snapshot and
appends the frame. A snapshot is a plain value (IDs and integers, no pointers), so a
frame stays valid however the structure changes later; a test frees and replaces every
node after an operation and checks that its frames have not changed.

### Rules every operation's frames follow (checked by the native tests)

`test::run()` (`engine/tests/support/Run.cpp`) checks these after **every** operation in
every test (contract 6.6):

1. Frame 0 is `begin`; the last frame is `done` (success) or `reject` (an operation
   error, with the same sentence as the error message).
2. The last frame's snapshot equals the response's final snapshot, which equals the
   engine's real state.
3. A rejected operation's frames all show the unchanged structure (rule 8).
4. Every `stepId` is a line of that operation's pseudocode in the catalog.
5. At most 2n + 16 frames (n = node count), and never more than 512.
6. Highlighted IDs exist in the frame, or were removed during this operation.

## 2. One response

Every request returns one JSON response (contract 5.2):

- `result`: `ok` with `data` (for example the new node's ID), or `error` with a typed
  `code` and a one-sentence `message`;
- `snapshot`: the structure after the operation;
- `frames`: the ordered list above (empty only for a protocol error such as a malformed
  request, which never reaches a structure);
- `metrics`: the algorithm counters (`metrics.algorithm`) and, separately, the tracing
  cost (`metrics.trace`: frames and snapshot node copies);
- `history`: how many steps can be undone and redone.

## 3. Playback in the browser

`web/src/playback/player.ts` has one `Player` per structure. It lives outside React;
components read it through `useSyncExternalStore` (`usePlayer.ts`).

**Two states, kept apart.** `latest` is the structure as C++ holds it now (the newest
response's snapshot). `animation` + `index` is the historical frame on screen. Anything
that needs the real structure (the next list index, a free slot for a random example)
reads `latest`; the drawing and the panels read the frame on screen.

**Laying out a response once.** When a response arrives, `animation.ts` computes node
positions for every frame in order (each continues from the previous one; the first
continues from what is on screen), and one viewBox for the whole operation so the
drawing never jumps. Stepping backwards is free because nothing is recomputed.

**Timing.** One `requestAnimationFrame` loop per structure, started and stopped only by
the player:

| Speed | Time per frame | Node movement |
| --- | --- | --- |
| 0.5× | 1200 ms | first 720 ms |
| 1× (default) | 600 ms | first 360 ms |
| 2× | 300 ms | first 180 ms |
| 4× | 150 ms | first 90 ms |

With `prefers-reduced-motion`, nodes do not move: frames still advance one by one.

**Controls** (buttons, scrubber and keys):

| Action | What the player does |
| --- | --- |
| Play / Pause (`Space`, `K`) | runs or stops the loop; pausing completes a move in progress, so the screen is always one exact frame |
| Step forward / back (`→`, `←`) | the next frame with its move, then pause / the previous frame, settled |
| Scrub (range input), First step (`Home`) | shows frame `i`, paused |
| Replay (`R`) | frame 0, playing |
| Jump to result (`End`) | the last frame; also sends any waiting batch steps, without animation, in order |
| Speed (`[`, `]`) | 0.5×, 1×, 2×, 4× |

**Busy.** While an operation plays, a batch waits, or any frame other than the result is
on screen, the player is *busy*: operation buttons are disabled and the status line says
"Playing insert 25: step 3 of 6. New operations wait for the result." with a **Jump to
result** button. `run()` refuses to start a second
operation while busy, so ten fast clicks send one request (an e2e test checks this).

**Batches** ("10, 20, 30", random examples, presets) are sent **one request at a time,
in order**: the next request is sent only when the previous operation's playback has
ended. Every request goes into the operation history, rejected ones included.

**Leaving a structure** cancels its playback and drops its waiting batch steps; the
structure itself keeps its state.

**What each frame draws.** For frame `i` at movement progress `t`, `sample()` returns the
frame and node positions interpolated between layout `i − 1` and layout `i`. Highlights,
pointers, output, counters, edges and labels all come from that one frame and that one
layout, so they can never disagree. React keys are node IDs, so a rotation animates as
the same nodes sliding to new places.

## 4. Undo and redo

Undo is **snapshot-based** and implemented in C++ (`History.h/.cpp`, `Engine.cpp`):

1. Before a successful change, `Engine::apply` keeps the structure's whole snapshot (ID
   counter included) on that structure's undo list, labelled with the operation
   ("insert 25"). A rejected operation changed nothing, so it adds no entry.
2. **Undo** puts the current snapshot on the redo list and calls `restore()` with the
   newest undo entry. **Redo** is the mirror image.
3. A new change after an undo **empties the redo list**: those states no longer follow
   from the current one.
4. **Bounded:** at most 50 entries per structure, undo and redo together. The 51st change
   drops the oldest undo entry.
5. `restore()` rebuilds the pool with the same IDs, values, links, heights or colors and
   the same next-ID counter. So after undoing an insert, inserting again gives the new
   node the same ID it had before (decision D30).

Undo and redo are operations like any other: they return frames (`begin`, `restore`,
`done`, or `begin`, `reject` with `NOTHING_TO_UNDO`), and the page plays them the same way.
`Ctrl+Z` and `Ctrl+Y` (or `Ctrl+Shift+Z`) trigger them.

Why snapshots and not "commands with inverses": one `restore()` per structure serves
undo, redo and import, it is exact by construction, and at 200 nodes a copy is cheap. A
command design would need a hand-written, separately tested inverse for every operation,
including AVL and red-black deletes. architecture.md section 2.9 has the full comparison.

**Reset** in the page sends `clear` (itself undoable) and empties the page's operation
log.

## 5. Export and import

- **Export** asks C++ for the structure as a versioned JSON document
  (`"format": "dslab-structure"`, `"formatVersion": 1`, the snapshot and the ID counter).
  The page saves the text as a file; it never reads or edits it.
- **Import** sends the file's text to C++ unread. C++ checks it in two layers before
  changing anything: the document (valid JSON, format, version, structure, exact fields
  and types), then the snapshot itself (IDs, links, tree shape, BST order, AVL heights and
  balances, red-black properties, the ID counter). Only then does `restore()` replace the
  structure, so an invalid file changes nothing and the response names the first broken
  rule. A successful import is one undoable operation.
