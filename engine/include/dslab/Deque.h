// A double-ended queue built from doubly linked nodes:
//
//     front -> [10] <-> [20] <-> [30] <- rear
//
// Values can be added, removed and read at both ends in O(1): every node
// stores "prev" as well as "next", so the node before the rear is one step
// away. Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include <string>

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

class Deque {
public:
    // Which end of the deque an operation works at.
    enum class End { Front, Rear };

    Deque() = default;

    // Nodes link to each other by address, so a Deque is never copied or moved.
    Deque(const Deque&) = delete;
    Deque& operator=(const Deque&) = delete;
    Deque(Deque&&) = delete;
    Deque& operator=(Deque&&) = delete;

    Result<NodeId> push(End end, Value value, Trace& trace);  // pushFront / pushBack
    Result<NodeValue> pop(End end, Trace& trace);             // popFront / popBack
    Result<NodeValue> peek(End end, Trace& trace);            // peekFront / peekBack
    Result<int> clear(Trace& trace);                          // data: how many nodes were freed

    // A copy of every node, in ascending ID order.
    Snapshot snapshot() const;

    // Replaces the whole structure with 'snapshot' (undo, redo, import). Only
    // called with a snapshot that passed validateSnapshot(). No frames.
    void restore(const Snapshot& snapshot);

    int size() const { return pool_.size(); }

private:
    struct Node {
        NodeId id = kNoNode;
        Value value = 0;
        Node* prev = nullptr;  // non-owning: the pool owns every node
        Node* next = nullptr;  // non-owning: the pool owns every node
    };

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const std::string& stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    NodePool<Node> pool_;
    Node* front_ = nullptr;  // non-owning: the pool owns every node
    Node* rear_ = nullptr;   // non-owning: the pool owns every node
};

}  // namespace dslab
