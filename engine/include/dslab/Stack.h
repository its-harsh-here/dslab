// A stack built from linked nodes (docs/operation-contract.md, 2.1):
//
//     top -> [30] -> [20] -> [10] -> NIL
//
// Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

class Stack {
public:
    Stack() = default;

    // Nodes link to each other by address, so a Stack is never copied or moved.
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;
    Stack(Stack&&) = delete;
    Stack& operator=(Stack&&) = delete;

    Result<NodeId> push(Value value, Trace& trace);  // data: the new node's ID
    Result<NodeValue> pop(Trace& trace);             // data: the removed node
    Result<NodeValue> peek(Trace& trace);            // data: the top node
    Result<int> clear(Trace& trace);                 // data: how many nodes were freed

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
    Node* top_ = nullptr;  // non-owning: the pool owns every node
};

}  // namespace dslab
