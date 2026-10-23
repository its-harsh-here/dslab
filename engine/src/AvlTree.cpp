#include "dslab/AvlTree.h"

#include <algorithm>
#include <string>

#include "dslab/Limits.h"

namespace dslab {

namespace {

std::string text(Value value) {
    return std::to_string(value);
}

// "+1", "0" or "-2": balances are shown with their sign.
std::string signedText(int number) {
    return number > 0 ? "+" + std::to_string(number) : std::to_string(number);
}

// "1 node" or "3 nodes".
std::string nodeCount(int count) {
    return std::to_string(count) + (count == 1 ? " node" : " nodes");
}

// "an empty AVL tree" or "an AVL tree that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty AVL tree" : "an AVL tree that holds " + nodeCount(size);
}

}  // namespace

// updateHeight(n):  n.height <- 1 + max(height(n.left), height(n.right))
void AvlTree::updateHeight(Node* node) {
    node->height = 1 + std::max(height(node->left), height(node->right));
}

std::string AvlTree::sentence(const std::string& text) const {
    std::string listed;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        const int b = balance(node.get());
        if (b > 1 || b < -1) {
            listed += (listed.empty() ? "" : ", ") + std::to_string(node->value) + " (" + signedText(b) + ")";
        }
    }
    if (listed.empty()) {
        return text + ".";
    }
    const std::string full = text + "; temporarily unbalanced: " + listed + ".";
    // Explanations are at most 140 characters; with very long values the list is dropped.
    return full.size() <= 140 ? full : text + "; temporarily unbalanced.";
}

// One step of the walk down: compare, then move to the left or right child.
//   if value < curr.value: curr <- curr.left        <operation>.goLeft
//   else:                  curr <- curr.right       <operation>.goRight
void AvlTree::stepDown(Value value, Node*& parent, Node*& curr, const char* operation, Trace& trace) const {
    const bool goLeft = value < curr->value;
    Node* child = goLeft ? curr->left : curr->right;  // non-owning: the pool owns every node
    const std::string words = text(value) + (goLeft ? " < " : " > ") + text(curr->value) + ", so move to the " +
                              (goLeft ? "left" : "right") + " child, " +
                              (child != nullptr ? text(child->value) : std::string("which is empty"));

    Highlight highlight{{curr->id}, {}};
    if (child != nullptr) {
        highlight.edges.push_back(Edge{curr->id, child->id});
        trace.counters.nodesVisited++;
    }
    parent = curr;
    curr = child;
    trace.frame(snapshot(), Event::Compare, std::string(operation) + (goLeft ? ".goLeft" : ".goRight"),
                sentence(words), highlight, {{"parent", parent->id}, {"curr", curr != nullptr ? curr->id : kNoNode}});
}

// insert(value): the BST's insert, then fixUp(parent).  (docs/avl.md, "Insert")
Result<NodeId> AvlTree::insert(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "avl.insert.enter",
                sentence("Insert " + text(value) + " into " + describe(size())));

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "avl.insert.full", ErrorCode::CapacityExceeded,
                              "The AVL tree already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "avl.insert.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this AVL tree.");
    }

    Node* parent = nullptr;  // non-owning: the pool owns every node
    Node* curr = root_;      // non-owning: the pool owns every node
    if (curr != nullptr) {
        trace.counters.nodesVisited++;
    }
    while (curr != nullptr) {
        trace.counters.comparisons++;
        if (value == curr->value) {
            return reject<NodeId>(trace, "avl.insert.duplicate", ErrorCode::DuplicateValue,
                                  text(value) + " is already in the AVL tree, and an AVL tree does not allow duplicates.");
        }
        stepDown(value, parent, curr, "avl.insert", trace);
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    node->height = 1;
    node->parent = parent;
    trace.counters.linksChanged++;
    const NodeId parentId = (parent != nullptr) ? parent->id : kNoNode;
    trace.frame(snapshot(), Event::Create, "avl.insert.create",
                sentence(parent != nullptr ? "Create a new node holding " + text(value) +
                                                 " with height 1, whose parent is " + text(parent->value)
                                           : "Create a new node holding " + text(value) + " with height 1; it has no parent"),
                {{node->id}, {}}, {{"parent", parentId}, {"node", node->id}});

    if (parent == nullptr) {
        root_ = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "avl.insert.linkRoot",
                    sentence("The tree was empty, so the new node becomes the root"), {{node->id}, {}},
                    {{"parent", parentId}, {"node", node->id}});
    } else {
        trace.counters.comparisons++;
        const bool left = value < parent->value;
        (left ? parent->left : parent->right) = node;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, left ? "avl.insert.linkLeft" : "avl.insert.linkRight",
                    sentence(text(value) + (left ? " < " : " > ") + text(parent->value) +
                             ", so attach the new node as the " + (left ? "left" : "right") + " child of " +
                             text(parent->value)),
                    {{parent->id, node->id}, {{parent->id, node->id}}}, {{"parent", parentId}, {"node", node->id}});
    }

    fixUp(parent, trace);

    trace.frame(snapshot(), Event::Done, "avl.insert.done",
                sentence("Inserted " + text(value) + "; the AVL tree now holds " + nodeCount(size())),
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// fixUp(n):                                       avl.fixUp.start
//   while n != NIL:
//     updateHeight(n)                             avl.fixUp.updateHeight
//     if balance(n) = +2 or balance(n) = -2:      avl.fixUp.unbalanced
//       n <- rebalance(n)
//     n <- n.parent
//   every balance on the path is -1, 0 or +1      avl.fixUp.done
void AvlTree::fixUp(Node* start, Trace& trace) {
    if (start == nullptr) {
        return;  // the root itself was added or removed: nothing above to fix
    }
    const Value from = start->value;
    trace.frame(snapshot(), Event::Move, "avl.fixUp.start",
                sentence("Walk back up from " + text(from) + " to the root, updating heights and checking balances"),
                {{start->id}, {}}, {{"n", start->id}});

    Node* n = start;  // non-owning: the pool owns every node
    while (n != nullptr) {
        trace.counters.nodesVisited++;
        const int before = n->height;
        updateHeight(n);
        const int b = balance(n);
        trace.frame(snapshot(), Event::Update, "avl.fixUp.updateHeight",
                    sentence("Height of " + text(n->value) + (n->height == before ? " stays " : " becomes ") +
                             std::to_string(n->height) + " (left " + std::to_string(height(n->left)) + ", right " +
                             std::to_string(height(n->right)) + "), balance " + signedText(b)),
                    {{n->id}, {}}, {{"n", n->id}});

        if (b == 2 || b == -2) {
            trace.frame(snapshot(), Event::Imbalance, "avl.fixUp.unbalanced",
                        sentence(text(n->value) + " has balance " + signedText(b) + ": its " +
                                 (b > 0 ? "left" : "right") + " side is two levels taller, so it must be rebalanced"),
                        {{n->id}, {}}, {{"n", n->id}});
            n = rebalance(n, trace);
        }
        n = n->parent;
    }

    trace.frame(snapshot(), Event::Balanced, "avl.fixUp.done",
                sentence("Rebalancing complete: every node from " + text(from) +
                         " up to the root has balance -1, 0 or +1"));
}

// rebalance(n):  returns the new root of n's subtree    (docs/avl.md, "rebalance")
//   balance +2: LL if balance(n.left) >= 0 else LR
//   balance -2: RR if balance(n.right) <= 0 else RL
AvlTree::Node* AvlTree::rebalance(Node* n, Trace& trace) {
    const bool leftHeavy = balance(n) > 0;
    Node* child = leftHeavy ? n->left : n->right;  // non-owning: the pool owns every node
    const int childBalance = balance(child);
    const std::string childWords = std::string(leftHeavy ? "Left" : "Right") + " child " + text(child->value) +
                                   " has balance " + signedText(childBalance);

    if (leftHeavy && childBalance >= 0) {
        trace.frame(snapshot(), Event::Imbalance, "avl.rebalance.LL",
                    sentence(childWords + ": LL case, one right rotation at " + text(n->value)),
                    {{n->id, child->id}, {{n->id, child->id}}}, {{"n", n->id}});
        return rotateRight(n, "LL case", trace);
    }
    if (leftHeavy) {
        trace.frame(snapshot(), Event::Imbalance, "avl.rebalance.LR",
                    sentence(childWords + ": LR case, left rotation at " + text(child->value) +
                             ", then right rotation at " + text(n->value)),
                    {{n->id, child->id}, {{n->id, child->id}}}, {{"n", n->id}});
        rotateLeft(child, "LR case, step 1", trace);
        return rotateRight(n, "LR case, step 2", trace);
    }
    if (childBalance <= 0) {
        trace.frame(snapshot(), Event::Imbalance, "avl.rebalance.RR",
                    sentence(childWords + ": RR case, one left rotation at " + text(n->value)),
                    {{n->id, child->id}, {{n->id, child->id}}}, {{"n", n->id}});
        return rotateLeft(n, "RR case", trace);
    }
    trace.frame(snapshot(), Event::Imbalance, "avl.rebalance.RL",
                sentence(childWords + ": RL case, right rotation at " + text(child->value) +
                         ", then left rotation at " + text(n->value)),
                {{n->id, child->id}, {{n->id, child->id}}}, {{"n", n->id}});
    rotateRight(child, "RL case, step 1", trace);
    return rotateLeft(n, "RL case, step 2", trace);
}

// rotateLeft(x):   x's right child y moves up; returns y     (docs/avl.md, "Rotations")
//   y <- x.right
//   x.right <- y.left; if y.left != NIL: y.left.parent <- x
//   y.parent <- x.parent; put y where x was (root, or x.parent's left/right)
//   y.left <- x; x.parent <- y
//   updateHeight(x); updateHeight(y)                  avl.rotateLeft.heights (one frame)
AvlTree::Node* AvlTree::rotateLeft(Node* x, const std::string& label, Trace& trace) {
    Node* y = x->right;       // non-owning: the pool owns every node
    Node* inner = y->left;    // non-owning: the pool owns every node
    Node* above = x->parent;  // non-owning: the pool owns every node

    x->right = inner;
    trace.counters.linksChanged++;
    if (inner != nullptr) {
        inner->parent = x;
        trace.counters.linksChanged++;
    }
    y->parent = above;
    if (above == nullptr) {
        root_ = y;
    } else if (above->left == x) {
        above->left = y;
    } else {
        above->right = y;
    }
    trace.counters.linksChanged += 2;
    y->left = x;
    x->parent = y;
    trace.counters.linksChanged += 2;
    updateHeight(x);
    updateHeight(y);
    trace.counters.rotations++;

    Highlight highlight{{x->id, y->id}, {{y->id, x->id}}};
    if (inner != nullptr) highlight.edges.push_back(Edge{x->id, inner->id});
    if (above != nullptr) highlight.edges.push_back(Edge{above->id, y->id});
    trace.frame(snapshot(), Event::Rotate, "avl.rotateLeft.heights",
                sentence(label + ": rotate left at " + text(x->value) + ", so " + text(y->value) + " moves up"),
                highlight, {{"x", x->id}, {"y", y->id}});
    return y;
}

// rotateRight(y):  mirror image: y's left child x moves up; returns x
//   x <- y.left
//   y.left <- x.right; if x.right != NIL: x.right.parent <- y
//   x.parent <- y.parent; put x where y was (root, or y.parent's left/right)
//   x.right <- y; y.parent <- x
//   updateHeight(y); updateHeight(x)                  avl.rotateRight.heights (one frame)
AvlTree::Node* AvlTree::rotateRight(Node* y, const std::string& label, Trace& trace) {
    Node* x = y->left;        // non-owning: the pool owns every node
    Node* inner = x->right;   // non-owning: the pool owns every node
    Node* above = y->parent;  // non-owning: the pool owns every node

    y->left = inner;
    trace.counters.linksChanged++;
    if (inner != nullptr) {
        inner->parent = y;
        trace.counters.linksChanged++;
    }
    x->parent = above;
    if (above == nullptr) {
        root_ = x;
    } else if (above->left == y) {
        above->left = x;
    } else {
        above->right = x;
    }
    trace.counters.linksChanged += 2;
    x->right = y;
    y->parent = x;
    trace.counters.linksChanged += 2;
    updateHeight(y);
    updateHeight(x);
    trace.counters.rotations++;

    Highlight highlight{{y->id, x->id}, {{x->id, y->id}}};
    if (inner != nullptr) highlight.edges.push_back(Edge{y->id, inner->id});
    if (above != nullptr) highlight.edges.push_back(Edge{above->id, x->id});
    trace.frame(snapshot(), Event::Rotate, "avl.rotateRight.heights",
                sentence(label + ": rotate right at " + text(y->value) + ", so " + text(x->value) + " moves up"),
                highlight, {{"x", x->id}, {"y", y->id}});
    return x;
}

// transplant(u, v):    (CLRS TRANSPLANT, as in the BST)
void AvlTree::transplant(Node* u, Node* v, Trace& trace) {
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

// delete(value): the BST's delete, remembering 'start' (the lowest node whose
// height may have changed), then fixUp(start).   (docs/avl.md, "Delete")
Result<NodeValue> AvlTree::remove(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "avl.delete.enter",
                sentence("Delete " + text(value) + " from " + describe(size())));

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
        stepDown(value, parent, z, "avl.delete", trace);
    }
    if (z == nullptr) {
        return reject<NodeValue>(trace, "avl.delete.notFound", ErrorCode::ValueNotFound,
                                 text(value) + " is not in the AVL tree, so there is nothing to delete.");
    }

    // Name the case before changing anything.
    if (z->left == nullptr && z->right == nullptr) {
        trace.frame(snapshot(), Event::Found, "avl.delete.caseLeaf",
                    sentence("Found " + text(value) + ". It has no children (case 1, a leaf): just unlink it"),
                    {{z->id}, {}}, {{"z", z->id}});
    } else if (z->left == nullptr || z->right == nullptr) {
        const Node* child = (z->left != nullptr) ? z->left : z->right;  // non-owning: the pool owns every node
        trace.frame(snapshot(), Event::Found, "avl.delete.caseOneChild",
                    sentence("Found " + text(value) + ". It has one child, " + text(child->value) +
                             " (case 2): that child moves up into its place"),
                    {{z->id, child->id}, {{z->id, child->id}}}, {{"z", z->id}});
    } else {
        trace.frame(snapshot(), Event::Found, "avl.delete.caseTwoChildren",
                    sentence("Found " + text(value) + ". It has two children (case 3): replace it with its in-order successor"),
                    {{z->id, z->left->id, z->right->id}, {}}, {{"z", z->id}});
    }

    Node* zParent = z->parent;  // non-owning: the pool owns every node
    // "Put 35 in 40's place: 30's right now points to 35" or "35 becomes the new root in place of 40".
    const auto newOccupant = [&](const Node* occupant) {
        if (zParent == nullptr) {
            return text(occupant->value) + " becomes the new root in place of " + text(value);
        }
        return "Put " + text(occupant->value) + " in " + text(value) + "'s place: " + text(zParent->value) + "'s " +
               (zParent->left == occupant ? "left" : "right") + " now points to " + text(occupant->value);
    };

    Node* start = nullptr;  // non-owning: the lowest node whose height may have changed
    if (z->left == nullptr || z->right == nullptr) {
        Node* child = (z->left != nullptr) ? z->left : z->right;  // non-owning: the pool owns every node
        const bool zWasLeft = (zParent != nullptr && zParent->left == z);
        start = zParent;
        transplant(z, child, trace);
        if (child == nullptr) {
            trace.frame(snapshot(), Event::Unlink, "avl.delete.replace",
                        sentence(zParent == nullptr ? text(value) + " was the root and a leaf, so root becomes NIL"
                                                    : "Set " + text(zParent->value) + "'s " + (zWasLeft ? "left" : "right") +
                                                          " link to NIL, so " + text(value) + " leaves the tree"),
                        {{z->id}, {}}, {{"z", z->id}});
        } else {
            Highlight highlight{{child->id, z->id}, {}};
            if (zParent != nullptr) {
                highlight.nodes.push_back(zParent->id);
                highlight.edges.push_back(Edge{zParent->id, child->id});
            }
            trace.frame(snapshot(), Event::Link, "avl.delete.replace", sentence(newOccupant(child)), highlight,
                        {{"z", z->id}});
        }
    } else {
        Node* y = z->right;  // non-owning: the pool owns every node
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "avl.delete.successorStart",
                    sentence("The in-order successor is the smallest value in " + text(value) +
                             "'s right subtree, so start at its right child, " + text(y->value)),
                    {{y->id}, {{z->id, y->id}}}, {{"z", z->id}, {"y", y->id}});
        while (y->left != nullptr) {
            const NodeId from = y->id;
            const Value fromValue = y->value;
            y = y->left;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "avl.delete.successorLeft",
                        sentence(text(fromValue) + " has a left child, so go left to " + text(y->value)),
                        {{y->id}, {{from, y->id}}}, {{"z", z->id}, {"y", y->id}});
        }
        trace.frame(snapshot(), Event::Found, "avl.delete.successorFound",
                    sentence(text(y->value) + " has no left child, so " + text(y->value) +
                             " is the in-order successor of " + text(value)),
                    {{y->id}, {}}, {{"z", z->id}, {"y", y->id}});

        if (y->parent != z) {
            Node* yParent = y->parent;  // non-owning: the pool owns every node
            Node* yRight = y->right;    // non-owning: the pool owns every node
            start = yParent;
            transplant(y, yRight, trace);
            if (yRight != nullptr) {
                trace.frame(snapshot(), Event::Link, "avl.delete.detachSuccessor",
                            sentence("Detach " + text(y->value) + ": its right child, " + text(yRight->value) +
                                     ", takes its place under " + text(yParent->value)),
                            {{y->id, yRight->id, yParent->id}, {{yParent->id, yRight->id}}},
                            {{"z", z->id}, {"y", y->id}});
            } else {
                trace.frame(snapshot(), Event::Unlink, "avl.delete.detachSuccessor",
                            sentence("Detach " + text(y->value) + ": " + text(yParent->value) + "'s left link becomes NIL"),
                            {{y->id, yParent->id}, {}}, {{"z", z->id}, {"y", y->id}});
            }

            y->right = z->right;
            y->right->parent = y;
            trace.counters.linksChanged += 2;
            trace.frame(snapshot(), Event::Link, "avl.delete.successorTakesRight",
                        sentence(text(y->value) + " takes over " + text(value) + "'s right subtree, starting at " +
                                 text(y->right->value)),
                        {{y->id, y->right->id}, {{y->id, y->right->id}}}, {{"z", z->id}, {"y", y->id}});
        } else {
            start = y;
        }

        transplant(z, y, trace);
        Highlight highlight{{y->id, z->id}, {}};
        if (zParent != nullptr) {
            highlight.nodes.push_back(zParent->id);
            highlight.edges.push_back(Edge{zParent->id, y->id});
        }
        trace.frame(snapshot(), Event::Link, "avl.delete.replaceWithSuccessor", sentence(newOccupant(y)), highlight,
                    {{"z", z->id}, {"y", y->id}});

        y->left = z->left;
        y->left->parent = y;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "avl.delete.successorTakesLeft",
                    sentence(text(y->value) + " takes over " + text(value) + "'s left subtree, starting at " +
                             text(y->left->value)),
                    {{y->id, y->left->id}, {{y->id, y->left->id}}}, {{"z", z->id}, {"y", y->id}});
    }

    const NodeValue removed{z->id, z->value};
    pool_.destroy(z);  // 'z' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "avl.delete.remove",
                sentence("Free the node that held " + text(removed.value)), {{removed.nodeId}, {}});

    fixUp(start, trace);

    trace.frame(snapshot(), Event::Done, "avl.delete.done",
                sentence(size() == 0 ? "Deleted " + text(value) + "; the AVL tree is now empty"
                                     : "Deleted " + text(value) + "; the AVL tree now holds " + nodeCount(size())));
    return Result<NodeValue>{okStatus(), removed};
}

// search(value): the BST's search; every frame highlights the path so far.
Result<BstSearchResult> AvlTree::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "avl.search.enter",
                sentence("Search " + describe(size()) + " for " + text(value)));

    Highlight path;
    BstSearchResult result;
    const Node* curr = root_;  // non-owning: the pool owns every node
    while (curr != nullptr) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        path.nodes.push_back(curr->id);
        if (value == curr->value) {
            result = BstSearchResult{true, curr->id};
            trace.frame(snapshot(), Event::Found, "avl.search.found",
                        sentence("Found " + text(value) + " after " + std::to_string(trace.counters.comparisons) +
                                 (trace.counters.comparisons == 1 ? " comparison" : " comparisons")),
                        path, {{"curr", curr->id}});
            break;
        }
        const bool goLeft = value < curr->value;
        const Node* child = goLeft ? curr->left : curr->right;  // non-owning: the pool owns every node
        if (child != nullptr) {
            path.edges.push_back(Edge{curr->id, child->id});
        }
        trace.frame(snapshot(), Event::Compare, goLeft ? "avl.search.goLeft" : "avl.search.goRight",
                    sentence(text(value) + (goLeft ? " < " : " > ") + text(curr->value) + ", so move to the " +
                             (goLeft ? "left" : "right") + " child, " +
                             (child != nullptr ? text(child->value) : std::string("which is empty"))),
                    path, {{"curr", child != nullptr ? child->id : kNoNode}});
        curr = child;
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "avl.search.done",
                    sentence("Search finished: " + text(value) + " is in the AVL tree; the highlighted path shows how"),
                    path);
    } else {
        trace.frame(snapshot(), Event::NotFound, "avl.search.notFound",
                    sentence("Reached an empty subtree: " + text(value) + " is not in the AVL tree"), path,
                    {{"curr", kNoNode}});
        trace.frame(snapshot(), Event::Done, "avl.search.done",
                    sentence("Search finished: " + text(value) + " is not in the AVL tree"), path);
    }
    return Result<BstSearchResult>{okStatus(), result};
}

Result<TraversalResult> AvlTree::traverse(TraversalOrder order, Trace& trace) const {
    TreeTraversal<AvlTree, Node> traversal(*this, "avl", "AVL tree", trace);
    return Result<TraversalResult>{okStatus(), traversal.run(order, root_, size())};
}

// freeSubtree(n):  post-order, children before their parent (as in the BST)
void AvlTree::freeSubtree(Node* node, int& removed, Trace& trace) {
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
    // Cut links make the remaining nodes' balances meaningless, so label them like any imbalance.
    trace.frame(snapshot(), Event::Remove, "avl.clear.remove",
                sentence("Free " + text(value) + "; its children, if it had any, were freed first"), {{id}, {}});
}

// clear():  cut root, then free the old tree in post-order.
Result<int> AvlTree::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "avl.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    if (root_ != nullptr) {
        Node* node = root_;  // non-owning: the pool owns every node
        root_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "avl.clear.cut",
                    "Set root to NIL; the old tree is freed bottom-up (post-order).", {{node->id}, {}},
                    {{"node", node->id}});
        freeSubtree(node, removed, trace);
    }

    trace.frame(snapshot(), Event::Done, "avl.clear.done",
                removed == 0 ? std::string("The AVL tree was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the AVL tree is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot AvlTree::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::Avl;
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
        view.height = node->height;
        view.balance = balance(node.get());
        copy.nodes.push_back(view);
    }
    return copy;
}

// restore(snapshot): create every node with its ID and value, then set the
// links. pool_.find(kNoNode) is nullptr, which is how NIL is stored here.
void AvlTree::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value);
    }
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.find(view.id);  // non-owning: the pool owns every node
        node->left = pool_.find(view.left);
        node->right = pool_.find(view.right);
        node->parent = pool_.find(view.parent);
        node->height = view.height;
    }
    root_ = pool_.find(snapshot.root);
}

}  // namespace dslab
