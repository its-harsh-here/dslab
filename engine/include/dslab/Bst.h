// A binary search tree in the CLRS style (docs/operation-contract.md, 2.5 and 4):
// smaller values go left, larger go right, duplicates are rejected. Nodes store
// left, right and parent. Deleting a node with two children moves its in-order
// successor node into its place (the node is relinked, values are never copied,
// so every value keeps its ID).
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
#include "dslab/TreeTraversal.h"
#include "dslab/Types.h"

namespace dslab {

// What a BST search found.
struct BstSearchResult {
    bool found = false;
    NodeId nodeId = kNoNode;  // kNoNode when not found
};

class Bst {
public:
    Bst() = default;

    // Nodes link to each other by address, so a tree is never copied or moved.
    Bst(const Bst&) = delete;
    Bst& operator=(const Bst&) = delete;
    Bst(Bst&&) = delete;
    Bst& operator=(Bst&&) = delete;

    Result<NodeId> insert(Value value, Trace& trace);     // data: the new node's ID
    Result<NodeValue> remove(Value value, Trace& trace);  // "delete"; data: the removed node
    Result<BstSearchResult> search(Value value, Trace& trace) const;
    Result<TraversalResult> traverse(TraversalOrder order, Trace& trace) const;
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

    // A copy of every node, in ascending ID order.
    Snapshot snapshot() const;

    int size() const { return pool_.size(); }

private:
    struct Node {
        NodeId id = kNoNode;
        Value value = 0;
        Node* left = nullptr;    // non-owning: the pool owns every node
        Node* right = nullptr;   // non-owning: the pool owns every node
        Node* parent = nullptr;  // non-owning: the pool owns every node
    };

    // CLRS TRANSPLANT: puts the subtree rooted at 'v' where the subtree rooted
    // at 'u' was. 'v' may be NIL. Counts the links it writes.
    void transplant(Node* u, Node* v, Trace& trace);

    // Records the comparison of 'value' with 'curr' while walking down the
    // tree, and moves 'curr' to the chosen child (also updating 'parent').
    void stepDown(Value value, Node*& parent, Node*& curr, const char* operation, Trace& trace) const;

    // Frees 'node' and everything below it, children before parents.
    void freeSubtree(Node* node, int& removed, Trace& trace);

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const char* stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    NodePool<Node> pool_;
    Node* root_ = nullptr;  // non-owning: the pool owns every node
};

}  // namespace dslab
