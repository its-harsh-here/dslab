// The four traversals, shared by the binary tree and the BST. They only read
// the tree, so sharing them is safe (decision D11 in operation-contract.md);
// every algorithm that changes a tree stays in that tree's own class.
//
// 'Tree' must have snapshot(). 'Node' must have id, value, left and right.
//
//   preorder(node):  if node = NIL: return; visit(node); preorder(node.left); preorder(node.right)
//   inorder(node):   if node = NIL: return; inorder(node.left); visit(node); inorder(node.right)
//   postorder(node): if node = NIL: return; postorder(node.left); postorder(node.right); visit(node)
//   levelorder(root):
//     queue <- [root]
//     while queue is not empty:
//       node <- take from the front of queue; visit(node)
//       add node.left and node.right (if not NIL) to the back of queue
//
// A call on NIL returns at once and gets no frame. Step IDs are
// "<structure>.<order>.<step>", e.g. "bst.inorder.visit".
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "dslab/Trace.h"
#include "dslab/Types.h"

namespace dslab {

enum class TraversalOrder { Pre, In, Post, Level };

// What a traversal returns: values and node IDs in visit order.
struct TraversalResult {
    std::vector<Value> values;
    std::vector<NodeId> nodeIds;
};

template <typename Tree, typename Node>
class TreeTraversal {
public:
    // 'structure' is the step-ID prefix ("binaryTree"); 'treeName' is used in
    // sentences ("binary tree").
    TreeTraversal(const Tree& tree, std::string structure, std::string treeName, Trace& trace)
        : tree_(tree), structure_(std::move(structure)), treeName_(std::move(treeName)), trace_(trace) {}

    TraversalResult run(TraversalOrder order, const Node* root, int size) {
        order_ = order;
        const std::string title = orderTitle();
        trace_.frame(tree_.snapshot(), Event::Begin, step("enter"),
                     title + " traversal of " +
                         (size == 0 ? "an empty " + treeName_
                                    : "a " + treeName_ + " that holds " + nodeCount(size)) + ".");

        if (root == nullptr) {
            trace_.frame(tree_.snapshot(), Event::Done, step("done"),
                         "The " + treeName_ + " is empty, so there is nothing to visit.");
            return result_;
        }

        if (order == TraversalOrder::Level) {
            levelOrder(root);
        } else {
            arrive(root, nullptr);
            depthFirst(root);
        }

        trace_.frame(tree_.snapshot(), Event::Done, step("done"),
                     title + " traversal finished: visited " +
                         nodeCount(static_cast<int>(result_.values.size())) + ".",
                     {}, {}, result_.values);
        return result_;
    }

private:
    // The pre-, in- and post-order recursion; only the position of visit() differs.
    void depthFirst(const Node* node) {
        if (order_ == TraversalOrder::Pre) visit(node);
        if (node->left != nullptr) {
            arrive(node->left, node);
            depthFirst(node->left);
        }
        if (order_ == TraversalOrder::In) visit(node);
        if (node->right != nullptr) {
            arrive(node->right, node);
            depthFirst(node->right);
        }
        if (order_ == TraversalOrder::Post) visit(node);
    }

    // Level order, with a plain array used as the queue: nodes are added at the
    // back and taken from position 'front'. (std::queue is deliberately not used.)
    void levelOrder(const Node* root) {
        std::vector<const Node*> queue{root};  // non-owning: the tree's pool owns every node
        std::size_t front = 0;
        while (front < queue.size()) {
            const Node* node = queue[front];  // non-owning: the tree's pool owns every node
            front = front + 1;
            trace_.counters.nodesVisited++;
            result_.values.push_back(node->value);
            result_.nodeIds.push_back(node->id);

            Highlight highlight{{node->id}, {}};
            std::string added;
            if (node->left != nullptr) {
                queue.push_back(node->left);
                highlight.edges.push_back(Edge{node->id, node->left->id});
                added = "its left child " + text(node->left->value);
            }
            if (node->right != nullptr) {
                queue.push_back(node->right);
                highlight.edges.push_back(Edge{node->id, node->right->id});
                added += (added.empty() ? "its right child " : " and its right child ") + text(node->right->value);
            }

            std::vector<NodeId> waiting;
            for (std::size_t i = front; i < queue.size(); ++i) {
                waiting.push_back(queue[i]->id);
            }
            trace_.frame(tree_.snapshot(), Event::Visit, step("visit"),
                         "Take " + text(node->value) + " from the front of the queue and visit it; " +
                             (added.empty() ? std::string("it has no children.") : "add " + added + " to the back."),
                         highlight, {{"node", node->id}}, result_.values, waiting);
        }
    }

    // The traversal arrives at 'node' (from 'parent', or at the root).
    void arrive(const Node* node, const Node* parent) {
        trace_.counters.nodesVisited++;
        if (parent == nullptr) {
            trace_.frame(tree_.snapshot(), Event::Move, step("start"),
                         "Start at the root, " + text(node->value) + ".", {{node->id}, {}},
                         {{"node", node->id}}, result_.values);
            return;
        }
        const bool left = (parent->left == node);
        trace_.frame(tree_.snapshot(), Event::Move, step(left ? "goLeft" : "goRight"),
                     "Go to " + text(node->value) + ", the " + (left ? "left" : "right") + " child of " +
                         text(parent->value) + ".",
                     {{node->id}, {{parent->id, node->id}}}, {{"node", node->id}}, result_.values);
    }

    void visit(const Node* node) {
        result_.values.push_back(node->value);
        result_.nodeIds.push_back(node->id);
        trace_.frame(tree_.snapshot(), Event::Visit, step("visit"),
                     "Visit " + text(node->value) + ": add it to the output.", {{node->id}, {}},
                     {{"node", node->id}}, result_.values);
    }

    std::string step(const char* name) const {
        const char* operation = order_ == TraversalOrder::Pre    ? "preorder"
                                : order_ == TraversalOrder::In   ? "inorder"
                                : order_ == TraversalOrder::Post ? "postorder"
                                                                 : "levelorder";
        return structure_ + "." + operation + "." + name;
    }

    std::string orderTitle() const {
        switch (order_) {
            case TraversalOrder::Pre: return "Pre-order";
            case TraversalOrder::In: return "In-order";
            case TraversalOrder::Post: return "Post-order";
            case TraversalOrder::Level: return "Level-order";
        }
        return "Tree";
    }

    static std::string text(Value value) { return std::to_string(value); }

    static std::string nodeCount(int count) {
        return std::to_string(count) + (count == 1 ? " node" : " nodes");
    }

    const Tree& tree_;  // the tree being read (not owned)
    std::string structure_;
    std::string treeName_;
    Trace& trace_;
    TraversalOrder order_ = TraversalOrder::Pre;
    TraversalResult result_;
};

}  // namespace dslab
