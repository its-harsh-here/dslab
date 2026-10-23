#include "dslab/RedBlackTree.h"

#include <string>
#include <vector>

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

// "an empty red-black tree" or "a red-black tree that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty red-black tree" : "a red-black tree that holds " + nodeCount(size);
}

std::string colorName(Color color) {
    return color == Color::Red ? "red" : "black";
}

}  // namespace

// The tree starts empty: the root is the sentinel, and the sentinel is black.
// Its own links point back to itself, so they are never null.
RedBlackTree::RedBlackTree() {
    nil_.color = Color::Black;
    nil_.left = &nil_;
    nil_.right = &nil_;
    nil_.parent = &nil_;
    root_ = &nil_;
}

std::string RedBlackTree::nameOf(const Node* node) const {
    return node == &nil_ ? std::string("NIL") : text(node->value);
}

int RedBlackTree::blackHeight(const Node* node, std::string& problem) const {
    if (node == &nil_) {
        return 1;  // property 3: every NIL leaf is black
    }
    // Property 4. The sentinel is black, so its color can be read like any node's.
    if (problem.empty() && node->color == Color::Red) {
        if (node->left->color == Color::Red) {
            problem = "red " + text(node->left->value) + " under red " + text(node->value);
        } else if (node->right->color == Color::Red) {
            problem = "red " + text(node->right->value) + " under red " + text(node->value);
        }
    }
    // Property 5: both sides must have the same black height.
    const int left = blackHeight(node->left, problem);
    const int right = blackHeight(node->right, problem);
    if (left < 0 || right < 0) {
        return -1;
    }
    if (left != right) {
        if (problem.empty()) {
            problem = "black heights differ under " + text(node->value);
        }
        return -1;
    }
    return left + (node->color == Color::Black ? 1 : 0);
}

std::string RedBlackTree::violation() const {
    if (root_->color == Color::Red) {
        return "red root";  // property 2
    }
    std::string problem;
    blackHeight(root_, problem);
    return problem;
}

std::string RedBlackTree::sentence(const std::string& text) const {
    const std::string problem = violation();
    if (problem.empty()) {
        return text + ".";
    }
    const std::string full = text + "; temporary violation: " + problem + ".";
    // Explanations are at most 140 characters; with very long values the detail is dropped.
    return full.size() <= 140 ? full : text + "; temporary violation.";
}

// Sets node.color and records one frame (docs/red-black.md, "Frames and events").
// Only a real change counts as a recolor in the algorithm's counters.
void RedBlackTree::recolor(Node* node, Color color, const std::string& stepId, const std::string& words,
                           const std::vector<NamedPointer>& pointers, Trace& trace) {
    if (node->color != color) {
        trace.counters.recolors++;
    }
    node->color = color;
    Highlight highlight;
    if (node != &nil_) {
        highlight.nodes.push_back(node->id);
    }
    trace.frame(snapshot(), Event::Recolor, stepId, sentence(words), highlight, pointers);
}

// One step of the walk down: compare, then move to the left or right child.
//   if value < node.value: move to node.left       <operation>.goLeft
//   else:                  move to node.right      <operation>.goRight
RedBlackTree::Node* RedBlackTree::stepDown(Value value, Node* node, const std::string& operation, const char* cursor,
                                           Trace& trace) const {
    const bool goLeft = value < node->value;
    Node* child = goLeft ? node->left : node->right;  // non-owning: the pool owns every node
    const std::string words = text(value) + (goLeft ? " < " : " > ") + text(node->value) + ", so move to the " +
                              (goLeft ? "left" : "right") + " child, " +
                              (child != &nil_ ? text(child->value) : std::string("which is NIL"));

    Highlight highlight{{node->id}, {}};
    if (child != &nil_) {
        highlight.edges.push_back(Edge{node->id, child->id});
        trace.counters.nodesVisited++;
    }
    trace.frame(snapshot(), Event::Compare, operation + (goLeft ? ".goLeft" : ".goRight"), sentence(words), highlight,
                {{cursor, idOf(child)}});
    return child;
}

// insert(value):  CLRS RB-INSERT                  (docs/red-black.md, "Insert")
//   y <- NIL; x <- root
//   while x != NIL: y <- x; x <- x.left or x.right
//   z <- new red node; z.parent <- y; put z where x was (root, y.left or y.right)
//   insertFixup(z)
Result<NodeId> RedBlackTree::insert(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "redBlack.insert.enter",
                sentence("Insert " + text(value) + " into " + describe(size())));

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "redBlack.insert.full", ErrorCode::CapacityExceeded,
                              "The red-black tree already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "redBlack.insert.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this red-black tree.");
    }

    Node* y = &nil_;  // non-owning: the parent of the new node, once the walk ends
    Node* x = root_;  // non-owning: the pool owns every node
    if (x != &nil_) {
        trace.counters.nodesVisited++;
    }
    while (x != &nil_) {
        trace.counters.comparisons++;
        if (value == x->value) {
            return reject<NodeId>(trace, "redBlack.insert.duplicate", ErrorCode::DuplicateValue,
                                  text(value) + " is already in the red-black tree, and a red-black tree does not allow duplicates.");
        }
        y = x;
        x = stepDown(value, x, "redBlack.insert", "x", trace);
    }

    Node* z = pool_.create(value);  // non-owning: the pool owns every node
    z->color = Color::Red;
    z->left = &nil_;
    z->right = &nil_;
    z->parent = y;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Create, "redBlack.insert.create",
                sentence(y != &nil_ ? "Create a new red node holding " + text(value) + ", whose parent is " + text(y->value)
                                    : "Create a new red node holding " + text(value) + "; it has no parent"),
                {{z->id}, {}}, {{"y", idOf(y)}, {"z", z->id}});

    if (y == &nil_) {
        root_ = z;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, "redBlack.insert.linkRoot",
                    sentence("The tree was empty, so the new node becomes the root"), {{z->id}, {}},
                    {{"y", kNoNode}, {"z", z->id}});
    } else {
        trace.counters.comparisons++;
        const bool left = value < y->value;
        if (left) {
            y->left = z;
        } else {
            y->right = z;
        }
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Link, left ? "redBlack.insert.linkLeft" : "redBlack.insert.linkRight",
                    sentence(text(value) + (left ? " < " : " > ") + text(y->value) + ", so attach the new node as the " +
                             (left ? "left" : "right") + " child of " + text(y->value)),
                    {{y->id, z->id}, {{y->id, z->id}}}, {{"y", y->id}, {"z", z->id}});
    }

    const NodeId newId = z->id;
    insertFixup(z, trace);

    trace.frame(snapshot(), Event::Done, "redBlack.insert.done",
                sentence("Inserted " + text(value) + "; the red-black tree now holds " + nodeCount(size())),
                {{newId}, {}});
    return Result<NodeId>{okStatus(), newId};
}

// insertFixup(z):  CLRS RB-INSERT-FIXUP; y is z's uncle.   (docs/red-black.md, "Insert fix-up")
// The else branch is the mirror image of the first, with left and right exchanged.
void RedBlackTree::insertFixup(Node* z, Trace& trace) {
    // z, its parent, grandparent and uncle, and the links between them.
    const auto family = [this](const Node* node, const Node* uncle) {
        const Node* parent = node->parent;            // non-owning: the pool owns every node
        const Node* grandparent = parent->parent;     // non-owning: the pool owns every node
        Highlight highlight{{node->id, parent->id, grandparent->id},
                            {{grandparent->id, parent->id}, {parent->id, node->id}}};
        if (uncle != &nil_) {
            highlight.nodes.push_back(uncle->id);
            highlight.edges.push_back(Edge{grandparent->id, uncle->id});
        }
        return highlight;
    };

    const Node* firstParent = z->parent;  // non-owning: for the final sentence only
    bool fixed = false;
    while (z->parent->color == Color::Red) {
        fixed = true;
        Node* parent = z->parent;               // non-owning: the pool owns every node
        Node* grandparent = parent->parent;     // non-owning: the pool owns every node
        if (parent == grandparent->left) {
            Node* y = grandparent->right;  // non-owning: the uncle (may be NIL)
            if (y->color == Color::Red) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case1",
                            sentence("Insert case 1: the uncle " + text(y->value) + " is red, like the parent " +
                                     text(parent->value) + ", so recolor instead of rotating"),
                            family(z, y), {{"z", z->id}, {"y", y->id}});
                recolor(parent, Color::Black, "redBlack.insertFixup.case1Parent",
                        "Color the parent " + text(parent->value) + " black", {{"z", z->id}, {"y", y->id}}, trace);
                recolor(y, Color::Black, "redBlack.insertFixup.case1Uncle",
                        "Color the uncle " + text(y->value) + " black", {{"z", z->id}, {"y", y->id}}, trace);
                recolor(grandparent, Color::Red, "redBlack.insertFixup.case1Grandparent",
                        "Color the grandparent " + text(grandparent->value) + " red", {{"z", z->id}, {"y", y->id}}, trace);
                z = grandparent;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "redBlack.insertFixup.case1Up",
                            sentence("z moves up two levels to " + text(z->value) + "; the loop checks its parent next"),
                            {{z->id}, {}}, {{"z", z->id}});
            } else {
                if (z == parent->right) {
                    trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case2",
                                sentence("Insert case 2: uncle " + nameOf(y) + " is black, " + text(z->value) +
                                         " is an inner child: rotate left at its parent " + text(parent->value)),
                                family(z, y), {{"z", z->id}, {"y", idOf(y)}});
                    z = parent;
                    rotateLeft(z, "Insert case 2", {{"z", z->id}, {"y", idOf(y)}}, trace);
                }
                trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case3",
                            sentence("Insert case 3: uncle " + nameOf(y) + " is black, " + text(z->value) +
                                     " is an outer child: recolor and rotate right at " + text(z->parent->parent->value)),
                            family(z, y), {{"z", z->id}, {"y", idOf(y)}});
                recolor(z->parent, Color::Black, "redBlack.insertFixup.case3Parent",
                        "Color the parent " + text(z->parent->value) + " black", {{"z", z->id}, {"y", idOf(y)}}, trace);
                recolor(z->parent->parent, Color::Red, "redBlack.insertFixup.case3Grandparent",
                        "Color the grandparent " + text(z->parent->parent->value) + " red", {{"z", z->id}, {"y", idOf(y)}},
                        trace);
                rotateRight(z->parent->parent, "Insert case 3", {{"z", z->id}, {"y", idOf(y)}}, trace);
            }
        } else {
            Node* y = grandparent->left;  // non-owning: the uncle (may be NIL)
            if (y->color == Color::Red) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case1Mirror",
                            sentence("Insert case 1: the uncle " + text(y->value) + " is red, like the parent " +
                                     text(parent->value) + ", so recolor instead of rotating"),
                            family(z, y), {{"z", z->id}, {"y", y->id}});
                recolor(parent, Color::Black, "redBlack.insertFixup.case1ParentMirror",
                        "Color the parent " + text(parent->value) + " black", {{"z", z->id}, {"y", y->id}}, trace);
                recolor(y, Color::Black, "redBlack.insertFixup.case1UncleMirror",
                        "Color the uncle " + text(y->value) + " black", {{"z", z->id}, {"y", y->id}}, trace);
                recolor(grandparent, Color::Red, "redBlack.insertFixup.case1GrandparentMirror",
                        "Color the grandparent " + text(grandparent->value) + " red", {{"z", z->id}, {"y", y->id}}, trace);
                z = grandparent;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "redBlack.insertFixup.case1UpMirror",
                            sentence("z moves up two levels to " + text(z->value) + "; the loop checks its parent next"),
                            {{z->id}, {}}, {{"z", z->id}});
            } else {
                if (z == parent->left) {
                    trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case2Mirror",
                                sentence("Insert case 2: uncle " + nameOf(y) + " is black, " + text(z->value) +
                                         " is an inner child: rotate right at its parent " + text(parent->value)),
                                family(z, y), {{"z", z->id}, {"y", idOf(y)}});
                    z = parent;
                    rotateRight(z, "Insert case 2", {{"z", z->id}, {"y", idOf(y)}}, trace);
                }
                trace.frame(snapshot(), Event::FixupCase, "redBlack.insertFixup.case3Mirror",
                            sentence("Insert case 3: uncle " + nameOf(y) + " is black, " + text(z->value) +
                                     " is an outer child: recolor and rotate left at " + text(z->parent->parent->value)),
                            family(z, y), {{"z", z->id}, {"y", idOf(y)}});
                recolor(z->parent, Color::Black, "redBlack.insertFixup.case3ParentMirror",
                        "Color the parent " + text(z->parent->value) + " black", {{"z", z->id}, {"y", idOf(y)}}, trace);
                recolor(z->parent->parent, Color::Red, "redBlack.insertFixup.case3GrandparentMirror",
                        "Color the grandparent " + text(z->parent->parent->value) + " red", {{"z", z->id}, {"y", idOf(y)}},
                        trace);
                rotateLeft(z->parent->parent, "Insert case 3", {{"z", z->id}, {"y", idOf(y)}}, trace);
            }
        }
    }

    if (root_->color == Color::Red) {
        fixed = true;
        recolor(root_, Color::Black, "redBlack.insertFixup.blackRoot",
                "Color the root " + text(root_->value) + " black (property 2)", {{"z", z->id}}, trace);
    }
    trace.frame(snapshot(), Event::Balanced, "redBlack.insertFixup.done",
                sentence(fixed ? std::string("Fix-up finished: all five red-black properties hold again")
                               : "Nothing to fix: the parent " + text(firstParent->value) +
                                     " is black, so all five red-black properties hold"));
}

// rotateLeft(x):   x's right child y moves up      (CLRS LEFT-ROTATE)
//   y <- x.right
//   x.right <- y.left; if y.left != NIL: y.left.parent <- x
//   y.parent <- x.parent; put y where x was (root, or x.parent's left/right)
//   y.left <- x; x.parent <- y                        redBlack.rotateLeft.linkDown (one frame)
void RedBlackTree::rotateLeft(Node* x, const std::string& label, const std::vector<NamedPointer>& pointers,
                              Trace& trace) {
    Node* y = x->right;       // non-owning: the pool owns every node
    Node* inner = y->left;    // non-owning: the pool owns every node (or it is &nil_)
    Node* above = x->parent;  // non-owning: the pool owns every node (or it is &nil_)

    x->right = inner;
    trace.counters.linksChanged++;
    if (inner != &nil_) {
        inner->parent = x;
        trace.counters.linksChanged++;
    }
    y->parent = above;
    if (above == &nil_) {
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
    trace.counters.rotations++;

    Highlight highlight{{x->id, y->id}, {{y->id, x->id}}};
    if (inner != &nil_) highlight.edges.push_back(Edge{x->id, inner->id});
    if (above != &nil_) highlight.edges.push_back(Edge{above->id, y->id});
    trace.frame(snapshot(), Event::Rotate, "redBlack.rotateLeft.linkDown",
                sentence(label + ": rotate left at " + text(x->value) + ", so " + text(y->value) + " moves up"),
                highlight, pointers);
}

// rotateRight(x):  mirror image: x's left child y moves up   (CLRS RIGHT-ROTATE)
//   y <- x.left
//   x.left <- y.right; if y.right != NIL: y.right.parent <- x
//   y.parent <- x.parent; put y where x was (root, or x.parent's left/right)
//   y.right <- x; x.parent <- y                       redBlack.rotateRight.linkDown (one frame)
void RedBlackTree::rotateRight(Node* x, const std::string& label, const std::vector<NamedPointer>& pointers,
                               Trace& trace) {
    Node* y = x->left;        // non-owning: the pool owns every node
    Node* inner = y->right;   // non-owning: the pool owns every node (or it is &nil_)
    Node* above = x->parent;  // non-owning: the pool owns every node (or it is &nil_)

    x->left = inner;
    trace.counters.linksChanged++;
    if (inner != &nil_) {
        inner->parent = x;
        trace.counters.linksChanged++;
    }
    y->parent = above;
    if (above == &nil_) {
        root_ = y;
    } else if (above->left == x) {
        above->left = y;
    } else {
        above->right = y;
    }
    trace.counters.linksChanged += 2;
    y->right = x;
    x->parent = y;
    trace.counters.linksChanged += 2;
    trace.counters.rotations++;

    Highlight highlight{{x->id, y->id}, {{y->id, x->id}}};
    if (inner != &nil_) highlight.edges.push_back(Edge{x->id, inner->id});
    if (above != &nil_) highlight.edges.push_back(Edge{above->id, y->id});
    trace.frame(snapshot(), Event::Rotate, "redBlack.rotateRight.linkDown",
                sentence(label + ": rotate right at " + text(x->value) + ", so " + text(y->value) + " moves up"),
                highlight, pointers);
}

// transplant(u, v):    (CLRS RB-TRANSPLANT)
//   put v where u was (root, or u.parent's left/right); v.parent <- u.parent
// Unlike the BST's version it sets v.parent even when v is NIL: the delete
// fix-up uses it to find the parent of a NIL x.
void RedBlackTree::transplant(Node* u, Node* v, Trace& trace) {
    if (u->parent == &nil_) {
        root_ = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
    trace.counters.linksChanged += 2;
}

// delete(value):  CLRS RB-DELETE                  (docs/red-black.md, "Delete")
// y is the node that really leaves its position (z itself, or z's successor);
// x is the node that moves into y's old position (possibly NIL).
Result<NodeValue> RedBlackTree::remove(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "redBlack.delete.enter",
                sentence("Delete " + text(value) + " from " + describe(size())));

    Node* z = root_;  // non-owning: the pool owns every node
    if (z != &nil_) {
        trace.counters.nodesVisited++;
    }
    while (z != &nil_) {
        trace.counters.comparisons++;
        if (value == z->value) {
            break;
        }
        z = stepDown(value, z, "redBlack.delete", "z", trace);
    }
    if (z == &nil_) {
        return reject<NodeValue>(trace, "redBlack.delete.notFound", ErrorCode::ValueNotFound,
                                 text(value) + " is not in the red-black tree, so there is nothing to delete.");
    }

    // Name the shape before changing anything.
    const std::string found = "Found " + colorName(z->color) + " " + text(value);
    if (z->left == &nil_ && z->right == &nil_) {
        trace.frame(snapshot(), Event::Found, "redBlack.delete.found",
                    sentence(found + ". It has no children, so NIL takes its place"), {{z->id}, {}}, {{"z", z->id}});
    } else if (z->left == &nil_ || z->right == &nil_) {
        const Node* child = (z->left != &nil_) ? z->left : z->right;  // non-owning: the pool owns every node
        trace.frame(snapshot(), Event::Found, "redBlack.delete.found",
                    sentence(found + ". It has one child, " + text(child->value) + ", which takes its place"),
                    {{z->id, child->id}, {{z->id, child->id}}}, {{"z", z->id}});
    } else {
        trace.frame(snapshot(), Event::Found, "redBlack.delete.found",
                    sentence(found + ". It has two children, so its in-order successor takes its place"),
                    {{z->id, z->left->id, z->right->id}, {}}, {{"z", z->id}});
    }

    Node* zParent = z->parent;  // non-owning: the pool owns every node (or it is &nil_)
    const bool zWasLeft = (zParent != &nil_ && zParent->left == z);
    // "Put 35 in 40's place: 30's right now points to 35" or "35 becomes the new root in place of 40".
    const auto newOccupant = [&](const Node* occupant) {
        if (zParent == &nil_) {
            return text(occupant->value) + " becomes the new root in place of " + text(value);
        }
        return "Put " + text(occupant->value) + " in " + text(value) + "'s place: " + text(zParent->value) + "'s " +
               (zWasLeft ? "left" : "right") + " now points to " + text(occupant->value);
    };

    Node* y = z;                            // non-owning: the pool owns every node
    Color yOriginalColor = y->color;
    Node* x = &nil_;                        // non-owning: the pool owns every node (or it is &nil_)
    if (z->left == &nil_ || z->right == &nil_) {
        const bool replaceWithRight = (z->left == &nil_);
        x = replaceWithRight ? z->right : z->left;
        transplant(z, x, trace);
        Highlight highlight{{z->id}, {}};
        if (x != &nil_) highlight.nodes.push_back(x->id);
        if (zParent != &nil_) highlight.nodes.push_back(zParent->id);
        if (zParent != &nil_ && x != &nil_) highlight.edges.push_back(Edge{zParent->id, x->id});
        std::string words;
        if (x != &nil_) {
            words = newOccupant(x);
        } else if (zParent == &nil_) {
            words = text(value) + " was the root and a leaf, so root becomes NIL";
        } else {
            words = "Set " + text(zParent->value) + "'s " + (zWasLeft ? "left" : "right") + " link to NIL, so " +
                    text(value) + " leaves the tree";
        }
        trace.frame(snapshot(), x == &nil_ ? Event::Unlink : Event::Link,
                    replaceWithRight ? "redBlack.delete.replaceWithRight" : "redBlack.delete.replaceWithLeft",
                    sentence(words), highlight, {{"z", z->id}, {"x", idOf(x)}});
    } else {
        y = z->right;
        trace.counters.nodesVisited++;
        trace.frame(snapshot(), Event::Move, "redBlack.delete.successorStart",
                    sentence("The in-order successor is the smallest value in " + text(value) +
                             "'s right subtree, so start at its right child, " + text(y->value)),
                    {{y->id}, {{z->id, y->id}}}, {{"z", z->id}, {"y", y->id}});
        while (y->left != &nil_) {
            const NodeId from = y->id;
            const Value fromValue = y->value;
            y = y->left;
            trace.counters.nodesVisited++;
            trace.frame(snapshot(), Event::Move, "redBlack.delete.successorLeft",
                        sentence(text(fromValue) + " has a left child, so go left to " + text(y->value)),
                        {{y->id}, {{from, y->id}}}, {{"z", z->id}, {"y", y->id}});
        }
        yOriginalColor = y->color;
        x = y->right;
        trace.frame(snapshot(), Event::Found, "redBlack.delete.successorFound",
                    sentence(text(y->value) + " has no left child, so " + colorName(y->color) + " " + text(y->value) +
                             " is the in-order successor of " + text(value)),
                    {{y->id}, {}}, {{"z", z->id}, {"y", y->id}, {"x", idOf(x)}});

        if (y->parent == z) {
            x->parent = y;  // when x is NIL this sets nil.parent, which the fix-up reads (CLRS)
            trace.counters.linksChanged++;
        } else {
            Node* yParent = y->parent;  // non-owning: the pool owns every node
            transplant(y, y->right, trace);
            if (x != &nil_) {
                trace.frame(snapshot(), Event::Link, "redBlack.delete.detachSuccessor",
                            sentence("Detach " + text(y->value) + ": its right child, " + text(x->value) +
                                     ", takes its place under " + text(yParent->value)),
                            {{y->id, x->id, yParent->id}, {{yParent->id, x->id}}},
                            {{"z", z->id}, {"y", y->id}, {"x", x->id}});
            } else {
                trace.frame(snapshot(), Event::Unlink, "redBlack.delete.detachSuccessor",
                            sentence("Detach " + text(y->value) + ": " + text(yParent->value) + "'s left link becomes NIL"),
                            {{y->id, yParent->id}, {}}, {{"z", z->id}, {"y", y->id}, {"x", kNoNode}});
            }

            y->right = z->right;
            y->right->parent = y;
            trace.counters.linksChanged += 2;
            trace.frame(snapshot(), Event::Link, "redBlack.delete.successorTakesRight",
                        sentence(text(y->value) + " takes over " + text(value) + "'s right subtree, starting at " +
                                 text(y->right->value)),
                        {{y->id, y->right->id}, {{y->id, y->right->id}}}, {{"z", z->id}, {"y", y->id}, {"x", idOf(x)}});
        }

        transplant(z, y, trace);
        Highlight highlight{{y->id, z->id}, {}};
        if (zParent != &nil_) {
            highlight.nodes.push_back(zParent->id);
            highlight.edges.push_back(Edge{zParent->id, y->id});
        }
        trace.frame(snapshot(), Event::Link, "redBlack.delete.replaceWithSuccessor", sentence(newOccupant(y)), highlight,
                    {{"z", z->id}, {"y", y->id}, {"x", idOf(x)}});

        y->left = z->left;
        y->left->parent = y;
        trace.counters.linksChanged += 2;
        trace.frame(snapshot(), Event::Link, "redBlack.delete.successorTakesLeft",
                    sentence(text(y->value) + " takes over " + text(value) + "'s left subtree, starting at " +
                             text(y->left->value)),
                    {{y->id, y->left->id}, {{y->id, y->left->id}}}, {{"z", z->id}, {"y", y->id}, {"x", idOf(x)}});

        recolor(y, z->color, "redBlack.delete.successorTakesColor",
                text(y->value) + " takes over " + text(value) + "'s color, " + colorName(z->color),
                {{"z", z->id}, {"y", y->id}, {"x", idOf(x)}}, trace);
    }

    // What the extra-black sentence needs, read before z is freed.
    const bool successorMoved = (y != z);
    const Value yValue = y->value;
    const NodeValue removed{z->id, z->value};
    pool_.destroy(z);  // 'z' dangles from here on and is not used again
    trace.frame(snapshot(), Event::Remove, "redBlack.delete.remove",
                sentence("Free the node that held " + text(removed.value)), {{removed.nodeId}, {}}, {{"x", idOf(x)}});

    Highlight atX;
    if (x != &nil_) atX.nodes.push_back(x->id);
    const std::string leaver = successorMoved ? "The successor " + text(yValue) : "The removed node " + text(yValue);
    if (yOriginalColor == Color::Black) {
        trace.frame(snapshot(), Event::Move, "redBlack.delete.extraBlack",
                    sentence(leaver + " was black, so x = " + nameOf(x) + " carries an extra black (" +
                             (x->color == Color::Red ? "red-and-black)" : "doubly black)")),
                    atX, {{"x", idOf(x)}});
        deleteFixup(x, trace);
    } else {
        trace.frame(snapshot(), Event::Balanced, "redBlack.delete.extraBlack",
                    sentence(leaver + " was red, so no path lost a black node: nothing to fix"), atX, {{"x", idOf(x)}});
    }

    trace.frame(snapshot(), Event::Done, "redBlack.delete.done",
                sentence(size() == 0 ? "Deleted " + text(value) + "; the red-black tree is now empty"
                                     : "Deleted " + text(value) + "; the red-black tree now holds " + nodeCount(size())));
    return Result<NodeValue>{okStatus(), removed};
}

// deleteFixup(x):  CLRS RB-DELETE-FIXUP; w is x's sibling.   (docs/red-black.md, "Delete fix-up")
// x carries the extra black: bookkeeping, not a third color. The else branch
// is the mirror image of the first, with left and right exchanged.
void RedBlackTree::deleteFixup(Node* x, Trace& trace) {
    // x (unless it is NIL), its parent and its sibling w, and the links to them.
    const auto family = [this](const Node* node, const Node* parent, const Node* w) {
        Highlight highlight{{parent->id, w->id}, {{parent->id, w->id}}};
        if (node != &nil_) {
            highlight.nodes.push_back(node->id);
            highlight.edges.push_back(Edge{parent->id, node->id});
        }
        return highlight;
    };

    bool usedUp = false;  // true once case 4 has absorbed the extra black
    while (x != root_ && x->color == Color::Black) {
        Node* parent = x->parent;  // non-owning: the pool owns every node
        if (x == parent->left) {
            Node* w = parent->right;  // non-owning: the sibling
            if (w->color == Color::Red) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case1",
                            sentence("Delete case 1: sibling " + text(w->value) +
                                     " is red; recolor and rotate left so x gets a black sibling"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, Color::Black, "redBlack.deleteFixup.case1Sibling",
                        "Color the sibling " + text(w->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(parent, Color::Red, "redBlack.deleteFixup.case1Parent",
                        "Color the parent " + text(parent->value) + " red", {{"x", idOf(x)}, {"w", w->id}}, trace);
                rotateLeft(parent, "Delete case 1", {{"x", idOf(x)}, {"w", w->id}}, trace);
                w = parent->right;
            }
            if (w->left->color == Color::Black && w->right->color == Color::Black) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case2",
                            sentence("Delete case 2: the sibling " + text(w->value) + " and both its children are black"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, Color::Red, "redBlack.deleteFixup.case2Sibling",
                        "Color the sibling " + text(w->value) + " red: x's side and the sibling's side each lose one black",
                        {{"x", idOf(x)}, {"w", w->id}}, trace);
                x = parent;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "redBlack.deleteFixup.case2Up",
                            sentence("The extra black moves up to the parent " + text(x->value) + ", which becomes x"),
                            {{x->id}, {}}, {{"x", x->id}});
            } else {
                if (w->right->color == Color::Black) {
                    trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case3",
                                sentence("Delete case 3: sibling " + text(w->value) + " is black, its near child " +
                                         text(w->left->value) + " red, its far child black"),
                                family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                    recolor(w->left, Color::Black, "redBlack.deleteFixup.case3Near",
                            "Color the near child " + text(w->left->value) + " black", {{"x", idOf(x)}, {"w", w->id}},
                            trace);
                    recolor(w, Color::Red, "redBlack.deleteFixup.case3Sibling",
                            "Color the sibling " + text(w->value) + " red", {{"x", idOf(x)}, {"w", w->id}}, trace);
                    rotateRight(w, "Delete case 3", {{"x", idOf(x)}, {"w", w->id}}, trace);
                    w = parent->right;
                }
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case4",
                            sentence("Delete case 4: the sibling " + text(w->value) + " is black and its far child " +
                                     text(w->right->value) + " is red"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, parent->color, "redBlack.deleteFixup.case4Sibling",
                        "The sibling " + text(w->value) + " takes the parent " + text(parent->value) + "'s color, " +
                            colorName(parent->color),
                        {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(parent, Color::Black, "redBlack.deleteFixup.case4Parent",
                        "Color the parent " + text(parent->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(w->right, Color::Black, "redBlack.deleteFixup.case4Far",
                        "Color the far child " + text(w->right->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                rotateLeft(parent, "Delete case 4", {{"x", idOf(x)}, {"w", w->id}}, trace);
                x = root_;
                usedUp = true;
            }
        } else {
            Node* w = parent->left;  // non-owning: the sibling
            if (w->color == Color::Red) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case1Mirror",
                            sentence("Delete case 1: sibling " + text(w->value) +
                                     " is red; recolor and rotate right so x gets a black sibling"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, Color::Black, "redBlack.deleteFixup.case1SiblingMirror",
                        "Color the sibling " + text(w->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(parent, Color::Red, "redBlack.deleteFixup.case1ParentMirror",
                        "Color the parent " + text(parent->value) + " red", {{"x", idOf(x)}, {"w", w->id}}, trace);
                rotateRight(parent, "Delete case 1", {{"x", idOf(x)}, {"w", w->id}}, trace);
                w = parent->left;
            }
            if (w->right->color == Color::Black && w->left->color == Color::Black) {
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case2Mirror",
                            sentence("Delete case 2: the sibling " + text(w->value) + " and both its children are black"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, Color::Red, "redBlack.deleteFixup.case2SiblingMirror",
                        "Color the sibling " + text(w->value) + " red: x's side and the sibling's side each lose one black",
                        {{"x", idOf(x)}, {"w", w->id}}, trace);
                x = parent;
                trace.counters.nodesVisited++;
                trace.frame(snapshot(), Event::Move, "redBlack.deleteFixup.case2UpMirror",
                            sentence("The extra black moves up to the parent " + text(x->value) + ", which becomes x"),
                            {{x->id}, {}}, {{"x", x->id}});
            } else {
                if (w->left->color == Color::Black) {
                    trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case3Mirror",
                                sentence("Delete case 3: sibling " + text(w->value) + " is black, its near child " +
                                         text(w->right->value) + " red, its far child black"),
                                family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                    recolor(w->right, Color::Black, "redBlack.deleteFixup.case3NearMirror",
                            "Color the near child " + text(w->right->value) + " black", {{"x", idOf(x)}, {"w", w->id}},
                            trace);
                    recolor(w, Color::Red, "redBlack.deleteFixup.case3SiblingMirror",
                            "Color the sibling " + text(w->value) + " red", {{"x", idOf(x)}, {"w", w->id}}, trace);
                    rotateLeft(w, "Delete case 3", {{"x", idOf(x)}, {"w", w->id}}, trace);
                    w = parent->left;
                }
                trace.frame(snapshot(), Event::FixupCase, "redBlack.deleteFixup.case4Mirror",
                            sentence("Delete case 4: the sibling " + text(w->value) + " is black and its far child " +
                                     text(w->left->value) + " is red"),
                            family(x, parent, w), {{"x", idOf(x)}, {"w", w->id}});
                recolor(w, parent->color, "redBlack.deleteFixup.case4SiblingMirror",
                        "The sibling " + text(w->value) + " takes the parent " + text(parent->value) + "'s color, " +
                            colorName(parent->color),
                        {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(parent, Color::Black, "redBlack.deleteFixup.case4ParentMirror",
                        "Color the parent " + text(parent->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                recolor(w->left, Color::Black, "redBlack.deleteFixup.case4FarMirror",
                        "Color the far child " + text(w->left->value) + " black", {{"x", idOf(x)}, {"w", w->id}}, trace);
                rotateRight(parent, "Delete case 4", {{"x", idOf(x)}, {"w", w->id}}, trace);
                x = root_;
                usedUp = true;
            }
        }
    }

    std::string words;
    if (usedUp) {
        words = "Case 4 used up the extra black; x is set to the root " + nameOf(x) + ", which is colored black";
    } else if (x == root_) {
        words = "x = " + nameOf(x) + " is the root, so the extra black is simply dropped; color it black";
    } else {
        words = "x = " + nameOf(x) + " is red-and-black: color it black, which uses up the extra black";
    }
    recolor(x, Color::Black, "redBlack.deleteFixup.blacken", words, {{"x", idOf(x)}}, trace);
    trace.frame(snapshot(), Event::Balanced, "redBlack.deleteFixup.done",
                sentence("Fix-up finished: all five red-black properties hold again"));
}

// search(value): the BST's search; every frame highlights the path so far.
Result<BstSearchResult> RedBlackTree::search(Value value, Trace& trace) const {
    trace.frame(snapshot(), Event::Begin, "redBlack.search.enter",
                sentence("Search " + describe(size()) + " for " + text(value)));

    Highlight path;
    BstSearchResult result;
    const Node* x = root_;  // non-owning: the pool owns every node (or it is &nil_)
    while (x != &nil_) {
        trace.counters.nodesVisited++;
        trace.counters.comparisons++;
        path.nodes.push_back(x->id);
        if (value == x->value) {
            result = BstSearchResult{true, x->id};
            trace.frame(snapshot(), Event::Found, "redBlack.search.found",
                        sentence("Found " + text(value) + " after " + std::to_string(trace.counters.comparisons) +
                                 (trace.counters.comparisons == 1 ? " comparison" : " comparisons")),
                        path, {{"x", x->id}});
            break;
        }
        const bool goLeft = value < x->value;
        const Node* child = goLeft ? x->left : x->right;  // non-owning: the pool owns every node (or it is &nil_)
        if (child != &nil_) {
            path.edges.push_back(Edge{x->id, child->id});
        }
        trace.frame(snapshot(), Event::Compare, goLeft ? "redBlack.search.goLeft" : "redBlack.search.goRight",
                    sentence(text(value) + (goLeft ? " < " : " > ") + text(x->value) + ", so move to the " +
                             (goLeft ? "left" : "right") + " child, " +
                             (child != &nil_ ? text(child->value) : std::string("which is NIL"))),
                    path, {{"x", idOf(child)}});
        x = child;
    }

    if (result.found) {
        trace.frame(snapshot(), Event::Done, "redBlack.search.done",
                    sentence("Search finished: " + text(value) + " is in the red-black tree; the highlighted path shows how"),
                    path);
    } else {
        trace.frame(snapshot(), Event::NotFound, "redBlack.search.notFound",
                    sentence("Reached NIL: " + text(value) + " is not in the red-black tree"), path, {{"x", kNoNode}});
        trace.frame(snapshot(), Event::Done, "redBlack.search.done",
                    sentence("Search finished: " + text(value) + " is not in the red-black tree"), path);
    }
    return Result<BstSearchResult>{okStatus(), result};
}

Result<TraversalResult> RedBlackTree::traverse(TraversalOrder order, Trace& trace) const {
    TreeTraversal<RedBlackTree, Node> traversal(*this, "redBlack", "red-black tree", trace, &nil_);
    return Result<TraversalResult>{okStatus(), traversal.run(order, root_, size())};
}

// freeSubtree(n):  post-order, children before their parent (as in the BST)
void RedBlackTree::freeSubtree(Node* node, int& removed, Trace& trace) {
    trace.counters.nodesVisited++;
    if (node->left != &nil_) {
        Node* child = node->left;  // non-owning: the pool owns every node
        node->left = &nil_;
        trace.counters.linksChanged++;
        freeSubtree(child, removed, trace);
    }
    if (node->right != &nil_) {
        Node* child = node->right;  // non-owning: the pool owns every node
        node->right = &nil_;
        trace.counters.linksChanged++;
        freeSubtree(child, removed, trace);
    }

    const NodeId id = node->id;
    const Value value = node->value;
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    removed = removed + 1;
    trace.frame(snapshot(), Event::Remove, "redBlack.clear.remove",
                "Free " + text(value) + "; its children, if it had any, were freed first.", {{id}, {}});
}

// clear():  cut root, then free the old tree in post-order.
Result<int> RedBlackTree::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "redBlack.clear.enter", "Clear " + describe(size()) + ".");

    int removed = 0;
    if (root_ != &nil_) {
        Node* node = root_;  // non-owning: the pool owns every node
        root_ = &nil_;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "redBlack.clear.cut",
                    "Set root to NIL; the old tree is freed bottom-up (post-order).", {{node->id}, {}},
                    {{"node", node->id}});
        freeSubtree(node, removed, trace);
    }

    trace.frame(snapshot(), Event::Done, "redBlack.clear.done",
                removed == 0 ? std::string("The red-black tree was already empty, so nothing was freed.")
                             : "Cleared " + nodeCount(removed) + "; the red-black tree is now empty.");
    return Result<int>{okStatus(), removed};
}

Snapshot RedBlackTree::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::RedBlack;
    copy.size = pool_.size();
    copy.nextId = pool_.nextId();
    copy.root = idOf(root_);
    copy.nilColor = nil_.color;
    for (const std::unique_ptr<Node>& node : pool_.nodes()) {
        NodeView view;
        view.id = node->id;
        view.value = node->value;
        view.left = idOf(node->left);
        view.right = idOf(node->right);
        view.parent = idOf(node->parent);
        view.color = node->color;
        copy.nodes.push_back(view);
    }
    return copy;
}

// restore(snapshot): create every node with its ID, value and color, then set
// the links. Here NIL is the sentinel, so a missing link becomes &nil_.
void RedBlackTree::restore(const Snapshot& snapshot) {
    pool_.reset(snapshot.nextId);
    for (const NodeView& view : snapshot.nodes) {
        pool_.createWithId(view.id, view.value)->color = view.color;
    }
    // The node with this ID, or the sentinel for kNoNode.
    const auto linkTo = [this](NodeId id) { return id == kNoNode ? &nil_ : pool_.find(id); };
    for (const NodeView& view : snapshot.nodes) {
        Node* node = pool_.find(view.id);  // non-owning: the pool owns every node
        node->left = linkTo(view.left);
        node->right = linkTo(view.right);
        node->parent = linkTo(view.parent);
    }
    nil_.parent = &nil_;  // its value only matters during a delete
    root_ = linkTo(snapshot.root);
}

}  // namespace dslab
