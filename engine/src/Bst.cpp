#include "dslab/Bst.h"

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

// "an empty BST" or "a BST that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty BST" : "a BST that holds " + nodeCount(size);
}

}  // namespace

// One step of the walk down: compare, then move to the left or right child.
//   if value < curr.value: curr <- curr.left        <operation>.goLeft
//   else:                  curr <- curr.right       <operation>.goRight
void Bst::stepDown(Value value, Node*& parent, Node*& curr, const char* operation, Trace& trace) const {
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

// insert(value):                                         bst.insert.enter
//   if size = 200: reject CAPACITY_EXCEEDED              bst.insert.full
//   parent <- NIL; curr <- root
//   while curr != NIL:
//     if value = curr.value: reject DUPLICATE_VALUE      bst.insert.duplicate
//     parent <- curr
//     if value < curr.value: curr <- curr.left           bst.insert.goLeft
//     else: curr <- curr.right                           bst.insert.goRight
//   node <- new Node(value); node.parent <- parent       bst.insert.create
//   if parent = NIL: root <- node                        bst.insert.linkRoot
//   else if value < parent.value: parent.left <- node    bst.insert.linkLeft
//   else: parent.right <- node                           bst.insert.linkRight
//   return node                                          bst.insert.done
Result<NodeId> Bst::insert(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "bst.insert.enter",
                "Insert " + text(value) + " into " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "bst.insert.full", ErrorCode::CapacityExceeded,
                              "The BST already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "bst.insert.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this BST.");
    }

    Node* parent = nullptr;  // non-owning: the pool owns every node
    Node* curr = root_;      // non-owning: the pool owns every node
    if (curr != nullptr) {
        trace.counters.nodesVisited++;
    }
    while (curr != nullptr) {
        trace.counters.comparisons++;
        if (value == curr->value) {
            return reject<NodeId>(trace, "bst.insert.duplicate", ErrorCode::DuplicateValue,
                                  text(value) + " is already in the BST, and a BST does not allow duplicates.");
        }
        stepDown(value, parent, curr, "bst.insert", trace);
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    node->parent = parent;
    trace.counters.linksChanged++;
    const NodeId parentId = (parent != nullptr) ? parent->id : kNoNode;
    trace.frame(snapshot(), Event::Create, "bst.insert.create",
                parent != nullptr
                    ? "Create a new node holding " + text(value) + " whose parent is " + text(parent->value) + "."
                    : "Create a new node holding " + text(value) + "; it has no parent.",
                {{node->id}, {}}, {{"parent", parentId}, {"node", node->id}});

    if (parent == nullptr) {
        root_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "bst.insert.linkRoot",
                    "The tree was empty, so the new node becomes the root.", {{node->id}, {}},
                    {{"parent", parentId}, {"node", node->id}});
    } else {
        trace.counters.comparisons++;
        const bool left = value < parent->value;
        (left ? parent->left : parent->right) = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, left ? "bst.insert.linkLeft" : "bst.insert.linkRight",
                    text(value) + (left ? " < " : " > ") + text(parent->value) +
                        ", so attach the new node as the " + (left ? "left" : "right") + " child of " +
                        text(parent->value) + ".",
                    {{parent->id, node->id}, {{parent->id, node->id}}},
                    {{"parent", parentId}, {"node", node->id}});
    }

    trace.frame(snapshot(), Event::Done, "bst.insert.done",
                "Inserted " + text(value) + "; the BST now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// transplant(u, v):    (CLRS TRANSPLANT)
//   if u.parent = NIL: root <- v
//   else if u = u.parent.left: u.parent.left <- v
//   else: u.parent.right <- v
//   if v != NIL: v.parent <- u.parent
void Bst::transplant(Node* u, Node* v, Trace& trace) {
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

// delete(value):                                         bst.delete.enter
//   z <- root
//   while z != NIL and value != z.value:                 bst.delete.goLeft / goRight
//     z <- (value < z.value) ? z.left : z.right
//   if z = NIL: reject VALUE_NOT_FOUND                   bst.delete.notFound
//   name the case                                        bst.delete.caseLeaf / caseOneChild / caseTwoChildren
//   if z.left = NIL: transplant(z, z.right)              bst.delete.replace
//   else if z.right = NIL: transplant(z, z.left)         bst.delete.replace
//   else:
//     y <- z.right                                       bst.delete.successorStart
//     while y.left != NIL: y <- y.left                   bst.delete.successorLeft
//                                                        bst.delete.successorFound
//     if y.parent != z:
//       transplant(y, y.right)                           bst.delete.detachSuccessor
//       y.right <- z.right; y.right.parent <- y          bst.delete.successorTakesRight
//     transplant(z, y)                                   bst.delete.replaceWithSuccessor
//     y.left <- z.left; y.left.parent <- y               bst.delete.successorTakesLeft
//   delete z                                             bst.delete.remove
//   return                                               bst.delete.done
Result<NodeValue> Bst::remove(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "bst.delete.enter",
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
        stepDown(value, parent, z, "bst.delete", trace);
    }
    if (z == nullptr) {
        return reject<NodeValue>(trace, "bst.delete.notFound", ErrorCode::ValueNotFound,
                                 text(value) + " is not in the BST, so there is nothing to delete.");
    }

    // Name the case before changing anything.
    if (z->left == nullptr && z->right == nullptr) {
        trace.frame(snapshot(), Event::Found, "bst.delete.caseLeaf",
                    "Found " + text(value) + ". It has no children (case 1, a leaf): just unlink it.",
                    {{z->id}, {}}, {{"z", z->id}});
    } else if (z->left == nullptr || z->right == nullptr) {
        const Node* child = (z->left != nullptr) ? z->left : z->right;  // non-owning: the pool owns every node
        trace.frame(snapshot(), Event::Found, "bst.delete.caseOneChild",
                    "Found " + text(value) + ". It has one child, " + text(child->value) +
                        " (case 2): that child moves up into its place.",
                    {{z->id, child->id}, {{z->id, child->id}}}, {{"z", z->id}});
    } else {
        trace.frame(snapshot(), Event::Found, "bst.delete.caseTwoChildren",
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
            trace.frame(snapshot(), Event::Unlink, "bst.delete.replace",
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
            trace.frame(snapshot(), Event::Link, "bst.delete.replace", newOccupant(child), highlight,
                        {{"z", z->id}});
        }
    } else {
        Node* y = z->right;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "bst.delete.successorStart",
                    "The in-order successor is the smallest value in " + text(value) +
                        "'s right subtree, so start at its right child, " + text(y->value) + ".",
                    {{y->id}, {{z->id, y->id}}}, {{"z", z->id}, {"y", y->id}});
        while (y->left != nullptr) {
            const NodeId from = y->id;
            const Value fromValue = y->value;
            y = y->left;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "bst.delete.successorLeft",
                        text(fromValue) + " has a left child, so go left to " + text(y->value) + ".",
                        {{y->id}, {{from, y->id}}}, {{"z", z->id}, {"y", y->id}});
        }
        trace.frame(snapshot(), Event::Found, "bst.delete.successorFound",
                    text(y->value) + " has no left child, so " + text(y->value) + " is the in-order successor of " +
                        text(value) + ".",
                    {{y->id}, {}}, {{"z", z->id}, {"y", y->id}});

        if (y->parent != z) {
            Node* yParent = y->parent;  // non-owning: the pool owns every node
            Node* yRight = y->right;    // non-owning: the pool owns every node
            transplant(y, yRight, trace);
            if (yRight != nullptr) {
                trace.frame(snapshot(), Event::Link, "bst.delete.detachSuccessor",
                            "Detach " + text(y->value) + ": its right child, " + text(yRight->value) +
                                ", takes its place under " + text(yParent->value) + ".",
                            {{y->id, yRight->id, yParent->id}, {{yParent->id, yRight->id}}},
                            {{"z", z->id}, {"y", y->id}});
            } else {
                trace.frame(snapshot(), Event::Unlink, "bst.delete.detachSuccessor",
                            "Detach " + text(y->value) + ": " + text(yParent->value) + "'s left link becomes NIL.",
                            {{y->id, yParent->id}, {}}, {{"z", z->id}, {"y", y->id}});
            }

            y->right = z->right;
            y->right->parent = y;
            trace.counters.linksChanged += 2;
            trace.frame(snapshot(), Event::Link, "bst.delete.successorTakesRight",
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
        trace.frame(snapshot(), Event::Link, "bst.delete.replaceWithSuccessor", newOccupant(y), highlight,
                    {{"z", z->id}, {"y", y->id}});

        y->left = z->left;
        y->left->parent = y;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "bst.delete.successorTakesLeft",
                    text(y->value) + " takes over " + text(value) + "'s left subtree, starting at " +
                        text(y->left->value) + ".",
                    {{y->id, y->left->id}, {{y->id, y->left->id}}}, {{"z", z->id}, {"y", y->id}});
    }

    const NodeValue removed{z->id, z->value};
    pool_.destroy(z);  // 'z' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "bst.delete.remove",
                "Free the node that held " + text(removed.value) + ".", {{removed.nodeId}, {}});

    trace.frame(snapshot(), Event::Done, "bst.delete.done",
                size() == 0 ? "Deleted " + text(value) + "; the BST is now empty."
                            : "Deleted " + text(value) + "; the BST now holds " + nodeCount(size()) + ".");
    return Result<NodeValue>{okStatus(), removed};
}

// search(value):                                         bst.search.enter
//   curr <- root
//   while curr != NIL:
//     if value = curr.value: return found                bst.search.found
//     if value < curr.value: curr <- curr.left           bst.search.goLeft
//     else: curr <- curr.right                           bst.search.goRight
//   return not found                                     bst.search.notFound
//                                                        bst.search.done
// Every frame highlights the whole path taken so far.
Result<BstSearchResult> Bst::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "bst.search.enter",
                "Search " + describe(size()) + " for " + text(value) + ".");

    Highlight path;
    BstSearchResult result;
    const Node* curr = root_;  // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        path.nodes.push_back(curr->id);
        if (value == curr->value) {
            result = BstSearchResult{true, curr->id};
            trace.frame(snapshot(), Event::Found, "bst.search.found",
                        "Found " + text(value) + " after " + std::to_string(trace.counters.comparisons) +
                            (trace.counters.comparisons == 1 ? " comparison." : " comparisons."),
                        path, {{"curr", curr->id}});
            break;
        }
        const bool goLeft = value < curr->value;
        const Node* child = goLeft ? curr->left : curr->right;  // non-owning: the pool owns every node
        if (child != nullptr) {
            path.edges.push_back(Edge{curr->id, child->id});
        }
        trace.frame(snapshot(), Event::Compare, goLeft ? "bst.search.goLeft" : "bst.search.goRight",
                    text(value) + (goLeft ? " < " : " > ") + text(curr->value) + ", so move to the " +
                        (goLeft ? "left" : "right") + " child, " +
                        (child != nullptr ? text(child->value) : std::string("which is empty")) + ".",
                    path, {{"curr", child != nullptr ? child->id : kNoNode}});
        curr = child;
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "bst.search.done",
                    "Search finished: " + text(value) + " is in the BST; the highlighted path shows how it was reached.",
                    path);
    } else {
        trace.frame(snapshot(), Event::NotFound, "bst.search.notFound",
                    "Reached an empty subtree: " + text(value) + " is not in the BST.", path, {{"curr", kNoNode}});
        trace.frame(snapshot(), Event::Done, "bst.search.done",
                    "Search finished: " + text(value) + " is not in the BST; the path shows where it would be.", path);
    }
    return Result<BstSearchResult>{okStatus(), result};
}

Result<TraversalResult> Bst::traverse(TraversalOrder order, Trace& trace) const {
    TreeTraversal<Bst, Node> traversal(*this, "bst", "BST", trace);
    return Result<TraversalResult>{okStatus(), traversal.run(order, root_, size())};
}

// freeSubtree(n):  post-order, so children are freed before their parent
//   if n.left != NIL:  child <- n.left;  n.left <- NIL;  freeSubtree(child)
//   if n.right != NIL: child <- n.right; n.right <- NIL; freeSubtree(child)
//   delete n                                             bst.clear.remove
void Bst::freeSubtree(Node* node, int& removed, Trace& trace) {
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
    trace.frame(snapshot(), Event::Remove, "bst.clear.remove",
                "Free " + text(value) + "; its children, if it had any, were freed first.", {{id}, {}});
}

// clear():                                               bst.clear.enter
//   if root != NIL: node <- root; root <- NIL            bst.clear.cut
//                   freeSubtree(node)                    bst.clear.remove
//   return the number freed                              bst.clear.done
Result<int> Bst::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "bst.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    if (root_ != nullptr) {
        Node* node = root_;  // non-owning: the pool owns every node
        root_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "bst.clear.cut",
                    "Set root to NIL; the old tree is freed bottom-up (post-order).", {{node->id}, {}},
                    {{"node", node->id}});
        freeSubtree(node, removed, trace);
    }

    trace.frame(snapshot(), Event::Done, "bst.clear.done",
                removed == 0 ? std::string("The BST was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the BST is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot Bst::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Bst;
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
void Bst::restore(const Snapshot& snapshot) {
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
