// A full copy of one structure at one moment (docs/operation-contract.md, 5.4).
// Plain data only: IDs and integers, no pointers, so copies can never dangle.
// One shape serves every structure; each structure fills only its own fields
// and the JSON layer writes only those.
#pragma once

#include <cstdint>
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
    int height = 0;           // AVL: the stored height (0 for other structures)
    int balance = 0;          // AVL: height(left) - height(right), from stored heights
    Color color = Color::None;  // red-black only
    NodeId prev = kNoNode;    // deque, doubly linked list
    int index = 0;            // heaps: the array slot the node sits in (0 elsewhere)
};

struct Snapshot {
    StructureKind structure = StructureKind::Stack;
    int size = 0;  // nodes owned; always equals nodes.size()

    // Entry pointers. Each structure uses only its own.
    NodeId top = kNoNode;    // stack
    NodeId front = kNoNode;  // queue
    NodeId rear = kNoNode;   // queue
    NodeId head = kNoNode;   // list, doubly linked list
    NodeId root = kNoNode;   // trees and heaps
    NodeId tail = kNoNode;   // doubly linked list, circular list

    // Hash table only: the first node of each bucket's chain (kHashBuckets
    // entries); empty for every other structure.
    std::vector<NodeId> buckets;

    // Red-black only: the color of the NIL sentinel, which is not a listed node.
    Color nilColor = Color::None;

    std::vector<NodeView> nodes;  // every owned node, in ascending ID order

    // The ID the next created node will get. It is part of the state: undo,
    // redo and import restore it exactly. Response snapshots do not show it;
    // the export document carries it (operation-contract.md 5.7).
    std::int64_t nextId = 1;
};

inline bool operator==(const NodeView& a, const NodeView& b) {
    return a.id == b.id && a.value == b.value && a.next == b.next && a.left == b.left &&
           a.right == b.right && a.parent == b.parent && a.height == b.height &&
           a.balance == b.balance && a.color == b.color && a.prev == b.prev && a.index == b.index;
}

inline bool operator!=(const NodeView& a, const NodeView& b) {
    return !(a == b);
}

inline bool operator==(const Snapshot& a, const Snapshot& b) {
    return a.structure == b.structure && a.size == b.size && a.top == b.top &&
           a.front == b.front && a.rear == b.rear && a.head == b.head && a.root == b.root &&
           a.tail == b.tail && a.buckets == b.buckets && a.nilColor == b.nilColor && a.nodes == b.nodes && a.nextId == b.nextId;
}

inline bool operator!=(const Snapshot& a, const Snapshot& b) {
    return !(a == b);
}

}  // namespace dslab
