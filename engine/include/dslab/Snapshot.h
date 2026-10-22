// A full copy of one structure at one moment (docs/operation-contract.md, 5.4).
// Plain data only: IDs and integers, no pointers, so copies can never dangle.
#pragma once

#include <vector>

#include "dslab/Types.h"

namespace dslab {

// One node, with every field it stores.
struct NodeView {
    NodeId id = kNoNode;
    Value value = 0;
    NodeId next = kNoNode;  // kNoNode means NIL
};

struct Snapshot {
    StructureKind structure = StructureKind::Stack;
    int size = 0;                 // nodes owned; always equals nodes.size()
    NodeId top = kNoNode;         // the stack's entry pointer
    std::vector<NodeView> nodes;  // every owned node, in ascending ID order
};

inline bool operator==(const NodeView& a, const NodeView& b) {
    return a.id == b.id && a.value == b.value && a.next == b.next;
}

inline bool operator!=(const NodeView& a, const NodeView& b) {
    return !(a == b);
}

inline bool operator==(const Snapshot& a, const Snapshot& b) {
    return a.structure == b.structure && a.size == b.size && a.top == b.top &&
           a.nodes == b.nodes;
}

inline bool operator!=(const Snapshot& a, const Snapshot& b) {
    return !(a == b);
}

}  // namespace dslab
