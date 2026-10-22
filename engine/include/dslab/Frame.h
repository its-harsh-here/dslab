// One animation step, recorded inside the running algorithm
// (docs/operation-contract.md, section 6).
#pragma once

#include <string>
#include <vector>

#include "dslab/Snapshot.h"
#include "dslab/Types.h"

namespace dslab {

// A link from one node to another. Its ID on the wire is "from->to".
struct Edge {
    NodeId from = kNoNode;
    NodeId to = kNoNode;
};

// A named node variable of the algorithm, such as "node" or "curr".
struct NamedPointer {
    std::string name;
    NodeId node = kNoNode;  // kNoNode means NIL
};

// What a step touched, so the UI can highlight it.
struct Highlight {
    std::vector<NodeId> nodes;
    std::vector<Edge> edges;
};

struct Frame {
    int index = 0;
    Event event = Event::Begin;
    std::string stepId;       // stable pseudocode step, e.g. "stack.push.create"
    std::string explanation;  // one English sentence
    Highlight highlight;
    std::vector<NamedPointer> pointers;
    std::vector<Value> output;    // traversals only (later milestones)
    std::vector<NodeId> pending;  // level-order traversal only (later milestones)
    Snapshot snapshot;            // the whole structure right after this step
};

}  // namespace dslab
