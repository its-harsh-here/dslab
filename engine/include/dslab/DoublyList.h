// A doubly linked list with head and tail pointers:
//
//     head -> [10] <-> [20] <-> [30] <- tail
//
// Every node stores prev and next. Compared with the singly linked list this
// buys two things, and both are visible in the frames: a selected node is
// deleted in O(1) (no walk to find its predecessor), and insertAt walks from
// whichever end is nearer. Duplicates are allowed.
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

class DoublyList {
public:
    DoublyList() = default;

    // Nodes link to each other by address, so a list is never copied or moved.
    DoublyList(const DoublyList&) = delete;
    DoublyList& operator=(const DoublyList&) = delete;
    DoublyList(DoublyList&&) = delete;
    DoublyList& operator=(DoublyList&&) = delete;

    Result<NodeId> insertAt(int index, Value value, Trace& trace);  // data: the new node's ID
    Result<NodeValue> deleteNode(NodeId id, Trace& trace);          // data: the removed node
    Result<ListSearchResult> search(Value value, Trace& trace) const;
    Status reverse(Trace& trace);
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
        Node* prev = nullptr;  // non-owning: the pool owns every node
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
    Node* tail_ = nullptr;  // non-owning: the pool owns every node
};

}  // namespace dslab
