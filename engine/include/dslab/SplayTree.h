// A splay tree: a binary search tree that reorganises itself on every access
// (Sleator and Tarjan). After insert, search or delete, the node that was
// reached is rotated up to the root by "splay steps":
//
//   zig      the node's parent is the root: one rotation;
//   zig-zig  node and parent are both left (or both right) children: rotate
//            the grandparent first, then the parent;
//   zig-zag  one is a left child and the other a right child: rotate the
//            parent first, then the grandparent.
//
// Nothing is stored besides left, right and parent: no heights, no colors.
// Recently used values end up near the root, and any sequence of m operations
// costs O(m log n) in total (amortised O(log n) each), although one single
// operation can cost O(n).
//
// insert and search are the BST's, followed by splaying the node reached (a
// failed search splays the last node on its path). delete is the BST's delete
// (CLRS, the in-order successor node relinked, never value-copied), followed by
// splaying the deleted node's parent. Duplicates are rejected.
//
// Every operation records frames into a Trace while it runs (rule 7) and
// changes nothing when it rejects (rule 8).
#pragma once

#include <string>
#include <vector>

#include "dslab/Bst.h"  // BstSearchResult
#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/TreeTraversal.h"
#include "dslab/Types.h"

namespace dslab {

class SplayTree {
public:
    SplayTree() = default;

    // Nodes link to each other by address, so a tree is never copied or moved.
    SplayTree(const SplayTree&) = delete;
    SplayTree& operator=(const SplayTree&) = delete;
    SplayTree(SplayTree&&) = delete;
    SplayTree& operator=(SplayTree&&) = delete;

    Result<NodeId> insert(Value value, Trace& trace);     // data: the new node's ID
    Result<NodeValue> remove(Value value, Trace& trace);  // "delete"; data: the removed node
    // Not const: the node reached is splayed to the root.
    Result<BstSearchResult> search(Value value, Trace& trace);
    Result<TraversalResult> traverse(TraversalOrder order, Trace& trace) const;
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

    // A copy of every node, in ascending ID order.
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
        Node* left = nullptr;    // non-owning: the pool owns every node
        Node* right = nullptr;   // non-owning: the pool owns every node
        Node* parent = nullptr;  // non-owning: the pool owns every node
    };

    // CLRS TRANSPLANT: puts the subtree rooted at 'v' where the subtree rooted
    // at 'u' was. 'v' may be NIL. Counts the links it writes.
    void transplant(Node* u, Node* v, Trace& trace);

    // CLRS LEFT-ROTATE / RIGHT-ROTATE: 'x' moves down, its right (left)
    // child moves up into its place. One frame each, under 'stepId'.
    // 'label' names the splay step ("Zig-zig, step 1"); 'splayed' is the node
    // being moved to the root, shown as the pointer "x".
    void rotateLeft(Node* x, const std::string& stepId, const std::string& label, const Node* splayed, Trace& trace);
    void rotateRight(Node* x, const std::string& stepId, const std::string& label, const Node* splayed, Trace& trace);

    // Rotates 'x' up until it is the root. 'operation' starts the step IDs
    // ("splay.insert"), so each operation's listing shows its own splay lines.
    void splay(Node* x, const std::string& operation, Trace& trace);

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
