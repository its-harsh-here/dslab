#include "dslab/SplayTree.h"

#include <string>

#include "dslab/Limits.h"

namespace dslab {

namespace {

std::string text(Value value) {
    return std::to_string(value);
}

// "1 node" or "3 nodes".
std::string nodeCount(int count) {
    return std::to_string(count) + (count == 1 ? " node" : " nodes");
}

// "an empty splay tree" or "a splay tree that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty splay tree" : "a splay tree that holds " + nodeCount(size);
}

}  // namespace

// One step of the walk down: compare, then move to the left or right child.
//   if value < curr.value: curr <- curr.left        <operation>.goLeft
//   else:                  curr <- curr.right       <operation>.goRight
void SplayTree::stepDown(Value value, Node*& parent, Node*& curr, const char* operation, Trace& trace) const {
    const bool goLeft = value < curr->value;
    Node* child = goLeft ? curr->left : curr->right;  // non-owning: the pool owns every node
    const std::string sentence = text(value) + (goLeft ? " < " : " > ") + text(curr->value) +
                                 ", so move to the " + (goLeft ? "left" : "right") + " child, " +
                                 (child != nullptr ? text(child->value) : std::string("which is empty")) + ".";

    Highlight highlight{{curr->id}, {}};
    if (child != nullptr) {
        highlight.edges.push_back(Edge{curr->id, child->id});
        trace.counters.nodesVisited++;
    }
    parent = curr;
    curr = child;
    trace.frame(snapshot(), Event::Compare, std::string(operation) + (goLeft ? ".goLeft" : ".goRight"),
                sentence, highlight,
                {{"parent", parent->id}, {"curr", curr != nullptr ? curr->id : kNoNode}});
}

// rotateLeft(x):   x's right child y moves up, x becomes y's left child
//   y <- x.right
//   x.right <- y.left; if y.left != NIL: y.left.parent <- x
//   y.parent <- x.parent
//   if x.parent = NIL: root <- y
//   else if x = x.parent.left: x.parent.left <- y  else: x.parent.right <- y
//   y.left <- x; x.parent <- y
void SplayTree::rotateLeft(Node* x, const std::string& stepId, const std::string& label, const Node* splayed,
                           Trace& trace) {
    Node* y = x->right;  // non-owning: the pool owns every node
    x->right = y->left;
    trace.counters.linksChanged++;
    if (y->left != nullptr) {
        y->left->parent = x;
        trace.counters.linksChanged++;
    }
    y->parent = x->parent;
    if (x->parent == nullptr) {
        root_ = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
    trace.counters.linksChanged += 4;
    trace.counters.rotations++;
    trace.frame(snapshot(), Event::Rotate, stepId,
                label + ": rotate left at " + text(x->value) + ", so " + text(y->value) + " moves up and " +
                    text(x->value) + " becomes its left child.",
                {{y->id, x->id}, {{y->id, x->id}}}, {{"x", splayed->id}});
}

// rotateRight(x): the mirror image; x's left child y moves up.
void SplayTree::rotateRight(Node* x, const std::string& stepId, const std::string& label, const Node* splayed,
                            Trace& trace) {
    Node* y = x->left;  // non-owning: the pool owns every node
    x->left = y->right;
    trace.counters.linksChanged++;
    if (y->right != nullptr) {
        y->right->parent = x;
        trace.counters.linksChanged++;
    }
    y->parent = x->parent;
    if (x->parent == nullptr) {
        root_ = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->right = x;
    x->parent = y;
    trace.counters.linksChanged += 4;
    trace.counters.rotations++;
    trace.frame(snapshot(), Event::Rotate, stepId,
                label + ": rotate right at " + text(x->value) + ", so " + text(y->value) + " moves up and " +
                    text(x->value) + " becomes its right child.",
                {{y->id, x->id}, {{y->id, x->id}}}, {{"x", splayed->id}});
}

// splay(x):   rotate x up until it is the root
//   while x.parent != NIL:
//     p <- x.parent; g <- p.parent
//     if g = NIL: rotate at p                            <operation>.zig
//     else if x and p are children on the same side:
//       rotate at g, then at p                           <operation>.zigZig
//     else:
//       rotate at p, then at g                           <operation>.zigZag
// "Rotate at n" turns the edge to the child on x's side: a right rotation
// when that child is a left child, a left rotation otherwise.
void SplayTree::splay(Node* x, const std::string& operation, Trace& trace) {
    while (x->parent != nullptr) {
        Node* p = x->parent;  // non-owning: the pool owns every node
        Node* g = p->parent;  // non-owning: the pool owns every node
        const bool xIsLeft = (p->left == x);
        if (g == nullptr) {
            const std::string step = operation + ".zig";
            if (xIsLeft) {
                rotateRight(p, step, "Zig", x, trace);
            } else {
                rotateLeft(p, step, "Zig", x, trace);
            }
        } else if ((g->left == p) == xIsLeft) {
            const std::string step = operation + ".zigZig";
            if (xIsLeft) {
                rotateRight(g, step, "Zig-zig, step 1", x, trace);
                rotateRight(p, step, "Zig-zig, step 2", x, trace);
            } else {
                rotateLeft(g, step, "Zig-zig, step 1", x, trace);
                rotateLeft(p, step, "Zig-zig, step 2", x, trace);
            }
        } else {
            const std::string step = operation + ".zigZag";
            if (xIsLeft) {
                rotateRight(p, step, "Zig-zag, step 1", x, trace);
                rotateLeft(g, step, "Zig-zag, step 2", x, trace);
            } else {
                rotateLeft(p, step, "Zig-zag, step 1", x, trace);
                rotateRight(g, step, "Zig-zag, step 2", x, trace);
            }
        }
    }
}

// insert(value):                                         splay.insert.enter
//   if size = 200: reject CAPACITY_EXCEEDED              splay.insert.full
//   parent <- NIL; curr <- root
//   while curr != NIL:
//     if value = curr.value: reject DUPLICATE_VALUE      splay.insert.duplicate
//     parent <- curr
//     if value < curr.value: curr <- curr.left           splay.insert.goLeft
//     else: curr <- curr.right                           splay.insert.goRight
//   node <- new Node(value); node.parent <- parent       splay.insert.create
//   if parent = NIL: root <- node                        splay.insert.linkRoot
//   else if value < parent.value: parent.left <- node    splay.insert.linkLeft
//   else: parent.right <- node                           splay.insert.linkRight
//   splay(node)                                          splay.insert.zig / zigZig / zigZag
//   return node                                          splay.insert.done
Result<NodeId> SplayTree::insert(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "splay.insert.enter",
                "Insert " + text(value) + " into " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "splay.insert.full", ErrorCode::CapacityExceeded,
                              "The splay tree already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "splay.insert.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this splay tree.");
    }

    Node* parent = nullptr;  // non-owning: the pool owns every node
    Node* curr = root_;      // non-owning: the pool owns every node
    if (curr != nullptr) {
        trace.counters.nodesVisited++;
    }
    while (curr != nullptr) {
        trace.counters.comparisons++;
        if (value == curr->value) {
            return reject<NodeId>(trace, "splay.insert.duplicate", ErrorCode::DuplicateValue,
                                  text(value) + " is already in the splay tree, and a splay tree does not allow duplicates.");
        }
        stepDown(value, parent, curr, "splay.insert", trace);
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    node->parent = parent;
    trace.counters.linksChanged++;
    const NodeId parentId = (parent != nullptr) ? parent->id : kNoNode;
    trace.frame(snapshot(), Event::Create, "splay.insert.create",
                parent != nullptr
                    ? "Create a new node holding " + text(value) + " whose parent is " + text(parent->value) + "."
                    : "Create a new node holding " + text(value) + "; it has no parent.",
                {{node->id}, {}}, {{"parent", parentId}, {"node", node->id}});

    if (parent == nullptr) {
        root_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "splay.insert.linkRoot",
                    "The tree was empty, so the new node becomes the root.", {{node->id}, {}},
                    {{"parent", parentId}, {"node", node->id}});
    } else {
        trace.counters.comparisons++;
        const bool left = value < parent->value;
        (left ? parent->left : parent->right) = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, left ? "splay.insert.linkLeft" : "splay.insert.linkRight",
                    text(value) + (left ? " < " : " > ") + text(parent->value) +
                        ", so attach the new node as the " + (left ? "left" : "right") + " child of " +
                        text(parent->value) + ".",
                    {{parent->id, node->id}, {{parent->id, node->id}}},
                    {{"parent", parentId}, {"node", node->id}});
    }

    splay(node, "splay.insert", trace);

    trace.frame(snapshot(), Event::Done, "splay.insert.done",
                "Inserted " + text(value) + " and splayed it to the root; the tree now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// transplant(u, v):    (CLRS TRANSPLANT)
//   if u.parent = NIL: root <- v
//   else if u = u.parent.left: u.parent.left <- v
//   else: u.parent.right <- v
//   if v != NIL: v.parent <- u.parent
void SplayTree::transplant(Node* u, Node* v, Trace& trace) {
    if (u->parent == nullptr) {
        root_ = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    trace.counters.linksChanged++;
    if (v != nullptr) {
        v->parent = u->parent;
        trace.counters.linksChanged++;
    }
}

// delete(value):                                         splay.delete.enter
//   z <- root
//   while z != NIL and value != z.value:                 splay.delete.goLeft / goRight
//     z <- (value < z.value) ? z.left : z.right
//   if z = NIL: reject VALUE_NOT_FOUND                   splay.delete.notFound
//   name the case                                        splay.delete.caseLeaf / caseOneChild / caseTwoChildren
//   if z.left = NIL: transplant(z, z.right)              splay.delete.replace
//   else if z.right = NIL: transplant(z, z.left)         splay.delete.replace
//   else:
//     y <- z.right                                       splay.delete.successorStart
//     while y.left != NIL: y <- y.left                   splay.delete.successorLeft
//                                                        splay.delete.successorFound
//     if y.parent != z:
//       transplant(y, y.right)                           splay.delete.detachSuccessor
//       y.right <- z.right; y.right.parent <- y          splay.delete.successorTakesRight
//     transplant(z, y)                                   splay.delete.replaceWithSuccessor
//     y.left <- z.left; y.left.parent <- y               splay.delete.successorTakesLeft
//   delete z                                             splay.delete.remove
//   if z had a parent: splay(that parent)                splay.delete.zig / zigZig / zigZag
//   return                                               splay.delete.done
Result<NodeValue> SplayTree::remove(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "splay.delete.enter",
                "Delete " + text(value) + " from " + describe(size()) + ".");

    Node* parent = nullptr;  // non-owning: the pool owns every node
    Node* z = root_;         // non-owning: the pool owns every node
    if (z != nullptr) {
        trace.counters.nodesVisited++;
    }
    while (z != nullptr) {
        trace.counters.comparisons++;
        if (value == z->value) {
            break;
        }
        stepDown(value, parent, z, "splay.delete", trace);
    }
    if (z == nullptr) {
        return reject<NodeValue>(trace, "splay.delete.notFound", ErrorCode::ValueNotFound,
                                 text(value) + " is not in the splay tree, so there is nothing to delete.");
    }

    // Name the case before changing anything.
    if (z->left == nullptr && z->right == nullptr) {
        trace.frame(snapshot(), Event::Found, "splay.delete.caseLeaf",
                    "Found " + text(value) + ". It has no children (case 1, a leaf): just unlink it.",
                    {{z->id}, {}}, {{"z", z->id}});
    } else if (z->left == nullptr || z->right == nullptr) {
        const Node* child = (z->left != nullptr) ? z->left : z->right;  // non-owning: the pool owns every node
        trace.frame(snapshot(), Event::Found, "splay.delete.caseOneChild",
                    "Found " + text(value) + ". It has one child, " + text(child->value) +
                        " (case 2): that child moves up into its place.",
                    {{z->id, child->id}, {{z->id, child->id}}}, {{"z", z->id}});
    } else {
        trace.frame(snapshot(), Event::Found, "splay.delete.caseTwoChildren",
                    "Found " + text(value) + ". It has two children (case 3): replace it with its in-order successor.",
                    {{z->id, z->left->id, z->right->id}, {}}, {{"z", z->id}});
    }

    Node* zParent = z->parent;  // non-owning: the pool owns every node
    // Sentences for "u's place is now taken by v": "50's left now points to 35" or "35 becomes the new root".
    const auto newOccupant = [&](const Node* occupant) {
        if (zParent == nullptr) {
            return text(occupant->value) + " becomes the new root in place of " + text(value) + ".";
        }
        return std::string("Put ") + text(occupant->value) + " in " + text(value) + "'s place: " +
               text(zParent->value) + "'s " + (zParent->left == occupant ? "left" : "right") +
               " now points to " + text(occupant->value) + ".";
    };

    if (z->left == nullptr || z->right == nullptr) {
        Node* child = (z->left != nullptr) ? z->left : z->right;  // non-owning: the pool owns every node
        const bool zWasLeft = (zParent != nullptr && zParent->left == z);
        transplant(z, child, trace);
        if (child == nullptr) {
            trace.frame(snapshot(), Event::Unlink, "splay.delete.replace",
                        zParent == nullptr
                            ? text(value) + " was the root and a leaf, so root becomes NIL."
                            : "Set " + text(zParent->value) + "'s " + (zWasLeft ? "left" : "right") +
                                  " link to NIL, so " + text(value) + " leaves the tree.",
                        {{z->id}, {}}, {{"z", z->id}});
        } else {
            Highlight highlight{{child->id, z->id}, {}};
            if (zParent != nullptr) {
                highlight.nodes.push_back(zParent->id);
                highlight.edges.push_back(Edge{zParent->id, child->id});
            }
            trace.frame(snapshot(), Event::Link, "splay.delete.replace", newOccupant(child), highlight,
                        {{"z", z->id}});
        }
    } else {
        Node* y = z->right;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "splay.delete.successorStart",
                    "The in-order successor is the smallest value in " + text(value) +
                        "'s right subtree, so start at its right child, " + text(y->value) + ".",
                    {{y->id}, {{z->id, y->id}}}, {{"z", z->id}, {"y", y->id}});
        while (y->left != nullptr) {
            const NodeId from = y->id;
            const Value fromValue = y->value;
            y = y->left;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "splay.delete.successorLeft",
                        text(fromValue) + " has a left child, so go left to " + text(y->value) + ".",
                        {{y->id}, {{from, y->id}}}, {{"z", z->id}, {"y", y->id}});
        }
        trace.frame(snapshot(), Event::Found, "splay.delete.successorFound",
                    text(y->value) + " has no left child, so " + text(y->value) + " is the in-order successor of " +
                        text(value) + ".",
                    {{y->id}, {}}, {{"z", z->id}, {"y", y->id}});

        if (y->parent != z) {
            Node* yParent = y->parent;  // non-owning: the pool owns every node
            Node* yRight = y->right;    // non-owning: the pool owns every node
            transplant(y, yRight, trace);
            if (yRight != nullptr) {
                trace.frame(snapshot(), Event::Link, "splay.delete.detachSuccessor",
                            "Detach " + text(y->value) + ": its right child, " + text(yRight->value) +
                                ", takes its place under " + text(yParent->value) + ".",
                            {{y->id, yRight->id, yParent->id}, {{yParent->id, yRight->id}}},
                            {{"z", z->id}, {"y", y->id}});
            } else {
                trace.frame(snapshot(), Event::Unlink, "splay.delete.detachSuccessor",
                            "Detach " + text(y->value) + ": " + text(yParent->value) + "'s left link becomes NIL.",
                            {{y->id, yParent->id}, {}}, {{"z", z->id}, {"y", y->id}});
            }

            y->right = z->right;
            y->right->parent = y;
            trace.counters.linksChanged += 2;
            trace.frame(snapshot(), Event::Link, "splay.delete.successorTakesRight",
                        text(y->value) + " takes over " + text(value) + "'s right subtree, starting at " +
                            text(y->right->value) + ".",
                        {{y->id, y->right->id}, {{y->id, y->right->id}}}, {{"z", z->id}, {"y", y->id}});
        }

        transplant(z, y, trace);
        Highlight highlight{{y->id, z->id}, {}};
        if (zParent != nullptr) {
            highlight.nodes.push_back(zParent->id);
            highlight.edges.push_back(Edge{zParent->id, y->id});
        }
        trace.frame(snapshot(), Event::Link, "splay.delete.replaceWithSuccessor", newOccupant(y), highlight,
                    {{"z", z->id}, {"y", y->id}});

        y->left = z->left;
        y->left->parent = y;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "splay.delete.successorTakesLeft",
                    text(y->value) + " takes over " + text(value) + "'s left subtree, starting at " +
                        text(y->left->value) + ".",
                    {{y->id, y->left->id}, {{y->id, y->left->id}}}, {{"z", z->id}, {"y", y->id}});
    }

    const NodeValue removed{z->id, z->value};
    pool_.destroy(z);  // 'z' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "splay.delete.remove",
                "Free the node that held " + text(removed.value) + ".", {{removed.nodeId}, {}});

    // z is gone; its parent was the last node the search touched that is still here.
    if (zParent != nullptr) {
        splay(zParent, "splay.delete", trace);
    }

    trace.frame(snapshot(), Event::Done, "splay.delete.done",
                size() == 0 ? "Deleted " + text(value) + "; the splay tree is now empty."
                            : "Deleted " + text(value) + "; the splay tree now holds " + nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                         splay.search.enter
//   curr <- root; last <- NIL
//   while curr != NIL:
//     last <- curr
//     if value = curr.value: found; stop                 splay.search.found
//     if value < curr.value: curr <- curr.left           splay.search.goLeft
//     else: curr <- curr.right                           splay.search.goRight
//   if curr = NIL: not found                             splay.search.notFound
//   if last != NIL: splay(last)                          splay.search.zig / zigZig / zigZag
//   return                                               splay.search.done
// The node that is splayed is the one found, or after a miss the last node on
// the path, so a repeated search for the same value takes one comparison.
Result<BstSearchResult> SplayTree::search(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "splay.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ".");

    Highlight path;
    BstSearchResult result;
    Node* last = nullptr;  // non-owning: the last node the search stood on
    Node* curr = root_;    // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        path.nodes.push_back(curr->id);
        last = curr;
        if (value == curr->value) {
            result = BstSearchResult{true, curr->id};
            trace.frame(snapshot(), Event::Found, "splay.search.found",
                        "Found " + text(value) + " after " + std::to_string(trace.counters.comparisons) +
                            (trace.counters.comparisons == 1 ? " comparison." : " comparisons."),
                        path, {{"curr", curr->id}});
            break;
        }
        const bool goLeft = value < curr->value;
        Node* child = goLeft ? curr->left : curr->right;  // non-owning: the pool owns every node
        if (child != nullptr) {
            path.edges.push_back(Edge{curr->id, child->id});
        }
        trace.frame(snapshot(), Event::Compare, goLeft ? "splay.search.goLeft" : "splay.search.goRight",
                    text(value) + (goLeft ? " < " : " > ") + text(curr->value) + ", so move to the " +
                        (goLeft ? "left" : "right") + " child, " +
                        (child != nullptr ? text(child->value) : std::string("which is empty")) + ".",
                    path, {{"curr", child != nullptr ? child->id : kNoNode}});
        curr = child;
    }

    if (!result.found) {
        trace.frame(snapshot(), Event::NotFound, "splay.search.notFound",
                    last == nullptr ? "The splay tree is empty, so " + text(value) + " is not in it."
                                    : "Reached an empty subtree: " + text(value) + " is not in the tree; splay " +
                                          text(last->value) + ", the last node on the path.",
                    path, {{"curr", kNoNode}});
    }
    if (last != nullptr) {
        splay(last, "splay.search", trace);
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "splay.search.done",
                    "Search finished: " + text(value) + " was found and is now the root.", {{result.nodeId}, {}});
    } else {
        trace.frame(snapshot(), Event::Done, "splay.search.done",
                    last == nullptr ? "Search finished: " + text(value) + " is not in the empty tree."
                                    : "Search finished: " + text(value) + " is not in the tree; " +
                                          text(last->value) + " is now the root.",
                    last == nullptr ? Highlight{} : Highlight{{last->id}, {}});
    }
    return Result<BstSearchResult>{okStatus(), result};
}

Result<TraversalResult> SplayTree::traverse(TraversalOrder order, Trace& trace) const {
    TreeTraversal<SplayTree, Node> traversal(*this, "splay", "splay tree", trace);
    return Result<TraversalResult>{okStatus(), traversal.run(order, root_, size())};
}

// freeSubtree(n):  post-order, so children are freed before their parent
//   if n.left != NIL:  child <- n.left;  n.left <- NIL;  freeSubtree(child)
//   if n.right != NIL: child <- n.right; n.right <- NIL; freeSubtree(child)
//   delete n                                             splay.clear.remove
void SplayTree::freeSubtree(Node* node, int& removed, Trace& trace) {
    trace.counters.nodesVisited++;
    if (node->left != nullptr) {
        Node* child = node->left;  // non-owning: the pool owns every node
        node->left = nullptr;
        trace.counters.linksChanged++;
        freeSubtree(child, removed, trace);
    }
    if (node->right != nullptr) {
        Node* child = node->right;  // non-owning: the pool owns every node
        node->right = nullptr;
        trace.counters.linksChanged++;
        freeSubtree(child, removed, trace);
    }

    const NodeId id = node->id;
    const Value value = node->value;
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    removed = removed + 1;
    trace.frame(snapshot(), Event::Remove, "splay.clear.remove",
                "Free " + text(value) + "; its children, if it had any, were freed first.", {{id}, {}});
}

// clear():                                               splay.clear.enter
//   if root != NIL: node <- root; root <- NIL            splay.clear.cut
//                   freeSubtree(node)                    splay.clear.remove
//   return the number freed                              splay.clear.done
Result<int> SplayTree::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "splay.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    if (root_ != nullptr) {
        Node* node = root_;  // non-owning: the pool owns every node
        root_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "splay.clear.cut",
                    "Set root to NIL; the old tree is freed bottom-up (post-order).", {{node->id}, {}},
                    {{"node", node->id}});
        freeSubtree(node, removed, trace);
    }

    trace.frame(snapshot(), Event::Done, "splay.clear.done",
                removed == 0 ? std::string("The splay tree was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the splay tree is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot SplayTree::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Splay;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.root = (root_ != nullptr) ? root_->id : kNoNode;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        NodeView view;
        view.id = node->id;
        view.value = node->value;
        view.left = (node->left != nullptr) ? node->left->id : kNoNode;
        view.right = (node->right != nullptr) ? node->right->id : kNoNode;
        view.parent = (node->parent != nullptr) ? node->parent->id : kNoNode;
        copy.nodes.push_back(view);
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links. pool_.find(kNoNode) is nullptr, which is how NIL is stored here.
void SplayTree::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.find(view.id);  // non-owning: the pool owns every node
        node->left = pool_.find(view.left);
        node->right = pool_.find(view.right);
        node->parent = pool_.find(view.parent);
    }
    root_ = pool_.find(snapshot.root);
}

}  // namespace dslab
