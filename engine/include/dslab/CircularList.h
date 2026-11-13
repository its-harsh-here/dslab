// A circular singly linked list with one pointer, tail:
//
//     tail -> [30] -> [10] -> [20] -> back to [30]
//              ^ last   ^ first (tail.next)
//
// The last node's next points back at the first, so there is no NIL link
// anywhere in a non-empty list. Keeping "tail" (not "head") makes both ends
// reachable in O(1): the first node is tail.next. rotate() moves the first
// node to the back by moving tail one step, without touching a link.
// Duplicates are allowed.
//
// Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include <string>

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/SinglyLinkedList.h"  // ListSearchResult
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

class CircularList {
public:
    CircularList() = default;

    // Nodes link to each other by address, so a list is never copied or moved.
    CircularList(const CircularList&) = delete;
    CircularList& operator=(const CircularList&) = delete;
    CircularList(CircularList&&) = delete;
    CircularList& operator=(CircularList&&) = delete;

    // insertFront / insertBack; data: the new node's ID.
    Result<NodeId> insert(bool atFront, Value value, Trace& trace);
    Result<NodeValue> deleteNode(NodeId id, Trace& trace);  // data: the removed node
    Result<ListSearchResult> search(Value value, Trace& trace) const;
    Status rotate(Trace& trace);
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

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
    Node* tail_ = nullptr;  // non-owning: the last node; tail_->next is the first
};

}  // namespace dslab
