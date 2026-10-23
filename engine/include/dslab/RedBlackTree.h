// A red-black tree (docs/red-black.md), exactly as in CLRS chapter 13: nodes
// with left, right, parent and a color, and one shared black NIL sentinel.
//
//   1. every node is red or black      2. the root is black
//   3. every leaf (NIL) is black       4. a red node has no red child
//   5. every path from a node down to NIL has the same number of black nodes
//
// Insert and delete are the BST's (CLRS RB-INSERT, RB-DELETE with
// RB-TRANSPLANT), followed by a fix-up that restores the properties with
// recolors and rotations. Rotations relink nodes, so every value keeps its node
// ID. Every operation records frames while it runs (rule 7) and changes nothing
// when it rejects (rule 8).
#pragma once

#include <string>
#include <vector>

#include "dslab/Bst.h"
#include "dslab/NodePool.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"
#include "dslab/TreeTraversal.h"
#include "dslab/Types.h"

namespace dslab {

class RedBlackTree {
public:
    RedBlackTree();

    // Nodes (and the sentinel) are linked by address, so a tree is never copied or moved.
    RedBlackTree(const RedBlackTree&) = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;
    RedBlackTree(RedBlackTree&&) = delete;
    RedBlackTree& operator=(RedBlackTree&&) = delete;

    Result<NodeId> insert(Value value, Trace& trace);     // data: the new node's ID
    Result<NodeValue> remove(Value value, Trace& trace);  // "delete"; data: the removed node
    Result<BstSearchResult> search(Value value, Trace& trace) const;
    Result<TraversalResult> traverse(TraversalOrder order, Trace& trace) const;
    Result<int> clear(Trace& trace);  // data: how many nodes were freed

    // A copy of every node, in ascending ID order, with colors. Links to the
    // sentinel are written as kNoNode; nilColor is the sentinel's color.
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
        Color color = Color::Red;
        Node* left = nullptr;    // non-owning: the pool owns every node (or it is &nil_)
        Node* right = nullptr;   // non-owning: the pool owns every node (or it is &nil_)
        Node* parent = nullptr;  // non-owning: the pool owns every node (or it is &nil_)
    };

    void insertFixup(Node* z, Trace& trace);
    void deleteFixup(Node* x, Trace& trace);

    // CLRS LEFT-ROTATE and RIGHT-ROTATE: one frame for the finished rotation.
    // 'label' names the fix-up case; 'pointers' are the caller's variables.
    void rotateLeft(Node* x, const std::string& label, const std::vector<NamedPointer>& pointers, Trace& trace);
    void rotateRight(Node* x, const std::string& label, const std::vector<NamedPointer>& pointers, Trace& trace);

    // CLRS RB-TRANSPLANT: like the BST's, but it always sets v.parent, even when v is NIL.
    void transplant(Node* u, Node* v, Trace& trace);

    // Sets one node's color and records one recolor frame.
    void recolor(Node* node, Color color, const std::string& stepId, const std::string& words,
                 const std::vector<NamedPointer>& pointers, Trace& trace);

    // The comparison at 'node' on the walk down: records a compare frame and
    // returns the child the walk moves to. 'cursor' names the moving variable.
    Node* stepDown(Value value, Node* node, const std::string& operation, const char* cursor, Trace& trace) const;
    void freeSubtree(Node* node, int& removed, Trace& trace);

    // Ends a frame explanation: adds "; temporary violation: ..." when the tree
    // reachable from the root breaks property 2, 4 or 5, then the full stop.
    std::string sentence(const std::string& text) const;
    // The first broken property ("red root", "red 10 under red 20",
    // "black heights differ under 30"), or "" when all hold.
    std::string violation() const;
    // The black height of 'node' (NIL counts 1), or -1 when two paths below it
    // differ; 'problem' gets the first violation found.
    int blackHeight(const Node* node, std::string& problem) const;

    // A node's ID, or kNoNode for the sentinel.
    NodeId idOf(const Node* node) const { return node == &nil_ ? kNoNode : node->id; }
    // A node's value as text, or "NIL" for the sentinel.
    std::string nameOf(const Node* node) const;

    // Records a Reject frame and returns the matching error.
    template <typename T>
    Result<T> reject(Trace& trace, const char* stepId, ErrorCode code,
                     const std::string& message) const {
        trace.frame(snapshot(), Event::Reject, stepId, message);
        return Result<T>{errorStatus(code, message), T{}};
    }

    NodePool<Node> pool_;
    // The NIL sentinel. The tree owns it as a member (it is not in the pool and
    // has no ID); it is always black. Every missing child and the root's parent
    // point to it. Its parent field is written by delete, as in CLRS.
    Node nil_;
    Node* root_;  // non-owning: the pool owns every node (or it is &nil_ when the tree is empty)
};

}  // namespace dslab
