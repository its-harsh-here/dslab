// Owns every node of one structure (rule 4, docs/operation-contract.md section 10).
//
// create() plays the role of the textbook 'new' and destroy() the role of
// 'delete'. The pool is the only owner: nodes never own each other, and every
// link between them is a non-owning raw pointer. Destroying the pool frees all
// of its nodes automatically (RAII).
#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "dslab/Types.h"

namespace dslab {

// 'Node' must have the members 'NodeId id' and 'Value value'.
template <typename Node>
class NodePool {
public:
    // 'firstId' is the ID the next created node gets. Tests start it near the
    // maximum to check what happens when IDs run out.
    explicit NodePool(std::int64_t firstId = 1) : nextId_(firstId) {}

    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;

    // True while another node can get an ID. IDs are never reused, so they can
    // run out after 2,147,483,647 creations.
    bool hasFreeId() const { return nextId_ <= std::numeric_limits<NodeId>::max(); }

    // Creates a node holding 'value' and gives it the next ID.
    // Precondition: hasFreeId(). The pool keeps ownership.
    Node* create(Value value) {
        std::unique_ptr<Node> owned = std::make_unique<Node>();
        owned->id = static_cast<NodeId>(nextId_);
        owned->value = value;
        nextId_ = nextId_ + 1;

        Node* node = owned.get();  // non-owning: the pool owns every node
        // IDs only grow, so appending keeps the vector in ascending ID order.
        nodes_.push_back(std::move(owned));
        return node;
    }

    // Frees 'node'. The caller must already have removed every link to it.
    void destroy(Node* node) {
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].get() == node) {
                nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    int size() const { return static_cast<int>(nodes_.size()); }

    // Every owned node, in ascending ID order (used to build snapshots).
    const std::vector<std::unique_ptr<Node>>& nodes() const { return nodes_; }

    // The ID the next created node will get.
    std::int64_t nextId() const { return nextId_; }

private:
    std::vector<std::unique_ptr<Node>> nodes_;
    // 64-bit so it can step past the largest NodeId without overflowing.
    std::int64_t nextId_;
};

}  // namespace dslab
