#include "dslab/BinaryTree.h"

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

// "an empty binary tree" or "a binary tree that holds 3 nodes".
std::string describe(int size) {
    return size == 0 ? "an empty binary tree" : "a binary tree that holds " + nodeCount(size);
}

const char* sideName(Side side) {
    return side == Side::Left ? "left" : "right";
}

// "#3 (30)": duplicates are allowed, so the ID says which node is meant.
template <typename Node>
std::string named(const Node* node) {
    return "#" + std::to_string(node->id) + " (" + text(node->value) + ")";
}

}  // namespace

// addRoot(value):                                       binaryTree.addRoot.enter
//   if size = 200: reject CAPACITY_EXCEEDED             binaryTree.addRoot.full
//   if root != NIL: reject ROOT_EXISTS                  binaryTree.addRoot.rootExists
//   node <- new Node(value)                             binaryTree.addRoot.create
//   root <- node                                        binaryTree.addRoot.linkRoot
//   return node                                         binaryTree.addRoot.done
Result<NodeId> BinaryTree::addRoot(Value value, Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "binaryTree.addRoot.enter",
                "Add " + text(value) + " as the root of " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "binaryTree.addRoot.full", ErrorCode::CapacityExceeded,
                              "The binary tree already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "binaryTree.addRoot.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this tree.");
    }
    if (root_ != nullptr) {
        return reject<NodeId>(trace, "binaryTree.addRoot.rootExists", ErrorCode::RootExists,
                              "The tree already has a root, " + named(root_) + ".");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Create, "binaryTree.addRoot.create",
                "Create a new node holding " + text(value) + "; it has no parent.", {{node->id}, {}},
                {{"node", node->id}});

    root_ = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, "binaryTree.addRoot.linkRoot", "Set root to the new node.",
                {{node->id}, {}}, {{"node", node->id}});

    trace.frame(snapshot(), Event::Done, "binaryTree.addRoot.done",
                "Added " + text(value) + " as the root; the tree now holds 1 node.", {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// addChild(parentId, side, value):                      binaryTree.addChild.enter
//   if size = 200: reject CAPACITY_EXCEEDED             binaryTree.addChild.full
//   parent <- the node with parentId
//   if there is none: reject NODE_NOT_FOUND             binaryTree.addChild.missing
//   if parent.side != NIL: reject CHILD_EXISTS          binaryTree.addChild.occupied
//   node <- new Node(value); node.parent <- parent      binaryTree.addChild.create
//   parent.side <- node                                 binaryTree.addChild.link
//   return node                                         binaryTree.addChild.done
Result<NodeId> BinaryTree::addChild(NodeId parentId, Side side, Value value, Trace& trace) {
    // Which node the user selected (support work, like a pointer the caller already holds).
    Node* parent = pool_.find(parentId);  // non-owning: the pool owns every node
    const std::string where = std::string("the ") + sideName(side) + " child of " +
                              (parent != nullptr ? named(parent) : "node #" + std::to_string(parentId));
    trace.frame(snapshot(), Event::Begin, "binaryTree.addChild.enter",
                "Add " + text(value) + " as " + where + " in " + describe(size()) + ".");

    if (size() == kMaxNodes) {
        return reject<NodeId>(trace, "binaryTree.addChild.full", ErrorCode::CapacityExceeded,
                              "The binary tree already holds 200 nodes, the maximum.");
    }
    if (!pool_.hasFreeId()) {
        return reject<NodeId>(trace, "binaryTree.addChild.full", ErrorCode::IdExhausted,
                              "No node IDs are left for this tree.");
    }
    if (parent == nullptr) {
        return reject<NodeId>(trace, "binaryTree.addChild.missing", ErrorCode::NodeNotFound,
                              "There is no node with ID " + std::to_string(parentId) + " in this tree.");
    }
    Node*& slot = (side == Side::Left) ? parent->left : parent->right;  // the link to fill
    if (slot != nullptr) {
        return reject<NodeId>(trace, "binaryTree.addChild.occupied", ErrorCode::ChildExists,
                              named(parent) + " already has a " + sideName(side) + " child, " +
                                  named(slot) + ".");
    }

    Node* node = pool_.create(value);  // non-owning: the pool owns every node
    node->parent = parent;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Create, "binaryTree.addChild.create",
                "Create a new node holding " + text(value) + " whose parent is " + named(parent) + ".",
                {{node->id}, {}}, {{"parent", parent->id}, {"node", node->id}});

    slot = node;
    trace.counters.linksChanged++;
    trace.frame(snapshot(), Event::Link, "binaryTree.addChild.link",
                "Set " + named(parent) + "'s " + sideName(side) + " link to the new node.",
                {{parent->id, node->id}, {{parent->id, node->id}}},
                {{"parent", parent->id}, {"node", node->id}});

    trace.frame(snapshot(), Event::Done, "binaryTree.addChild.done",
                "Added " + text(value) + " as the " + sideName(side) + " child of " + text(parent->value) +
                    "; the tree now holds " + nodeCount(size()) + ".",
                {{node->id}, {}});
    return Result<NodeId>{okStatus(), node->id};
}

// deleteSubtree(id):                                    binaryTree.deleteSubtree.enter
//   node <- the node with id
//   if there is none: reject NODE_NOT_FOUND             binaryTree.deleteSubtree.missing
//   cut the link to node: root <- NIL, or
//     node.parent.left / node.parent.right <- NIL       binaryTree.deleteSubtree.cut
//   freeSubtree(node)                                   binaryTree.deleteSubtree.remove
//   return the freed IDs                                binaryTree.deleteSubtree.done
Result<std::vector<NodeId>> BinaryTree::deleteSubtree(NodeId id, Trace& trace) {
    Node* node = pool_.find(id);  // non-owning: the pool owns every node
    trace.frame(snapshot(), Event::Begin, "binaryTree.deleteSubtree.enter",
                "Delete the subtree rooted at " +
                    (node != nullptr ? named(node) : "node #" + std::to_string(id)) + " from " +
                    describe(size()) + ".");

    if (node == nullptr) {
        return reject<std::vector<NodeId>>(trace, "binaryTree.deleteSubtree.missing", ErrorCode::NodeNotFound,
                                           "There is no node with ID " + std::to_string(id) + " in this tree.");
    }

    Node* parent = node->parent;  // non-owning: the pool owns every node
    if (parent == nullptr) {
        root_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "binaryTree.deleteSubtree.cut",
                    named(node) + " is the root, so set root to NIL; the whole tree is cut off.",
                    {{node->id}, {}}, {{"node", node->id}});
    } else {
        const bool left = (parent->left == node);
        (left ? parent->left : parent->right) = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "binaryTree.deleteSubtree.cut",
                    "Set " + named(parent) + "'s " + (left ? "left" : "right") +
                        " link to NIL, cutting off the subtree rooted at " + named(node) + ".",
                    {{parent->id, node->id}, {}}, {{"parent", parent->id}, {"node", node->id}});
    }

    std::vector<NodeId> removed;
    freeSubtree(node, "binaryTree.deleteSubtree.remove", removed, trace);

    trace.frame(snapshot(), Event::Done, "binaryTree.deleteSubtree.done",
                "Deleted a subtree of " + nodeCount(static_cast<int>(removed.size())) +
                    (size() == 0 ? "; the tree is now empty." : "; the tree now holds " + nodeCount(size()) + "."));
    return Result<std::vector<NodeId>>{okStatus(), removed};
}

// freeSubtree(n):  post-order, so children are freed before their parent
//   if n.left != NIL:  child <- n.left;  n.left <- NIL;  freeSubtree(child)
//   if n.right != NIL: child <- n.right; n.right <- NIL; freeSubtree(child)
//   delete n                                            (one frame per node)
// Each link is cut before its child is freed, so no remaining node ever points
// at freed memory.
void BinaryTree::freeSubtree(Node* node, const std::string& stepId, std::vector<NodeId>& removed,
                             Trace& trace) {
    trace.counters.nodesVisited++;
    if (node->left != nullptr) {
        Node* child = node->left;  // non-owning: the pool owns every node
        node->left = nullptr;
        trace.counters.linksChanged++;
        freeSubtree(child, stepId, removed, trace);
    }
    if (node->right != nullptr) {
        Node* child = node->right;  // non-owning: the pool owns every node
        node->right = nullptr;
        trace.counters.linksChanged++;
        freeSubtree(child, stepId, removed, trace);
    }

    const NodeId id = node->id;
    const std::string name = named(node);
    pool_.destroy(node);  // 'node' dangles from here on and is not used again
    removed.push_back(id);
    trace.frame(snapshot(), Event::Remove, stepId,
                "Free " + name + "; its children, if it had any, were freed first.", {{id}, {}});
}

Result<TraversalResult> BinaryTree::traverse(TraversalOrder order, Trace& trace) const {
    TreeTraversal<BinaryTree, Node> traversal(*this, "binaryTree", "binary tree", trace);
    return Result<TraversalResult>{okStatus(), traversal.run(order, root_, size())};
}

// clear():                                              binaryTree.clear.enter
//   if root != NIL:
//     node <- root; root <- NIL                         binaryTree.clear.cut
//     freeSubtree(node)                                 binaryTree.clear.remove
//   return the number freed                             binaryTree.clear.done
Result<int> BinaryTree::clear(Trace& trace) {
    trace.frame(snapshot(), Event::Begin, "binaryTree.clear.enter", "Clear " + describe(size()) + ".");

    std::vector<NodeId> removed;
    if (root_ != nullptr) {
        Node* node = root_;  // non-owning: the pool owns every node
        root_ = nullptr;
        trace.counters.linksChanged++;
        trace.frame(snapshot(), Event::Unlink, "binaryTree.clear.cut",
                    "Set root to NIL; the old tree is freed bottom-up (post-order).", {{node->id}, {}},
                    {{"node", node->id}});
        freeSubtree(node, "binaryTree.clear.remove", removed, trace);
    }

    const int count = static_cast<int>(removed.size());
    trace.frame(snapshot(), Event::Done, "binaryTree.clear.done",
                count == 0 ? std::string("The tree was already empty, so nothing was freed.")
                           : "Cleared " + nodeCount(count) + "; the tree is now empty.");
    return Result<int>{okStatus(), count};
}

Snapshot BinaryTree::snapshot() const {
    Snapshot copy;
    copy.structure = StructureKind::BinaryTree;
    copy.size = pool_.size();
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

}  // namespace dslab
