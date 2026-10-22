// A queue built from linked nodes (docs/operation-contract.md, 2.2):
//
//     front -> [10] -> [20] -> [30] -> NIL
//                               ^
//                              rear
//
// enqueue links after rear, dequeue unlinks the front node. Every operation
// records frames into a Trace while it runs (rule 7) and changes nothing when
// it rejects (rule 8).
#pragma once

#include <string>

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

class Queue {
public:
    Queue() = default;

    // Nodes link to each other by address, so a Queue is never copied or moved.
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue&&) = delete;

    Result<NodeId> enqueue(Value value, Trace& trace);  // data: the new node's ID
    Result<NodeValue> dequeue(Trace& trace);            // data: the removed node
    Result<NodeValue> front(Trace& trace);              // data: the front node
    Result<int> clear(Trace& trace);                    // data: how many nodes were freed

    // A copy of every node, in ascending ID order.
    Snapshot snapshot() const;

    int size() const { return pool_.size(); }

private:
    struct Node {
        NodeId id = kNoNode;
        Value value = 0;
        Node* next = nullptr;  // non-owning: the pool owns every node
    };

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const char* stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    NodePool<Node> pool_;
    Node* front_ = nullptr;  // non-owning: the pool owns every node
    Node* rear_ = nullptr;   // non-owning: the pool owns every node
};

}  // namespace dslab
