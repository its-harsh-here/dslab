#include "dslab/Catalog.h"

#include <string>
#include <vector>

namespace dslab {

namespace {

// Lines are written as {stepId, text}. A line with stepId "" records no frame.
using Lines = std::vector<PseudocodeLine>;

// clear() of a linear structure: unlink the first node until none is left.
Lines linearClear(const std::string& p, const std::string& entry, bool clearRear) {
    Lines lines = {
        {p + ".clear.enter", "clear():"},
        {"", "  removed <- 0"},
        {"", "  while " + entry + " != NIL:"},
        {"", "    node <- " + entry},
        {"", "    " + entry + " <- node.next"},
    };
    if (clearRear) {
        lines.push_back({"", "    if front = NIL: rear <- NIL"});
    }
    lines.push_back({p + ".clear.remove", "    delete node"});
    lines.push_back({"", "    removed <- removed + 1"});
    lines.push_back({p + ".clear.done", "  return removed"});
    return lines;
}

// freeSubtree(n), shared by clear() and the binary tree's deleteSubtree().
Lines freeSubtree(const std::string& removeStep) {
    return {
        {"", "freeSubtree(n):     post-order: children are freed before their parent"},
        {"", "  if n.left != NIL: child <- n.left; n.left <- NIL; freeSubtree(child)"},
        {"", "  if n.right != NIL: child <- n.right; n.right <- NIL; freeSubtree(child)"},
        {removeStep, "  delete n"},
    };
}

Lines treeClear(const std::string& p) {
    Lines lines = {
        {p + ".clear.enter", "clear():"},
        {"", "  if root != NIL:"},
        {p + ".clear.cut", "    node <- root; root <- NIL"},
        {"", "    freeSubtree(node)"},
        {p + ".clear.done", "  return the number freed"},
    };
    for (const PseudocodeLine& line : freeSubtree(p + ".clear.remove")) {
        lines.push_back(line);
    }
    return lines;
}

// The four traversals (TreeTraversal.h), shared by every tree.
Lines traversal(const std::string& p, Operation order) {
    if (order == Operation::Levelorder) {
        const std::string s = p + ".levelorder.";
        return {
            {s + "enter", "levelorder():"},
            {"", "  queue <- [root] if root != NIL, else []"},
            {"", "  while queue is not empty:"},
            {s + "visit", "    node <- take from the front; visit node; add its children to the back"},
            {s + "done", "  return the visited values"},
        };
    }
    const std::string name = order == Operation::Preorder ? "preorder" : order == Operation::Inorder ? "inorder" : "postorder";
    const std::string s = p + "." + name + ".";
    const PseudocodeLine visit = {s + "visit", "  visit node: add node.value to the output"};
    const PseudocodeLine left = {s + "goLeft", "  if node.left != NIL: go to node.left; walk(node.left)"};
    const PseudocodeLine right = {s + "goRight", "  if node.right != NIL: go to node.right; walk(node.right)"};
    Lines lines = {
        {s + "enter", name + "():"},
        {s + "start", "  if root != NIL: start at root; walk(root)"},
        {s + "done", "  return the visited values"},
        {"", "walk(node):"},
    };
    if (order == Operation::Preorder) {
        lines.insert(lines.end(), {visit, left, right});
    } else if (order == Operation::Inorder) {
        lines.insert(lines.end(), {left, visit, right});
    } else {
        lines.insert(lines.end(), {left, right, visit});
    }
    return lines;
}

// search(value) of the BST family; 'x' is the cursor's name.
Lines treeSearch(const std::string& p, const std::string& x) {
    return {
        {p + ".search.enter", "search(value):"},
        {"", "  " + x + " <- root"},
        {"", "  while " + x + " != NIL:"},
        {p + ".search.found", "    if value = " + x + ".value: found; stop"},
        {p + ".search.goLeft", "    if value < " + x + ".value: " + x + " <- " + x + ".left"},
        {p + ".search.goRight", "    else: " + x + " <- " + x + ".right"},
        {p + ".search.notFound", "  if " + x + " = NIL: not found"},
        {p + ".search.done", "  return the result"},
    };
}

// The BST's insert walk, shared word for word by the AVL tree.
Lines bstStyleInsert(const std::string& p, const std::string& create) {
    return {
        {p + ".insert.enter", "insert(value):"},
        {p + ".insert.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
        {"", "  parent <- NIL; curr <- root"},
        {"", "  while curr != NIL:"},
        {p + ".insert.duplicate", "    if value = curr.value: reject DUPLICATE_VALUE"},
        {"", "    parent <- curr"},
        {p + ".insert.goLeft", "    if value < curr.value: curr <- curr.left"},
        {p + ".insert.goRight", "    else: curr <- curr.right"},
        {p + ".insert.create", create},
        {p + ".insert.linkRoot", "  if parent = NIL: root <- node"},
        {p + ".insert.linkLeft", "  else if value < parent.value: parent.left <- node"},
        {p + ".insert.linkRight", "  else: parent.right <- node"},
    };
}

// The BST's delete (CLRS, successor node relinked), shared by the AVL tree.
// 'afterRemove' are the lines between "delete z" and "return".
Lines bstStyleDelete(const std::string& p, const Lines& afterRemove) {
    Lines lines = {
        {p + ".delete.enter", "delete(value):"},
        {"", "  z <- root"},
        {"", "  while z != NIL and value != z.value:"},
        {p + ".delete.goLeft", "    if value < z.value: z <- z.left"},
        {p + ".delete.goRight", "    else: z <- z.right"},
        {p + ".delete.notFound", "  if z = NIL: reject VALUE_NOT_FOUND"},
        {p + ".delete.caseLeaf", "  if z has no children: case 1, a leaf"},
        {p + ".delete.caseOneChild", "  else if z has one child: case 2"},
        {p + ".delete.caseTwoChildren", "  else: case 3, two children"},
        {"", "  if z.left = NIL or z.right = NIL:"},
        {p + ".delete.replace", "    transplant(z, the child of z, or NIL)"},
        {"", "  else:"},
        {p + ".delete.successorStart", "    y <- z.right"},
        {p + ".delete.successorLeft", "    while y.left != NIL: y <- y.left"},
        {p + ".delete.successorFound", "    y is the in-order successor of z"},
        {"", "    if y.parent != z:"},
        {p + ".delete.detachSuccessor", "      transplant(y, y.right)"},
        {p + ".delete.successorTakesRight", "      y.right <- z.right; y.right.parent <- y"},
        {p + ".delete.replaceWithSuccessor", "    transplant(z, y)"},
        {p + ".delete.successorTakesLeft", "    y.left <- z.left; y.left.parent <- y"},
        {p + ".delete.remove", "  delete z"},
    };
    lines.insert(lines.end(), afterRemove.begin(), afterRemove.end());
    lines.push_back({p + ".delete.done", "  return"});
    lines.push_back({"", "transplant(u, v):"});
    lines.push_back({"", "  if u.parent = NIL: root <- v"});
    lines.push_back({"", "  else if u = u.parent.left: u.parent.left <- v"});
    lines.push_back({"", "  else: u.parent.right <- v"});
    lines.push_back({"", "  if v != NIL: v.parent <- u.parent"});
    return lines;
}

Lines join(std::vector<Lines> parts) {
    Lines all;
    for (const Lines& part : parts) {
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

std::vector<OperationListing> stackCatalog() {
    return {
        {Operation::Push, "O(1)",
         {{"stack.push.enter", "push(value):"},
          {"stack.push.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {"stack.push.create", "  node <- new Node(value)"},
          {"stack.push.linkNext", "  node.next <- top"},
          {"stack.push.moveTop", "  top <- node"},
          {"stack.push.done", "  return node"}}},
        {Operation::Pop, "O(1)",
         {{"stack.pop.enter", "pop():"},
          {"stack.pop.empty", "  if top = NIL: reject EMPTY_STRUCTURE"},
          {"stack.pop.take", "  node <- top"},
          {"stack.pop.moveTop", "  top <- node.next"},
          {"", "  value <- node.value"},
          {"stack.pop.remove", "  delete node"},
          {"stack.pop.done", "  return value"}}},
        {Operation::Peek, "O(1)",
         {{"stack.peek.enter", "peek():"},
          {"stack.peek.empty", "  if top = NIL: reject EMPTY_STRUCTURE"},
          {"stack.peek.read", "  value <- top.value"},
          {"stack.peek.done", "  return value"}}},
        {Operation::Clear, "O(n)", linearClear("stack", "top", false)},
    };
}

std::vector<OperationListing> queueCatalog() {
    return {
        {Operation::Enqueue, "O(1)",
         {{"queue.enqueue.enter", "enqueue(value):"},
          {"queue.enqueue.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {"queue.enqueue.create", "  node <- new Node(value)"},
          {"queue.enqueue.linkFront", "  if rear = NIL: front <- node"},
          {"queue.enqueue.linkRear", "  else: rear.next <- node"},
          {"queue.enqueue.moveRear", "  rear <- node"},
          {"queue.enqueue.done", "  return node"}}},
        {Operation::Dequeue, "O(1)",
         {{"queue.dequeue.enter", "dequeue():"},
          {"queue.dequeue.empty", "  if front = NIL: reject EMPTY_STRUCTURE"},
          {"queue.dequeue.take", "  node <- front"},
          {"queue.dequeue.moveFront", "  front <- node.next"},
          {"queue.dequeue.clearRear", "  if front = NIL: rear <- NIL"},
          {"", "  value <- node.value"},
          {"queue.dequeue.remove", "  delete node"},
          {"queue.dequeue.done", "  return value"}}},
        {Operation::Front, "O(1)",
         {{"queue.front.enter", "front():"},
          {"queue.front.empty", "  if front = NIL: reject EMPTY_STRUCTURE"},
          {"queue.front.read", "  value <- front.value"},
          {"queue.front.done", "  return value"}}},
        {Operation::Clear, "O(n)", linearClear("queue", "front", true)},
    };
}

std::vector<OperationListing> listCatalog() {
    return {
        {Operation::InsertAt, "O(n): walks index - 1 links",
         {{"list.insertAt.enter", "insertAt(index, value):"},
          {"list.insertAt.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {"list.insertAt.badIndex", "  if index < 0 or index > size: reject INDEX_OUT_OF_RANGE"},
          {"", "  if index > 0:"},
          {"list.insertAt.start", "    prev <- head"},
          {"list.insertAt.walk", "    repeat index - 1 times: prev <- prev.next"},
          {"list.insertAt.create", "  node <- new Node(value)"},
          {"list.insertAt.linkNext", "  node.next <- head if index = 0, else prev.next"},
          {"list.insertAt.linkHead", "  if index = 0: head <- node"},
          {"list.insertAt.linkPrev", "  else: prev.next <- node"},
          {"list.insertAt.done", "  return node"}}},
        {Operation::DeleteNode, "O(n)",
         {{"list.deleteNode.enter", "deleteNode(id):"},
          {"list.deleteNode.missing", "  if no node has this ID: reject NODE_NOT_FOUND"},
          {"", "  if head is the node:"},
          {"list.deleteNode.atHead", "    node <- head"},
          {"list.deleteNode.moveHead", "    head <- node.next"},
          {"", "  else:"},
          {"list.deleteNode.start", "    prev <- head"},
          {"list.deleteNode.walk", "    while prev.next is not the node: prev <- prev.next"},
          {"list.deleteNode.found", "    node <- prev.next"},
          {"list.deleteNode.bypass", "    prev.next <- node.next"},
          {"list.deleteNode.remove", "  delete node"},
          {"list.deleteNode.done", "  return"}}},
        {Operation::Search, "O(n)",
         {{"list.search.enter", "search(value):"},
          {"", "  curr <- head; index <- 0"},
          {"", "  while curr != NIL:"},
          {"list.search.found", "    if curr.value = value: found at index; stop"},
          {"list.search.next", "    curr <- curr.next; index <- index + 1"},
          {"list.search.notFound", "  if curr = NIL: not found"},
          {"list.search.done", "  return the result"}}},
        {Operation::Reverse, "O(n)",
         {{"list.reverse.enter", "reverse():"},
          {"", "  prev <- NIL; curr <- head"},
          {"", "  while curr != NIL:"},
          {"", "    next <- curr.next"},
          {"list.reverse.flip", "    curr.next <- prev"},
          {"", "    prev <- curr; curr <- next"},
          {"list.reverse.moveHead", "  head <- prev"},
          {"list.reverse.done", "  return"}}},
        {Operation::Clear, "O(n)", linearClear("list", "head", false)},
    };
}

std::vector<OperationListing> binaryTreeCatalog() {
    const std::string p = "binaryTree";
    return {
        {Operation::AddRoot, "O(1)",
         {{"binaryTree.addRoot.enter", "addRoot(value):"},
          {"binaryTree.addRoot.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {"binaryTree.addRoot.rootExists", "  if root != NIL: reject ROOT_EXISTS"},
          {"binaryTree.addRoot.create", "  node <- new Node(value)"},
          {"binaryTree.addRoot.linkRoot", "  root <- node"},
          {"binaryTree.addRoot.done", "  return node"}}},
        {Operation::AddChild, "O(1)",
         {{"binaryTree.addChild.enter", "addChild(parentId, side, value):"},
          {"binaryTree.addChild.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {"", "  parent <- the node with parentId"},
          {"binaryTree.addChild.missing", "  if there is none: reject NODE_NOT_FOUND"},
          {"binaryTree.addChild.occupied", "  if parent.side != NIL: reject CHILD_EXISTS"},
          {"binaryTree.addChild.create", "  node <- new Node(value); node.parent <- parent"},
          {"binaryTree.addChild.link", "  parent.side <- node"},
          {"binaryTree.addChild.done", "  return node"}}},
        {Operation::DeleteSubtree, "O(k), k = nodes in the subtree",
         join({{{"binaryTree.deleteSubtree.enter", "deleteSubtree(id):"},
                {"", "  node <- the node with this ID"},
                {"binaryTree.deleteSubtree.missing", "  if there is none: reject NODE_NOT_FOUND"},
                {"binaryTree.deleteSubtree.cut", "  set the link to node (root, or its parent's child link) to NIL"},
                {"", "  freeSubtree(node)"},
                {"binaryTree.deleteSubtree.done", "  return the freed IDs"}},
               freeSubtree("binaryTree.deleteSubtree.remove")})},
        {Operation::Preorder, "O(n)", traversal(p, Operation::Preorder)},
        {Operation::Inorder, "O(n)", traversal(p, Operation::Inorder)},
        {Operation::Postorder, "O(n)", traversal(p, Operation::Postorder)},
        {Operation::Levelorder, "O(n)", traversal(p, Operation::Levelorder)},
        {Operation::Clear, "O(n)", treeClear(p)},
    };
}

std::vector<OperationListing> bstCatalog() {
    const std::string p = "bst";
    const char* height = "O(h), h = height: O(log n) if balanced, O(n) in the worst case";
    Lines insert = bstStyleInsert(p, "  node <- new Node(value); node.parent <- parent");
    insert.push_back({"bst.insert.done", "  return node"});
    return {
        {Operation::Insert, height, insert},
        {Operation::Delete, height, bstStyleDelete(p, {})},
        {Operation::Search, height, treeSearch(p, "curr")},
        {Operation::Preorder, "O(n)", traversal(p, Operation::Preorder)},
        {Operation::Inorder, "O(n)", traversal(p, Operation::Inorder)},
        {Operation::Postorder, "O(n)", traversal(p, Operation::Postorder)},
        {Operation::Levelorder, "O(n)", traversal(p, Operation::Levelorder)},
        {Operation::Clear, "O(n)", treeClear(p)},
    };
}

// fixUp, rebalance and the two rotations (docs/avl.md), used by insert and delete.
Lines avlRebalancing() {
    return {
        {"avl.fixUp.start", "fixUp(n):"},
        {"", "  while n != NIL:"},
        {"avl.fixUp.updateHeight", "    n.height <- 1 + max(height(n.left), height(n.right))"},
        {"avl.fixUp.unbalanced", "    if balance(n) = +2 or balance(n) = -2:"},
        {"", "      n <- rebalance(n)"},
        {"", "    n <- n.parent"},
        {"avl.fixUp.done", "  every balance on the path is -1, 0 or +1"},
        {"", "rebalance(n):"},
        {"avl.rebalance.LL", "  if balance(n) = +2 and balance(n.left) >= 0: LL: rotateRight(n)"},
        {"avl.rebalance.LR", "  else if balance(n) = +2: LR: rotateLeft(n.left); rotateRight(n)"},
        {"avl.rebalance.RR", "  else if balance(n.right) <= 0: RR: rotateLeft(n)"},
        {"avl.rebalance.RL", "  else: RL: rotateRight(n.right); rotateLeft(n)"},
        {"", "rotateLeft(x):     x's right child y moves up"},
        {"", "  y <- x.right"},
        {"", "  x.right <- y.left; if y.left != NIL: y.left.parent <- x"},
        {"", "  y.parent <- x.parent; put y where x was (root, or x.parent's child)"},
        {"", "  y.left <- x; x.parent <- y"},
        {"avl.rotateLeft.heights", "  update the heights of x, then y"},
        {"", "rotateRight(y):    y's left child x moves up"},
        {"", "  x <- y.left"},
        {"", "  y.left <- x.right; if x.right != NIL: x.right.parent <- y"},
        {"", "  x.parent <- y.parent; put x where y was (root, or y.parent's child)"},
        {"", "  x.right <- y; y.parent <- x"},
        {"avl.rotateRight.heights", "  update the heights of y, then x"},
    };
}

std::vector<OperationListing> avlCatalog() {
    const std::string p = "avl";
    Lines insert = bstStyleInsert(p, "  node <- new Node(value), height 1; node.parent <- parent");
    insert.push_back({"", "  fixUp(parent)"});
    insert.push_back({"avl.insert.done", "  return node"});
    const Lines remove = bstStyleDelete(p, {{"", "  fixUp(the lowest node whose height may have changed)"}});
    return {
        {Operation::Insert, "O(log n)", join({insert, avlRebalancing()})},
        {Operation::Delete, "O(log n)", join({remove, avlRebalancing()})},
        {Operation::Search, "O(log n)", treeSearch(p, "curr")},
        {Operation::Preorder, "O(n)", traversal(p, Operation::Preorder)},
        {Operation::Inorder, "O(n)", traversal(p, Operation::Inorder)},
        {Operation::Postorder, "O(n)", traversal(p, Operation::Postorder)},
        {Operation::Levelorder, "O(n)", traversal(p, Operation::Levelorder)},
        {Operation::Clear, "O(n)", treeClear(p)},
    };
}

// CLRS LEFT-ROTATE and RIGHT-ROTATE (docs/red-black.md, "Rotations").
Lines redBlackRotations() {
    return {
        {"", "rotateLeft(x):     x's right child y moves up"},
        {"", "  y <- x.right"},
        {"", "  x.right <- y.left; if y.left != NIL: y.left.parent <- x"},
        {"", "  y.parent <- x.parent; put y where x was (root, or x.parent's child)"},
        {"redBlack.rotateLeft.linkDown", "  y.left <- x; x.parent <- y"},
        {"", "rotateRight(x):    x's left child y moves up"},
        {"", "  y <- x.left"},
        {"", "  x.left <- y.right; if y.right != NIL: y.right.parent <- x"},
        {"", "  y.parent <- x.parent; put y where x was (root, or x.parent's child)"},
        {"redBlack.rotateRight.linkDown", "  y.right <- x; x.parent <- y"},
    };
}

// CLRS RB-INSERT and RB-INSERT-FIXUP, both sides written out (docs/red-black.md).
Lines redBlackInsert() {
    const std::string f = "redBlack.insertFixup.";
    return {
        {"redBlack.insert.enter", "insert(value):"},
        {"redBlack.insert.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
        {"", "  y <- NIL; x <- root"},
        {"", "  while x != NIL:"},
        {"redBlack.insert.duplicate", "    if value = x.value: reject DUPLICATE_VALUE"},
        {"", "    y <- x"},
        {"redBlack.insert.goLeft", "    if value < x.value: x <- x.left"},
        {"redBlack.insert.goRight", "    else: x <- x.right"},
        {"redBlack.insert.create", "  z <- new red Node(value); z.left <- NIL; z.right <- NIL; z.parent <- y"},
        {"redBlack.insert.linkRoot", "  if y = NIL: root <- z"},
        {"redBlack.insert.linkLeft", "  else if value < y.value: y.left <- z"},
        {"redBlack.insert.linkRight", "  else: y.right <- z"},
        {"", "  insertFixup(z)"},
        {"redBlack.insert.done", "  return z"},
        {"", "insertFixup(z):"},
        {"", "  while z.parent.color = RED:"},
        {"", "    if z.parent = z.parent.parent.left:"},
        {"", "      y <- z.parent.parent.right        (the uncle)"},
        {f + "case1", "      if y.color = RED:                 case 1"},
        {f + "case1Parent", "        z.parent.color <- BLACK"},
        {f + "case1Uncle", "        y.color <- BLACK"},
        {f + "case1Grandparent", "        z.parent.parent.color <- RED"},
        {f + "case1Up", "        z <- z.parent.parent"},
        {"", "      else:"},
        {f + "case2", "        if z = z.parent.right:          case 2"},
        {"", "          z <- z.parent; rotateLeft(z)"},
        {f + "case3", "        case 3: z is an outer child"},
        {f + "case3Parent", "        z.parent.color <- BLACK"},
        {f + "case3Grandparent", "        z.parent.parent.color <- RED"},
        {"", "        rotateRight(z.parent.parent)"},
        {"", "    else:                                 (mirror: left and right exchanged)"},
        {"", "      y <- z.parent.parent.left         (the uncle)"},
        {f + "case1Mirror", "      if y.color = RED:                 case 1"},
        {f + "case1ParentMirror", "        z.parent.color <- BLACK"},
        {f + "case1UncleMirror", "        y.color <- BLACK"},
        {f + "case1GrandparentMirror", "        z.parent.parent.color <- RED"},
        {f + "case1UpMirror", "        z <- z.parent.parent"},
        {"", "      else:"},
        {f + "case2Mirror", "        if z = z.parent.left:           case 2"},
        {"", "          z <- z.parent; rotateRight(z)"},
        {f + "case3Mirror", "        case 3: z is an outer child"},
        {f + "case3ParentMirror", "        z.parent.color <- BLACK"},
        {f + "case3GrandparentMirror", "        z.parent.parent.color <- RED"},
        {"", "        rotateLeft(z.parent.parent)"},
        {f + "blackRoot", "  if root.color = RED: root.color <- BLACK"},
        {f + "done", "  all five properties hold"},
    };
}

// CLRS RB-DELETE, RB-TRANSPLANT and RB-DELETE-FIXUP, both sides written out.
Lines redBlackDelete() {
    const std::string d = "redBlack.delete.";
    const std::string f = "redBlack.deleteFixup.";
    return {
        {d + "enter", "delete(value):"},
        {"", "  z <- root"},
        {"", "  while z != NIL and value != z.value:"},
        {d + "goLeft", "    if value < z.value: z <- z.left"},
        {d + "goRight", "    else: z <- z.right"},
        {d + "notFound", "  if z = NIL: reject VALUE_NOT_FOUND"},
        {d + "found", "  z found: a leaf, one child or two children"},
        {"", "  y <- z; yOriginalColor <- y.color"},
        {"", "  if z.left = NIL:"},
        {d + "replaceWithRight", "    x <- z.right; transplant(z, z.right)"},
        {"", "  else if z.right = NIL:"},
        {d + "replaceWithLeft", "    x <- z.left; transplant(z, z.left)"},
        {"", "  else:"},
        {d + "successorStart", "    y <- z.right"},
        {d + "successorLeft", "    while y.left != NIL: y <- y.left"},
        {d + "successorFound", "    yOriginalColor <- y.color; x <- y.right"},
        {"", "    if y.parent = z: x.parent <- y      (x may be NIL)"},
        {"", "    else:"},
        {d + "detachSuccessor", "      transplant(y, y.right)"},
        {d + "successorTakesRight", "      y.right <- z.right; y.right.parent <- y"},
        {d + "replaceWithSuccessor", "    transplant(z, y)"},
        {d + "successorTakesLeft", "    y.left <- z.left; y.left.parent <- y"},
        {d + "successorTakesColor", "    y.color <- z.color"},
        {d + "remove", "  free z"},
        {d + "extraBlack", "  if yOriginalColor = BLACK: x carries an extra black; deleteFixup(x)"},
        {d + "done", "  return"},
        {"", "transplant(u, v):"},
        {"", "  if u.parent = NIL: root <- v"},
        {"", "  else if u = u.parent.left: u.parent.left <- v"},
        {"", "  else: u.parent.right <- v"},
        {"", "  v.parent <- u.parent                  (also when v is NIL)"},
        {"", "deleteFixup(x):"},
        {"", "  while x != root and x.color = BLACK:"},
        {"", "    if x = x.parent.left:"},
        {"", "      w <- x.parent.right               (the sibling)"},
        {f + "case1", "      if w.color = RED:                 case 1"},
        {f + "case1Sibling", "        w.color <- BLACK"},
        {f + "case1Parent", "        x.parent.color <- RED"},
        {"", "        rotateLeft(x.parent); w <- x.parent.right"},
        {f + "case2", "      if w.left.color = BLACK and w.right.color = BLACK:   case 2"},
        {f + "case2Sibling", "        w.color <- RED"},
        {f + "case2Up", "        x <- x.parent"},
        {"", "      else:"},
        {f + "case3", "        if w.right.color = BLACK:       case 3"},
        {f + "case3Near", "          w.left.color <- BLACK"},
        {f + "case3Sibling", "          w.color <- RED"},
        {"", "          rotateRight(w); w <- x.parent.right"},
        {f + "case4", "        case 4: w.right is red"},
        {f + "case4Sibling", "        w.color <- x.parent.color"},
        {f + "case4Parent", "        x.parent.color <- BLACK"},
        {f + "case4Far", "        w.right.color <- BLACK"},
        {"", "        rotateLeft(x.parent); x <- root"},
        {"", "    else:                                 (mirror: left and right exchanged)"},
        {"", "      w <- x.parent.left                (the sibling)"},
        {f + "case1Mirror", "      if w.color = RED:                 case 1"},
        {f + "case1SiblingMirror", "        w.color <- BLACK"},
        {f + "case1ParentMirror", "        x.parent.color <- RED"},
        {"", "        rotateRight(x.parent); w <- x.parent.left"},
        {f + "case2Mirror", "      if w.right.color = BLACK and w.left.color = BLACK:   case 2"},
        {f + "case2SiblingMirror", "        w.color <- RED"},
        {f + "case2UpMirror", "        x <- x.parent"},
        {"", "      else:"},
        {f + "case3Mirror", "        if w.left.color = BLACK:        case 3"},
        {f + "case3NearMirror", "          w.right.color <- BLACK"},
        {f + "case3SiblingMirror", "          w.color <- RED"},
        {"", "          rotateLeft(w); w <- x.parent.left"},
        {f + "case4Mirror", "        case 4: w.left is red"},
        {f + "case4SiblingMirror", "        w.color <- x.parent.color"},
        {f + "case4ParentMirror", "        x.parent.color <- BLACK"},
        {f + "case4FarMirror", "        w.left.color <- BLACK"},
        {"", "        rotateRight(x.parent); x <- root"},
        {f + "blacken", "  x.color <- BLACK"},
        {f + "done", "  all five properties hold"},
    };
}

std::vector<OperationListing> redBlackCatalog() {
    const std::string p = "redBlack";
    return {
        {Operation::Insert, "O(log n)", join({redBlackInsert(), redBlackRotations()})},
        {Operation::Delete, "O(log n)", join({redBlackDelete(), redBlackRotations()})},
        {Operation::Search, "O(log n)", treeSearch(p, "x")},
        {Operation::Preorder, "O(n)", traversal(p, Operation::Preorder)},
        {Operation::Inorder, "O(n)", traversal(p, Operation::Inorder)},
        {Operation::Postorder, "O(n)", traversal(p, Operation::Postorder)},
        {Operation::Levelorder, "O(n)", traversal(p, Operation::Levelorder)},
        {Operation::Clear, "O(n)", treeClear(p)},
    };
}

// ---------------------------------------------------------------------------
// Deque: every operation exists once per end; the two listings are mirror images.

Lines dequePush(bool front) {
    const std::string s = front ? "deque.pushFront." : "deque.pushBack.";
    const std::string end = front ? "front" : "rear";
    const std::string other = front ? "rear" : "front";
    return {
        {s + "enter", std::string(front ? "pushFront" : "pushBack") + "(value):"},
        {s + "full", "  if size = 200: reject CAPACITY_EXCEEDED"},
        {s + "create", "  node <- new Node(value)"},
        {s + "linkEmpty", "  if " + end + " = NIL: " + other + " <- node"},
        {s + "link", front ? "  else: node.next <- front; front.prev <- node" : "  else: node.prev <- rear; rear.next <- node"},
        {s + "moveEnd", "  " + end + " <- node"},
        {s + "done", "  return node"},
    };
}

Lines dequePop(bool front) {
    const std::string s = front ? "deque.popFront." : "deque.popBack.";
    const std::string end = front ? "front" : "rear";
    const std::string other = front ? "rear" : "front";
    return {
        {s + "enter", std::string(front ? "popFront" : "popBack") + "():"},
        {s + "empty", "  if " + end + " = NIL: reject EMPTY_STRUCTURE"},
        {s + "take", "  node <- " + end},
        {s + "moveEnd", "  " + end + " <- node." + (front ? "next" : "prev")},
        {s + "clearOther", "  if " + end + " = NIL: " + other + " <- NIL"},
        {s + "unlink", "  else: " + end + "." + (front ? "prev" : "next") + " <- NIL"},
        {"", "  value <- node.value"},
        {s + "remove", "  delete node"},
        {s + "done", "  return value"},
    };
}

Lines dequePeek(bool front) {
    const std::string s = front ? "deque.peekFront." : "deque.peekBack.";
    const std::string end = front ? "front" : "rear";
    return {
        {s + "enter", std::string(front ? "peekFront" : "peekBack") + "():"},
        {s + "empty", "  if " + end + " = NIL: reject EMPTY_STRUCTURE"},
        {s + "read", "  value <- " + end + ".value"},
        {s + "done", "  return value"},
    };
}

std::vector<OperationListing> dequeCatalog() {
    return {
        {Operation::PushFront, "O(1)", dequePush(true)},
        {Operation::PushBack, "O(1)", dequePush(false)},
        {Operation::PopFront, "O(1)", dequePop(true)},
        {Operation::PopBack, "O(1)", dequePop(false)},
        {Operation::PeekFront, "O(1)", dequePeek(true)},
        {Operation::PeekBack, "O(1)", dequePeek(false)},
        {Operation::Clear, "O(n)",
         {{"deque.clear.enter", "clear():"},
          {"", "  removed <- 0"},
          {"", "  while front != NIL:"},
          {"", "    node <- front; front <- node.next"},
          {"", "    if front = NIL: rear <- NIL  else: front.prev <- NIL"},
          {"deque.clear.remove", "    delete node"},
          {"", "    removed <- removed + 1"},
          {"deque.clear.done", "  return removed"}}},
    };
}

std::vector<OperationListing> doublyListCatalog() {
    const std::string p = "doublyList";
    return {
        {Operation::InsertAt, "O(min(i, n - i))",
         {{p + ".insertAt.enter", "insertAt(index, value):"},
          {p + ".insertAt.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {p + ".insertAt.badIndex", "  if index < 0 or index > size: reject INDEX_OUT_OF_RANGE"},
          {"", "  if 0 < index < size:          find the node now at index"},
          {"", "    if index <= size / 2:"},
          {p + ".insertAt.startHead", "      curr <- head"},
          {p + ".insertAt.walkNext", "      repeat index times: curr <- curr.next"},
          {"", "    else:"},
          {p + ".insertAt.startTail", "      curr <- tail"},
          {p + ".insertAt.walkPrev", "      repeat size - 1 - index times: curr <- curr.prev"},
          {p + ".insertAt.create", "  node <- new Node(value)"},
          {p + ".insertAt.linkOnly", "  if head = NIL: head <- node; tail <- node"},
          {"", "  else if index = 0:"},
          {p + ".insertAt.linkBeforeHead", "    node.next <- head; head.prev <- node"},
          {p + ".insertAt.moveHead", "    head <- node"},
          {"", "  else if index = size:"},
          {p + ".insertAt.linkAfterTail", "    node.prev <- tail; tail.next <- node"},
          {p + ".insertAt.moveTail", "    tail <- node"},
          {"", "  else:"},
          {p + ".insertAt.linkNode", "    node.prev <- curr.prev; node.next <- curr"},
          {p + ".insertAt.linkNeighbours", "    curr.prev.next <- node; curr.prev <- node"},
          {p + ".insertAt.done", "  return node"}}},
        {Operation::DeleteNode, "O(1)",
         {{p + ".deleteNode.enter", "deleteNode(id):"},
          {p + ".deleteNode.missing", "  if no node has this ID: reject NODE_NOT_FOUND"},
          {p + ".deleteNode.found", "  node <- the selected node      (no walk is needed)"},
          {p + ".deleteNode.moveHead", "  if node.prev = NIL: head <- node.next"},
          {p + ".deleteNode.bypassNext", "  else: node.prev.next <- node.next"},
          {p + ".deleteNode.moveTail", "  if node.next = NIL: tail <- node.prev"},
          {p + ".deleteNode.bypassPrev", "  else: node.next.prev <- node.prev"},
          {p + ".deleteNode.remove", "  delete node"},
          {p + ".deleteNode.done", "  return"}}},
        {Operation::Search, "O(n)",
         {{p + ".search.enter", "search(value):"},
          {"", "  curr <- head; index <- 0"},
          {"", "  while curr != NIL:"},
          {p + ".search.found", "    if curr.value = value: found"},
          {p + ".search.next", "    curr <- curr.next; index <- index + 1"},
          {p + ".search.notFound", "  not found"},
          {p + ".search.done", "  return the result"}}},
        {Operation::Reverse, "O(n)",
         {{p + ".reverse.enter", "reverse():"},
          {"", "  curr <- head"},
          {"", "  while curr != NIL:"},
          {p + ".reverse.flip", "    swap curr.prev and curr.next"},
          {"", "    curr <- curr.prev           (the old next)"},
          {p + ".reverse.swapEnds", "  swap head and tail"},
          {p + ".reverse.done", "  return"}}},
        {Operation::Clear, "O(n)",
         {{p + ".clear.enter", "clear():"},
          {"", "  removed <- 0"},
          {"", "  while head != NIL:"},
          {"", "    node <- head; head <- node.next"},
          {"", "    if head = NIL: tail <- NIL  else: head.prev <- NIL"},
          {p + ".clear.remove", "    delete node"},
          {"", "    removed <- removed + 1"},
          {p + ".clear.done", "  return removed"}}},
    };
}

Lines circularInsert(bool front) {
    const std::string s = front ? "circularList.insertFront." : "circularList.insertBack.";
    Lines lines = {
        {s + "enter", std::string(front ? "insertFront" : "insertBack") + "(value):"},
        {s + "full", "  if size = 200: reject CAPACITY_EXCEEDED"},
        {s + "create", "  node <- new Node(value)"},
        {"", "  if tail = NIL:"},
        {s + "linkSelf", "    node.next <- node"},
        {s + "setTail", "    tail <- node"},
        {"", "  else:"},
        {s + "linkNext", "    node.next <- tail.next        (the first node)"},
        {s + "linkTail", "    tail.next <- node"},
    };
    if (!front) {
        lines.push_back({s + "moveTail", "    tail <- node"});
    }
    lines.push_back({s + "done", "  return node"});
    return lines;
}

std::vector<OperationListing> circularListCatalog() {
    const std::string p = "circularList";
    return {
        {Operation::InsertFront, "O(1)", circularInsert(true)},
        {Operation::InsertBack, "O(1)", circularInsert(false)},
        {Operation::DeleteNode, "O(n)",
         {{p + ".deleteNode.enter", "deleteNode(id):"},
          {p + ".deleteNode.missing", "  if no node has this ID: reject NODE_NOT_FOUND"},
          {p + ".deleteNode.onlyNode", "  if node.next = node: tail <- NIL"},
          {"", "  else:"},
          {p + ".deleteNode.start", "    prev <- tail"},
          {p + ".deleteNode.walk", "    while prev.next is not node: prev <- prev.next"},
          {p + ".deleteNode.bypass", "    prev.next <- node.next"},
          {p + ".deleteNode.moveTail", "    if node = tail: tail <- prev"},
          {p + ".deleteNode.remove", "  delete node"},
          {p + ".deleteNode.done", "  return"}}},
        {Operation::Search, "O(n)",
         {{p + ".search.enter", "search(value):"},
          {"", "  if tail != NIL:"},
          {"", "    curr <- tail.next; index <- 0"},
          {"", "    repeat:"},
          {p + ".search.found", "      if curr.value = value: found"},
          {p + ".search.next", "      curr <- curr.next; index <- index + 1"},
          {"", "    until curr = tail.next        (a full circle: there is no NIL)"},
          {p + ".search.notFound", "  not found"},
          {p + ".search.done", "  return the result"}}},
        {Operation::Rotate, "O(1)",
         {{p + ".rotate.enter", "rotate():"},
          {p + ".rotate.moveTail", "  if tail != NIL: tail <- tail.next"},
          {p + ".rotate.done", "  return"}}},
        {Operation::Clear, "O(n)",
         {{p + ".clear.enter", "clear():"},
          {"", "  removed <- 0"},
          {"", "  while tail != NIL:"},
          {"", "    first <- tail.next"},
          {"", "    if first = tail: tail <- NIL  else: tail.next <- first.next"},
          {p + ".clear.remove", "    delete first"},
          {"", "    removed <- removed + 1"},
          {p + ".clear.done", "  return removed"}}},
    };
}

// The two heaps share their listings; only the comparison and the prefix differ.
std::vector<OperationListing> heapCatalog(bool isMin) {
    const std::string p = isMin ? "minHeap" : "maxHeap";
    const std::string above = isMin ? "<" : ">";
    const std::string pick = isMin ? "smaller" : "larger";
    return {
        {Operation::Insert, "O(log n)",
         {{p + ".insert.enter", "insert(value):"},
          {p + ".insert.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {p + ".insert.append", "  i <- size; A[i] <- new Node(value)"},
          {"", "  while i > 0:                    (sift up)"},
          {"", "    p <- (i - 1) / 2"},
          {p + ".insert.compare", "    if not A[i] " + above + " A[p]: stop"},
          {p + ".insert.swap", "    swap A[i] and A[p]; i <- p"},
          {p + ".insert.done", "  return the node"}}},
        {Operation::Extract, "O(log n)",
         {{p + ".extract.enter", "extract():"},
          {p + ".extract.empty", "  if size = 0: reject EMPTY_STRUCTURE"},
          {p + ".extract.take", "  top <- A[0]"},
          {p + ".extract.moveLast", "  if size > 1: swap A[0] and A[size - 1]"},
          {p + ".extract.remove", "  remove the last slot; delete top"},
          {"", "  i <- 0"},
          {"", "  while i has a child:            (sift down)"},
          {"", "    c <- the child of i with the " + pick + " value"},
          {p + ".extract.compare", "    if not A[c] " + above + " A[i]: stop"},
          {p + ".extract.swap", "    swap A[i] and A[c]; i <- c"},
          {p + ".extract.done", "  return top.value"}}},
        {Operation::Peek, "O(1)",
         {{p + ".peek.enter", "peek():"},
          {p + ".peek.empty", "  if size = 0: reject EMPTY_STRUCTURE"},
          {p + ".peek.read", "  value <- A[0].value"},
          {p + ".peek.done", "  return value"}}},
        {Operation::Clear, "O(n)",
         {{p + ".clear.enter", "clear():"},
          {"", "  removed <- 0"},
          {"", "  while size > 0:"},
          {p + ".clear.remove", "    remove the last slot and delete its node"},
          {"", "    removed <- removed + 1"},
          {p + ".clear.done", "  return removed"}}},
    };
}

// splay(x) as it appears in the listing of 'operation' ("insert", ...), then
// the two rotations it uses.
Lines splayLines(const std::string& operation) {
    const std::string s = "splay." + operation + ".";
    return {
        {"", "splay(x):                         rotate x up until it is the root"},
        {"", "  while x.parent != NIL:"},
        {"", "    p <- x.parent; g <- p.parent"},
        {s + "zig", "    if g = NIL: rotate at p                         (zig)"},
        {s + "zigZig", "    else if x, p are children on the same side: rotate at g, then at p   (zig-zig)"},
        {s + "zigZag", "    else: rotate at p, then at g                  (zig-zag)"},
        {"", "rotateLeft(x):                    x's right child y moves up"},
        {"", "  y <- x.right; x.right <- y.left; if y.left != NIL: y.left.parent <- x"},
        {"", "  y.parent <- x.parent; put y where x was (root, or x.parent's child)"},
        {"", "  y.left <- x; x.parent <- y"},
        {"", "rotateRight(x): the mirror image (left and right exchanged)"},
    };
}

std::vector<OperationListing> splayCatalog() {
    const std::string p = "splay";
    Lines insert = bstStyleInsert(p, "  node <- new Node(value); node.parent <- parent");
    insert.push_back({"", "  splay(node)"});
    insert.push_back({p + ".insert.done", "  return node"});
    const Lines search = {
        {p + ".search.enter", "search(value):"},
        {"", "  curr <- root; last <- NIL"},
        {"", "  while curr != NIL:"},
        {"", "    last <- curr"},
        {p + ".search.found", "    if value = curr.value: found; stop"},
        {p + ".search.goLeft", "    if value < curr.value: curr <- curr.left"},
        {p + ".search.goRight", "    else: curr <- curr.right"},
        {p + ".search.notFound", "  if curr = NIL: not found"},
        {"", "  if last != NIL: splay(last)"},
        {p + ".search.done", "  return the result"},
    };
    return {
        {Operation::Insert, "O(log n) amortised", join({insert, splayLines("insert")})},
        {Operation::Delete, "O(log n) amortised",
         join({bstStyleDelete(p, {{"", "  if z had a parent: splay(that parent)"}}), splayLines("delete")})},
        {Operation::Search, "O(log n) amortised", join({search, splayLines("search")})},
        {Operation::Preorder, "O(n)", traversal(p, Operation::Preorder)},
        {Operation::Inorder, "O(n)", traversal(p, Operation::Inorder)},
        {Operation::Postorder, "O(n)", traversal(p, Operation::Postorder)},
        {Operation::Levelorder, "O(n)", traversal(p, Operation::Levelorder)},
        {Operation::Clear, "O(n)", treeClear(p)},
    };
}

std::vector<OperationListing> hashTableCatalog() {
    const std::string p = "hashTable";
    return {
        {Operation::Insert, "O(1) average, O(n) worst",
         {{p + ".insert.enter", "insert(value):"},
          {p + ".insert.full", "  if size = 200: reject CAPACITY_EXCEEDED"},
          {p + ".insert.hash", "  b <- value mod 7"},
          {"", "  curr <- buckets[b]"},
          {"", "  while curr != NIL:"},
          {p + ".insert.duplicate", "    if curr.value = value: reject DUPLICATE_VALUE"},
          {p + ".insert.next", "    curr <- curr.next"},
          {p + ".insert.create", "  node <- new Node(value)"},
          {p + ".insert.linkNext", "  node.next <- buckets[b]"},
          {p + ".insert.linkBucket", "  buckets[b] <- node"},
          {p + ".insert.done", "  return node"}}},
        {Operation::Delete, "O(1) average, O(n) worst",
         {{p + ".delete.enter", "delete(value):"},
          {p + ".delete.hash", "  b <- value mod 7"},
          {"", "  prev <- NIL; curr <- buckets[b]"},
          {"", "  while curr != NIL and curr.value != value:"},
          {p + ".delete.next", "    prev <- curr; curr <- curr.next"},
          {p + ".delete.notFound", "  if curr = NIL: reject VALUE_NOT_FOUND"},
          {p + ".delete.found", "  curr holds the value"},
          {p + ".delete.unlinkHead", "  if prev = NIL: buckets[b] <- curr.next"},
          {p + ".delete.bypass", "  else: prev.next <- curr.next"},
          {p + ".delete.remove", "  delete curr"},
          {p + ".delete.done", "  return"}}},
        {Operation::Search, "O(1) average, O(n) worst",
         {{p + ".search.enter", "search(value):"},
          {p + ".search.hash", "  b <- value mod 7"},
          {"", "  curr <- buckets[b]"},
          {"", "  while curr != NIL:"},
          {p + ".search.found", "    if curr.value = value: found"},
          {p + ".search.next", "    curr <- curr.next"},
          {p + ".search.notFound", "  not found"},
          {p + ".search.done", "  return the result"}}},
        {Operation::Clear, "O(n)",
         {{p + ".clear.enter", "clear():"},
          {"", "  removed <- 0"},
          {"", "  for b <- 0 to 6:"},
          {"", "    while buckets[b] != NIL:"},
          {"", "      node <- buckets[b]; buckets[b] <- node.next"},
          {p + ".clear.remove", "      delete node"},
          {"", "      removed <- removed + 1"},
          {p + ".clear.done", "  return removed"}}},
    };
}

}  // namespace

const std::vector<OperationListing>& catalogFor(StructureKind structure) {
    // Built once, on first use.
    static const std::vector<OperationListing> stack = stackCatalog();
    static const std::vector<OperationListing> queue = queueCatalog();
    static const std::vector<OperationListing> list = listCatalog();
    static const std::vector<OperationListing> binaryTree = binaryTreeCatalog();
    static const std::vector<OperationListing> bst = bstCatalog();
    static const std::vector<OperationListing> avl = avlCatalog();
    static const std::vector<OperationListing> redBlack = redBlackCatalog();
    static const std::vector<OperationListing> deque = dequeCatalog();
    static const std::vector<OperationListing> doublyList = doublyListCatalog();
    static const std::vector<OperationListing> circularList = circularListCatalog();
    static const std::vector<OperationListing> minHeap = heapCatalog(true);
    static const std::vector<OperationListing> maxHeap = heapCatalog(false);
    static const std::vector<OperationListing> splay = splayCatalog();
    static const std::vector<OperationListing> hashTable = hashTableCatalog();
    switch (structure) {
        case StructureKind::Stack: return stack;
        case StructureKind::Queue: return queue;
        case StructureKind::List: return list;
        case StructureKind::BinaryTree: return binaryTree;
        case StructureKind::Bst: return bst;
        case StructureKind::Avl: return avl;
        case StructureKind::RedBlack: return redBlack;
        case StructureKind::Deque: return deque;
        case StructureKind::DoublyList: return doublyList;
        case StructureKind::CircularList: return circularList;
        case StructureKind::MinHeap: return minHeap;
        case StructureKind::MaxHeap: return maxHeap;
        case StructureKind::Splay: return splay;
        case StructureKind::HashTable: return hashTable;
    }
    return stack;
}

const OperationListing* findListing(StructureKind structure, Operation operation) {
    for (const OperationListing& listing : catalogFor(structure)) {
        if (listing.operation == operation) {
            return &listing;
        }
    }
    return nullptr;
}

}  // namespace dslab
