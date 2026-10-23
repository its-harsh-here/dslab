# Time complexity

Every operation in DSLab costs three different things, and this document keeps them
apart (rule 11):

1. **Algorithm cost**: the data-structure work itself, which is what a textbook's Big-O
   describes: key comparisons, nodes visited, links changed, rotations, recolors. It
   would be the same if no frame were recorded. The page shows it as "Algorithm work so
   far" (the counters) and "Theoretical complexity" (the catalog's Big-O).
2. **Tracing cost**: recording frames for the animation. Every frame copies the whole
   structure, so an operation that records F frames on n nodes costs O(F × n) time and
   memory in C++, and the same again to write it as JSON. The page shows it as "Tracing
   cost" (frames and snapshot nodes).
3. **Rendering cost**: what the browser does with the frames: layout, interpolation and
   SVG. TypeScript only.

Notation: n = nodes in the structure (at most 200), h = height of a tree (h ≤ n − 1;
h = O(log n) when balanced), i = a list index, k = nodes in a subtree, F = frames
recorded by one operation (at most 2n + 16, hard cap 512).

The Big-O column matches the C++ catalog (`engine/src/Catalog.cpp`), which is what the
page's pseudocode panel shows, and the contract tables (operation-contract.md section 2).

## 1. Algorithm cost per operation

### Stack (linked nodes, `top`)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `push(value)` | O(1) | O(1) | create a node, `node.next ← top`, `top ← node` |
| `pop()` | O(1) | O(1) | `top ← top.next`, free the old top |
| `peek()` | O(1) | O(1) | read `top` |
| `clear()` | O(n) | O(1) | free every node, one at a time |

### Queue (linked nodes, `front` and `rear`)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `enqueue(value)` | O(1) | O(1) | the `rear` pointer gives the end directly |
| `dequeue()` | O(1) | O(1) | `front ← front.next`; `rear ← NIL` if it was the last node |
| `front()` | O(1) | O(1) | read `front` |
| `clear()` | O(n) | O(1) | free every node |

### Singly linked list (`head` only)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `insertAt(i, value)` | O(i), so O(n) worst | O(1) | walk i − 1 links from `head` to the predecessor; index 0 is O(1) |
| `deleteNode(id)` | O(n) worst | O(1) | walk from `head` to the predecessor (no back links); the head is O(1) |
| `search(value)` | O(n) | O(1) | linear scan |
| `reverse()` | O(n) | O(1) | one pass with `prev`, `curr`, `next` |
| `clear()` | O(n) | O(1) | free every node |

### Ordinary binary tree (`root`, parent links)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `addRoot(value)` | O(1) | O(1) | only when the tree is empty |
| `addChild(parentId, side, value)` | O(1) given the parent | O(1) | the user chose the parent by clicking it (finding it by ID is support work, section 3) |
| `deleteSubtree(id)` | O(k) | O(h) recursion | post-order: children before parent |
| `preorder`, `inorder`, `postorder` | O(n) | O(h) recursion | each node visited once |
| `levelorder` | O(n) | O(n) queue | each node enqueued and dequeued once |
| `clear()` | O(n) | O(h) | post-order free |

### Binary search tree (CLRS, parent links)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `insert(value)` | O(h) | O(1) | one path from the root to a NIL child |
| `delete(value)` | O(h) | O(1) | search, then for two children find the in-order successor (down the right subtree) and relink it with `transplant` |
| `search(value)` | O(h) | O(1) | one path from the root |
| traversals | O(n) | O(h) or O(n) | as for the binary tree |
| `clear()` | O(n) | O(h) | |

h is O(log n) for a random insertion order but n − 1 for sorted input, so BST
operations are O(n) in the worst case. The BST preset "Sorted inserts (a degenerate tree)"
shows it.

### AVL tree (heights stored, parent links)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `insert(value)` | O(log n) | O(1) | BST insert, then a fix-up walk to the root updating heights; at most one single or double rotation |
| `delete(value)` | O(log n) | O(1) | BST delete, then the fix-up walk; up to O(log n) rotations, one per level |
| `search(value)` | O(log n) | O(1) | the height is always below 1.45 log₂(n + 2) |
| traversals, `clear()` | O(n) | O(log n) recursion, O(n) level-order queue | |

### Red-black tree (CLRS, one black NIL sentinel)

| Operation | Time | Extra space | Why |
| --- | --- | --- | --- |
| `insert(value)` | O(log n) | O(1) | BST insert, then `insertFixup`: O(log n) recolors moving up, at most 2 rotations |
| `delete(value)` | O(log n) | O(1) | BST delete, then `deleteFixup`: at most 3 rotations |
| `search(value)` | O(log n) | O(1) | the height is at most 2 log₂(n + 1) |
| traversals, `clear()` | O(n) | O(log n) recursion, O(n) level-order queue | |

### Shared operations (every structure)

| Operation | Time | Notes |
| --- | --- | --- |
| `snapshot`, `catalog` | O(n), O(1) | read-only |
| `undo`, `redo` | O(n²) worst, n ≤ 200 | swap a stored snapshot in, then `restore()` (section 3) |
| `export` | O(n) | the snapshot as JSON text |
| `import` | O(n log n) validation, then `restore()` | every rule is checked before anything changes |

Undo, redo, export and import are support operations of the simulator, not part of any
data structure; their costs are listed so nothing is hidden.

## 2. Tracing cost, measured

Each frame copies all n nodes (`snapshot()`, O(n)), so tracing costs O(F × n) time and
memory per operation, plus O(F × n) to write the JSON. F follows the algorithm: O(1)
frames for O(1) operations, O(h) for a tree path, O(n) for a traversal or a list walk.

Measured with the native command runner (`build/native/dslab_cli`) on structures of 200
nodes (the maximum), 2026-10-23. "Snapshot nodes" is the total number of node copies in
all frames (`metrics.trace.snapshotNodes`); "response" is the size of the JSON text.

| Structure | Operation (n ≈ 200) | Algorithm work (counters) | Frames | Snapshot nodes | Response |
| --- | --- | --- | --- | --- | --- |
| stack | `push` | 2 links | 5 | 999 | 40 KB |
| stack | `clear` | 200 visited, 200 links | 202 | 20,100 | 687 KB |
| queue | `dequeue` | 1 visited, 1 link | 5 | 998 | 40 KB |
| list | `insertAt` at the end | 199 visited, 2 links | 204 | 40,600 | 1.3 MB |
| list | `insertAt` at index 0 | 2 links | 5 | 999 | 40 KB |
| list | `reverse` | 199 visited, 200 links | 202 | 40,198 | 1.3 MB |
| binary tree | `inorder` | 199 visited | 400 | 79,600 | 4.6 MB |
| binary tree | `levelorder` | 199 visited | 201 | 39,999 | 2.4 MB |
| BST (random order) | `insert` | 5 comparisons, 4 visited | 8 | 1,595 | 105 KB |
| BST (random order) | `search` (missing) | 7 comparisons | 10 | 2,000 | 128 KB |
| BST (sorted order) | `insert` the largest | 200 comparisons, 199 visited | 203 | 40,400 | 2.3 MB |
| BST | `inorder` | 200 visited | 402 | 80,400 | 4.7 MB |
| AVL (sorted order) | `insert` the largest | 9 comparisons, 1 rotation | 25 | 4,991 | 421 KB |
| AVL (random order) | `delete` | 4 comparisons, 13 visited | 23 | 4,590 | 388 KB |
| red-black (sorted order) | `insert` the largest | 14 comparisons, 1 rotation, 5 recolors | 27 | 5,386 | 415 KB |
| red-black (random order) | `delete` | 4 comparisons, 1 recolor | 19 | 3,795 | 293 KB |
| red-black | `inorder` | 200 visited | 402 | 80,400 | 5.9 MB |

What the table shows:

- The same sorted input costs the BST 200 comparisons per insert and the AVL and
  red-black trees 9 and 14: the difference between O(n) and O(log n), in the counters.
- Tracing dominates real time. A BST `insert` does 5 comparisons but copies 1,595 nodes
  for its frames. That is why the counters (algorithm) and the frame counts (tracing)
  are always shown separately.
- The worst response is a traversal of 200 nodes: 2n + 2 = 402 frames, about 6 MB of
  JSON. The bounds in operation-contract.md section 8 (512 frames, 128 MiB Wasm memory)
  leave room for it.

To measure again, send JSON requests (one per line) to `build/native/dslab_cli` and read
`metrics` from each response; `npm run test:cpp` builds it.

## 3. Support cost (C++, not counted as algorithm work)

These stand in for things a textbook takes for granted, such as `new`, `delete` and
"the caller already has a pointer to the node":

| Work | Cost | Where |
| --- | --- | --- |
| Create a node | O(1) amortized (append to the pool's vector) | `NodePool::create` |
| Free a node | O(n): find it in the pool's vector and erase it | `NodePool::destroy` |
| Find a node by ID (the node the user clicked) | O(n) linear search, n ≤ 200 | `NodePool::find` |
| History entry before each change | O(n): one snapshot copy | `Engine::apply` |
| `restore()` (undo, redo, import) | O(n²): n nodes, each link found by `find` | each structure's `restore` |
| Import validation | O(n log n): `std::map` and `std::set` registries | `engine/src/Validate.cpp` |
| Request parsing and response writing | O(request size) and O(F × n) | `engine/src/json/Protocol.cpp` |

With n ≤ 200 these are microseconds; they are simple on purpose (rule 12).

## 4. Rendering cost (TypeScript, browser)

| Work | Cost | When |
| --- | --- | --- |
| Parse the response (`JSON.parse`) | O(F × n) | once per operation |
| Layout every frame (positions from links) | O(n) per frame, O(F × n) per operation | once per operation, cached; stepping back is free |
| Draw one animation tick: interpolate positions, render SVG nodes and edges (React keyed by node ID) | O(n) | every display frame (about 60 per second) while nodes move |
| Pseudocode, explanation, counters, node properties | O(listing length) | when the frame changes |
| Text view of the structure | O(n) | when the frame changes |

The page keeps only the newest response per structure; older frames are dropped.
