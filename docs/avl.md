# AVL tree

The AVL tree is the BST of milestone 3 (CLRS, nodes with left, right and parent) plus a
height in every node and a fix-up walk that restores balance after every change.
Insert and delete are exactly the BST's; only `fixUp`, `rebalance` and the rotations are new.

## Conventions (used everywhere: engine, frames, UI, tests)

- **height(NIL) = 0**, height(leaf) = 1, height(n) = 1 + max(height(n.left), height(n.right)).
  Every node stores its height.
- **balance(n) = height(n.left) − height(n.right).** +1 means "one level taller on the
  left", −1 "taller on the right". A tree is AVL when every balance is −1, 0 or +1.
  +2 is left-heavy and −2 right-heavy; nothing else can appear, because each insert or
  delete changes a height by at most 1.
- Duplicates are rejected (DUPLICATE_VALUE). Deleting an absent value is VALUE_NOT_FOUND.
- **Node IDs survive rotations.** A rotation relinks nodes; values never move between
  nodes. Delete moves the in-order successor *node* (as in the BST).
- In the UI, `bf` is this balance and `h` this height.

## Helpers (no frames)

```
height(n):   if n = NIL: return 0 else return n.height
balance(n):  return height(n.left) - height(n.right)
updateHeight(n):  n.height <- 1 + max(height(n.left), height(n.right))
```

## Search                                    (frame on every line marked *)

```
search(value):                                   avl.search.enter        *
  curr <- root                                   avl.search.start
  while curr != NIL:                             avl.search.loop
    if value = curr.value: return found          avl.search.found        *
    if value < curr.value: curr <- curr.left     avl.search.goLeft       *
    else: curr <- curr.right                     avl.search.goRight      *
  return not found                               avl.search.notFound     *
                                                 avl.search.done         *
```
Every search frame highlights the whole path taken so far.

## Insert

```
insert(value):                                         avl.insert.enter        *
  if size = 200: reject CAPACITY_EXCEEDED              avl.insert.full         * (reject)
  parent <- NIL; curr <- root                          avl.insert.start
  while curr != NIL:                                   avl.insert.loop
    if value = curr.value: reject DUPLICATE_VALUE      avl.insert.duplicate    * (reject)
    parent <- curr                                     avl.insert.setParent
    if value < curr.value: curr <- curr.left           avl.insert.goLeft       *
    else: curr <- curr.right                           avl.insert.goRight      *
  node <- new Node(value); node.height <- 1;
    node.parent <- parent                              avl.insert.create       *
  if parent = NIL: root <- node                        avl.insert.linkRoot     *
  else if value < parent.value: parent.left <- node    avl.insert.linkLeft     *
  else: parent.right <- node                           avl.insert.linkRight    *
  fixUp(parent)                                        avl.insert.fixUp
  return node                                          avl.insert.done         *
```

## Delete

```
delete(value):                                         avl.delete.enter              *
  z <- root                                            avl.delete.start
  while z != NIL and value != z.value:                 avl.delete.loop
    if value < z.value: z <- z.left                    avl.delete.goLeft             *
    else: z <- z.right                                 avl.delete.goRight            *
  if z = NIL: reject VALUE_NOT_FOUND                   avl.delete.notFound           * (reject)
  name the case:                                       avl.delete.caseLeaf           * (case 1)
                                                       avl.delete.caseOneChild       * (case 2)
                                                       avl.delete.caseTwoChildren    * (case 3)
  if z.left = NIL or z.right = NIL:
    start <- z.parent                                  avl.delete.startBelow
    transplant(z, the child of z or NIL)               avl.delete.replace            *
  else:
    y <- z.right                                       avl.delete.successorStart     *
    while y.left != NIL: y <- y.left                   avl.delete.successorLeft      *
                                                       avl.delete.successorFound     *
    if y.parent != z:
      start <- y.parent                                avl.delete.startAtOldParent
      transplant(y, y.right)                           avl.delete.detachSuccessor    *
      y.right <- z.right; y.right.parent <- y          avl.delete.successorTakesRight *
    else:
      start <- y                                       avl.delete.startAtSuccessor
    transplant(z, y)                                   avl.delete.replaceWithSuccessor *
    y.left <- z.left; y.left.parent <- y               avl.delete.successorTakesLeft *
  free z                                               avl.delete.remove             *
  fixUp(start)                                         avl.delete.fixUp
  return                                               avl.delete.done               *
```
`start` is the lowest node whose subtree height may have changed. TRANSPLANT is the
BST's (CLRS).

## fixUp: walk to the root, updating heights and rebalancing

```
fixUp(n):                                              avl.fixUp.start      * (if n != NIL)
  while n != NIL:                                      avl.fixUp.loop
    updateHeight(n)                                    avl.fixUp.updateHeight * (every node)
    if balance(n) = +2 or balance(n) = -2:             avl.fixUp.unbalanced   * (imbalance detected)
      n <- rebalance(n)                                avl.fixUp.rebalance
    n <- n.parent                                      avl.fixUp.up
  every balance on the path is -1, 0 or +1             avl.fixUp.done         * (rebalancing complete)
```
Insert walks all the way to the root too (no early stop), so every height on the path
gets its own frame. Delete may rebalance at several ancestors on the way up.

## rebalance: choose the case

```
rebalance(n):          returns the new root of n's subtree
  if balance(n) = +2:                                  (left-heavy)
    if balance(n.left) >= 0:                           avl.rebalance.LL   * LL: rotateRight(n)
      return rotateRight(n)
    else:                                              avl.rebalance.LR   * LR: rotateLeft(n.left), then rotateRight(n)
      rotateLeft(n.left)
      return rotateRight(n)
  else:                                                (balance -2, right-heavy)
    if balance(n.right) <= 0:                          avl.rebalance.RR   * RR: rotateLeft(n)
      return rotateLeft(n)
    else:                                              avl.rebalance.RL   * RL: rotateRight(n.right), then rotateLeft(n)
      rotateRight(n.right)
      return rotateLeft(n)
```
The case frame names the case and says why: "Left child 20 has balance +1: LL case, one
right rotation at 30". The `>= 0` / `<= 0` matters only for delete: a child with
balance 0 is fixed by one rotation.

## Rotations

```
rotateLeft(x):         x's right child y moves up; returns y
  y <- x.right                                         avl.rotateLeft.takeChild
  x.right <- y.left                                    avl.rotateLeft.moveInner
  if y.left != NIL: y.left.parent <- x                 avl.rotateLeft.moveInnerParent
  y.parent <- x.parent                                 avl.rotateLeft.liftParent
  if x.parent = NIL: root <- y                         avl.rotateLeft.linkAbove
  else if x = x.parent.left: x.parent.left <- y
  else: x.parent.right <- y
  y.left <- x; x.parent <- y                           avl.rotateLeft.linkDown
  updateHeight(x); updateHeight(y)                     avl.rotateLeft.heights   * (one frame: the finished rotation)
  return y                                             avl.rotateLeft.return

rotateRight(y):        mirror image: y's left child x moves up; returns x
  x <- y.left                                          avl.rotateRight.takeChild
  y.left <- x.right                                    avl.rotateRight.moveInner
  if x.right != NIL: x.right.parent <- y               avl.rotateRight.moveInnerParent
  x.parent <- y.parent                                 avl.rotateRight.liftParent
  if y.parent = NIL: root <- x                         avl.rotateRight.linkAbove
  else if y = y.parent.left: y.parent.left <- x
  else: y.parent.right <- x
  x.right <- y; y.parent <- x                          avl.rotateRight.linkDown
  updateHeight(y); updateHeight(x)                     avl.rotateRight.heights  * (one frame)
  return x                                             avl.rotateRight.return
```
A rotation is **one frame** recorded after all its links and both heights are updated,
so the picture before and after is a consistent tree and the UI animates nodes and edges
together. Heights are updated child first (x before y in rotateLeft) because the new
parent's height depends on the child's.

## Frames and events

| Step | Event | Example explanation |
| --- | --- | --- |
| descent comparison | compare | "10 < 30, so move to the left child, 20." |
| allocation / link / removal | create / link / remove | as in the BST |
| fixUp.start | move | "Walk back up from 20 to the root, updating heights and checking balances." |
| fixUp.updateHeight | **update** | "Height of 20 becomes 2 (left 1, right 0), balance +1." |
| fixUp.unbalanced | **imbalance** (new event) | "30 has balance +2: its left side is two levels taller, so it must be rebalanced." |
| rebalance.LL/LR/RR/RL | imbalance | "Left child 20 has balance +1: LL case, one right rotation at 30." / "Left child 10 has balance -1: LR case, left rotation at 10, then right rotation at 30." |
| rotateLeft/Right.heights | **rotate** | "LL case: rotate right at 30, so 20 moves up." / "LR case, step 1: rotate left at 10, so 20 moves up." |
| fixUp.done | **balanced** (new event) | "Rebalancing complete: every node from 20 up to the root has balance -1, 0 or +1." |

**Temporary imbalance is always labelled.** Heights are updated bottom-up, so between
the update that makes a node ±2 and the rotation that fixes it, frames show that node out
of range. Every frame whose snapshot contains a node with |balance| ≥ 2 ends its
explanation with "; temporarily unbalanced: 30 (+2)." (If listing the values would push
the sentence past the 140-character limit, it ends with "; temporarily unbalanced." instead.)
The test checker enforces this, and the UI marks such nodes in red. `clear` frames are
labelled the same way: cutting links while freeing the tree leaves the remaining nodes'
balances meaningless. No completed operation leaves any node out of range.

The explanations of the examples above are the exact sentences the engine writes
(`engine/src/AvlTree.cpp`); `engine/tests/test_avl.cpp` checks the case names, the
rotation steps and the labelling.

## Snapshot fields

`id, value, left, right, parent, height, balance` (`balance` is derived from the stored
child heights, so frames show exactly what the algorithm has computed so far).

## Cost (rule 11)

Height ≤ 1.44·log2(n + 2), so search, insert and delete are O(log n). `fixUp` visits
O(log n) ancestors. Insert needs at most one rebalance (one or two rotations); delete may
rebalance at O(log n) ancestors. Tracing costs O(frames × n) on top, as for every structure.

## Invariant checker (tests)

Independent of `AvlTree`: from a snapshot alone it checks the tree shape and parent
fields (as for the BST), strictly increasing in-order values, recomputes every height from
scratch and compares it with the stored height, checks the stored `balance`, and checks
every balance is in [−1, 1]. It runs after every operation in every test. A frame-level
check requires the "temporarily unbalanced" label whenever an intermediate frame has a
node out of range.
