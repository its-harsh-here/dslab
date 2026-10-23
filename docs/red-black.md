# Red-black tree

A red-black tree is a BST whose nodes carry a color, kept almost balanced by a few rules.
DSLab follows CLRS (Cormen et al., *Introduction to Algorithms*, chapter 13) exactly: one
shared black NIL sentinel, RB-INSERT with RB-INSERT-FIXUP, RB-DELETE with RB-TRANSPLANT
and RB-DELETE-FIXUP, and the same parent-pointer node shape, successor-relinking delete
and rotations as the BST and AVL trees in this project.

## The NIL sentinel, and who owns it

- Each tree has exactly **one** sentinel node, `nil_`, a member object of `RedBlackTree`.
  The tree owns it by value: it is created with the tree and destroyed with it. It is
  **not** in the node pool, has **no ID**, is never freed on its own and never appears in
  a snapshot's node list.
- It is **always black**. Every missing child, and the root's parent, point to it instead
  of being null. That lets the code read `w.left.color` or `x.parent` without special
  cases for leaves.
- CLRS deliberately **writes `nil.parent`**: RB-TRANSPLANT sets `v.parent` even when `v`
  is NIL, and RB-DELETE sets `x.parent` when `x` is NIL. That is how the delete fix-up
  finds the parent of a NIL `x`. The value means nothing outside a delete.
- Snapshots write every link to the sentinel as `null` and report its color as
  `"nilColor": "black"`. The UI can draw the NIL leaves (a toggle); they are drawn from
  the null links.

## The five properties

1. Every node is red or black.
2. The root is black.
3. Every leaf (NIL) is black.
4. If a node is red, both its children are black (no red node has a red child).
5. For each node, every path from it down to a NIL leaf has the same number of black
   nodes (the node's *black height*).

Together they keep the height ≤ 2·log2(n + 1), so search, insert and delete are O(log n).

## Conventions

- Duplicates are rejected (DUPLICATE_VALUE); deleting an absent value is VALUE_NOT_FOUND.
- A new node is **red**. Delete moves the in-order successor **node** (values never move),
  so every value keeps its node ID through deletes and rotations.
- CLRS writes each fix-up's second branch as "same as the first, with left and right
  exchanged". DSLab writes that **mirror** branch out in full in the code, and its step
  IDs end in `Mirror` (e.g. `redBlack.insertFixup.case1Mirror`), so tests can show that
  both sides of every case run.
- **Temporary violations are labelled.** Between the change that breaks a property and the
  fix-up step that restores it, every frame ends its explanation with
  "; temporary violation: …", naming the first problem found: `red root`,
  `red 10 under red 20`, or `black heights differ under 30`. (If that would exceed the
  140-character limit, it ends with "; temporary violation." instead.) The test checker
  enforces this. Completed operations never show a violation.

## Pseudocode and step IDs   (a * marks lines that record a frame)

### Search (the BST's; every frame highlights the whole path so far)

```
search(value):                                   redBlack.search.enter     *
  x <- root                                      redBlack.search.start
  while x != NIL:                                redBlack.search.loop
    if value = x.value: return found             redBlack.search.found     *
    if value < x.value: x <- x.left              redBlack.search.goLeft    *
    else: x <- x.right                           redBlack.search.goRight   *
  return not found                               redBlack.search.notFound  *
                                                 redBlack.search.done      *
```

### Rotations (CLRS LEFT-ROTATE; one frame for the finished rotation)

```
rotateLeft(x):         x's right child y moves up
  y <- x.right                                   redBlack.rotateLeft.takeChild
  x.right <- y.left                              redBlack.rotateLeft.moveInner
  if y.left != NIL: y.left.parent <- x           redBlack.rotateLeft.moveInnerParent
  y.parent <- x.parent                           redBlack.rotateLeft.liftParent
  if x.parent = NIL: root <- y                   redBlack.rotateLeft.linkAbove
  else if x = x.parent.left: x.parent.left <- y
  else: x.parent.right <- y
  y.left <- x; x.parent <- y                     redBlack.rotateLeft.linkDown   *

rotateRight(x):        mirror image: x's left child y moves up   (redBlack.rotateRight.*)
```
The frame's explanation names the fix-up case that asked for the rotation.

### Insert (CLRS RB-INSERT)

```
insert(value):                                         redBlack.insert.enter       *
  if size = 200: reject CAPACITY_EXCEEDED              redBlack.insert.full        * (reject)
  y <- NIL; x <- root                                  redBlack.insert.start
  while x != NIL:                                      redBlack.insert.loop
    if value = x.value: reject DUPLICATE_VALUE         redBlack.insert.duplicate   * (reject)
    y <- x                                             redBlack.insert.setParent
    if value < x.value: x <- x.left                    redBlack.insert.goLeft      *
    else: x <- x.right                                 redBlack.insert.goRight     *
  z <- new Node(value); z.color <- RED;
    z.left <- NIL; z.right <- NIL; z.parent <- y       redBlack.insert.create      *
  if y = NIL: root <- z                                redBlack.insert.linkRoot    *
  else if value < y.value: y.left <- z                 redBlack.insert.linkLeft    *
  else: y.right <- z                                   redBlack.insert.linkRight   *
  insertFixup(z)                                       redBlack.insert.fixup
  return z                                             redBlack.insert.done        *
```

### Insert fix-up (CLRS RB-INSERT-FIXUP)

The new red node z may sit under a red parent (property 4) or be a red root
(property 2). y is z's **uncle**.

```
insertFixup(z):
  while z.parent.color = RED:                          redBlack.insertFixup.loop
    if z.parent = z.parent.parent.left:
      y <- z.parent.parent.right                       redBlack.insertFixup.uncle
      if y.color = RED:                                redBlack.insertFixup.case1              * case 1: red uncle
        z.parent.color <- BLACK                        redBlack.insertFixup.case1Parent        * recolor
        y.color <- BLACK                               redBlack.insertFixup.case1Uncle         * recolor
        z.parent.parent.color <- RED                   redBlack.insertFixup.case1Grandparent   * recolor
        z <- z.parent.parent                           redBlack.insertFixup.case1Up            * move up two levels
      else:
        if z = z.parent.right:                         redBlack.insertFixup.case2              * case 2: black uncle, z an inner child
          z <- z.parent                                redBlack.insertFixup.case2Up
          rotateLeft(z)                                redBlack.insertFixup.case2Rotate        (rotation frame)
                                                       redBlack.insertFixup.case3              * case 3: black uncle, z an outer child
        z.parent.color <- BLACK                        redBlack.insertFixup.case3Parent        * recolor
        z.parent.parent.color <- RED                   redBlack.insertFixup.case3Grandparent   * recolor
        rotateRight(z.parent.parent)                   redBlack.insertFixup.case3Rotate        (rotation frame)
    else:
      (mirror: the same lines with left and right exchanged; every step ID + "Mirror")
  if root.color = RED: root.color <- BLACK             redBlack.insertFixup.blackRoot          * recolor
  all five properties hold                             redBlack.insertFixup.done               *
```
Case 1 can repeat up the tree (O(log n) times); case 2 always leads to case 3, and case 3
ends the loop. So an insert makes at most 2 rotations.

### Delete (CLRS RB-DELETE)

```
delete(value):                                         redBlack.delete.enter                 *
  z <- root
  while z != NIL and value != z.value:                 redBlack.delete.goLeft / goRight      *
    z <- (value < z.value) ? z.left : z.right
  if z = NIL: reject VALUE_NOT_FOUND                   redBlack.delete.notFound              * (reject)
                                                       redBlack.delete.found                 * (says: leaf, one child or two children)
  y <- z; yOriginalColor <- y.color                    redBlack.delete.rememberColor
  if z.left = NIL:
    x <- z.right; transplant(z, z.right)               redBlack.delete.replaceWithRight      *
  else if z.right = NIL:
    x <- z.left; transplant(z, z.left)                 redBlack.delete.replaceWithLeft       *
  else:
    y <- z.right                                       redBlack.delete.successorStart        *
    while y.left != NIL: y <- y.left                   redBlack.delete.successorLeft         *
    yOriginalColor <- y.color                          redBlack.delete.successorFound        *
    x <- y.right
    if y.parent = z: x.parent <- y                     redBlack.delete.xParent   (x may be NIL: sets nil.parent)
    else:
      transplant(y, y.right)                           redBlack.delete.detachSuccessor       *
      y.right <- z.right; y.right.parent <- y          redBlack.delete.successorTakesRight   *
    transplant(z, y)                                   redBlack.delete.replaceWithSuccessor  *
    y.left <- z.left; y.left.parent <- y               redBlack.delete.successorTakesLeft    *
    y.color <- z.color                                 redBlack.delete.successorTakesColor   * recolor
  free z                                               redBlack.delete.remove                *
  if yOriginalColor = BLACK: deleteFixup(x)            redBlack.delete.extraBlack            * (x gets the extra black;
                                                                                                if y was red: nothing to fix)
  return                                               redBlack.delete.done                  *

transplant(u, v):    (CLRS RB-TRANSPLANT: like the BST's, but always sets v.parent, even for NIL)
  if u.parent = NIL: root <- v
  else if u = u.parent.left: u.parent.left <- v
  else: u.parent.right <- v
  v.parent <- u.parent
```
y is the node that is really taken out of its position (z itself, or z's successor);
x is the node that moves into y's old position (possibly NIL).

### The extra black (why delete needs a fix-up)

If y was **red**, removing or moving it changes no black height: nothing to fix.

If y was **black**, every path that went through y now has one black node too few
(property 5 breaks). CLRS restores the count with **bookkeeping**: x is treated as holding
one *extra black*. With that extra black counted, every path has the right number again,
but x now counts as "doubly black" (if x is black) or "red-and-black" (if x is red), which
no real node can be.

**The extra black is not a third color.** x's stored color stays RED or BLACK in memory,
in every snapshot and in the drawing. The extra black belongs to the pointer x: it is
"where the missing black is owed". Frames show it with the pointer label `x` and say so in
the explanation ("x = 40 carries an extra black"). Because the stored colors really are one
black short on those paths, the frames are labelled "temporary violation: black heights
differ under …" until the fix-up pays the black back.

The fix-up moves the extra black up the tree or uses it up:
- if x is **red-and-black**, coloring x plain black pays the debt (the loop ends at once);
- if x reaches the **root**, the extra black is simply dropped (every path loses one black
  together);
- otherwise one of the four cases below applies.

### Delete fix-up (CLRS RB-DELETE-FIXUP)

w is x's **sibling**. The loop runs while x is doubly black and not the root.

```
deleteFixup(x):
  while x != root and x.color = BLACK:                         redBlack.deleteFixup.loop
    if x = x.parent.left:
      w <- x.parent.right                                      redBlack.deleteFixup.sibling
      if w.color = RED:                                        redBlack.deleteFixup.case1         * case 1: red sibling
        w.color <- BLACK                                       redBlack.deleteFixup.case1Sibling  * recolor
        x.parent.color <- RED                                  redBlack.deleteFixup.case1Parent   * recolor
        rotateLeft(x.parent)                                   redBlack.deleteFixup.case1Rotate   (rotation frame)
        w <- x.parent.right                                    redBlack.deleteFixup.case1NewSibling
      if w.left.color = BLACK and w.right.color = BLACK:       redBlack.deleteFixup.case2         * case 2: black sibling, two black children
        w.color <- RED                                         redBlack.deleteFixup.case2Sibling  * recolor
        x <- x.parent                                          redBlack.deleteFixup.case2Up       * the extra black moves up
      else:
        if w.right.color = BLACK:                              redBlack.deleteFixup.case3         * case 3: black sibling, near child red, far child black
          w.left.color <- BLACK                                redBlack.deleteFixup.case3Near     * recolor
          w.color <- RED                                       redBlack.deleteFixup.case3Sibling  * recolor
          rotateRight(w)                                       redBlack.deleteFixup.case3Rotate   (rotation frame)
          w <- x.parent.right                                  redBlack.deleteFixup.case3NewSibling
                                                               redBlack.deleteFixup.case4         * case 4: black sibling, far child red
        w.color <- x.parent.color                              redBlack.deleteFixup.case4Sibling  * recolor
        x.parent.color <- BLACK                                redBlack.deleteFixup.case4Parent   * recolor
        w.right.color <- BLACK                                 redBlack.deleteFixup.case4Far      * recolor
        rotateLeft(x.parent)                                   redBlack.deleteFixup.case4Rotate   (rotation frame)
        x <- root                                              redBlack.deleteFixup.case4Done
    else:
      (mirror: the same lines with left and right exchanged; every step ID + "Mirror")
  x.color <- BLACK                                             redBlack.deleteFixup.blacken       * recolor: the extra black is absorbed
  all five properties hold                                     redBlack.deleteFixup.done          *
```
- **Case 1** fixes nothing by itself: it makes the sibling black so that case 2, 3 or 4
  applies next.
- **Case 2** takes one black off both x and w (w turns red) and moves the extra black up
  to the parent. If the parent was red, the loop stops and `blacken` colors it black.
- **Case 3** turns into case 4 with one rotation.
- **Case 4** uses up the extra black with recolors and one rotation; the loop ends.
- So a delete makes at most 3 rotations; only case 2 can repeat (O(log n) times).

## Frames and events

| Step | Event | Example explanation |
| --- | --- | --- |
| descent comparison | compare | "25 < 30, so move to the left child, 20." |
| new node, link, removal | create / link / remove | "Create a new red node holding 10 whose parent is 20." |
| a fix-up case is recognised | **fixupCase** (new) | "Insert case 1: the uncle 40 is red, so recolor instead of rotating." |
| each color change | recolor | "Color the parent 20 black; temporary violation: black heights differ under 30." |
| each rotation | rotate | "Insert case 3: rotate right at 30, so 20 moves up." |
| z or the extra black moves up | move | "Delete case 2: the extra black moves up to 30." |
| x receives the extra black | move | "The removed node was black, so x = 40 carries an extra black (doubly black)." |
| y was red, so no fix-up | balanced | "The removed node 10 was red, so no path lost a black node: nothing to fix." |
| fix-up finished | balanced | "All five red-black properties hold again." |

## Cost (rule 11)

Height ≤ 2·log2(n + 1): search, insert and delete are O(log n). Insert: O(log n)
recolors, at most 2 rotations. Delete: O(log n) recolors, at most 3 rotations.
Tracing costs O(frames × n) on top, as for every structure.

## Invariant checker (tests)

Independent of `RedBlackTree`, from a snapshot alone: tree shape, valid parent/child links
and every node reachable (as for the BST); strictly increasing in-order values; every
color red or black; the sentinel black (`nilColor`); a black root when non-empty; no red
node with a red child; equal black height on every root-to-NIL path. It runs after every
operation in every test. For intermediate frames, it requires the "temporary violation"
label whenever any of those properties fails.

## Presets (UI)

| Preset | Set-up (not animated) | Animated | Cases it runs |
| --- | --- | --- | --- |
| Insert case 1 | insert 30, 20, 40 | insert 10 | 1 |
| Insert case 2 | insert 30, 10 | insert 20 | 2, then 3 |
| Insert case 3 | insert 30, 20 | insert 10 | 3 |
| Delete case 1 | insert 10, 20, 30, 40, 60, 70 | delete 10 | 1, then 2 |
| Delete case 2 | insert 10, 20, 30, 40, 70, 90, 60, 50 | delete 10 | 2 |
| Delete case 3 | insert 10, 20, 40, 30 | delete 10 | 3, then 4 |
| Delete case 4 | insert 10, 20, 30, 40 | delete 10 | 4 |

The mirror cases are covered by the native tests (e.g. insert 30, 20, 40 then 50 runs
insert case 1 mirror; insert 20, 30, 40, 10 then delete 30 runs delete case 4 mirror).
