# The seven structures added in milestone 10

Deque, doubly linked list, circular list, min-heap, max-heap, splay tree and hash table.
This document is their design, in the same role as [avl.md](avl.md) and
[red-black.md](red-black.md): what each one stores, its operations, errors, pseudocode and
step IDs. The wire schema they share with the first seven structures is in
[operation-contract.md](operation-contract.md); only the additions are described here.

The pseudocode below is the catalog text (`engine/src/Catalog.cpp`), and each listing is
also a comment above its function in the structure's `.cpp` file.

Every rule of CLAUDE.md holds for these structures too: the algorithms are C++ (no
`std::deque`, `std::list`, `std::priority_queue`, `std::push_heap`, `std::unordered_set`),
nodes have stable IDs, a `NodePool` owns every node, frames are recorded inside the
running algorithm, a rejected operation changes nothing, and each structure has an
independent invariant checker (`engine/tests/support/Checkers.cpp`) that runs after
every operation in the tests.

---

## 1. What was added to the shared schema

| Addition | Where | Meaning |
| --- | --- | --- |
| Wire names | request `structure` | `deque`, `doublyList`, `circularList`, `minHeap`, `maxHeap`, `splay`, `hashTable` |
| Node field `prev` | deque, doubly linked list | The previous node's ID or `null`. Written before `next`. |
| Node field `index` | heaps | The array slot the node sits in, from 0. Written before `left`. |
| Entry field `tail` | doubly linked list, circular list | A node ID or `null`. |
| Entry field `buckets` | hash table | A list of exactly 7 entries: the first node of each bucket's chain, or `null`. |
| Frame field `bucket` | hash table frames only | The bucket the step works in (0 to 6), or `null` before the hash step. No other structure's frames have this key. |
| Events `swap`, `hash` | frames | `swap`: two heap nodes exchanged array slots. `hash`: a value's bucket was computed. |
| Edge IDs for `prev` | highlights | A `prev` link is an edge like any other: `"<fromId>-><toId>"`. Node 2's prev link to node 1 is `"2->1"`; node 1's next link to node 2 is `"1->2"`. |

No counter was added: golden files fix the counter keys. A heap swap writes two array
slots and counts 2 `linksChanged`; a splay rotation counts 1 `rotations`.

Snapshot key order (5.4 of the contract):

| `structure` | Entry fields | Node fields (in this order) |
| --- | --- | --- |
| `deque` | `front`, `rear` | `id`, `value`, `prev`, `next` |
| `doublyList` | `head`, `tail` | `id`, `value`, `prev`, `next` |
| `circularList` | `tail` | `id`, `value`, `next` |
| `minHeap`, `maxHeap` | `root` | `id`, `value`, `index`, `left`, `right`, `parent` |
| `splay` | `root` | `id`, `value`, `left`, `right`, `parent` |
| `hashTable` | `buckets` | `id`, `value`, `next` |

Duplicates: allowed in the deque, both lists and both heaps; rejected
(`DUPLICATE_VALUE`) in the splay tree and the hash table, which store sets.

---

## 2. Deque (doubly linked nodes, `front` and `rear`)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `pushFront`, `pushBack` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED | `{nodeId}` | O(1) |
| `popFront`, `popBack` | — | yes | EMPTY_STRUCTURE | `{nodeId, value}` | O(1) |
| `peekFront`, `peekBack` | — | no | EMPTY_STRUCTURE | `{nodeId, value}` | O(1) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

`popBack` is O(1) because the rear node stores `prev`; a singly linked queue would have to
walk to the node before the rear. The two ends are mirror images, written once in C++
(`Deque::push(End, …)`) with the end as a parameter; each end has its own step IDs.

```
pushFront(value):                                  deque.pushFront.enter
  if size = 200: reject CAPACITY_EXCEEDED          deque.pushFront.full
  node <- new Node(value)                          deque.pushFront.create
  if front = NIL: rear <- node                     deque.pushFront.linkEmpty
  else: node.next <- front; front.prev <- node     deque.pushFront.link     (one frame, two links)
  front <- node                                    deque.pushFront.moveEnd
  return node                                      deque.pushFront.done

popFront():                                        deque.popFront.enter
  if front = NIL: reject EMPTY_STRUCTURE           deque.popFront.empty
  node <- front                                    deque.popFront.take
  front <- node.next                               deque.popFront.moveEnd
  if front = NIL: rear <- NIL                      deque.popFront.clearOther
  else: front.prev <- NIL                          deque.popFront.unlink
  delete node                                      deque.popFront.remove
  return value                                     deque.popFront.done
```

`pushBack`, `popBack`, `peekFront`, `peekBack` and `clear` follow the same pattern
(`deque.pushBack.*`, `deque.popBack.*`, `deque.peekFront.read`, `deque.clear.remove`).

Invariant: the chain from `front` reaches every node once and ends at NIL; `rear` is its
last node; every `prev` is the node whose `next` points at it (NIL for the front).

---

## 3. Doubly linked list (`head` and `tail`)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insertAt` | `index`, `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, INDEX_OUT_OF_RANGE | `{nodeId}` | O(min(i, n − i)) |
| `deleteNode` | `nodeId` | yes | NODE_NOT_FOUND | `{nodeId, value}` | **O(1)** |
| `search` | `value` | no | — | `{found, nodeId, index}` | O(n) |
| `reverse` | — | yes | — | `{}` | O(n) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

Two things differ from the singly linked list, and both show in the frames:

- `deleteNode` does not walk. The selected node's own `prev` and `next` lead to its
  neighbours, so it is unlinked in at most two link writes
  (`moveHead`/`bypassNext`, then `moveTail`/`bypassPrev`).
- A middle `insertAt` walks from whichever end is nearer: from `head` with `next`
  (`startHead`, `walkNext`) when index ≤ n/2, otherwise from `tail` with `prev`
  (`startTail`, `walkPrev`).

`reverse` swaps `prev` and `next` in every node (`doublyList.reverse.flip`, one frame per
node) and then swaps `head` and `tail` (`doublyList.reverse.swapEnds`).

Full listings: `engine/src/DoublyList.cpp`. Invariant: as the deque's, with `head` and
`tail`.

---

## 4. Circular list (one pointer, `tail`)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insertFront`, `insertBack` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED | `{nodeId}` | O(1) |
| `deleteNode` | `nodeId` | yes | NODE_NOT_FOUND | `{nodeId, value}` | O(n) |
| `search` | `value` | no | — | `{found, nodeId, index}` | O(n) |
| `rotate` | — | yes | — | `{}` | O(1) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

The last node's `next` is the first node, so a non-empty list has no NIL link. Storing
`tail` rather than `head` makes both ends O(1): the first node is `tail.next`.

```
insertFront(value):                                circularList.insertFront.enter
  if size = 200: reject CAPACITY_EXCEEDED          circularList.insertFront.full
  node <- new Node(value)                          circularList.insertFront.create
  if tail = NIL:
    node.next <- node                              circularList.insertFront.linkSelf
    tail <- node                                   circularList.insertFront.setTail
  else:
    node.next <- tail.next                         circularList.insertFront.linkNext
    tail.next <- node                              circularList.insertFront.linkTail
  return node                                      circularList.insertFront.done
```

`insertBack` is the same plus `tail <- node` (`circularList.insertBack.moveTail`).
`rotate` is `tail <- tail.next` (`circularList.rotate.moveTail`): the first node becomes
the last and no link between nodes changes. On an empty list it succeeds and does
nothing. `search` starts at `tail.next` and stops after one full circle, because there
is no NIL to stop at. `deleteNode` walks from `tail` to the node's predecessor
(`start`, `walk`, `bypass`, and `moveTail` when the tail itself is deleted; `onlyNode`
when the list has one node).

Indexes count from the first node (`tail.next` is index 0). Invariant: `tail` is NIL
exactly when the list is empty; n steps along `next` from `tail` pass n different nodes
and end on `tail`.

---

## 5. Min-heap and max-heap (a binary heap in an array)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insert` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED | `{nodeId}` | O(log n) |
| `extract` | — | yes | EMPTY_STRUCTURE | `{nodeId, value}` of the root | O(log n) |
| `peek` | — | no | EMPTY_STRUCTURE | `{nodeId, value}` of the root | O(1) |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

One class, `BinaryHeap`, constructed as a min-heap or a max-heap; the engine holds one
object of each, and they are separate structures with separate state and history.

**Storage.** The heap is an array `A` of node pointers (a plain array; every heap rule is
written by hand). For slot i: parent (i − 1) / 2, left child 2i + 1, right child 2i + 2.
The tree is always complete, so it never needs links: a snapshot *derives* `left`,
`right` and `parent` from the slots, and reports each node's slot as `index`, so the page
can draw the tree and the array from one snapshot. A swap exchanges two slots; the nodes
keep their IDs and values, so on screen the two nodes change places (rule 3).

```
insert(value):                                     minHeap.insert.enter
  if size = 200: reject CAPACITY_EXCEEDED          minHeap.insert.full
  i <- size; A[i] <- new Node(value)               minHeap.insert.append    (create)
  while i > 0:                    (sift up)
    p <- (i - 1) / 2
    if not A[i] < A[p]: stop                       minHeap.insert.compare   (one frame per comparison)
    swap A[i] and A[p]; i <- p                     minHeap.insert.swap      (swap)
  return the node                                  minHeap.insert.done

extract():                                         minHeap.extract.enter
  if size = 0: reject EMPTY_STRUCTURE              minHeap.extract.empty
  top <- A[0]                                      minHeap.extract.take     (read)
  if size > 1: swap A[0] and A[size - 1]           minHeap.extract.moveLast (swap)
  remove the last slot; delete top                 minHeap.extract.remove
  i <- 0
  while i has a child:            (sift down)
    c <- the child of i with the smaller value
    if not A[c] < A[i]: stop                       minHeap.extract.compare
    swap A[i] and A[c]; i <- c                     minHeap.extract.swap
  return top.value                                 minHeap.extract.done
```

The max-heap is the same with `>` and "larger"; its step IDs start with `maxHeap`.
`comparisons` counts one for choosing between two children and one for comparing the
chosen child with its parent. `clear` removes the last slot repeatedly
(`minHeap.clear.remove`), which never breaks the heap order.

Invariant: the slots are 0 … n − 1, each used once; `root` is slot 0; every link is the
one the slot arithmetic gives; no child is smaller (min) or larger (max) than its parent.

---

## 6. Splay tree

Same operations, arguments, errors and data as the BST (contract 2.5). Nodes store only
`left`, `right` and `parent`.

| Operation | Algorithm |
| --- | --- |
| `insert`, `delete`, `search` | amortised O(log n); a single operation can cost O(n) |
| traversals, `clear` | O(n) |

After an access, the node that was reached is rotated to the root by splay steps:

```
splay(x):
  while x.parent != NIL:
    p <- x.parent; g <- p.parent
    if g = NIL: rotate at p                              <op>.zig
    else if x, p are children on the same side:
      rotate at g, then at p                             <op>.zigZig   (two rotate frames)
    else: rotate at p, then at g                         <op>.zigZag   (two rotate frames)
```

`<op>` is `splay.insert`, `splay.delete` or `splay.search`, so each operation's listing
shows its own splay lines. Rotations are CLRS LEFT-ROTATE and RIGHT-ROTATE; each is one
frame showing a consistent tree, and its explanation names the step ("Zig-zig, step 1:
rotate right at 30, …").

- **insert**: the BST's insert, then `splay(node)`. The new value ends at the root.
- **search**: the BST's descent. A hit splays the node found; a miss splays the last node
  on the path (nothing, on an empty tree). **A search therefore changes the tree**: it is
  the one read-style operation that gets an undo entry (`isMutating(structure,
  operation)` in `Request.h`), and the catalog reports `"mutates": true` for it.
- **delete**: the BST's delete (CLRS, the in-order successor node relinked, never
  value-copied), then `splay` of the deleted node's old parent, if it had one. This
  bottom-up variant was chosen over "splay the node, remove the root, join the subtrees"
  because the join can need about 3n frames on a degenerate tree, more than the contract's
  2n + 16 frames per operation; this one stays within it (tested on a 200-node spine).
- **A rejected operation does not splay.** A duplicate insert and a delete of an absent
  value change nothing, shape included (rule 8).

Invariant: the BST's (shape, parent fields, strictly increasing in-order values). There is
no balance rule to check: any BST shape is a legal splay tree.

---

## 7. Hash table (separate chaining, 7 buckets)

| Operation | Args | Mutates | Errors | Success `data` | Algorithm |
| --- | --- | --- | --- | --- | --- |
| `insert` | `value` | yes | CAPACITY_EXCEEDED, ID_EXHAUSTED, DUPLICATE_VALUE | `{nodeId}` | O(1) average, O(n) worst |
| `delete` | `value` | yes | VALUE_NOT_FOUND | `{nodeId, value}` | O(1) average, O(n) worst |
| `search` | `value` | no | — | `{found, nodeId, bucket}`; `nodeId` null when not found | O(1) average, O(n) worst |
| `clear` | — | yes | — | `{removedCount}` | O(n) |

- **Hash function**: bucket = value mod 7, made non-negative (`kHashBuckets` in
  `Limits.h`). C++'s `%` keeps the sign of the value, so a negative remainder has 7 added:
  −1 goes to bucket 6. −2,147,483,648 goes to bucket 5.
- **Collisions**: each bucket is a singly linked chain. A new value is linked at the head
  of its chain, after the whole chain was checked for a duplicate.
- **No rehashing**: the bucket count is fixed, so with 200 values the chains average 29
  nodes. That is deliberate: the table shows what a load factor does. The worst case, all
  200 values in one bucket, is tested.

```
insert(value):                                     hashTable.insert.enter
  if size = 200: reject CAPACITY_EXCEEDED          hashTable.insert.full
  b <- value mod 7                                 hashTable.insert.hash       (hash)
  curr <- buckets[b]
  while curr != NIL:
    if curr.value = value: reject DUPLICATE_VALUE  hashTable.insert.duplicate
    curr <- curr.next                              hashTable.insert.next       (compare)
  node <- new Node(value)                          hashTable.insert.create
  node.next <- buckets[b]                          hashTable.insert.linkNext
  buckets[b] <- node                               hashTable.insert.linkBucket
  return node                                      hashTable.insert.done
```

`delete` hashes, walks with `prev` and `curr` (`next`), rejects at the chain's end
(`notFound`), else `found`, then `unlinkHead` or `bypass`, `remove`, `done`. `search`
hashes, walks (`next`), then `found` or `notFound`, `done`. `clear` empties the buckets
in order (`hashTable.clear.remove`, one frame per node).

Every frame after the hash step carries `"bucket": b`, which the page uses to light that
bucket; the page never computes a hash itself.

Invariant: exactly 7 buckets; every chain ends at NIL; every node is in exactly one
chain, the one its value hashes to; no value is stored twice. The test checker computes
the hash with its own formula, not the engine's.

---

## 8. Import validation (contract 5.7) for these structures

Rules 1 to 4 and 10 apply unchanged, with the fields above. In place of rules 5 to 9:

| Structure | An import is rejected unless |
| --- | --- |
| deque, doubly linked list | the chain rule (5) holds from `front`/`head`; `rear`/`tail` is the chain's last node; every `prev` is the node whose `next` points at it |
| circular list | `tail` is `null` exactly when there are no nodes; no `next` is `null`; following `next` from `tail` visits every node once and returns to `tail` |
| heaps | every `index` is in 0 … n − 1 and used once; `root` is the node in slot 0; `left`, `right` and `parent` are exactly what the slots imply; no child beats its parent |
| splay tree | the tree rules (6) and the order rule (7) hold |
| hash table | `buckets` has exactly 7 entries; every chain ends at `null`; every node is in exactly one chain; each value is in the bucket it hashes to; no value appears twice |

---

## 9. Drawing (TypeScript: layout and SVG only)

| Structure | Layout | Drawing |
| --- | --- | --- |
| deque, doubly linked list | a row, like the queue and the list (`layout/linear.ts`) | `ChainSvg` with `doubly`: `next` links on the upper line, `prev` links (dotted) on the lower line |
| circular list | a ring, clockwise from the first node at the top; a new node waits in the middle | `RingSvg`: every `next` link an arrow to the neighbour, a loop for a list of one |
| heaps | the tree layout | `TreeSvg` with `heapArray`: each node labelled with its slot, and the array drawn under the tree from the same snapshot |
| splay tree | the tree layout | `TreeSvg`, on the shared search-tree page |
| hash table | the bucket array down the left, each chain to the right of its bucket | `HashSvg`: the bucket named by `frame.bucket` is lit |

As everywhere, layout reads links only to place nodes; it decides nothing.
