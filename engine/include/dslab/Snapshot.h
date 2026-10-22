// A full copy of one structure at one moment (docs/operation-contract.md, 5.4).
// Plain data only: IDs and integers, no pointers, so copies can never dangle.
// One shape serves every structure; each structure fills only its own fields
// and the JSON layer writes only those.
#pragma once

#include <vector>

#include "dslab/Types.h"

namespace dslab {

// One node, with every field it stores. kNoNode means NIL.
struct NodeView {
    NodeId id = kNoNode;
    Value value = 0;
    NodeId next = kNoNode;    // stack, queue, list
    NodeId left = kNoNode;    // trees
    NodeId right = kNoNode;   // trees
    NodeId parent = kNoNode;  // trees that store a parent pointer
};

struct Snapshot {
    StructureKind structure = StructureKind::Stack;
    int size = 0;  // nodes owned; always equals nodes.size()

    // Entry pointers. Each structure uses only its own.
    NodeId top = kNoNode;    // stack
    NodeId front = kNoNode;  // queue
    NodeId rear = kNoNode;   // queue
    NodeId head = kNoNode;   // list
    NodeId root = kNoNode;   // trees

    std::vector<NodeView> nodes;  // every owned node, in ascending ID order
};

inline bool operator==(const NodeView& a, const NodeView& b) {
    return a.id == b.id && a.value == b.value && a.next == b.next && a.left == b.left &&
           a.right == b.right && a.parent == b.parent;
}

inline bool operator!=(const NodeView& a, const NodeView& b) {
    return !(a == b);
}

inline bool operator==(const Snapshot& a, const Snapshot& b) {
    return a.structure == b.structure && a.size == b.size && a.top == b.top &&
           a.front == b.front && a.rear == b.rear && a.head == b.head && a.root == b.root &&
           a.nodes == b.nodes;
}

inline bool operator!=(const Snapshot& a, const Snapshot& b) {
    return !(a == b);
}

}  // namespace dslab
