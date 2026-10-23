// An AVL tree (docs/avl.md): the BST of milestone 3 (CLRS, nodes with left,
// right and parent) plus a height in every node and a fix-up walk that keeps
// every balance in -1..+1.
//
//   height(NIL) = 0, height(leaf) = 1
//   balance(n)  = height(n.left) - height(n.right)
//
// Insert and delete are the BST's; afterwards fixUp walks from the lowest
// changed node to the root, updating heights and rebalancing with rotations.
// Rotations relink nodes, so every value keeps its node ID. Every operation
// records frames while it runs (rule 7) and changes nothing when it rejects
// (rule 8).
#pragma once

#include <string>

#include "dslab/Bst.h"
#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/TreeTraversal.h"
#include "dslab/Types.h"

namespace dslab {

class AvlTree {
public:
    AvlTree() = default;

    // Nodes link to each other by address, so a tree is never copied or moved.
    AvlTree(const AvlTree&) = delete;
    AvlTree& operator=(const AvlTree&) = delete;
    AvlTree(AvlTree&&) = delete;
    AvlTree& operator=(AvlTree&&) = delete;

    Result<NodeId> insert(Value value, Trace& trace);     // data: the new node's ID
    Result<NodeValue> remove(Value value, Trace& trace);  // "delete"; data: the removed node
    Result<BstSearchResult> search(Value value, Trace& trace) const;
    Result<TraversalResult> traverse(TraversalOrder order, Trace& trace) const;
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

    // A copy of every node, in ascending ID order, with heights and balances.
    Snapshot snapshot() const;

    // Replaces the whole structure with 'snapshot' (undo, redo, import): the
    // same IDs, values, links and ID counter. Only called with a snapshot that
    // passed validateSnapshot(), so every link names a listed node. No frames.
    void restore(const Snapshot& snapshot);

    int size() const { return pool_.size(); }

private:
    struct Node {
        NodeId id = kNoNode;
        Value value = 0;
        int height = 1;
        Node* left = nullptr;    // non-owning: the pool owns every node
        Node* right = nullptr;   // non-owning: the pool owns every node
        Node* parent = nullptr;  // non-owning: the pool owns every node
    };

    // height(NIL) = 0; balance = height(left) - height(right).
    static int height(const Node* node) { return node == nullptr ? 0 : node->height; }
    static int balance(const Node* node) { return height(node->left) - height(node->right); }
    static void updateHeight(Node* node);

    // Walks from 'start' up to the root, updating every height and rebalancing
    // any node whose balance is +2 or -2.
    void fixUp(Node* start, Trace& trace);
    // Chooses the case (LL, LR, RR, RL), rotates, and returns the new root of n's subtree.
    Node* rebalance(Node* n, Trace& trace);
    // CLRS rotations: they also relink the parent (or root), so the tree is
    // consistent after each one. 'label' names the case in the frame.
    Node* rotateLeft(Node* x, const std::string& label, Trace& trace);
    Node* rotateRight(Node* y, const std::string& label, Trace& trace);

    // As in the BST.
    void transplant(Node* u, Node* v, Trace& trace);
    void stepDown(Value value, Node*& parent, Node*& curr, const char* operation, Trace& trace) const;
    void freeSubtree(Node* node, int& removed, Trace& trace);

    // Ends a frame explanation: adds "; temporarily unbalanced: 30 (+2)" when
    // any node's balance is out of range, then the full stop (docs/avl.md).
    std::string sentence(const std::string& text) const;

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
