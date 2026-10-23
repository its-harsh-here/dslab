# Five-minute demo script

A spoken demo for the evaluation: what to click and what to say. Times are cumulative.
Everything happens in one browser tab on the live site.

**Before you start**

- Open the live site, https://its-harsh-here.github.io/dslab/, in Chrome, Edge or
  Firefox, at full screen. Reload once so the engine is loaded.
- Keep a terminal ready in the project folder in case the network fails: `npm run build`
  then `npm run preview`, and open http://localhost:4173/dslab/ instead. Same site, same
  build.
- Speed starts at 1×. In each structure, open **Examples** (the disclosure above the
  drawing) when a step says to use a batch or a preset.

---

## 0:00–0:30 What DSLab is

Say: "DSLab is a data-structure simulator. Every structure and algorithm is written in
C++17 by us; the same C++ is compiled to WebAssembly and runs in the browser. The web page
only draws what C++ sends back. There is no JavaScript copy of any algorithm and no
server. Every step you will see is a frame recorded inside the real C++ algorithm as it
runs."

Point at the layout: structures on the left, the drawing in the middle, playback controls
under it, and on the right the explanation, the pseudocode, the selected node and the
cost.

## 0:30–1:15 Stack: frames, pseudocode and IDs

1. **Stack** is selected. In **Examples → Batch**, type `10, 20, 30` and click
   **Push all**. Each push plays in turn.
2. Click **Pop**. While it plays, press **Space** to pause.

Say: "Each step has one sentence from C++, and the highlighted pseudocode line is the
step's ID. The `#` numbers are node IDs: every node has an ID that never changes, and
the page selects nodes by ID, never by value. Here `top` moves to the next node, then the
old top is freed. The pool in C++ owns every node; links are non-owning pointers."

3. Press **→** twice to step forward, **←** once to step back, then **End** to jump to the
   result.

## 1:15–2:00 Linked list: pointer manipulation

1. Choose **Linked list**. Batch `1, 2, 3, 4`, **Append all**, then **End**.
2. Set speed to **0.5×** and click **Reverse**.

Say: "Reversal uses three pointers, `prev`, `curr` and `next`, shown as labels under the
nodes. Each frame flips one link. Nodes that are not reachable from `head` yet are drawn
dashed: they still exist because the pool owns them, so nothing leaks half-way through."

3. Drag the timeline slider back to the middle to show any step again, then **End**. Set
   speed back to **1×**.

## 2:00–2:45 BST: two-child delete keeps the node's identity

1. Choose **Binary search tree**. **Examples → Preset → "Delete with a deeper
   successor"**, **Run**.
2. While it plays, point at the successor 60.

Say: "Deleting 50, which has two children: we find the in-order successor, the leftmost
node of the right subtree, 60. Following CLRS we relink the successor node itself with
`transplant`, we do not copy its value, so node 60 keeps its ID and you see the same node
move up."

3. When it finishes, click node **60**. The **Selected node** panel shows its ID, value,
   parent and children.

## 2:45–3:30 AVL: imbalance and a double rotation

1. Choose **AVL tree**. **Preset → "LR: insert 30, 10, then 20"**, **Run**.

Say: "Each node shows its height and balance: height of NIL is 0, a leaf is 1, balance is
left height minus right height. After inserting 20 we walk up updating heights; 30 has
balance +2 and its left child −1, so it is the LR case: rotate left at 10, then right at
30. The node IDs survive both rotations."

2. Point at **Cost**: "Algorithm work, counted by C++: 2 rotations. Below it, separately,
   the tracing cost: how many frames and node copies the animation needed. We never mix
   the two."

## 3:30–4:20 Red-black tree, undo and redo, export

1. Choose **Red-black tree**. Tick **Show NIL leaves**. **Preset → "Delete case 3: near
   child red"**, **Run**.

Say: "CLRS red-black tree with one black NIL sentinel. Deleting black 10 leaves an extra
black; the sibling's near child is red, so case 3 rotates it into case 4, which recolors
and rotates once more and ends. Frames between steps may break a property for a moment;
the page labels those as a temporary violation."

2. Press **Ctrl+Z** (undo): the tree before the delete returns, with the same IDs.
   **Ctrl+Y** (redo) deletes again.

Say: "Undo is snapshot-based and runs in C++: before each change we store a copy of the
structure, including the next-ID counter, and undo restores it exactly. At most 50 steps
per structure."

3. Click **Export**: a JSON file is saved. "Import validates the file completely in C++
   before changing anything, so a bad file changes nothing."

## 4:20–5:00 Complexity and testing

1. Choose **Binary search tree**, **Preset → "Sorted inserts (a degenerate tree)"**,
   **Run**, **End**. Insert `60` and look at the **Cost** panel's comparisons.
2. Choose **AVL tree**, batch `10, 20, 30, 40, 50`, **Insert all**, **End**, insert `60`.

Say: "Sorted input turns the BST into a list, O(n) per insert; the AVL tree rebalances and
stays O(log n): compare the comparison counts.

How we know it is correct: 106 native C++ test cases with an independent invariant
checker after every operation, random sequences compared with the standard library,
the same tests under AddressSanitizer, a browser test suite that checks every step on
screen against its frame, and a check that the WebAssembly build answers 1,298 requests
exactly like the native build. GitHub Actions runs the tests on every push and deploys
this site only if they pass."

---

**If asked to go deeper:** docs/viva-questions.md has 30 questions with model answers;
docs/complexity.md has the cost tables; docs/memory.md explains ownership.
