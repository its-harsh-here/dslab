// A singly linked list (docs/operation-contract.md, 2.3):
//
//     head -> [10] -> [20] -> [30] -> NIL
//
// The list stores only `head`. (The page's TAIL marker is drawn at the last
// node for readability; there is no tail pointer.) Every operation records
// frames into a Trace while it runs (rule 7) and changes nothing when it
// rejects (rule 8).
#pragma once

#include <string>

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

// What search found: the first node from head holding the value.
struct ListSearchResult {
    bool found = false;
    NodeId nodeId = kNoNode;  // kNoNode when not found
    int index = -1;           // -1 when not found
};

class SinglyLinkedList {
public:
    SinglyLinkedList() = default;

    // Nodes link to each other by address, so a list is never copied or moved.
    SinglyLinkedList(const SinglyLinkedList&) = delete;
    SinglyLinkedList& operator=(const SinglyLinkedList&) = delete;
    SinglyLinkedList(SinglyLinkedList&&) = delete;
    SinglyLinkedList& operator=(SinglyLinkedList&&) = delete;

    Result<NodeId> insertAt(int index, Value value, Trace& trace);  // data: the new node's ID
    Result<NodeValue> deleteNode(NodeId id, Trace& trace);          // data: the removed node
    Result<ListSearchResult> search(Value value, Trace& trace);
    Status reverse(Trace& trace);
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

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
    Node* head_ = nullptr;  // non-owning: the pool owns every node
};

}  // namespace dslab
