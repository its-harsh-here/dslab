// Basic types shared by the whole engine (docs/operation-contract.md, section 1).
#pragma once

#include <cstdint>

namespace dslab {

// Every value stored in a structure is a 32-bit signed integer.
using Value = std::int32_t;

// Every node has a stable ID that never changes and is never reused.
// IDs start at 1; 0 means "no node" (NIL) and is written as null in JSON.
using NodeId = std::int32_t;
constexpr NodeId kNoNode = 0;

// The structures this engine implements.
enum class StructureKind { Stack, Queue, List, BinaryTree, Bst, Avl, RedBlack };

// Stack, queue and list nodes link with "next"; tree nodes with left/right/parent.
inline bool isLinear(StructureKind structure) {
    return structure == StructureKind::Stack || structure == StructureKind::Queue ||
           structure == StructureKind::List;
}

// Which child link of a tree node.
enum class Side { Left, Right };

// A red-black node's color. Nodes of every other structure have no color (None).
enum class Color { None, Black, Red };

// What one frame shows (operation-contract.md, section 6.2).
// Only the events used so far exist.
enum class Event {
    Begin,     // first frame: the state before anything happened
    Compare,   // a value was compared with a node's value
    Move,      // a cursor variable moved to a node
    Read,      // a value was read without changing anything
    Visit,     // a traversal output a node's value
    Found,     // a search found what it looked for
    NotFound,  // a search reached NIL
    Create,    // a node was allocated (not linked yet)
    Link,      // a link or entry pointer was set to a node
    Unlink,    // a link or entry pointer was set to NIL
    Remove,    // a node was freed
    Update,    // a stored field other than a link changed (AVL height)
    Imbalance, // AVL: a balance of +2 or -2 was found, or its case was chosen
    FixupCase, // red-black: a fix-up case (insert 1-3, delete 1-4) was recognised
    Rotate,    // a left or right rotation finished
    Recolor,   // red-black: one node's color was assigned
    Balanced,  // a fix-up finished; the tree's invariants hold again
    Restore,   // undo, redo or import replaced the whole structure
    Reject,    // the operation was rejected (last frame)
    Done       // the operation succeeded (last frame)
};

}  // namespace dslab
