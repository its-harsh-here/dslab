#include "dslab/Validate.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "dslab/Limits.h"

namespace dslab {

namespace {

// The largest value the ID counter may hold: one past the largest ID.
constexpr std::int64_t kLargestNextId = 2147483648LL;

// The nodes of a snapshot by ID (a node registry: allowed by rule 1).
using Registry = std::map<NodeId, NodeView>;

std::string text(std::int64_t number) {
    return std::to_string(number);
}

Status invalid(const std::string& message) {
    return errorStatus(ErrorCode::ImportInvalid, message);
}

// "a BST", "an AVL tree", "a red-black tree": used in messages.
std::string article(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return "a stack";
        case StructureKind::Queue: return "a queue";
        case StructureKind::List: return "a list";
        case StructureKind::BinaryTree: return "a binary tree";
        case StructureKind::Bst: return "a BST";
        case StructureKind::Avl: return "an AVL tree";
        case StructureKind::RedBlack: return "a red-black tree";
    }
    return "a structure";
}

// Rule 3: at most 200 nodes, the size matches, IDs from 1 in ascending order
// without repeats. Rule 10: the ID counter is past the largest ID.
Status checkNodes(const Snapshot& snapshot) {
    const std::int64_t count = static_cast<std::int64_t>(snapshot.nodes.size());
    if (count > kMaxNodes) {
        return invalid("The file lists " + text(count) + " nodes, but a structure holds at most 200.");
    }
    if (snapshot.size != count) {
        return invalid("The file says size " + text(snapshot.size) + " but lists " + text(count) + " nodes.");
    }
    NodeId previous = kNoNode;
    for (const NodeView& node : snapshot.nodes) {
        if (node.id < 1) {
            return invalid("Node ID " + text(node.id) + " is not allowed: IDs start at 1.");
        }
        if (node.id == previous) {
            return invalid("Node ID " + text(node.id) + " appears twice.");
        }
        if (node.id < previous) {
            return invalid("Nodes must be listed in ascending ID order, but node " + text(node.id) +
                           " comes after node " + text(previous) + ".");
        }
        previous = node.id;
    }
    if (snapshot.nextId <= previous || snapshot.nextId < 1 || snapshot.nextId > kLargestNextId) {
        return invalid("nextId is " + text(snapshot.nextId) + ", but it must be above the largest node ID (" +
                       text(previous) + ") and at most 2147483648.");
    }
    return okStatus();
}

// Rule 4: every link and entry pointer is NIL or names a listed node.
Status checkLinks(const Snapshot& snapshot, const Registry& byId) {
    const std::vector<std::pair<const char*, NodeId>> entries = {
        {"top", snapshot.top}, {"front", snapshot.front}, {"rear", snapshot.rear},
        {"head", snapshot.head}, {"root", snapshot.root}};
    for (const auto& [name, target] : entries) {
        if (target != kNoNode && byId.count(target) == 0) {
            return invalid(std::string(name) + " points to node " + text(target) + ", which is not in the file.");
        }
    }
    for (const NodeView& node : snapshot.nodes) {
        const std::vector<std::pair<const char*, NodeId>> links = {
            {"next", node.next}, {"left", node.left}, {"right", node.right}, {"parent", node.parent}};
        for (const auto& [name, target] : links) {
            if (target != kNoNode && byId.count(target) == 0) {
                return invalid("Node " + text(node.id) + "'s " + name + " points to node " + text(target) +
                               ", which is not in the file.");
            }
        }
    }
    return okStatus();
}

// Rule 5: the chain from the entry pointer visits every node once and ends at
// NIL; a queue's rear is the chain's last node.
Status checkChain(const Snapshot& snapshot, const Registry& byId, NodeId entry, const std::string& entryName) {
    if ((entry == kNoNode) != snapshot.nodes.empty()) {
        return invalid(entryName + " must be null exactly when there are no nodes.");
    }
    std::set<NodeId> seen;
    NodeId last = kNoNode;
    for (NodeId node = entry; node != kNoNode; node = byId.at(node).next) {
        if (!seen.insert(node).second) {
            return invalid("Following next from " + entryName + " comes back to node " + text(node) +
                           ": the links form a cycle.");
        }
        last = node;
    }
    for (const NodeView& node : snapshot.nodes) {
        if (seen.count(node.id) == 0) {
            return invalid("Node " + text(node.id) + " cannot be reached from " + entryName + ".");
        }
    }
    if (snapshot.structure == StructureKind::Queue && snapshot.rear != last) {
        return invalid("rear is " + (snapshot.rear == kNoNode ? std::string("null") : "node " + text(snapshot.rear)) +
                       ", but the last node of the chain is node " + text(last) + ".");
    }
    return okStatus();
}

// Rule 6: one root, every other node exactly one parent link, every node
// reachable from the root (so no cycles), and parent fields that match.
Status checkTreeShape(const Snapshot& snapshot, const Registry& byId) {
    if ((snapshot.root == kNoNode) != snapshot.nodes.empty()) {
        return invalid("root must be null exactly when there are no nodes.");
    }
    std::map<NodeId, NodeId> parentOf;  // child -> the node whose link points at it
    for (const NodeView& node : snapshot.nodes) {
        if (node.left != kNoNode && node.left == node.right) {
            return invalid("Node " + text(node.id) + " has node " + text(node.left) + " as both its left and right child.");
        }
        for (const NodeId child : {node.left, node.right}) {
            if (child == kNoNode) continue;
            if (child == snapshot.root) {
                return invalid("The root, node " + text(child) + ", is also a child of node " + text(node.id) + ".");
            }
            if (parentOf.count(child) != 0) {
                return invalid("Node " + text(child) + " has two parents: node " + text(parentOf[child]) + " and node " +
                               text(node.id) + ".");
            }
            parentOf[child] = node.id;
        }
    }

    // Walk down from the root; with one parent per node this cannot loop.
    std::set<NodeId> reached;
    std::vector<NodeId> waiting;
    if (snapshot.root != kNoNode) waiting.push_back(snapshot.root);
    while (!waiting.empty()) {
        const NodeView& node = byId.at(waiting.back());
        waiting.pop_back();
        reached.insert(node.id);
        if (node.left != kNoNode) waiting.push_back(node.left);
        if (node.right != kNoNode) waiting.push_back(node.right);
    }
    for (const NodeView& node : snapshot.nodes) {
        if (reached.count(node.id) == 0) {
            return invalid("Node " + text(node.id) + " cannot be reached from the root.");
        }
        const NodeId realParent = parentOf.count(node.id) != 0 ? parentOf[node.id] : kNoNode;
        if (node.parent != realParent) {
            const std::string says = node.parent == kNoNode ? "has no parent" : "has parent node " + text(node.parent);
            const std::string is = realParent == kNoNode ? "it is the root" : "node " + text(realParent) + " links to it";
            return invalid("Node " + text(node.id) + " says it " + says + ", but " + is + ".");
        }
    }
    return okStatus();
}

// Rule 7: in-order values strictly increase (so there are no duplicates).
Status checkOrder(const Snapshot& snapshot, const Registry& byId) {
    std::vector<NodeId> stack;  // the in-order walk's own stack (a plain array)
    const NodeView* previous = nullptr;  // non-owning: points into byId
    NodeId current = snapshot.root;
    while (current != kNoNode || !stack.empty()) {
        while (current != kNoNode) {
            stack.push_back(current);
            current = byId.at(current).left;
        }
        const NodeView& node = byId.at(stack.back());
        stack.pop_back();
        if (previous != nullptr && previous->value == node.value) {
            return invalid("The value " + text(node.value) + " appears twice (nodes " + text(previous->id) + " and " +
                           text(node.id) + "), but " + article(snapshot.structure) + " has no duplicates.");
        }
        if (previous != nullptr && previous->value > node.value) {
            return invalid("In-order values must increase, but " + text(previous->value) + " (node " +
                           text(previous->id) + ") comes before " + text(node.value) + " (node " + text(node.id) + ").");
        }
        previous = &node;
        current = node.right;
    }
    return okStatus();
}

// The real height of the subtree at 'node' (NIL 0, leaf 1), from the links.
int realHeight(const Registry& byId, NodeId node) {
    if (node == kNoNode) return 0;
    const NodeView& view = byId.at(node);
    const int left = realHeight(byId, view.left);
    const int right = realHeight(byId, view.right);
    return 1 + (left > right ? left : right);
}

// Rule 8: stored heights and balances match the real ones, and every balance is -1, 0 or +1.
Status checkAvl(const Snapshot& snapshot, const Registry& byId) {
    for (const NodeView& node : snapshot.nodes) {
        const int height = realHeight(byId, node.id);
        if (node.height != height) {
            return invalid("Node " + text(node.id) + " stores height " + text(node.height) + ", but its real height is " +
                           text(height) + ".");
        }
        const int balance = realHeight(byId, node.left) - realHeight(byId, node.right);
        if (node.balance != balance) {
            return invalid("Node " + text(node.id) + " stores balance " + text(node.balance) +
                           ", but height(left) - height(right) is " + text(balance) + ".");
        }
        if (balance < -1 || balance > 1) {
            return invalid("Node " + text(node.id) + " has balance " + (balance > 0 ? "+" : "") + text(balance) +
                           ", but an AVL tree allows only -1, 0 or +1.");
        }
    }
    return okStatus();
}

// The black height below 'node' (NIL counts 1), or -1 if two paths differ.
// 'where' gets the first node whose two sides differ.
int blackHeight(const Registry& byId, NodeId node, NodeId& where) {
    if (node == kNoNode) return 1;
    const NodeView& view = byId.at(node);
    const int left = blackHeight(byId, view.left, where);
    const int right = blackHeight(byId, view.right, where);
    if (left < 0 || right < 0) return -1;
    if (left != right) {
        where = node;
        return -1;
    }
    return left + (view.color == Color::Black ? 1 : 0);
}

// Rule 9: red or black nodes, a black NIL, a black root, no red node with a
// red child, and the same number of black nodes on every path to NIL.
Status checkRedBlack(const Snapshot& snapshot, const Registry& byId) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.color != Color::Red && node.color != Color::Black) {
            return invalid("Node " + text(node.id) + " is neither red nor black.");
        }
    }
    if (snapshot.nilColor != Color::Black) {
        return invalid("nilColor must be black: every NIL leaf of a red-black tree is black.");
    }
    if (snapshot.root != kNoNode && byId.at(snapshot.root).color == Color::Red) {
        return invalid("The root, node " + text(snapshot.root) + ", is red, but a red-black root must be black.");
    }
    for (const NodeView& node : snapshot.nodes) {
        for (const NodeId child : {node.left, node.right}) {
            if (node.color == Color::Red && child != kNoNode && byId.at(child).color == Color::Red) {
                return invalid("Node " + text(node.id) + " is red and so is its child, node " + text(child) + ".");
            }
        }
    }
    NodeId where = kNoNode;
    if (blackHeight(byId, snapshot.root, where) < 0) {
        return invalid("The paths below node " + text(where) + " have different numbers of black nodes.");
    }
    return okStatus();
}

}  // namespace

Status validateSnapshot(const Snapshot& snapshot) {
    Status status = checkNodes(snapshot);
    if (!status.ok()) return status;

    Registry byId;
    for (const NodeView& node : snapshot.nodes) {
        byId[node.id] = node;
    }
    status = checkLinks(snapshot, byId);
    if (!status.ok()) return status;

    switch (snapshot.structure) {
        case StructureKind::Stack: return checkChain(snapshot, byId, snapshot.top, "top");
        case StructureKind::Queue: return checkChain(snapshot, byId, snapshot.front, "front");
        case StructureKind::List: return checkChain(snapshot, byId, snapshot.head, "head");
        case StructureKind::BinaryTree:
        case StructureKind::Bst:
        case StructureKind::Avl:
        case StructureKind::RedBlack:
            break;
    }
    status = checkTreeShape(snapshot, byId);
    if (!status.ok() || snapshot.structure == StructureKind::BinaryTree) return status;
    status = checkOrder(snapshot, byId);
    if (!status.ok()) return status;
    if (snapshot.structure == StructureKind::Avl) return checkAvl(snapshot, byId);
    if (snapshot.structure == StructureKind::RedBlack) return checkRedBlack(snapshot, byId);
    return okStatus();
}

}  // namespace dslab
