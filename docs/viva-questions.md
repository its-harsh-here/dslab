# Viva questions

Thirty questions an examiner is likely to ask about DSLab, with short model answers.
Each answer refers to this project's code where it helps. Conventions are the course's:
AVL height(NIL) = 0, a leaf has height 1, balance = h(left) − h(right); red-black trees
follow CLRS with one black NIL sentinel; a two-child delete moves the in-order successor
node (relinked, never value-copied).

## Pointers, memory and linked nodes

**1. What is the difference between an owning and a non-owning pointer, and which does
DSLab use for links?**
An owning pointer is responsible for freeing what it points to (`std::unique_ptr`); a
non-owning pointer only refers to it. In DSLab each structure's `NodePool` owns every node
through a `std::vector<std::unique_ptr<Node>>`, and every link (`next`, `left`, `right`,
`parent`, `top`, `root`…) is a raw non-owning pointer, commented as such.

**2. Why not make `left` and `right` `unique_ptr`s, so the tree frees itself?**
Then every relink would be an ownership transfer (`std::move`), and a rotation done in the
wrong order could free a subtree. It also gives a recursive destructor that would recurse
200 deep on a degenerate tree. With a pool, a rotation is plain pointer assignments, just
like the textbook, and freeing is flat.

**3. What is RAII, and where does DSLab rely on it?**
Resource Acquisition Is Initialization: an object acquires a resource in its constructor
and releases it in its destructor, so cleanup is automatic. The pool's `unique_ptr`s free
the nodes when the pool is destroyed; the `Engine` owns the structures by value; the Wasm
`Engine` handle owns the C++ `Engine`.

**4. What is a dangling pointer, and how does `Stack::pop` avoid one?**
A pointer to memory that has been freed. `pop` first moves `top` to `node->next` (so no
link points at the node), copies the ID and value it must return, and only then calls
`pool_.destroy(node)`; the pointer is never used after that line.

**5. Why does every node have an integer ID as well as a pointer?**
Pointers are memory addresses: they change between runs and mean nothing to JavaScript.
The ID is stable, independent of the value, survives rotations and relinks, and is how the
UI selects a node, how frames name nodes, and how snapshots describe links.

**6. Why does a frame store a snapshot of IDs instead of pointers to the live nodes?**
A later operation may free or move those nodes, and the frame would then point at freed
memory. A snapshot is a plain value (IDs and integers), so a recorded frame stays correct
forever; `test_audit.cpp` checks this by freeing every node after an operation.

**7. What special case does `dequeue` have?**
When the last node is removed, `front` becomes NIL and `rear` must also be set to NIL,
otherwise `rear` would dangle.

**8. How do you reverse a singly linked list in place?**
Three pointers: `prev = NIL`, `curr = head`. Repeat: `next = curr.next`,
`curr.next = prev`, `prev = curr`, `curr = next`. At the end `head = prev`. O(n) time,
O(1) extra space.

## Traversals

**9. Give the order of the three depth-first traversals.**
Pre-order: node, left, right. In-order: left, node, right. Post-order: left, right, node.

**10. Why does an in-order traversal of a BST print the values in sorted order?**
Every value in a node's left subtree is smaller and every value in its right subtree is
larger, and in-order visits the whole left subtree, then the node, then the right subtree.

**11. When is post-order the right order, and where does DSLab use it?**
When children must be handled before their parent. `deleteSubtree` and `clear` free nodes
in post-order, so no node is freed while a child still points up to it.

**12. How is level-order traversal done, and what is its space cost?**
With a queue: enqueue the root; repeatedly dequeue a node, visit it, enqueue its non-NIL
children. O(n) time, O(n) space for the queue (up to a whole level). The page shows the
queue's contents in each frame (`pending`).

**13. What is the time and recursion depth of a recursive traversal?**
O(n) time, since each node is visited once; recursion depth equals the height, up to
n − 1 = 199 for a degenerate tree. That is why the Wasm stack is set to 1 MiB.

## Deletion

**14. What are the three cases of BST deletion?**
The node has no children (unlink it), one child (the child takes its place), or two
children (its in-order successor takes its place).

**15. Where is the in-order successor of a node with two children, and why can it have
at most one child?**
It is the leftmost node of the right subtree. Being leftmost, it has no left child; it may
have a right child.

**16. DSLab moves the successor node instead of copying its value. Why does that matter?**
Copying the value would make a node's ID suddenly hold a different value, and the UI and
frames select nodes by ID. Relinking the successor node (CLRS `transplant`) keeps every
node's ID and value together, and the animation shows the real node moving.

**17. What does `transplant(u, v)` do?**
It replaces the subtree rooted at `u` with the subtree rooted at `v` in `u`'s parent: the
parent's left or right link (or `root`) now points at `v`, and `v.parent` becomes
`u.parent`. It does not change `v`'s children.

## Rotations and AVL trees

**18. What does a left rotation at x do, and what does it preserve?**
x's right child y moves up: x's right link takes y's left subtree, y takes x's place
under x's parent (or as root), and x becomes y's left child. Three links (plus parent
links) change; the in-order sequence, and so the BST property, is preserved. O(1).

**19. Define the AVL balance factor and when a node is unbalanced.**
balance = height(left) − height(right), with height(NIL) = 0 and a leaf's height 1. A node
is unbalanced when its balance is +2 or −2; an AVL tree keeps every balance in −1..+1.

**20. Name the four AVL cases and their fixes.**
LL (balance +2, left child ≥ 0): rotate right at the node. RR (−2, right child ≤ 0):
rotate left. LR (+2, left child −1): rotate left at the left child, then right at the
node. RL (−2, right child +1): rotate right at the right child, then left at the node.

**21. After an AVL insert, how many rotations can be needed? After a delete?**
Insert: at most one single or double rotation, because it restores the subtree's old
height. Delete: one per level in the worst case (O(log n)), because a rotation can shorten
the subtree. DSLab walks from the changed node up to the root (`fixUp`), updating heights
and rebalancing on the way.

## Red-black trees

**22. State the five red-black properties.**
(1) Every node is red or black. (2) The root is black. (3) Every leaf (NIL) is black.
(4) A red node has two black children. (5) Every path from a node down to a NIL below it
has the same number of black nodes (the black-height).

**23. Why do these properties keep the height O(log n)?**
By property 4 at most half the nodes on any root-to-leaf path are red, and by property 5
all paths have the same number of black nodes, so the longest path is at most twice the
shortest. Hence height ≤ 2 log₂(n + 1).

**24. Describe the three insert fix-up cases (z is the new red node with a red parent).**
Case 1, the uncle is red: recolor parent and uncle black and the grandparent red, then
continue from the grandparent. Case 2, black uncle, z an inner child: rotate at the
parent to turn it into case 3. Case 3, black uncle, z an outer child: recolor parent black
and grandparent red, rotate at the grandparent. Finally, the root is colored black.

**25. Why does deleting a black node need a fix-up, and what is the "extra black"?**
Removing a black node shortens the black-height of the paths through it, breaking
property 5. CLRS gives the node x that took its place an extra black to keep the count;
the four delete cases (red sibling; black sibling with two black children; near child red;
far child red) move the extra black up or get rid of it with recolors and at most three
rotations.

**26. What is the NIL sentinel and why use one?**
One black node object (`nil_`, a member of `RedBlackTree`) that every empty child points
to, instead of `nullptr`. It is black, so property 3 holds automatically, and the fix-up
code can read `x.color` and `x.parent` even when x is "empty" without special cases.

## WebAssembly

**27. What is WebAssembly, and why does DSLab use it?**
WebAssembly (Wasm) is a portable binary instruction format that browsers run in a sandbox
at near-native speed. It lets the browser run the same C++17 code that the native tests
check, so there is exactly one implementation of every algorithm and none in TypeScript.
Emscripten compiles the engine to `dslab_engine.wasm` plus a JavaScript loader.

**28. How do C++ and TypeScript talk to each other?**
Through Embind, which exposes one C++ class to JavaScript as `Engine`, with one method
`handle(requestJson) → responseJson`. TypeScript sends a request as a JSON string; C++
parses it, runs the real algorithm, and returns the result, the final snapshot and every
frame as a JSON string. No C++ pointer or object other than that one handle reaches
JavaScript, so there is one simple, testable boundary.

**29. How is the `.wasm` file loaded, and what if that fails?**
Vite gives the built file's URL (under the site's base path, `/dslab/assets/…`). The page
fetches and compiles it once, through a module-level promise so React cannot load it
twice, then starts Emscripten's runtime with it and creates one `Engine`. If any step
fails, the page names the file, the exact URL and the cause (for example HTTP 404) and how
to fix it. Wasm memory can grow up to a 128 MiB cap, and its stack is 1 MiB, enough for
200-deep recursion.

**30. How do you know the Wasm build behaves exactly like the native build you tested?**
They compile the same sources, and `web/e2e/parity.spec.ts` sends the same 1,298
requests (every structure and operation, invalid input, imports, undo/redo, a 200-node
tree) to the native command runner `dslab_cli` and to the Wasm engine of the production
build: every response is identical after normalizing.
