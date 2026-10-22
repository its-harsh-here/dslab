// A request as plain C++ data, after the JSON layer has checked it.
#pragma once

#include <cstdint>

#include "dslab/Types.h"

namespace dslab {

enum class Operation {
    Snapshot,  // shared by every structure
    // stack
    Push,
    Pop,
    Peek,
    // queue
    Enqueue,
    Dequeue,
    Front,
    // list
    InsertAt,
    DeleteNode,
    Search,
    Reverse,
    // binary tree
    AddRoot,
    AddChild,
    DeleteSubtree,
    // trees
    Preorder,
    Inorder,
    Postorder,
    Levelorder,
    // shared by every structure
    Clear
};

// Operations that change the structure get an undo-history entry when they succeed.
inline bool isMutating(Operation operation) {
    switch (operation) {
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
        case Operation::Clear:
            return true;
        default:
            return false;
    }
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
};

}  // namespace dslab
