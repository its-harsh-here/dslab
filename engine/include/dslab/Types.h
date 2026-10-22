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

// The structures this engine implements. Later milestones add the other six.
enum class StructureKind { Stack };

// What one frame shows (operation-contract.md, section 6.2).
// Only the events the stack uses exist so far.
enum class Event {
    Begin,   // first frame: the state before anything happened
    Move,    // a cursor variable moved to a node
    Read,    // a value was read without changing anything
    Create,  // a node was allocated (not linked yet)
    Link,    // a link or entry pointer was set to a node
    Unlink,  // a link or entry pointer was set to NIL
    Remove,  // a node was freed
    Reject,  // the operation was rejected (last frame)
    Done     // the operation succeeded (last frame)
};

}  // namespace dslab
