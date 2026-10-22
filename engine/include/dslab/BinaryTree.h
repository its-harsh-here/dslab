// An ordinary binary tree (docs/operation-contract.md, 2.4 and 4): the user
// decides where every node goes, values have no order, duplicates are allowed.
//
//     root -> [30]
//               +-- left:  [20]
//               +-- right: [40]
//
// Nodes store left, right and parent. Every operation records frames into a
// Trace while it runs (rule 7) and changes nothing when it rejects (rule 8).
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

class BinaryTree {
public:
    BinaryTree() = default;

    // Nodes link to each other by address, so a tree is never copied or moved.
    BinaryTree(const BinaryTree&) = delete;
    BinaryTree& operator=(const BinaryTree&) = delete;
    BinaryTree(BinaryTree&&) = delete;
    BinaryTree& operator=(BinaryTree&&) = delete;

    Result<NodeId> addRoot(Value value, Trace& trace);
    Result<NodeId> addChild(NodeId parentId, Side side, Value value, Trace& trace);
    // data: the removed IDs in the order they were freed (post-order)
    Result<std::vector<NodeId>> deleteSubtree(NodeId id, Trace& trace);
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

    // Frees 'node' and everything below it, children before parents
    // (post-order), recording one frame per node freed.
    void freeSubtree(Node* node, const std::string& stepId, std::vector<NodeId>& removed, Trace& trace);

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
