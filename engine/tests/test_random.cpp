// Seeded random operation sequences for the structures that had no oracle
// test yet: queue, singly linked list, ordinary binary tree and BST. Each is
// compared after every operation with a model built from standard containers
// (allowed as test oracles, CLAUDE.md rule 1); the structures themselves stay
// ours. Invalid requests are mixed in. Every request goes through test::run(),
// so the structure and frame checkers also run after each one. The traversals
// of every tree are compared with traversals computed here from the snapshot.
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Run.h"

using namespace dslab;
using test::booleanData;
using test::integerData;
using test::listData;

namespace {

Request request(StructureKind structure, Operation operation, Value value = 0) {
    Request r;
    r.structure = structure;
    r.operation = operation;
    r.value = value;
    return r;
}

const NodeView& find(const Snapshot& snapshot, NodeId id) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.id == id) return node;
    }
    FAIL("no node " << id);
    return snapshot.nodes.front();
}

// (ID, value) pairs along the next links from 'entry'.
std::vector<std::pair<NodeId, Value>> chain(const Snapshot& snapshot, NodeId entry) {
    std::vector<std::pair<NodeId, Value>> items;
    for (NodeId id = entry; id != kNoNode && items.size() <= snapshot.nodes.size(); id = find(snapshot, id).next) {
        items.emplace_back(id, find(snapshot, id).value);
    }
    return items;
}

// The four traversals, computed here from the snapshot's links alone.
std::vector<std::int64_t> traversal(const Snapshot& snapshot, Operation order) {
    std::vector<std::int64_t> out;
    if (order == Operation::Levelorder) {
        std::deque<NodeId> waiting;
        if (snapshot.root != kNoNode) waiting.push_back(snapshot.root);
        while (!waiting.empty()) {
            const NodeView& node = find(snapshot, waiting.front());
            waiting.pop_front();
            out.push_back(node.value);
            if (node.left != kNoNode) waiting.push_back(node.left);
            if (node.right != kNoNode) waiting.push_back(node.right);
        }
        return out;
    }
    const std::function<void(NodeId)> walk = [&](NodeId id) {
        if (id == kNoNode) return;
        const NodeView& node = find(snapshot, id);
        if (order == Operation::Preorder) out.push_back(node.value);
        walk(node.left);
        if (order == Operation::Inorder) out.push_back(node.value);
        walk(node.right);
        if (order == Operation::Postorder) out.push_back(node.value);
    };
    walk(snapshot.root);
    return out;
}

// Every traversal of the engine agrees with the one computed from its snapshot.
void checkTraversals(Engine& engine, StructureKind structure) {
    const Snapshot snapshot = engine.snapshot(structure);
    for (const Operation order : {Operation::Preorder, Operation::Inorder, Operation::Postorder, Operation::Levelorder}) {
        CAPTURE(static_cast<int>(order));
        REQUIRE(listData(test::run(engine, request(structure, order)), "values") == traversal(snapshot, order));
    }
}

}  // namespace

TEST_CASE("queue: 10,000 seeded random operations agree with std::deque") {
    Engine engine;
    std::deque<std::pair<NodeId, Value>> oracle;  // front first: (ID, value)
    NodeId nextId = 1;
    std::mt19937 random(8001);
    for (int step = 0; step < 10000; ++step) {
        CAPTURE(step);
        const unsigned kind = random() % 100;
        const Value value = static_cast<Value>(random() % 1000) - 500;
        if (kind < 50) {
            const Response response = test::run(engine, request(StructureKind::Queue, Operation::Enqueue, value));
            REQUIRE(response.status.ok() == (oracle.size() < 200));
            if (response.status.ok()) {
                REQUIRE(integerData(response, "nodeId") == nextId);
                oracle.emplace_back(nextId++, value);
            } else {
                REQUIRE(response.status.code == ErrorCode::CapacityExceeded);
            }
        } else if (kind < 85) {
            const Response response = test::run(engine, request(StructureKind::Queue, Operation::Dequeue));
            REQUIRE(response.status.ok() == !oracle.empty());
            if (!oracle.empty()) {
                REQUIRE(integerData(response, "nodeId") == oracle.front().first);
                REQUIRE(integerData(response, "value") == oracle.front().second);
                oracle.pop_front();
            }
        } else if (kind < 97) {
            const Response response = test::run(engine, request(StructureKind::Queue, Operation::Front));
            REQUIRE(response.status.ok() == !oracle.empty());
            if (!oracle.empty()) REQUIRE(integerData(response, "value") == oracle.front().second);
        } else {
            const Response response = test::run(engine, request(StructureKind::Queue, Operation::Clear));
            REQUIRE(integerData(response, "removedCount") == static_cast<std::int64_t>(oracle.size()));
            oracle.clear();
        }
        const Snapshot snapshot = engine.snapshot(StructureKind::Queue);
        REQUIRE(chain(snapshot, snapshot.front) == std::vector<std::pair<NodeId, Value>>(oracle.begin(), oracle.end()));
        REQUIRE(snapshot.rear == (oracle.empty() ? kNoNode : oracle.back().first));
        REQUIRE(snapshot.nextId == nextId);
    }
}

TEST_CASE("list: 5,000 seeded random operations agree with std::vector") {
    Engine engine;
    std::vector<std::pair<NodeId, Value>> oracle;  // head first: (ID, value)
    NodeId nextId = 1;
    std::mt19937 random(8002);
    for (int step = 0; step < 5000; ++step) {
        CAPTURE(step);
        const unsigned kind = random() % 100;
        const Value value = static_cast<Value>(random() % 20);  // small range: repeated values
        if (kind < 45) {
            // Sometimes out of range on purpose (-1 or size + 1).
            const std::int32_t index = static_cast<std::int32_t>(random() % (oracle.size() + 3)) - 1;
            Request r = request(StructureKind::List, Operation::InsertAt, value);
            r.index = index;
            const Response response = test::run(engine, r);
            const bool valid = index >= 0 && index <= static_cast<std::int32_t>(oracle.size());
            REQUIRE(response.status.ok() == (valid && oracle.size() < 200));
            if (response.status.ok()) {
                oracle.insert(oracle.begin() + index, {nextId++, value});
            }
        } else if (kind < 75) {
            // An existing node, or an ID that is not in the list.
            const bool existing = !oracle.empty() && random() % 4 != 0;
            const NodeId id = existing ? oracle[random() % oracle.size()].first : nextId + 7;
            Request r = request(StructureKind::List, Operation::DeleteNode);
            r.nodeId = id;
            const Response response = test::run(engine, r);
            REQUIRE(response.status.ok() == existing);
            if (existing) {
                oracle.erase(std::find_if(oracle.begin(), oracle.end(), [id](const auto& item) { return item.first == id; }));
            } else {
                REQUIRE(response.status.code == ErrorCode::NodeNotFound);
            }
        } else if (kind < 90) {
            const Response response = test::run(engine, request(StructureKind::List, Operation::Search, value));
            const auto first = std::find_if(oracle.begin(), oracle.end(), [value](const auto& item) { return item.second == value; });
            REQUIRE(booleanData(response, "found") == (first != oracle.end()));
            if (first != oracle.end()) {
                REQUIRE(integerData(response, "nodeId") == first->first);
                REQUIRE(integerData(response, "index") == first - oracle.begin());
            }
        } else if (kind < 98) {
            REQUIRE(test::run(engine, request(StructureKind::List, Operation::Reverse)).status.ok());
            std::reverse(oracle.begin(), oracle.end());
        } else {
            test::run(engine, request(StructureKind::List, Operation::Clear));
            oracle.clear();
        }
        const Snapshot snapshot = engine.snapshot(StructureKind::List);
        REQUIRE(chain(snapshot, snapshot.head) == oracle);
        REQUIRE(snapshot.nextId == nextId);
    }
}

TEST_CASE("binaryTree: 3,000 seeded random operations agree with a std::map model") {
    struct ModelNode {
        Value value = 0;
        NodeId left = kNoNode;
        NodeId right = kNoNode;
        NodeId parent = kNoNode;
    };
    Engine engine;
    std::map<NodeId, ModelNode> model;  // the oracle: every node by ID
    NodeId root = kNoNode;
    NodeId nextId = 1;
    std::mt19937 random(8003);

    // Removes 'id' and everything below it from the model; returns how many.
    const std::function<int(NodeId)> eraseSubtree = [&](NodeId id) -> int {
        if (id == kNoNode) return 0;
        const ModelNode node = model.at(id);
        model.erase(id);
        return 1 + eraseSubtree(node.left) + eraseSubtree(node.right);
    };

    for (int step = 0; step < 3000; ++step) {
        CAPTURE(step);
        const unsigned kind = random() % 100;
        const Value value = static_cast<Value>(random() % 100);
        // A random existing node (or 0 when there is none).
        NodeId some = kNoNode;
        if (!model.empty()) some = std::next(model.begin(), static_cast<long>(random() % model.size()))->first;

        if (kind < 8) {
            const Response response = test::run(engine, request(StructureKind::BinaryTree, Operation::AddRoot, value));
            REQUIRE(response.status.ok() == (root == kNoNode));
            if (response.status.ok()) {
                root = nextId;
                model[nextId++] = ModelNode{value, kNoNode, kNoNode, kNoNode};
            } else {
                REQUIRE(response.status.code == ErrorCode::RootExists);
            }
        } else if (kind < 70) {
            const bool missing = model.empty() || random() % 10 == 0;
            const NodeId parent = missing ? nextId + 5 : some;
            const Side side = random() % 2 == 0 ? Side::Left : Side::Right;
            Request r = request(StructureKind::BinaryTree, Operation::AddChild, value);
            r.parentId = parent;
            r.side = side;
            const Response response = test::run(engine, r);
            if (missing) {
                REQUIRE(response.status.code == (model.size() == 200 ? ErrorCode::CapacityExceeded : ErrorCode::NodeNotFound));
                continue;
            }
            NodeId& slot = side == Side::Left ? model[parent].left : model[parent].right;
            const ErrorCode expected = model.size() == 200 ? ErrorCode::CapacityExceeded
                                       : slot != kNoNode     ? ErrorCode::ChildExists
                                                             : ErrorCode::Ok;
            REQUIRE(response.status.code == expected);
            if (expected == ErrorCode::Ok) {
                slot = nextId;
                model[nextId++] = ModelNode{value, kNoNode, kNoNode, parent};
            }
        } else if (kind < 88) {
            const bool missing = model.empty() || random() % 8 == 0;
            const NodeId id = missing ? nextId + 9 : some;
            Request r = request(StructureKind::BinaryTree, Operation::DeleteSubtree);
            r.nodeId = id;
            const Response response = test::run(engine, r);
            REQUIRE(response.status.ok() == !missing);
            if (!missing) {
                const NodeId parent = model.at(id).parent;
                if (parent == kNoNode) {
                    root = kNoNode;
                } else if (model[parent].left == id) {
                    model[parent].left = kNoNode;
                } else {
                    model[parent].right = kNoNode;
                }
                const int removed = eraseSubtree(id);
                REQUIRE(integerData(response, "removedCount") == removed);
            }
        } else if (kind < 97) {
            checkTraversals(engine, StructureKind::BinaryTree);
        } else {
            test::run(engine, request(StructureKind::BinaryTree, Operation::Clear));
            model.clear();
            root = kNoNode;
        }

        // The engine's tree is exactly the model: same IDs, values and links.
        const Snapshot snapshot = engine.snapshot(StructureKind::BinaryTree);
        REQUIRE(snapshot.root == root);
        REQUIRE(snapshot.nodes.size() == model.size());
        for (const NodeView& node : snapshot.nodes) {
            CAPTURE(node.id);
            REQUIRE(model.count(node.id) == 1);
            const ModelNode& expected = model.at(node.id);
            REQUIRE(node.value == expected.value);
            REQUIRE(node.left == expected.left);
            REQUIRE(node.right == expected.right);
            REQUIRE(node.parent == expected.parent);
        }
        REQUIRE(snapshot.nextId == nextId);
    }
}

TEST_CASE("bst: 5,000 seeded random operations agree with std::set") {
    Engine engine;
    std::set<Value> oracle;
    std::mt19937 random(8004);
    for (int step = 0; step < 5000; ++step) {
        CAPTURE(step);
        const unsigned kind = random() % 100;
        const Value value = static_cast<Value>(random() % 151) - 75;
        if (kind < 45) {
            const Response response = test::run(engine, request(StructureKind::Bst, Operation::Insert, value));
            const bool expected = oracle.count(value) == 0 && oracle.size() < 200;
            REQUIRE(response.status.ok() == expected);
            if (expected) oracle.insert(value);
        } else if (kind < 80) {
            const Response response = test::run(engine, request(StructureKind::Bst, Operation::Delete, value));
            REQUIRE(response.status.ok() == (oracle.count(value) == 1));
            oracle.erase(value);
        } else if (kind < 95) {
            const Response response = test::run(engine, request(StructureKind::Bst, Operation::Search, value));
            REQUIRE(booleanData(response, "found") == (oracle.count(value) == 1));
        } else {
            checkTraversals(engine, StructureKind::Bst);
        }
        const Snapshot snapshot = engine.snapshot(StructureKind::Bst);
        REQUIRE(traversal(snapshot, Operation::Inorder) == std::vector<std::int64_t>(oracle.begin(), oracle.end()));
    }
    checkTraversals(engine, StructureKind::Bst);
}

TEST_CASE("avl and redBlack: every traversal agrees with the snapshot on random trees") {
    for (const StructureKind structure : {StructureKind::Avl, StructureKind::RedBlack}) {
        Engine engine;
        std::mt19937 random(8005);
        for (int step = 0; step < 400; ++step) {
            const Value value = static_cast<Value>(random() % 120);
            test::run(engine, request(structure, random() % 3 == 0 ? Operation::Delete : Operation::Insert, value));
            if (step % 40 == 39) checkTraversals(engine, structure);
        }
    }
}
