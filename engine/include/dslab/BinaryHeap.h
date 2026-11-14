// A binary heap stored in an array, the way a priority queue is implemented:
//
//     slot:   0   1   2   3   4            10
//     value: 10  20  15  40  30          /    \       parent(i) = (i - 1) / 2
//                                      20      15     left(i)   = 2i + 1
//                                     /  \            right(i)  = 2i + 2
//                                   40    30
//
// The same class is a min-heap (every parent <= its children, smallest value
// at the root) or a max-heap (every parent >= its children), chosen when it
// is constructed. insert appends at the end and sifts up; extract takes the
// root, moves the last node there and sifts down. Both are O(log n).
//
// The array holds pointers to nodes, and a swap exchanges two slots, so a
// value keeps its node ID while it moves (rule 3). Snapshots report each
// node's slot ("index") and the left/right/parent links that follow from it,
// so the heap can be drawn as a tree and as an array at once. Duplicates are
// allowed. We do not use std::priority_queue, std::push_heap or std::pop_heap.
//
// Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include <string>
#include <vector>

#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

class BinaryHeap {
public:
    // 'kind' is StructureKind::MinHeap or StructureKind::MaxHeap.
    explicit BinaryHeap(StructureKind kind);

    // Slots point at nodes by address, so a heap is never copied or moved.
    BinaryHeap(const BinaryHeap&) = delete;
    BinaryHeap& operator=(const BinaryHeap&) = delete;
    BinaryHeap(BinaryHeap&&) = delete;
    BinaryHeap& operator=(BinaryHeap&&) = delete;

    Result<NodeId> insert(Value value, Trace& trace);  // data: the new node's ID
    Result<NodeValue> extract(Trace& trace);           // data: the removed root
    Result<NodeValue> peek(Trace& trace) const;        // data: the root
    Result<int> clear(Trace& trace);                   // data: how many nodes were freed

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
        int index = 0;  // the array slot this node sits in
    };

    // True when 'a' must be above 'b': a < b in a min-heap, a > b in a max-heap.
    bool goesAbove(Value a, Value b) const { return isMin_ ? a < b : a > b; }

    // Exchanges the nodes in slots i and j (two array writes).
    void swapSlots(int i, int j, Trace& trace);

    // The ID of the node in 'slot', or kNoNode when the slot is past the end.
    NodeId idAt(int slot) const;

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const std::string& stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    const StructureKind kind_;
    const bool isMin_;
    const std::string prefix_;  // "minHeap" or "maxHeap": the start of every step ID
    const std::string name_;    // "min-heap" or "max-heap": used in sentences

    NodePool<Node> pool_;
    // The heap array. Non-owning: the pool owns every node. A plain array of
    // pointers; every heap rule is implemented by hand in BinaryHeap.cpp.
    std::vector<Node*> slots_;
};

}  // namespace dslab
