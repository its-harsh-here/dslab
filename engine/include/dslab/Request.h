// A request as plain C++ data, after the JSON layer has checked it.
#pragma once

#include <cstdint>
#include <string>

#include "dslab/Snapshot.h"
#include "dslab/Types.h"

namespace dslab {

enum class Operation {
    // shared by every structure
    Snapshot,
    Catalog,  // the pseudocode listings
    Undo,
    Redo,
    Export,
    Import,
    // stack
    Push,
    Pop,
    Peek,
    // queue
    Enqueue,
    Dequeue,
    Front,
    // list (Search is also the BST's)
    InsertAt,
    DeleteNode,
    Search,
    Reverse,
    // binary tree
    AddRoot,
    AddChild,
    DeleteSubtree,
    // BST
    Insert,
    Delete,
    // deque
    PushFront,
    PushBack,
    PopFront,
    PopBack,
    PeekFront,
    PeekBack,
    // circular list
    InsertFront,
    InsertBack,
    Rotate,
    // heaps (Insert, Peek and Clear are shared)
    Extract,
    // trees
    Preorder,
    Inorder,
    Postorder,
    Levelorder,
    // shared by every structure
    Clear
};

// Operations that change the structure get an undo-history entry when they
// succeed. Undo and redo change it too, but move entries between the history
// lists instead of adding one, so they are not listed here.
inline bool isMutating(Operation operation) {
    switch (operation) {
        case Operation::PushFront:
        case Operation::PushBack:
        case Operation::PopFront:
        case Operation::PopBack:
        case Operation::InsertFront:
        case Operation::InsertBack:
        case Operation::Rotate:
        case Operation::Extract:
        case Operation::Push:
        case Operation::Pop:
        case Operation::Enqueue:
        case Operation::Dequeue:
        case Operation::InsertAt:
        case Operation::DeleteNode:
        case Operation::Reverse:
        case Operation::AddRoot:
        case Operation::AddChild:
        case Operation::DeleteSubtree:
        case Operation::Insert:
        case Operation::Delete:
        case Operation::Clear:
        case Operation::Import:
            return true;
        default:
            return false;
    }
}

// The same question for one structure. A splay tree's search is the one
// exception to the rule above: it rotates the node it reached to the root, so
// it changes the tree and gets an undo entry like any other change.
inline bool isMutating(StructureKind structure, Operation operation) {
    if (structure == StructureKind::Splay && operation == Operation::Search) {
        return true;
    }
    return isMutating(operation);
}

struct Request {
    std::int32_t requestId = 0;
    StructureKind structure = StructureKind::Stack;
    Operation operation = Operation::Snapshot;
    // Arguments; each operation uses only its own.
    Value value = 0;
    std::int32_t index = 0;
    NodeId nodeId = kNoNode;
    NodeId parentId = kNoNode;
    Side side = Side::Left;
    // import: the document's snapshot as the JSON layer decoded it (rules 1
    // and 2 of contract 5.7), or why it could not be decoded ("" if it could).
    Snapshot importSnapshot;
    std::string importProblem;
};

}  // namespace dslab
