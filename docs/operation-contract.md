# DSLab operation contract

Milestone 1 document. This is the contract between the C++ engine and everything that
calls it: the browser, native tests and golden files. It defines every operation, its
preconditions and error codes, the duplicate policy, the placement rules, the versioned
JSON request/response schema, the frame schema, the limits and node ownership. The
decisions made while writing it are listed at the end.

When code and this document disagree, the document is the specification. Either fix the
code, or change the document deliberately in the same commit (section 9 says when that
needs a new schema version).

Contents:
1. Common rules
2. Operations per structure
3. Errors
4. Ordinary binary tree versus BST placement rules
5. JSON request/response schema (version 1)
6. Frame schema
7. Worked examples
8. Limits
9. Versioning rules
10. Node ownership
11. Decisions

---

## 1. Common rules

- **Values** are 32-bit signed integers, −2,147,483,648 to 2,147,483,647.
- **Node IDs** are positive 32-bit integers. Each structure has its own counter, starting
  at 1 and increasing by one for every node it creates. An ID is never reused within the
  structure for the life of the page: `clear`, `undo`, `redo` and `import` never lower the
  counter, and after a restore the counter becomes max(counter, largest restored ID + 1).
  A node keeps its ID through every rotation, transplant and relink. ID 0 means "no node"
  inside C++ and is written as `null` in JSON.
- **Capacity**: at most 200 nodes per structure.
- **Structures** and their wire names: `stack`, `queue`, `list` (singly linked list),
  `binaryTree` (ordinary binary tree), `bst`, `avl`, `redBlack`.
- **Selection is by ID.** The UI identifies nodes by ID only. The BST-family operations
  `delete` and `search` take a value because the textbook algorithm is keyed by value.
  Clicking a node in those tabs only copies its value into the input box.
- **Mutating operations** that succeed push exactly one undo entry (the snapshot from
  before the operation) and clear the redo list. Read-only and rejected operations do not
  touch history.
- **Rejected operations change nothing**: no node, link, size, ID counter or history
  entry changes (rule 8). Algorithms make every check before their first write.

---

## 2. Operations per structure

Notation: n = nodes in the structure, h = height of the tree, i = the index argument,
k = nodes in the removed subtree. "Algorithm" is the data-structure complexity only.
Tracing and serialization cost O(frames × n) on top and are reported separately (section
6.5). "Errors" lists operation errors in the order they are checked. Protocol errors
(section 3.1) can occur on every operation and are not repeated in the tables.

Every operation that creates a node first checks `CAPACITY_EXCEEDED`, then `ID_EXHAUSTED`.

### 2.1 Stack (linked nodes, `top` pointer)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `push` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED | `{nodeId}` | O(1) |
| `pop` | — | yes | EMPTY_STRUCTURE | `{nodeId, value}` of the removed node | O(1) |
| `peek` | — | no | EMPTY_STRUCTURE | `{nodeId, value}` of the top node | O(1) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

Duplicates allowed. `push` links the new node in front of `top`. `pop` removes the `top` node.

Step IDs (push is listed in 7.1; the same listings are comments in `engine/src/Stack.cpp`):

```
pop():                                         stack.pop.enter
  if top = NIL: reject EMPTY_STRUCTURE         stack.pop.empty
  node <- top                                  stack.pop.take       (move)
  top <- node.next                             stack.pop.moveTop    (link, or unlink when NIL)
  value <- node.value
  delete node                                  stack.pop.remove
  return value                                 stack.pop.done

peek():                                        stack.peek.enter
  if top = NIL: reject EMPTY_STRUCTURE         stack.peek.empty
  value <- top.value                           stack.peek.read      (read)
  return value                                 stack.peek.done

clear():                                       stack.clear.enter
  removed <- 0
  while top != NIL:
    node <- top
    top <- node.next
    delete node                                stack.clear.remove   (one frame per node)
    removed <- removed + 1
  return removed                               stack.clear.done
```

`push` also rejects `ID_EXHAUSTED` on the `stack.push.full` line, and its `linkNext`
step is an `unlink` event when the stack was empty (`node.next <- NIL`).

### 2.2 Queue (linked nodes, `front` and `rear` pointers)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `enqueue` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED | `{nodeId}` | O(1) |
| `dequeue` | — | yes | EMPTY_STRUCTURE | `{nodeId, value}` of the removed node | O(1) |
| `front` | — | no | EMPTY_STRUCTURE | `{nodeId, value}` of the front node | O(1) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

Duplicates allowed. `enqueue` links after `rear`. When the queue is empty, both `front`
and `rear` point at the new node. When `dequeue` removes the last node, both become null.

Step IDs (the same listings are comments in `engine/src/Queue.cpp`):

```
enqueue(value):                                queue.enqueue.enter
  if size = 200: reject CAPACITY_EXCEEDED      queue.enqueue.full
  node <- new Node(value)                      queue.enqueue.create
  if rear = NIL: front <- node                 queue.enqueue.linkFront
  else: rear.next <- node                      queue.enqueue.linkRear
  rear <- node                                 queue.enqueue.moveRear
  return node                                  queue.enqueue.done

dequeue():                                     queue.dequeue.enter
  if front = NIL: reject EMPTY_STRUCTURE       queue.dequeue.empty
  node <- front                                queue.dequeue.take       (move)
  front <- node.next                           queue.dequeue.moveFront  (link, or unlink when NIL)
  if front = NIL: rear <- NIL                  queue.dequeue.clearRear  (unlink)
  value <- node.value
  delete node                                  queue.dequeue.remove
  return value                                 queue.dequeue.done

front():                                       queue.front.enter
  if front = NIL: reject EMPTY_STRUCTURE       queue.front.empty
  value <- front.value                         queue.front.read
  return value                                 queue.front.done

clear():                                       queue.clear.enter
  while front != NIL:
    node <- front; front <- node.next
    if front = NIL: rear <- NIL
    delete node                                queue.clear.remove   (one frame per node)
  return removed                               queue.clear.done
```

### 2.3 Singly linked list (`head` pointer only)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insertAt` | `index`, `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, INDEX_OUT_OF_RANGE (unless 0 ≤ index ≤ n) | `{nodeId}` | O(i) |
| `deleteNode` | `nodeId` | yes | NODE_NOT_FOUND | `{nodeId, value}` | O(n): walks from `head` to the predecessor; O(1) for the head |
| `search` | `value` | no | — | `{found, nodeId, index}`; `nodeId` and `index` are null when not found | O(n) |
| `reverse` | — | yes | — | `{}` | O(n) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

Duplicates allowed. `search` returns the first match from `head`. `insertAt` with
index = n appends. `reverse` is the iterative three-pointer reversal (`prev`, `curr`,
`next`). It is valid on empty and one-node lists.

The list stores only `head`. The page draws a **TAIL** marker at the last node reachable
from `head` for readability. It is a display marker, not a pointer, so appending still
walks the whole list (O(n)). Explanations name a node as `#id (value)` where duplicates
could make a value ambiguous.

Step IDs (the same listings are comments in `engine/src/SinglyLinkedList.cpp`):

```
insertAt(index, value):                                  list.insertAt.enter
  if size = 200: reject CAPACITY_EXCEEDED                list.insertAt.full
  if index < 0 or index > size: reject INDEX_OUT_OF_RANGE  list.insertAt.badIndex
  if index = 0:
    node <- new Node(value)                              list.insertAt.create
    node.next <- head                                    list.insertAt.linkNext
    head <- node                                         list.insertAt.linkHead
  else:
    prev <- head                                         list.insertAt.start   (move)
    repeat index - 1 times: prev <- prev.next            list.insertAt.walk    (move)
    node <- new Node(value)                              list.insertAt.create
    node.next <- prev.next                               list.insertAt.linkNext
    prev.next <- node                                    list.insertAt.linkPrev
  return node                                            list.insertAt.done

deleteNode(id):                                          list.deleteNode.enter
  if no node has this ID: reject NODE_NOT_FOUND          list.deleteNode.missing
  if head is the node:
    node <- head                                         list.deleteNode.atHead   (found)
    head <- node.next                                    list.deleteNode.moveHead
  else:
    prev <- head                                         list.deleteNode.start    (move)
    while prev.next is not the node: prev <- prev.next   list.deleteNode.walk     (move)
    node <- prev.next                                    list.deleteNode.found    (found)
    prev.next <- node.next                               list.deleteNode.bypass
  delete node                                            list.deleteNode.remove
  return                                                 list.deleteNode.done

search(value):                                           list.search.enter
  curr <- head; index <- 0
  while curr != NIL:
    if curr.value = value: found                         list.search.found    (found)
    curr <- curr.next; index <- index + 1                list.search.next     (compare)
  not found                                              list.search.notFound (notFound)
  return                                                 list.search.done

reverse():                                               list.reverse.enter
  prev <- NIL; curr <- head
  while curr != NIL:
    next <- curr.next; curr.next <- prev                 list.reverse.flip    (one frame per node)
    prev <- curr; curr <- next
  head <- prev                                           list.reverse.moveHead
  return                                                 list.reverse.done

clear():                                                 list.clear.enter
  while head != NIL: node <- head; head <- node.next; delete node   list.clear.remove
  return removed                                         list.clear.done
```

### 2.4 Ordinary binary tree (`root`, nodes have `parent`)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `addRoot` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, ROOT_EXISTS | `{nodeId}` | O(1) |
| `addChild` | `parentId`, `side` (`"left"`/`"right"`), `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, NODE_NOT_FOUND, CHILD_EXISTS | `{nodeId}` | O(1) given the parent |
| `deleteSubtree` | `nodeId` | yes | NODE_NOT_FOUND | `{removedIds, removedCount}`; `removedIds` in post-order | O(k) |
| `preorder`, `inorder`, `postorder`, `levelorder` | — | no | — | `{values, nodeIds}` in visit order | O(n) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

Duplicates allowed. Placement is chosen by the user (section 4). `deleteSubtree` first
unlinks the selected node from its parent (or sets `root` to null), then frees the
subtree in post-order. Deleting the root empties the tree. Traversing an empty tree
succeeds with empty lists.

Step IDs (the same listings are comments in `engine/src/BinaryTree.cpp`):

```
addRoot(value):                                   binaryTree.addRoot.enter
  if size = 200: reject CAPACITY_EXCEEDED         binaryTree.addRoot.full
  if root != NIL: reject ROOT_EXISTS              binaryTree.addRoot.rootExists
  node <- new Node(value)                         binaryTree.addRoot.create
  root <- node                                    binaryTree.addRoot.linkRoot
  return node                                     binaryTree.addRoot.done

addChild(parentId, side, value):                  binaryTree.addChild.enter
  if size = 200: reject CAPACITY_EXCEEDED         binaryTree.addChild.full
  parent <- the node with parentId
  if there is none: reject NODE_NOT_FOUND         binaryTree.addChild.missing
  if parent.side != NIL: reject CHILD_EXISTS      binaryTree.addChild.occupied
  node <- new Node(value); node.parent <- parent  binaryTree.addChild.create
  parent.side <- node                             binaryTree.addChild.link
  return node                                     binaryTree.addChild.done

deleteSubtree(id):                                binaryTree.deleteSubtree.enter
  node <- the node with id
  if there is none: reject NODE_NOT_FOUND         binaryTree.deleteSubtree.missing
  root <- NIL, or node.parent.side <- NIL         binaryTree.deleteSubtree.cut     (unlink)
  freeSubtree(node)                               binaryTree.deleteSubtree.remove  (one frame per node)
  return the freed IDs (post-order)               binaryTree.deleteSubtree.done

freeSubtree(n):   (post-order: children before their parent)
  if n.left != NIL:  child <- n.left;  n.left <- NIL;  freeSubtree(child)
  if n.right != NIL: child <- n.right; n.right <- NIL; freeSubtree(child)
  delete n
```

`clear` cuts `root` (`binaryTree.clear.cut`) and frees the old tree the same way
(`binaryTree.clear.remove`). Cutting each link before its child is freed means no
remaining node ever points at freed memory, which matters because every frame copies the
whole structure.

**Traversals** (shared by the binary tree and the BST; `engine/include/dslab/TreeTraversal.h`).
Step IDs are `<structure>.<order>.<step>`:

| Step | Event | Meaning |
| --- | --- | --- |
| `enter` | begin | — |
| `start` | move | arrive at the root (pre-, in- and post-order) |
| `goLeft` / `goRight` | move | arrive at a child; the parent→child edge is highlighted |
| `visit` | visit | output the node's value (`output` grows). In level order: take the node from the front of the queue, visit it, add its children to the back (`pending` = the queue afterwards) |
| `done` | done | — |

Depth-first traversals record 2n + 2 frames (one arrival and one visit per node); level
order records n + 2. Level order uses a plain array as its queue, never `std::queue`.

### 2.5 Binary search tree (`root`, nodes have `parent`; CLRS style)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insert` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, DUPLICATE_VALUE | `{nodeId}` | O(h) |
| `delete` | `value` | yes | VALUE_NOT_FOUND | `{nodeId, value}` of the removed node | O(h) |
| `search` | `value` | no | — | `{found, nodeId}`; `nodeId` null when not found | O(h) |
| `preorder`, `inorder`, `postorder`, `levelorder` | — | no | — | `{values, nodeIds}` | O(n) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

- `insert`: iterative CLRS TREE-INSERT plus an equality check during the descent. Full
  pseudocode is in section 7.2.
- `delete`: CLRS TREE-DELETE with TRANSPLANT. A node with two children is replaced by its
  **in-order successor** (the minimum of its right subtree). The successor **node itself
  is relinked** into the deleted node's place; values are never copied between nodes, so
  every value keeps its ID. The deleted node's ID disappears.
- `search`: iterative. Every search frame highlights the whole path taken so far
  (nodes and edges), so the final frame shows the actual path.
- h ≤ n − 1, so a degenerate BST costs O(n).

Delete and search step IDs (insert is listed in 7.2; the same listings are comments in
`engine/src/Bst.cpp`):

```
delete(value):                                       bst.delete.enter
  z <- root
  while z != NIL and value != z.value:               bst.delete.goLeft / goRight   (compare)
    z <- (value < z.value) ? z.left : z.right
  if z = NIL: reject VALUE_NOT_FOUND                 bst.delete.notFound
  name the case:                                     bst.delete.caseLeaf          (case 1, no children)
                                                     bst.delete.caseOneChild      (case 2)
                                                     bst.delete.caseTwoChildren   (case 3)
  if z.left = NIL: transplant(z, z.right)            bst.delete.replace
  else if z.right = NIL: transplant(z, z.left)       bst.delete.replace
  else:
    y <- z.right                                     bst.delete.successorStart    (move)
    while y.left != NIL: y <- y.left                 bst.delete.successorLeft     (move)
                                                     bst.delete.successorFound    (found)
    if y.parent != z:
      transplant(y, y.right)                         bst.delete.detachSuccessor
      y.right <- z.right; y.right.parent <- y        bst.delete.successorTakesRight
    transplant(z, y)                                 bst.delete.replaceWithSuccessor
    y.left <- z.left; y.left.parent <- y             bst.delete.successorTakesLeft
  delete z                                           bst.delete.remove
  return                                             bst.delete.done

search(value):                                       bst.search.enter
  curr <- root
  while curr != NIL:
    if value = curr.value: found                     bst.search.found     (found)
    if value < curr.value: curr <- curr.left         bst.search.goLeft    (compare)
    else: curr <- curr.right                         bst.search.goRight   (compare)
  not found                                          bst.search.notFound  (notFound)
  return                                             bst.search.done
```

In frames, `pointers` use the CLRS names: `parent`/`curr` while walking down, `z` for the
node being deleted, `y` for its successor. `clear` cuts `root` and frees the tree in
post-order, like the binary tree.

### 2.6 AVL tree (`root`, nodes have `height`; no parent pointers)

Same operations, arguments, errors and data as the BST (2.5). Algorithm: `insert`,
`delete` and `search` are O(log n); traversals and `clear` are O(n).

Conventions:

- height(NIL) = 0, height(leaf) = 1, height(node) = 1 + max(height(left), height(right));
- balance(node) = height(left) − height(right); the tree is valid when every balance is
  −1, 0 or +1.

Algorithm (recursive, as usually taught):

- `insert(node, value)`: descend comparing values, create the node at the NIL position,
  then on the way back up update the height and rebalance each node on the path. A
  duplicate is detected on the way down. The recursion then returns without updating or
  rotating anything, and no height or rotation frames are emitted.
- `delete(node, value)`: descend to the node. With fewer than two children, its only
  child (or NIL) takes its place. With two children, `removeMin(node.right)` detaches the
  **in-order successor node**, rebalancing on the way back up, and that node takes the
  deleted node's place with the deleted node's children. Then update heights and
  rebalance on the way up to the root. Several rotations can happen in one delete.
- `rebalance(n)`:
  - balance +2 and balance(n.left) ≥ 0 → **LL**: rotateRight(n)
  - balance +2 and balance(n.left) < 0 → **LR**: rotateLeft(n.left), then rotateRight(n)
  - balance −2 and balance(n.right) ≤ 0 → **RR**: rotateLeft(n)
  - balance −2 and balance(n.right) > 0 → **RL**: rotateRight(n.right), then rotateLeft(n)
- `rotateLeft(x)`: y ← x.right; x.right ← y.left; y.left ← x; update height(x), then
  height(y); return y. `rotateRight` is the mirror. Rotations relink nodes; IDs and values
  never move between nodes.

### 2.7 Red-black tree (`root`, nodes have `parent` and `color`; CLRS)

Same operations, arguments, errors and data as the BST (2.5). Algorithm: `insert`,
`delete` and `search` are O(log n); traversals and `clear` are O(n).

- Follows CLRS: one shared black **NIL sentinel** per tree, RB-INSERT (the new node is
  red) with RB-INSERT-FIXUP (cases 1–3 and their mirrors), and RB-DELETE with
  RB-TRANSPLANT and RB-DELETE-FIXUP (cases 1–4 and their mirrors). Two-child delete uses
  the in-order successor node, relinked, as in 2.5.
- The sentinel is not a node of the structure: it has no ID, is not in the pool, is not
  counted in `size` and never appears in a snapshot. Links to it are written as `null`.
  When CLRS's `x` is the sentinel, the frame's `pointers` show `"x": null` and the
  explanation says "NIL".
- Properties checked by tests: every node is red or black; the root is black; a red node
  has no red child; every path from a node down to NIL has the same number of black
  nodes; in-order values strictly increase.
- Mirror cases share a step ID (for example `redBlack.insert.case1`). The explanation
  sentence says "left" or "right".

### 2.8 Shared operations (every structure)

| Operation | Args | Mutates | Errors | Success `data` | Milestone |
| --- | --- | --- | --- | --- | --- |
| `snapshot` | — | no | — | `{}` (the state is in `snapshot`) | 2 |
| `catalog` | — | no | — | `{operations: [...]}` (section 5.6) | 6 (moved from 2) |
| `undo` | — | yes | NOTHING_TO_UNDO | `{}` | 7 |
| `redo` | — | yes | NOTHING_TO_REDO | `{}` | 7 |
| `export` | — | no | — | `{text}`: the export document, pretty-printed | 7 |
| `import` | `text` | yes | IMPORT_INVALID | `{nodeCount}` | 7 |

- `undo` puts the current snapshot on the redo list and restores the newest undo entry.
  `redo` is the mirror. Neither pushes an undo entry of its own. Frames: `begin`,
  `restore`, `done`.
- `import` replaces the whole structure with the document's snapshot after full
  validation (section 5.7). It is undoable like any mutating operation. IDs in the file
  are kept exactly.
- `export` and `import` exchange text. TypeScript only saves or loads the file; it never
  parses or edits the document.
- Until a structure's milestone is reached, the engine answers requests for it with
  `UNKNOWN_STRUCTURE`. Until an operation's milestone is reached, it answers that
  operation with `UNKNOWN_OPERATION` (plan.md lists the milestones).

---

## 3. Errors

Every error has a stable code, a one-sentence English message generated in C++, and one
of three kinds.

### 3.1 Protocol errors: checked before the structure is touched

Response: `frames` is `[]`. `snapshot` and `history` describe the named structure if it
was recognized, otherwise they are `null`.

| Code | Meaning |
| --- | --- |
| `BAD_REQUEST` | Not JSON; not an object; a required field is missing; an unknown field is present (top level or in `args`); a field has the wrong type; a number that should be an integer is not one; `side` is not `"left"`/`"right"`; request text longer than 1 MiB. |
| `UNSUPPORTED_VERSION` | `schemaVersion` is present but is not 1. Checked first, so future versions are recognized before their fields are judged. |
| `UNKNOWN_STRUCTURE` | `structure` is not one of the seven wire names. |
| `UNKNOWN_OPERATION` | The structure has no such operation, or it is not implemented yet in the current milestone. |
| `VALUE_OUT_OF_RANGE` | An integer argument (`value`, `index`, `nodeId`, `parentId`) lies outside the 32-bit signed range. The message names the argument. |

### 3.2 Operation errors: the operation started and was rejected

Response: `frames` = `begin`, the frames of any steps that ran before the problem was
detected, then `reject`. Every frame's snapshot equals the begin snapshot. The final
`snapshot` is unchanged. The reject frame's explanation equals `result.message`.

| Code | Raised by | Example message |
| --- | --- | --- |
| `CAPACITY_EXCEEDED` | any operation that creates a node, when n = 200 | "The stack already holds 200 nodes, the maximum." |
| `ID_EXHAUSTED` | creating a node when the next ID would exceed 2,147,483,647 (reachable only after importing a file with huge IDs) | "No node IDs are left for this structure." |
| `EMPTY_STRUCTURE` | `pop`, `peek`, `dequeue`, `front` on an empty structure | "Cannot pop: the stack is empty." |
| `INDEX_OUT_OF_RANGE` | `insertAt` with index < 0 or index > n | "Index 7 is outside 0 to 3 for a list of 3 nodes." |
| `NODE_NOT_FOUND` | `deleteNode`, `addChild`, `deleteSubtree` with an ID that is not a node of this structure | "There is no node with ID 12 in this tree." |
| `ROOT_EXISTS` | `addRoot` on a non-empty binary tree | "The tree already has a root." |
| `CHILD_EXISTS` | `addChild` into an occupied side | "Node 15 already has a left child." |
| `DUPLICATE_VALUE` | `insert` of a value already present in `bst`, `avl`, `redBlack` | "20 is already in the BST, and a BST does not allow duplicates." |
| `VALUE_NOT_FOUND` | `delete` of an absent value in `bst`, `avl`, `redBlack` | "25 is not in the AVL tree, so there is nothing to delete." |
| `NOTHING_TO_UNDO` / `NOTHING_TO_REDO` | `undo` / `redo` with an empty list | "There is nothing to undo." |
| `IMPORT_INVALID` | `import` text that is not valid JSON, has the wrong format, version or structure, or breaks any rule in 5.7 | "Node 4 has balance 2, but an AVL tree allows only −1, 0 or +1." |

A search that finds nothing is **not** an error: it succeeds with `found: false`.

**Check order** (the first failing check wins): protocol checks → capacity → ID
exhaustion → structural preconditions (empty, index, node exists, root/child free) →
errors found by the algorithm while running (duplicate, value not found). Example: inserting
a duplicate into a full BST reports `CAPACITY_EXCEEDED`.

### 3.3 Internal error: should never happen

| Code | Meaning |
| --- | --- |
| `INTERNAL_ERROR` | An unexpected C++ exception or failed internal check. The engine restores the pre-operation snapshot, so state is unchanged. `frames` is `[]`. Tests treat any occurrence as a failure. (Milestone 2 status: `INTERNAL_ERROR` is answered, but the rollback arrives with `restore()`; see architecture.md section 6.) |

---

## 4. Ordinary binary tree versus BST placement rules

| Question | Ordinary binary tree (`binaryTree`) | BST family (`bst`, `avl`, `redBlack`) |
| --- | --- | --- |
| Who decides where a new node goes? | **The user.** `addRoot` on an empty tree, or `addChild` to a chosen side of a node selected by ID. | **The algorithm.** Start at the root; value < node → go left, value > node → go right, until a NIL position is reached. |
| Ordering of values | None. Any value can go anywhere. | Strict: every value in a node's left subtree is smaller, every value in its right subtree is larger. |
| Duplicates | Allowed. | Rejected with `DUPLICATE_VALUE` (equality is detected during the descent). |
| Occupied position | `ROOT_EXISTS` / `CHILD_EXISTS`. | Cannot happen; the descent always ends at NIL. |
| Delete | `deleteSubtree`: the selected node **and all its descendants**. | `delete(value)`: one node. Its children are re-attached (0 or 1 child: the child moves up; 2 children: the in-order successor node takes its place). |
| Search | Not offered (it would be an unordered O(n) traversal). | `search(value)`: O(h). |
| Shape | Exactly what the user built. | BST: depends on insertion order. AVL and red-black trees rebalance with rotations. |
| Traversals | pre-, in-, post- and level order. | The same; in-order lists values in increasing order. |

---

## 5. JSON request/response schema (version 1)

One request goes in and one response comes out, both as JSON text through
`Engine.handle(text)`. On the wire the JSON is compact. Keys appear in the order shown
here (the engine writes with `nlohmann::ordered_json`). Golden tests compare parsed JSON
in that order and ignore whitespace.

### 5.1 Request

```json
{
  "schemaVersion": 1,
  "requestId": 4,
  "structure": "bst",
  "operation": "insert",
  "args": { "value": 25 }
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `schemaVersion` | integer | Must be 1. |
| `requestId` | integer 0 to 2,147,483,647 | Chosen by the caller (TypeScript counts up from 1). Echoed back; the engine never relies on it. |
| `structure` | string | One of the seven wire names. |
| `operation` | string | An operation of that structure (section 2). |
| `args` | object | Exactly the arguments the operation lists. Always present, `{}` when there are none. Unknown keys are `BAD_REQUEST`. |

Argument types: `value`, `index`, `nodeId` and `parentId` are JSON integers; `side` is
`"left"` or `"right"`; `text` is a string.

### 5.2 Response

The smallest real response: request 1, `snapshot`, on an empty BST. Section 7 has
larger ones.

```json
{
  "schemaVersion": 1,
  "requestId": 1,
  "structure": "bst",
  "operation": "snapshot",
  "result": { "status": "ok", "data": {} },
  "snapshot": { "structure": "bst", "size": 0, "root": null, "nodes": [] },
  "frames": [
    {
      "index": 0,
      "event": "begin",
      "stepId": "common.snapshot.enter",
      "explanation": "Read the current state of the BST.",
      "highlight": { "nodes": [], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": { "structure": "bst", "size": 0, "root": null, "nodes": [] }
    },
    {
      "index": 1,
      "event": "done",
      "stepId": "common.snapshot.done",
      "explanation": "The BST is empty.",
      "highlight": { "nodes": [], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": { "structure": "bst", "size": 0, "root": null, "nodes": [] }
    }
  ],
  "metrics": {
    "algorithm": { "comparisons": 0, "nodesVisited": 0, "linksChanged": 0, "rotations": 0, "recolors": 0 },
    "trace": { "frames": 2, "snapshotNodes": 0, "truncated": false }
  },
  "history": { "undo": 0, "redo": 0 }
}
```

| Field | Rule |
| --- | --- |
| `schemaVersion` | Always 1. TypeScript refuses any other value. |
| `requestId`, `structure`, `operation` | Echoed when they were valid and recognized, otherwise `null`. |
| `result` | `{"status": "ok", "data": {...}}` or `{"status": "error", "code": "...", "message": "..."}`. |
| `snapshot` | The structure after the operation (section 5.4), or `null` for a protocol error without a recognized structure. |
| `frames` | Ordered frames (section 6). `[]` for protocol and internal errors. |
| `metrics.algorithm` | Counters incremented by the algorithm itself (section 6.5). |
| `metrics.trace` | `frames` = number of frames; `snapshotNodes` = total node views copied into all frame snapshots; `truncated` = true only if the frame cap was hit, which tests treat as a bug. |
| `history` | `{"undo": u, "redo": r}`: entries available for this structure, or `null` when no structure was recognized. |

### 5.3 Protocol error response example

```json
{
  "schemaVersion": 1,
  "requestId": 9,
  "structure": "stack",
  "operation": "push",
  "result": {
    "status": "error",
    "code": "VALUE_OUT_OF_RANGE",
    "message": "The value 3000000000 is outside the 32-bit signed integer range."
  },
  "snapshot": { "structure": "stack", "size": 0, "top": null, "nodes": [] },
  "frames": [],
  "metrics": {
    "algorithm": { "comparisons": 0, "nodesVisited": 0, "linksChanged": 0, "rotations": 0, "recolors": 0 },
    "trace": { "frames": 0, "snapshotNodes": 0, "truncated": false }
  },
  "history": { "undo": 0, "redo": 0 }
}
```

### 5.4 Snapshot

A snapshot lists **every node the structure owns**, in ascending ID order, with **every
field the node stores**, plus `balance` for AVL (derived, sent so that TypeScript never
computes it). Nodes are listed even when they are temporarily unreachable in the middle of
an operation. That is what makes mid-operation frames complete.

| `structure` | Entry fields | Node fields (in this order) |
| --- | --- | --- |
| `stack` | `top` | `id`, `value`, `next` |
| `queue` | `front`, `rear` | `id`, `value`, `next` |
| `list` | `head` | `id`, `value`, `next` |
| `binaryTree` | `root` | `id`, `value`, `left`, `right`, `parent` |
| `bst` | `root` | `id`, `value`, `left`, `right`, `parent` |
| `avl` | `root` | `id`, `value`, `left`, `right`, `height`, `balance` |
| `redBlack` | `root` | `id`, `value`, `left`, `right`, `parent`, `color` (`"red"`/`"black"`) |

Key order: `structure`, `size`, the entry fields, `nodes`. Every link and entry field is
a node ID or `null`. `size` is the number of nodes owned, which equals `nodes.length`.

**Edges.** Every non-null `next`, `left` or `right` link is an edge whose ID is the string
`"<fromId>-><toId>"`, for example `"1->2"`. `parent` links mirror child links: they are not
drawn and have no edge ID. Entry pointers (`top`, `front`, `rear`, `head`, `root`) are
drawn as labels, not edges. C++ (highlights) and TypeScript (rendering) build edge IDs with
the same rule.

### 5.5 Result `data` shapes

| Operation(s) | `data` |
| --- | --- |
| `push`, `enqueue`, `insertAt`, `addRoot`, `addChild`, `insert` | `{"nodeId": 7}` |
| `pop`, `dequeue`, `deleteNode`, `delete` | `{"nodeId": 7, "value": 42}` (the removed node) |
| `peek`, `front` | `{"nodeId": 7, "value": 42}` |
| `search` (list) | `{"found": true, "nodeId": 7, "index": 2}` or `{"found": false, "nodeId": null, "index": null}` |
| `search` (BST family) | `{"found": true, "nodeId": 7}` or `{"found": false, "nodeId": null}` |
| `preorder`, `inorder`, `postorder`, `levelorder` | `{"values": [20, 30, 40], "nodeIds": [2, 1, 3]}` |
| `deleteSubtree` | `{"removedIds": [5, 6, 3], "removedCount": 3}` |
| `clear` | `{"removedCount": 3}` |
| `reverse`, `snapshot`, `undo`, `redo` | `{}` |
| `export` | `{"text": "<export document, 2-space indented>"}` |
| `import` | `{"nodeCount": 12}` |
| `catalog` | see 5.6 |

### 5.6 Catalog

`catalog` returns the pseudocode that the UI shows, with the step IDs that frames refer
to. The C++ catalog is the single source of the listings. Tests check that every step ID
emitted by an operation appears in that operation's listing.

```json
{
  "operations": [
    {
      "operation": "push",
      "args": ["value"],
      "mutates": true,
      "complexity": "O(1)",
      "pseudocode": [
        { "stepId": "stack.push.enter", "text": "push(value):" },
        { "stepId": "stack.push.full", "text": "  if size = 200: reject CAPACITY_EXCEEDED" },
        { "stepId": "stack.push.create", "text": "  node <- new Node(value)" },
        { "stepId": "stack.push.linkNext", "text": "  node.next <- top" },
        { "stepId": "stack.push.moveTop", "text": "  top <- node" },
        { "stepId": "stack.push.done", "text": "  return node" }
      ]
    }
  ]
}
```

Lines that never produce a frame have `"stepId": null`. Leading spaces in `text` are the
indentation.

### 5.7 Export document and import validation

```json
{
  "format": "dslab-structure",
  "formatVersion": 1,
  "snapshot": { "structure": "stack", "size": 1, "top": 1, "nodes": [ { "id": 1, "value": 10, "next": null } ] }
}
```

The `snapshot` is exactly the snapshot schema of 5.4. `import` (and every internal
restore) accepts it only if all of the following hold. Otherwise it fails with
`IMPORT_INVALID`, and the message names the first rule broken and the node ID.

1. Valid JSON; `format` is `"dslab-structure"`; `formatVersion` is 1;
   `snapshot.structure` equals the structure being imported into.
2. Exactly the fields of 5.4 for that structure: none missing, none extra.
3. At most 200 nodes; `size` equals the number of nodes; IDs unique, in 1 to
   2,147,483,647 and listed in ascending order; values are 32-bit integers.
4. Every link refers to a listed node or is `null`.
5. Linear structures: the entry pointer is `null` exactly when there are no nodes;
   following `next` from it visits every node exactly once and ends at `null`; a queue's
   `rear` is the last node of that chain.
6. Trees: `root` is `null` exactly when there are no nodes; the root has no incoming
   child link and every other node exactly one; every node is reachable from the root;
   `parent` (where stored) equals the real parent.
7. BST family: in-order values strictly increase (so there are no duplicates).
8. AVL: every `height` equals the recomputed height, and every `balance` equals
   height(left) − height(right) and is −1, 0 or +1.
9. Red-black: colors are `"red"` or `"black"`; the root is black; no red node has a red
   child; every path to NIL has the same number of black nodes.

Hand-edited files with inconsistent derived fields (`size`, `rear`, `parent`, `height`,
`balance`) are rejected, never silently repaired: the file must be a snapshot the engine
could have produced.

---

## 6. Frame schema

A frame is one animation step, recorded **inside the running algorithm** at the moment
the step happens (rule 7). It shows the state **immediately after** its pseudocode step
ran.

### 6.1 Fields

| Field | Type | Meaning |
| --- | --- | --- |
| `index` | integer | 0, 1, 2… within this response. |
| `event` | string | One of the event types in 6.2. |
| `stepId` | string | Stable pseudocode step ID (6.3). |
| `explanation` | string | One English sentence (6.4). |
| `highlight` | `{"nodes": [ids], "edges": ["from->to"]}` | What the step touched. IDs may name a node removed in this step; the renderer ignores IDs that are not in the snapshot. |
| `pointers` | object, name → ID or `null` | The algorithm's named node variables that this step uses, for example `{"parent": 2, "curr": null}`. `null` means NIL. Entry pointers are not repeated here; they are in the snapshot. |
| `output` | array of values | Traversals only: the values visited so far. `[]` otherwise. |
| `pending` | array of IDs | Level-order traversal only: the queue's contents, front first. `[]` otherwise. |
| `snapshot` | snapshot | The full structure at this moment (5.4). |

Rule 6 asks for snapshot, highlighted node and edge IDs, event type, explanation and step
ID. `index`, `pointers`, `output` and `pending` are additions that make playback and
teaching clearer.

### 6.2 Event types (closed list)

| Event | Meaning |
| --- | --- |
| `begin` | First frame of every operation that reaches a structure: the state before anything happened. |
| `compare` | A value was compared with a node's value; usually a cursor then moves along the highlighted edge. |
| `move` | A cursor moved to another node without a key comparison (list walks, successor search, traversal descent). |
| `visit` | A traversal output a node's value (`output` grows). |
| `read` | A value was read without changing anything (`peek`, `front`). |
| `found` | A search found its value. |
| `notFound` | A search reached NIL or the end of the list. |
| `create` | A node was allocated (it appears, not yet linked). |
| `link` | A link or entry pointer was set to a node. |
| `unlink` | A link or entry pointer was set to NIL, or a node was detached. |
| `remove` | A node was freed; it is absent from this frame's snapshot. |
| `update` | A stored field other than a link or color changed (AVL height). |
| `rotate` | A left or right rotation finished. |
| `recolor` | One or more red-black colors changed in one step. |
| `restore` | `undo`, `redo` or `import` replaced the whole structure. |
| `reject` | The operation was rejected. Always the last frame of an operation error. |
| `done` | The operation succeeded. Always the last frame of a success. |

### 6.3 Step IDs

- Format: `<structure>.<operation>.<step>`, all lower camelCase, for example
  `bst.insert.goLeft` or `redBlack.delete.case2`. Shared operations from 2.8 use `common`
  as the structure (`common.undo.restore`).
- Every step ID appears in the catalog listing for its operation (5.6).
- Step IDs are **stable**: once released, renaming or removing one is a breaking change
  (section 9).
- A reject frame uses the step ID of the line that rejects (`stack.pop.empty`,
  `bst.insert.duplicate`).

### 6.4 Explanations

One English sentence, at most 140 characters, ending with a period, written in C++. It
names **values**, not IDs ("25 < 30, so move to the left child, 20."). The reject frame's
explanation is identical to `result.message`.

### 6.5 Counters: algorithm work versus tracing cost (rule 11)

`metrics.algorithm` counts only what the algorithm does. The counts would be the same if
no frame were recorded:

| Counter | Counts |
| --- | --- |
| `comparisons` | Key comparisons. A three-way comparison of the value with one node's value counts once. |
| `nodesVisited` | Times a cursor variable is set to a node (not to NIL). |
| `linksChanged` | Writes to `next`, `left`, `right`, `parent` and to entry pointers. |
| `rotations` | Single rotations (an LR or RL case counts 2). |
| `recolors` | Nodes whose color changed. |

`metrics.trace` measures the cost of tracing (`frames`, `snapshotNodes`). The catalog's
`complexity` is the Big-O of the algorithm alone.

### 6.6 Frame-list invariants (checked by native tests for every operation)

1. Every success and every operation error has at least 2 frames; frame 0 is `begin`
   and the last is `done` (success) or `reject` (operation error).
2. The last frame's snapshot equals the response's `snapshot`.
3. For a rejected operation, every frame's snapshot equals frame 0's snapshot.
4. Every `stepId` is in the catalog for that operation.
5. Frame count ≤ 2n + 16, where n is the larger of the node counts before and after (the
   hard cap is 512, see section 8).
6. `index` runs 0, 1, 2, … without gaps.
7. Highlighted IDs exist in that frame's snapshot or were removed during this operation.

---

## 7. Worked examples

Both examples are the first golden files (`engine/tests/golden/`). The pseudocode listings
below are the catalog entries for these operations.

### 7.1 Stack push

Starting state: requests 1 and 2 pushed 10 (node 1) and 20 (node 2). Request 3 pushes 30.

Pseudocode and step IDs:

```
push(value):                                   stack.push.enter
  if size = 200: reject CAPACITY_EXCEEDED      stack.push.full
  node <- new Node(value)                      stack.push.create
  node.next <- top                             stack.push.linkNext
  top <- node                                  stack.push.moveTop
  return node                                  stack.push.done
```

Request:

```json
{
  "schemaVersion": 1,
  "requestId": 3,
  "structure": "stack",
  "operation": "push",
  "args": { "value": 30 }
}
```

Response:

```json
{
  "schemaVersion": 1,
  "requestId": 3,
  "structure": "stack",
  "operation": "push",
  "result": { "status": "ok", "data": { "nodeId": 3 } },
  "snapshot": {
    "structure": "stack", "size": 3, "top": 3,
    "nodes": [
      { "id": 1, "value": 10, "next": null },
      { "id": 2, "value": 20, "next": 1 },
      { "id": 3, "value": 30, "next": 2 }
    ]
  },
  "frames": [
    {
      "index": 0,
      "event": "begin",
      "stepId": "stack.push.enter",
      "explanation": "Push 30 onto a stack that holds 2 nodes.",
      "highlight": { "nodes": [], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "stack", "size": 2, "top": 2,
        "nodes": [
          { "id": 1, "value": 10, "next": null },
          { "id": 2, "value": 20, "next": 1 }
        ]
      }
    },
    {
      "index": 1,
      "event": "create",
      "stepId": "stack.push.create",
      "explanation": "Create a new node holding 30.",
      "highlight": { "nodes": [3], "edges": [] },
      "pointers": { "node": 3 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "stack", "size": 3, "top": 2,
        "nodes": [
          { "id": 1, "value": 10, "next": null },
          { "id": 2, "value": 20, "next": 1 },
          { "id": 3, "value": 30, "next": null }
        ]
      }
    },
    {
      "index": 2,
      "event": "link",
      "stepId": "stack.push.linkNext",
      "explanation": "Point the new node's next at the current top, 20.",
      "highlight": { "nodes": [3, 2], "edges": ["3->2"] },
      "pointers": { "node": 3 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "stack", "size": 3, "top": 2,
        "nodes": [
          { "id": 1, "value": 10, "next": null },
          { "id": 2, "value": 20, "next": 1 },
          { "id": 3, "value": 30, "next": 2 }
        ]
      }
    },
    {
      "index": 3,
      "event": "link",
      "stepId": "stack.push.moveTop",
      "explanation": "Move top to the new node, so 30 is now on top.",
      "highlight": { "nodes": [3], "edges": [] },
      "pointers": { "node": 3 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "stack", "size": 3, "top": 3,
        "nodes": [
          { "id": 1, "value": 10, "next": null },
          { "id": 2, "value": 20, "next": 1 },
          { "id": 3, "value": 30, "next": 2 }
        ]
      }
    },
    {
      "index": 4,
      "event": "done",
      "stepId": "stack.push.done",
      "explanation": "Pushed 30; the stack now holds 3 nodes.",
      "highlight": { "nodes": [3], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "stack", "size": 3, "top": 3,
        "nodes": [
          { "id": 1, "value": 10, "next": null },
          { "id": 2, "value": 20, "next": 1 },
          { "id": 3, "value": 30, "next": 2 }
        ]
      }
    }
  ],
  "metrics": {
    "algorithm": { "comparisons": 0, "nodesVisited": 0, "linksChanged": 2, "rotations": 0, "recolors": 0 },
    "trace": { "frames": 5, "snapshotNodes": 14, "truncated": false }
  },
  "history": { "undo": 3, "redo": 0 }
}
```

Notes: in frame 1, node 3 exists but is not reachable from `top`, so the layout puts it
in the staging spot next to the top node. `linksChanged` = 2 (`node.next`, `top`).
`snapshotNodes` = 2 + 3 + 3 + 3 + 3 = 14. `history.undo` = 3 because each of the three
pushes made one entry.

### 7.2 Insert 25 into a BST holding 30, 20 and 40

Starting state: requests 1–3 inserted 30 (node 1, the root), 20 (node 2, left of 30) and
40 (node 3, right of 30). Request 4 inserts 25.

```
        30 (1)                         30 (1)
       /      \          →            /      \
   20 (2)    40 (3)               20 (2)    40 (3)
                                       \
                                       25 (4)
```

Pseudocode and step IDs (CLRS TREE-INSERT with a duplicate check):

```
insert(value):                                         bst.insert.enter
  if size = 200: reject CAPACITY_EXCEEDED              bst.insert.full
  parent <- NIL; curr <- root
  while curr != NIL:
    if value = curr.value: reject DUPLICATE_VALUE      bst.insert.duplicate
    parent <- curr
    if value < curr.value: curr <- curr.left           bst.insert.goLeft
    else: curr <- curr.right                           bst.insert.goRight
  node <- new Node(value); node.parent <- parent       bst.insert.create
  if parent = NIL: root <- node                        bst.insert.linkRoot
  else if value < parent.value: parent.left <- node    bst.insert.linkLeft
  else: parent.right <- node                           bst.insert.linkRight
  return node                                          bst.insert.done
```

Request:

```json
{
  "schemaVersion": 1,
  "requestId": 4,
  "structure": "bst",
  "operation": "insert",
  "args": { "value": 25 }
}
```

Response:

```json
{
  "schemaVersion": 1,
  "requestId": 4,
  "structure": "bst",
  "operation": "insert",
  "result": { "status": "ok", "data": { "nodeId": 4 } },
  "snapshot": {
    "structure": "bst", "size": 4, "root": 1,
    "nodes": [
      { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
      { "id": 2, "value": 20, "left": null, "right": 4, "parent": 1 },
      { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 },
      { "id": 4, "value": 25, "left": null, "right": null, "parent": 2 }
    ]
  },
  "frames": [
    {
      "index": 0,
      "event": "begin",
      "stepId": "bst.insert.enter",
      "explanation": "Insert 25 into a BST that holds 3 nodes.",
      "highlight": { "nodes": [], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 3, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": null, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 }
        ]
      }
    },
    {
      "index": 1,
      "event": "compare",
      "stepId": "bst.insert.goLeft",
      "explanation": "25 < 30, so move to the left child, 20.",
      "highlight": { "nodes": [1], "edges": ["1->2"] },
      "pointers": { "parent": 1, "curr": 2 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 3, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": null, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 }
        ]
      }
    },
    {
      "index": 2,
      "event": "compare",
      "stepId": "bst.insert.goRight",
      "explanation": "25 > 20, so move to the right child, which is empty.",
      "highlight": { "nodes": [2], "edges": [] },
      "pointers": { "parent": 2, "curr": null },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 3, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": null, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 }
        ]
      }
    },
    {
      "index": 3,
      "event": "create",
      "stepId": "bst.insert.create",
      "explanation": "Create a new node holding 25 whose parent is 20.",
      "highlight": { "nodes": [4], "edges": [] },
      "pointers": { "parent": 2, "node": 4 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 4, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": null, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 },
          { "id": 4, "value": 25, "left": null, "right": null, "parent": 2 }
        ]
      }
    },
    {
      "index": 4,
      "event": "link",
      "stepId": "bst.insert.linkRight",
      "explanation": "25 > 20, so attach the new node as the right child of 20.",
      "highlight": { "nodes": [2, 4], "edges": ["2->4"] },
      "pointers": { "parent": 2, "node": 4 },
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 4, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": 4, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 },
          { "id": 4, "value": 25, "left": null, "right": null, "parent": 2 }
        ]
      }
    },
    {
      "index": 5,
      "event": "done",
      "stepId": "bst.insert.done",
      "explanation": "Inserted 25; the BST now holds 4 nodes.",
      "highlight": { "nodes": [4], "edges": [] },
      "pointers": {},
      "output": [],
      "pending": [],
      "snapshot": {
        "structure": "bst", "size": 4, "root": 1,
        "nodes": [
          { "id": 1, "value": 30, "left": 2, "right": 3, "parent": null },
          { "id": 2, "value": 20, "left": null, "right": 4, "parent": 1 },
          { "id": 3, "value": 40, "left": null, "right": null, "parent": 1 },
          { "id": 4, "value": 25, "left": null, "right": null, "parent": 2 }
        ]
      }
    }
  ],
  "metrics": {
    "algorithm": { "comparisons": 3, "nodesVisited": 2, "linksChanged": 2, "rotations": 0, "recolors": 0 },
    "trace": { "frames": 6, "snapshotNodes": 21, "truncated": false }
  },
  "history": { "undo": 4, "redo": 0 }
}
```

Notes:

- `comparisons` = 3: 25 against 30 and against 20 in the loop, then 25 against 20 again
  to choose the side when linking (exactly as CLRS writes it). `nodesVisited` = 2: `curr`
  was set to node 1 and then node 2; its final NIL does not count. `linksChanged` = 2:
  `node.parent` and `parent.right`.
- In frame 3, node 4 already has its `parent` field, but 20's `right` is still `null`.
  Node 4 is owned and listed, but not yet reachable from the root.
- `snapshotNodes` = 3 + 3 + 3 + 4 + 4 + 4 = 21.
- `history.undo` = 4: one entry for each of the three setup inserts and one for this
  insert. (Milestone 1 printed 3 here, which contradicted the rule in section 1; the
  golden test caught it in milestone 3.)

**Same tree, rejected:** inserting 20 instead gives frames `begin`
(`bst.insert.enter`), `compare` (`bst.insert.goLeft`, "20 < 30, so move to the left child,
20.") and `reject` (`bst.insert.duplicate`, "20 is already in the BST, and a BST does not
allow duplicates."). All three snapshots equal the starting state, `result` is
`DUPLICATE_VALUE`, no node ID is used and `history` stays at `{"undo": 3, "redo": 0}`.

---

## 8. Limits

All of these live in `engine/include/dslab/Limits.h`; tests check each bound.

| Limit | Value | Reason / consequence |
| --- | --- | --- |
| Nodes per structure | 200 | `CAPACITY_EXCEEDED` beyond it. |
| Value range | 32-bit signed | `VALUE_OUT_OF_RANGE` (protocol) outside it. |
| Node ID range | 1 to 2,147,483,647, never reused | `ID_EXHAUSTED` if the counter would pass the maximum. |
| Frames per operation | 512 | The worst real operation is a traversal of 200 nodes: 2n + 2 = 402 frames. Tests require ≤ 2n + 16. If the cap were ever hit, step frames stop being stored, the final `done`/`reject` frame is still added and `metrics.trace.truncated` is true. Tests fail on that. |
| Node views per response | ≤ 512 × 200 = 102,400 | About 3.7 MB in C++; at most about 8 MB of JSON (about 6.5 MB for a 200-node red-black traversal; about 0.5 MB for a typical AVL insert into 200 nodes). |
| History | 50 undo + 50 redo snapshots per structure | The oldest undo entry is dropped when a 51st is pushed. All 7 structures together hold ≤ 7 × 100 × 200 = 140,000 node views (about 5 MB). |
| Request text | 1 MiB (1,048,576 bytes) | `BAD_REQUEST` beyond it. An export of 200 nodes is about 16 KB. |
| Explanation | 1 sentence, ≤ 140 characters | Checked by tests. |
| Recursion depth | ≤ 200 | Tree height ≤ n. The Wasm stack is set to 1 MiB, and an end-to-end test traverses a degenerate 200-node BST. |
| Wasm memory | grows on demand, capped at 128 MiB | Far above the bounds in this table. |
| TypeScript retention | latest response per structure | Older frames are dropped when a new response arrives. |

---

## 9. Versioning rules

- Requests must carry `schemaVersion: 1`; responses always carry 1.
- **Compatible additions keep version 1:** a new optional response field, a new
  operation, a new error code, a new event type or a new step ID.
- **Breaking changes need version 2:** renaming or removing a field, changing a field's
  type or meaning, or renaming or removing a step ID or event type. Update this document
  and the TypeScript types in the same change.
- Export files carry their own `formatVersion` (1). It changes only when the snapshot
  schema of 5.4 changes.

---

## 10. Node ownership

1. **The `Engine` owns the structures.** It holds one instance of each of the seven
   structures and one `History` per structure as plain members, for as long as the
   JavaScript `Engine` handle lives (the page lifetime).
2. **Each structure owns its nodes through a `NodePool`**: a
   `std::vector<std::unique_ptr<Node>>` in ascending ID order. `pool.create(value)`
   allocates, assigns the next ID and stores the node (the textbook `new`).
   `pool.destroy(node)` frees it (the textbook `delete`) and is called only after the
   node has been unlinked. No node owns another node.
3. **Every link is a non-owning raw pointer**: `next`, `left`, `right`, `parent` and the
   entry pointers `top`, `front`, `rear`, `head`, `root`. Each declaration carries the
   comment `// non-owning: the pool owns every node`. So do local cursors (`curr`,
   `prev`, `parent`, `x`, `y`, …) and the pointers held in a traversal's temporary array.
4. **The red-black NIL sentinel** is a member object of the `RedBlackTree` (not in the
   pool): one per tree, black, with no ID. Pointers to it are non-owning like every other
   link.
5. **Structures can be neither copied nor moved**, so the addresses of nodes and of the
   sentinel never change while links point at them. Undo, redo and import go through
   `restore(snapshot)`: it validates first, then rebuilds the pool and links in place.
6. **Destruction is automatic and flat.** Destroying a structure destroys its pool, which
   frees every node. Nodes do not own each other, so there is no deep recursive
   destructor chain.
7. **Snapshots, frames, history entries, requests and responses are plain values** (IDs
   and integers, no pointers). Copying them can never create a dangling pointer.
8. **Only IDs cross into JavaScript.** JavaScript never holds a node pointer. It owns
   one Embind `Engine` handle and calls `.delete()` on it only when the page module is
   disposed (Vite hot reload).
9. **ASan + UBSan** run the full native suite (milestone 2 onward) to catch any use of a
   freed node.

---

## 11. Decisions

Decisions marked **(asked)** were answered by the project owner on 2026-10-22. The rest
are defaults chosen while writing these documents; change any of them by editing this
list and the affected sections together.

| # | Decision | Why |
| --- | --- | --- |
| D1 | **(asked)** Stack and queue are linked nodes (stack: `top`; queue: `front` + `rear`), not arrays. | Same node/edge/ID model as every other structure. |
| D2 | **(asked)** Two-child delete in BST, AVL and red-black uses the **in-order successor**. | Course convention (CLRS). |
| D3 | The successor **node is relinked**, never value-copied. | Keeps every value attached to its ID (rule 3), so animations stay truthful. |
| D4 | **(asked)** AVL: height(NIL) = 0, leaf = 1, balance = height(left) − height(right). | Course convention; no negative heights on screen. |
| D5 | **(asked)** Red-black follows CLRS with one shared black NIL sentinel; the sentinel is never drawn. | Course convention. |
| D6 | **(asked)** Vendored third-party code: nlohmann/json (protocol layer and native tests only) and doctest (tests only). Nothing else without asking. | A tested JSON parser and test framework; we write only the code that matters for the course. |
| D7 | Ownership: one `NodePool` per structure owns all nodes; every link is a non-owning raw pointer. | Mid-operation snapshots stay complete; rotations and transplants read like the textbook; RAII frees everything. |
| D8 | Node IDs: per structure, from 1, monotonic, never reused (not lowered by `clear`, `undo`, `import`). | IDs stay unambiguous across history and animation. |
| D9 | One Embind class `Engine` with one method `handle(string) → string`, carrying JSON. | One contract for browser and native tests, and golden files; no per-call Embind object lifetimes. |
| D10 | AVL tree is recursive without parent pointers; BST and red-black are iterative CLRS with parent pointers; the binary tree stores parent pointers. | Each matches how that structure is usually taught. |
| D11 | Traversals are four operations (`preorder`, `inorder`, `postorder`, `levelorder`), shared as read-only templates across the four trees; level order uses a plain array as its queue (never `std::queue`). | Simple step IDs; one place to explain traversals. |
| D12 | BST-family `delete` and `search` take a value. Clicking a node only fills the value box. | The textbook algorithms are keyed by value; selection itself stays by ID. |
| D13 | `clear` exists on every structure (the brief lists it for stack and queue). | Needed to start over on any tab; frees nodes one by one with frames. |
| D14 | A search that finds nothing succeeds with `found: false`; a delete of an absent value is `VALUE_NOT_FOUND`. | A miss is a normal search answer; a delete that cannot happen is a rejection. |
| D15 | Protocol errors return no frames. Operation errors return `begin` … `reject` with unchanged snapshots. | Frames exist only when an algorithm actually ran. |
| D16 | Snapshots list all owned nodes in ascending ID order with every stored field (plus AVL `balance`); TypeScript lays out from links; unreachable nodes keep their previous position. | Complete mid-operation frames; one simple layout rule for every structure. |
| D17 | A frame shows the state immediately **after** its step. | One unambiguous reading for pointers and snapshot. |
| D18 | Edge ID = `"<fromId>-><toId>"`; `parent` links are not edges. | Stable, derivable on both sides, no extra state. |
| D19 | Limits: 512 frames/operation, 50 + 50 history entries, 1 MiB requests, 140-character explanations, 1 MiB Wasm stack, 128 MiB Wasm memory cap. | Covers proven worst cases with margin (section 8). |
| D20 | Exceptions are enabled (native default; Wasm `-fwasm-exceptions`), but algorithms never throw for expected errors. The protocol and engine layers catch stray exceptions and roll back. | The module never aborts; errors stay typed values. |
| D21 | Input boxes accept only an optional minus sign and 1–10 digits; C++ checks the range. | Exact in JavaScript numbers; all rules stay in C++. |
| D22 | Export and import exchange text; the document is the snapshot plus a format header; import validates everything and keeps IDs; import is undoable. | TypeScript never interprets structure data; a round trip is exact. |
| D23 | Undo/redo keep final snapshots only (no frames); `undo`/`redo` show a single `restore` frame. History recording starts in milestone 2 so `history` counts are correct from the first golden file; the `undo`/`redo` operations arrive in milestone 7. | Bounded memory; no golden-file churn. |
| D24 | Step IDs are `<structure>.<operation>.<step>`, mirror cases share an ID, shared operations use `common`. | Stable and readable. |
| D25 | Explanations are English sentences generated in C++ and name values, not IDs. | Students think in values; the text is next to the code that knows the facts. |
| D26 | `comparisons` counts one per three-way comparison with a node's value. | Matches how textbooks count key comparisons. |
| D27 | Wire JSON is compact; golden files and exports are 2-space indented; comparison ignores whitespace but not key order. | Small messages, readable files, deterministic tests. |
| D28 | Deployment: GitHub Pages through GitHub Actions, Vite `base: "./"`. | Free static hosting; a relative base works under any sub-path. Changing hosts touches only milestone 9. |
| D29 | Pseudocode listings live in the C++ catalog and reach the UI through the `catalog` operation. | Step IDs and pseudocode text cannot drift apart. |
