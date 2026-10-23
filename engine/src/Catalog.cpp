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
    switch (structure) {
        case StructureKind::Stack: return stack;
        case StructureKind::Queue: return queue;
        case StructureKind::List: return list;
        case StructureKind::BinaryTree: return binaryTree;
        case StructureKind::Bst: return bst;
        case StructureKind::Avl: return avl;
        case StructureKind::RedBlack: return redBlack;
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
