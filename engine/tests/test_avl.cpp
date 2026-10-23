// AVL tree behaviour through the Engine (docs/avl.md). The shared run() checks,
// after every operation, the tree shape, BST order, every height recomputed
// from scratch, every balance in -1..+1, and that any intermediate frame that
// shows a temporary imbalance labels it.
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Run.h"

using namespace dslab;
using test::booleanData;
using test::events;
using test::integerData;
using test::listData;

namespace {

Request avlRequest(Operation operation, Value value = 0) {
    Request request;
    request.structure = StructureKind::Avl;
    request.operation = operation;
    request.value = value;
    return request;
}

Response insert(Engine& engine, Value value) { return test::run(engine, avlRequest(Operation::Insert, value)); }
Response remove(Engine& engine, Value value) { return test::run(engine, avlRequest(Operation::Delete, value)); }

const NodeView& nodeById(const Snapshot& snapshot, NodeId id) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.id == id) return node;
    }
    FAIL("no node " << id);
    return snapshot.nodes.front();
}

// The tree as text: "20(10,30)"; a missing child is "-", a leaf is just its value.
std::string shape(const Snapshot& snapshot, NodeId id) {
    if (id == kNoNode) return "-";
    const NodeView& node = nodeById(snapshot, id);
    if (node.left == kNoNode && node.right == kNoNode) return std::to_string(node.value);
    return std::to_string(node.value) + "(" + shape(snapshot, node.left) + "," + shape(snapshot, node.right) + ")";
}

std::string shape(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(StructureKind::Avl);
    return shape(snapshot, snapshot.root);
}

const NodeView& nodeWithValue(const Snapshot& snapshot, Value value) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.value == value) return node;
    }
    FAIL("no node holds " << value);
    return snapshot.nodes.front();
}

std::map<Value, NodeId> idsByValue(Engine& engine) {
    std::map<Value, NodeId> ids;
    for (const NodeView& node : engine.snapshot(StructureKind::Avl).nodes) {
        ids[node.value] = node.id;
    }
    return ids;
}

// The step IDs of the frames whose event is 'event', in order.
std::vector<std::string> stepsWith(const Response& response, Event event) {
    std::vector<std::string> steps;
    for (const Frame& frame : response.frames) {
        if (frame.event == event) steps.push_back(frame.stepId);
    }
    return steps;
}

int treeHeight(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(StructureKind::Avl);
    return snapshot.root == kNoNode ? 0 : nodeById(snapshot, snapshot.root).height;
}

}  // namespace

TEST_CASE("avl: the four rotation cases on insert end in the same balanced shape") {
    struct Case {
        const char* name;
        std::vector<Value> values;
        std::vector<std::string> rotations;  // the rotation step IDs, in order
    };
    const std::vector<Case> cases = {
        {"LL", {30, 20, 10}, {"avl.rotateRight.heights"}},
        {"RR", {10, 20, 30}, {"avl.rotateLeft.heights"}},
        {"LR", {30, 10, 20}, {"avl.rotateLeft.heights", "avl.rotateRight.heights"}},
        {"RL", {10, 30, 20}, {"avl.rotateRight.heights", "avl.rotateLeft.heights"}},
    };

    for (const Case& c : cases) {
        CAPTURE(c.name);
        Engine engine;
        insert(engine, c.values[0]);
        insert(engine, c.values[1]);
        const std::map<Value, NodeId> idsBefore = idsByValue(engine);
        const Response last = insert(engine, c.values[2]);

        // The final shape, not just membership: 20 at the root, 10 left, 30 right.
        const Snapshot snapshot = engine.snapshot(StructureKind::Avl);
        CHECK(shape(snapshot, snapshot.root) == "20(10,30)");
        CHECK(nodeWithValue(snapshot, 20).height == 2);
        CHECK(nodeWithValue(snapshot, 10).height == 1);
        CHECK(nodeWithValue(snapshot, 30).height == 1);
        CHECK(nodeWithValue(snapshot, 20).balance == 0);
        CHECK(nodeWithValue(snapshot, 10).parent == nodeWithValue(snapshot, 20).id);

        // The case is detected, named, and fixed with the right rotations.
        const std::string caseStep = std::string("avl.rebalance.") + c.name;
        CHECK(stepsWith(last, Event::Imbalance) == std::vector<std::string>{"avl.fixUp.unbalanced", caseStep});
        CHECK(stepsWith(last, Event::Rotate) == c.rotations);
        CHECK(last.counters.rotations == static_cast<int>(c.rotations.size()));
        for (const Frame& frame : last.frames) {
            if (frame.event == Event::Rotate || frame.stepId == caseStep) {
                CHECK(frame.explanation.find(std::string(c.name) + " case") != std::string::npos);
            }
        }
        CHECK(stepsWith(last, Event::Balanced) == std::vector<std::string>{"avl.fixUp.done"});

        // Rotations relink nodes: the two old values keep their IDs.
        for (const auto& [value, id] : idsBefore) {
            CHECK(nodeWithValue(snapshot, value).id == id);
        }
    }
}

TEST_CASE("avl: the frame that makes a node +2 labels the temporary imbalance") {
    Engine engine;
    insert(engine, 30);
    insert(engine, 20);
    const Response last = insert(engine, 10);

    // Height of 20 becomes 2 before 30 is rebalanced: that frame shows 30 at +2.
    bool labelled = false;
    for (const Frame& frame : last.frames) {
        if (frame.event == Event::Update && nodeWithValue(frame.snapshot, 30).balance == 2) {
            CHECK(frame.explanation.find("temporarily unbalanced: 30 (+2)") != std::string::npos);
            labelled = true;
        }
    }
    CHECK(labelled);
    CHECK(last.frames.back().explanation == "Inserted 10; the AVL tree now holds 3 nodes.");
}

TEST_CASE("avl: deleting can rebalance the root, and can rebalance more than one ancestor") {
    // Built without any rotation:
    //   50 -> 30 (20 (10), 40) | 80 (70 (60 (-, 65), 75), 90 (-, 95))
    const std::vector<Value> values = {50, 30, 80, 20, 40, 70, 90, 10, 60, 75, 95, 65};
    Engine engine;
    for (const Value value : values) {
        REQUIRE(insert(engine, value).counters.rotations == 0);
    }
    REQUIRE(shape(engine) == "50(30(20(10,-),40),80(70(60(-,65),75),90(-,95)))");

    SUBCASE("delete 10: one RL rebalance at the root") {
        const Response response = remove(engine, 10);
        CHECK(stepsWith(response, Event::Imbalance) ==
              std::vector<std::string>{"avl.fixUp.unbalanced", "avl.rebalance.RL"});
        CHECK(response.counters.rotations == 2);
        CHECK(shape(engine) == "70(50(30(20,40),60(-,65)),80(75,90(-,95)))");
    }
    SUBCASE("delete 40: LL at 30, then RL at the root 50") {
        const std::map<Value, NodeId> idsBefore = idsByValue(engine);
        const Response response = remove(engine, 40);
        CHECK(stepsWith(response, Event::Imbalance) ==
              std::vector<std::string>{"avl.fixUp.unbalanced", "avl.rebalance.LL", "avl.fixUp.unbalanced",
                                       "avl.rebalance.RL"});
        CHECK(response.counters.rotations == 3);
        CHECK(shape(engine) == "70(50(20(10,30),60(-,65)),80(75,90(-,95)))");

        // The two rebalanced nodes are different ancestors.
        std::vector<NodeId> unbalanced;
        for (const Frame& frame : response.frames) {
            if (frame.stepId == "avl.fixUp.unbalanced") unbalanced.push_back(frame.pointers.front().node);
        }
        CHECK(unbalanced == std::vector<NodeId>{idsBefore.at(30), idsBefore.at(50)});

        std::map<Value, NodeId> expected = idsBefore;
        expected.erase(40);
        CHECK(idsByValue(engine) == expected);  // every remaining value kept its ID
    }
}

TEST_CASE("avl: duplicates and absent keys are rejected and change nothing") {
    Engine engine;
    test::checkRejected(engine, avlRequest(Operation::Delete, 5), ErrorCode::ValueNotFound);  // empty tree
    for (const Value value : {20, 10, 30}) insert(engine, value);
    test::checkRejected(engine, avlRequest(Operation::Insert, 10), ErrorCode::DuplicateValue);
    test::checkRejected(engine, avlRequest(Operation::Delete, 25), ErrorCode::ValueNotFound);
    CHECK(integerData(insert(engine, 25), "nodeId") == 4);  // no ID was used up
}

TEST_CASE("avl: search, traversals and the empty and single-node trees") {
    Engine engine;
    CHECK_FALSE(booleanData(test::run(engine, avlRequest(Operation::Search, 1)), "found"));

    insert(engine, 7);
    CHECK(listData(test::run(engine, avlRequest(Operation::Levelorder)), "values") == std::vector<std::int64_t>{7});
    const Response only = remove(engine, 7);
    CHECK(only.snapshot.root == kNoNode);
    CHECK(stepsWith(only, Event::Balanced).empty());  // nothing above the root to fix

    for (const Value value : {1, 2, 3, 4, 5, 6, 7}) insert(engine, value);
    CHECK(shape(engine) == "4(2(1,3),6(5,7))");
    CHECK(listData(test::run(engine, avlRequest(Operation::Inorder)), "values") ==
          std::vector<std::int64_t>{1, 2, 3, 4, 5, 6, 7});
    const Response found = test::run(engine, avlRequest(Operation::Search, 5));
    CHECK(booleanData(found, "found"));
    CHECK(found.frames.back().highlight.nodes.size() == 3);  // 4 -> 6 -> 5
}

TEST_CASE("avl: 200 increasing values stay balanced") {
    Engine engine;
    for (int i = 0; i < 200; ++i) {
        REQUIRE(insert(engine, i).status.ok());
    }
    CHECK(treeHeight(engine) == 8);  // a plain BST would be 200 deep
    test::checkRejected(engine, avlRequest(Operation::Insert, 1000), ErrorCode::CapacityExceeded);
    CHECK(integerData(test::run(engine, avlRequest(Operation::Clear)), "removedCount") == 200);
}

TEST_CASE("avl: 2,000 seeded random inserts and deletes agree with std::set") {
    Engine engine;
    std::set<Value> oracle;  // allowed as a test oracle (CLAUDE.md, rule 1)
    std::mt19937 random(20261022);
    std::map<std::string, int> casesSeen;  // how often each rebalancing case ran

    for (int step = 0; step < 2000; ++step) {
        CAPTURE(step);
        const Value value = static_cast<Value>(random() % 121) - 60;
        Response response;
        if (random() % 100 < 55) {
            response = insert(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 0));
            oracle.insert(value);
        } else {
            response = remove(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 1));
            oracle.erase(value);
        }
        for (const std::string& caseStep : stepsWith(response, Event::Imbalance)) {
            casesSeen[caseStep] += 1;
        }

        // Membership matches the oracle (order is checked by run()'s checker),
        // and the height stays within the AVL bound.
        std::set<Value> members;
        for (const NodeView& node : engine.snapshot(StructureKind::Avl).nodes) {
            members.insert(node.value);
        }
        REQUIRE(members == oracle);
        const double bound = 1.44 * std::log2(static_cast<double>(oracle.size()) + 2.0);
        REQUIRE(treeHeight(engine) <= bound);
    }
    const std::vector<std::int64_t> expected(oracle.begin(), oracle.end());
    CHECK(listData(test::run(engine, avlRequest(Operation::Inorder)), "values") == expected);

    // The run exercised every rebalancing case.
    for (const char* name : {"avl.rebalance.LL", "avl.rebalance.LR", "avl.rebalance.RR", "avl.rebalance.RL"}) {
        CAPTURE(name);
        CHECK(casesSeen[name] > 0);
    }
    MESSAGE("rebalancing cases seen: LL " << casesSeen["avl.rebalance.LL"] << ", LR " << casesSeen["avl.rebalance.LR"]
            << ", RR " << casesSeen["avl.rebalance.RR"] << ", RL " << casesSeen["avl.rebalance.RL"]);
}
