# Node ownership and memory management

Who owns each piece of memory in DSLab, how a node is created and freed, and how the
memory stays bounded. The binding rules are in operation-contract.md section 10; this
document explains them with the code.

## 1. The ownership tree

```
JavaScript page
 └─ one Embind handle `Engine` (web/src/engine/loadEngine.ts)
     └─ WasmEngine (engine/bindings/wasm_bindings.cpp)          owns ↓ by value
         └─ dslab::Engine (Engine.h)                             owns ↓ by value
             ├─ Stack, Queue, SinglyLinkedList, BinaryTree,
             │  Bst, AvlTree, RedBlackTree (one of each)        each owns ↓
             │   ├─ NodePool<Node>                               owns every node
             │   │   └─ std::vector<std::unique_ptr<Node>>       one unique_ptr per node
             │   ├─ entry pointers: top / front, rear / head / root    non-owning
             │   └─ (red-black only) Node nil_                   the NIL sentinel, a member
             └─ one History per structure                        owns snapshots (plain values)
```

Every arrow above is ownership by value or by `std::unique_ptr`, so every object has
exactly one owner and is freed automatically when its owner is destroyed (RAII). The
only `new`/`delete` in the engine is inside `std::make_unique` and `std::unique_ptr`,
in `NodePool`.

## 2. The pool owns the nodes; links do not

`engine/include/dslab/NodePool.h` is the only owner of nodes:

| Textbook | DSLab | What happens |
| --- | --- | --- |
| `node = new Node(value)` | `Node* node = pool_.create(value);` | `std::make_unique<Node>()`, the next ID is assigned, the `unique_ptr` is appended to the pool's vector, and a raw pointer is returned |
| `delete node` | `pool_.destroy(node);` | the `unique_ptr` is erased from the vector, which frees the node |

Every link is a raw, **non-owning** pointer, and each one says so in a comment:

```cpp
struct Node {
    NodeId id = kNoNode;
    Value value = 0;
    Node* next = nullptr;  // non-owning: the pool owns every node
};
NodePool<Node> pool_;
Node* top_ = nullptr;      // non-owning: the pool owns every node
```

Why this design:

- **Rotations and relinks read like the textbook.** `x->right = y->left;` changes a link
  and nothing else. With owning `unique_ptr` links, every relink would be a
  `std::move` that could free a subtree by mistake.
- **A half-finished operation still has every node.** During a BST delete the successor
  is detached before it is moved up; during a list reverse half the nodes are not
  reachable from `head`. The pool still owns them, so a frame recorded at that moment
  can show them, and nothing leaks.
- **No deep destructor chain.** Nodes do not own each other, so destroying a 200-node
  degenerate tree does not recurse 200 levels: the vector frees each node in turn.

## 3. The life of one node

Using `Stack::pop` (`engine/src/Stack.cpp`) as the example:

```cpp
Node* node = top_;     // non-owning: the pool owns every node
top_ = node->next;     // unlink: no link points at the node any more
const NodeValue removed{node->id, node->value};   // keep what the result needs
pool_.destroy(node);   // 'node' dangles from here on and is not used again
```

The rules every algorithm follows:

1. **Create**: `pool_.create(value)` after every check has passed (a rejected operation
   never creates a node, so no ID is used up).
2. **Link**: set the links that point at it.
3. **Unlink**: remove every link to it first.
4. **Destroy**: `pool_.destroy(node)` last. After that the pointer is never used again;
   values needed later are copied out first, as above.

Local cursors (`curr`, `prev`, `parent`, `x`, `y`, `w`) are non-owning too and carry the
same comment.

### The red-black NIL sentinel

`RedBlackTree` has one member `Node nil_` (black, no ID) and every empty child, the
root's parent and an empty tree's `root_` point at it (CLRS chapter 13). It is not in
the pool, so it is never freed separately; it lives exactly as long as the tree.
Pointers to it are non-owning like every other link.

### Why structures cannot be copied or moved

Every structure deletes its copy and move constructors and assignments; `NodePool`
deletes its copy operations (so it has no implicit move either). Copying would duplicate raw links that still point into the
original's nodes, and moving would change the address of `nil_` that the links point at.
The `Engine` holds each structure as a plain member for its whole life, so no node or
sentinel address ever changes while a link points at it.

## 4. Snapshots, frames and history are plain values

A `Snapshot` (`Snapshot.h`) is a vector of `NodeView`s: ID, value and links **as IDs**
(integers, 0 for NIL), plus heights or colors. It contains no pointers.

So:

- a frame keeps its snapshot correct forever, whatever happens to the live nodes
  (`test_audit.cpp` frees and replaces every node after an operation and checks the
  old frames are unchanged);
- copying a snapshot into history, into a response or into JSON can never create a
  dangling pointer;
- **undo, redo and import** rebuild a structure from a snapshot with `restore()`:
  `pool_.reset(nextId)` frees every node, `pool_.createWithId(id, value)` re-creates each
  one with its old ID, and a second pass sets the links by looking the IDs up. The
  structure object itself stays where it is, so `nil_` keeps its address.

Node IDs come from the pool's 64-bit counter and are never reused. The counter is part
of every snapshot, so undo and import restore it exactly (decision D30).

## 5. Bounds: memory cannot grow without limit

| What | Bound | Where |
| --- | --- | --- |
| Nodes per structure | 200 (`CAPACITY_EXCEEDED` beyond) | `Limits.h` |
| Frames per operation | 512 (in practice ≤ 2n + 16) | `Trace::frame` |
| One response | ≤ 512 × 200 node copies: about 3.7 MB in C++, at most about 8 MB of JSON | contract section 8 |
| History | 50 snapshots per structure, undo and redo together; the oldest is dropped | `History::record` |
| All history | 7 × 50 × 200 = 70,000 node copies, under 3 MB | |
| Request text | 1 MiB | `json/Protocol.cpp` |
| Wasm linear memory | grows on demand, capped at 128 MiB (`-sMAXIMUM_MEMORY`) | `CMakeLists.txt` |
| Wasm stack | 1 MiB (`-sSTACK_SIZE`), enough for 200-deep recursion on a degenerate tree | `CMakeLists.txt` |
| TypeScript | keeps only the newest response per structure; older frames become garbage | `player.ts` |

A trace is local to one call: `Engine::apply` creates it, the JSON layer writes its
frames out one at a time, and it is freed when the call returns.

## 6. The JavaScript side

- Only JSON strings cross the boundary. Embind copies the request string into Wasm
  memory as a `std::string` and copies the response string back. JavaScript never holds
  a node pointer, only node IDs.
- The page creates **one** `Engine` handle through a module-level promise
  (`loadEngine.ts`), so React StrictMode cannot create a second one. The handle lives as
  long as the page and is released with the page; the page never calls `.delete()` on it.
- Engines created by the end-to-end tests (`window.__dslab.createEngine()`) are deleted
  with `.delete()` after use; one test creates and deletes 300 of them.
- JavaScript memory (responses, layouts) is garbage-collected; the player drops a
  response when the next one arrives.

## 7. How this is checked

- `npm run test:cpp:asan` runs the whole native suite under AddressSanitizer
  (use-after-free, double free, buffer overflows, leaks through LeakSanitizer) and
  UndefinedBehaviorSanitizer. Last run: passed, no reports (README, "Tests").
- `test_audit.cpp`: frames never alias live nodes; engines are created, filled, reset,
  imported and destroyed repeatedly; ID exhaustion on every structure.
- `test::run()` checks after every operation in every test that the response's snapshot
  is the engine's real state and that every invariant holds (every node reachable,
  parent links consistent), so a lost or doubly-linked node is caught at once.
