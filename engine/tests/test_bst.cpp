// Binary search tree behaviour through the Engine; the shared run() checks the
// tree invariants (including BST order) and the frame contract after every
// operation.
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Checkers.h"
#include "support/Run.h"

using namespace dslab;
using test::booleanData;
using test::events;
using test::integerData;
using test::isNullData;
using test::listData;

namespace {

Request bstRequest(Operation operation, Value value = 0) {
    Request request;
    request.structure = StructureKind::Bst;
    request.operation = operation;
    request.value = value;
    return request;
}

Response insert(Engine& engine, Value value) { return test::run(engine, bstRequest(Operation::Insert, value)); }
Response remove(Engine& engine, Value value) { return test::run(engine, bstRequest(Operation::Delete, value)); }
Response search(Engine& engine, Value value) { return test::run(engine, bstRequest(Operation::Search, value)); }

void insertAll(Engine& engine, const std::vector<Value>& values) {
    for (const Value value : values) {
        REQUIRE(insert(engine, value).status.ok());
    }
}

std::vector<std::int64_t> inorder(Engine& engine) {
    return listData(test::run(engine, bstRequest(Operation::Inorder)), "values");
}

// value -> node ID for every node, to check that values keep their IDs.
std::map<Value, NodeId> idsByValue(Engine& engine) {
    std::map<Value, NodeId> ids;
    for (const NodeView& node : engine.snapshot(StructureKind::Bst).nodes) {
        ids[node.value] = node.id;
    }
    return ids;
}

NodeView nodeWithValue(Engine& engine, Value value) {
    for (const NodeView& node : engine.snapshot(StructureKind::Bst).nodes) {
        if (node.value == value) {
            return node;
        }
    }
    FAIL("no node holds " << value);
    return NodeView{};
}

std::vector<std::string> stepIds(const Response& response) {
    std::vector<std::string> ids;
    for (const Frame& frame : response.frames) {
        ids.push_back(frame.stepId);
    }
    return ids;
}

bool hasStep(const Response& response, const std::string& stepId) {
    for (const Frame& frame : response.frames) {
        if (frame.stepId == stepId) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("bst: insert places values by comparison, negatives included") {
    Engine engine;
    const Response first = insert(engine, 50);
    CHECK(events(first) == std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Done});
    CHECK(first.frames[2].stepId == "bst.insert.linkRoot");

    insertAll(engine, {30, 70, 20, 40, 60, 80, -10});
    CHECK(inorder(engine) == std::vector<std::int64_t>{-10, 20, 30, 40, 50, 60, 70, 80});
    CHECK(nodeWithValue(engine, -10).parent == nodeWithValue(engine, 20).id);
    CHECK(nodeWithValue(engine, 20).left == nodeWithValue(engine, -10).id);
}

TEST_CASE("bst: a duplicate is rejected after the comparisons that find it, and nothing changes") {
    Engine engine;
    insertAll(engine, {30, 20, 40});

    const Response duplicate = test::run(engine, bstRequest(Operation::Insert, 20));
    CHECK(duplicate.status.code == ErrorCode::DuplicateValue);
    CHECK(duplicate.status.message == "20 is already in the BST, and a BST does not allow duplicates.");
    CHECK(events(duplicate) == std::vector<Event>{Event::Begin, Event::Compare, Event::Reject});
    CHECK(stepIds(duplicate) ==
          std::vector<std::string>{"bst.insert.enter", "bst.insert.goLeft", "bst.insert.duplicate"});
    CHECK(duplicate.frames[1].explanation == "20 < 30, so move to the left child, 20.");
    CHECK(duplicate.counters.comparisons == 2);

    test::checkRejected(engine, bstRequest(Operation::Insert, 30), ErrorCode::DuplicateValue);
    CHECK(integerData(insert(engine, 25), "nodeId") == 4);  // no ID was used up
}

TEST_CASE("bst: search highlights the actual path it takes") {
    Engine engine;
    insertAll(engine, {50, 30, 70, 20, 40});  // IDs 1..5
    const Response found = search(engine, 40);
    CHECK(booleanData(found, "found"));
    CHECK(integerData(found, "nodeId") == 5);
    CHECK(found.counters.comparisons == 3);
    CHECK(stepIds(found) == std::vector<std::string>{"bst.search.enter", "bst.search.goLeft", "bst.search.goRight",
                                                     "bst.search.found", "bst.search.done"});
    // Each frame shows the whole path so far; the last shows 50 -> 30 -> 40.
    CHECK(found.frames[1].highlight.nodes == std::vector<NodeId>{1});
    CHECK(found.frames.back().highlight.nodes == std::vector<NodeId>{1, 2, 5});
    REQUIRE(found.frames.back().highlight.edges.size() == 2);
    CHECK(found.frames.back().highlight.edges[0].from == 1);
    CHECK(found.frames.back().highlight.edges[0].to == 2);
    CHECK(found.frames.back().highlight.edges[1].from == 2);
    CHECK(found.frames.back().highlight.edges[1].to == 5);

    const Response absent = search(engine, 45);  // 50 -> 30 -> 40 -> (empty right)
    CHECK_FALSE(booleanData(absent, "found"));
    CHECK(isNullData(absent, "nodeId"));
    CHECK(absent.frames[absent.frames.size() - 2].event == Event::NotFound);
    CHECK(absent.frames.back().highlight.nodes == std::vector<NodeId>{1, 2, 5});

    Engine empty;
    const Response nothing = search(empty, 1);
    CHECK_FALSE(booleanData(nothing, "found"));
    CHECK(events(nothing) == std::vector<Event>{Event::Begin, Event::NotFound, Event::Done});
}

TEST_CASE("bst: delete a leaf (case 1)") {
    Engine engine;
    insertAll(engine, {50, 30, 70});
    const Response leaf = remove(engine, 70);
    CHECK(integerData(leaf, "value") == 70);
    CHECK(hasStep(leaf, "bst.delete.caseLeaf"));
    CHECK(leaf.frames[2].explanation == "Found 70. It has no children (case 1, a leaf): just unlink it.");
    CHECK(nodeWithValue(engine, 50).right == kNoNode);
    CHECK(inorder(engine) == std::vector<std::int64_t>{30, 50});
}

TEST_CASE("bst: delete a node with one child (case 2), on either side") {
    Engine engine;
    SUBCASE("only a left child") {
        insertAll(engine, {50, 30, 20});
        const NodeId child = nodeWithValue(engine, 20).id;
        const Response response = remove(engine, 30);
        CHECK(hasStep(response, "bst.delete.caseOneChild"));
        CHECK(nodeWithValue(engine, 20).id == child);  // the child moved up, keeping its ID
        CHECK(nodeWithValue(engine, 20).parent == nodeWithValue(engine, 50).id);
        CHECK(nodeWithValue(engine, 50).left == child);
    }
    SUBCASE("only a right child") {
        insertAll(engine, {50, 30, 40});
        const Response response = remove(engine, 30);
        CHECK(hasStep(response, "bst.delete.caseOneChild"));
        CHECK(nodeWithValue(engine, 50).left == nodeWithValue(engine, 40).id);
    }
    CHECK(test::checkSnapshot(engine.snapshot(StructureKind::Bst)) == "");
}

TEST_CASE("bst: delete a node with two children (case 3) via the in-order successor") {
    Engine engine;
    SUBCASE("the successor is the right child itself") {
        insertAll(engine, {50, 30, 70});
        const auto before = idsByValue(engine);
        const Response response = remove(engine, 50);
        CHECK(stepIds(response) ==
              std::vector<std::string>{"bst.delete.enter", "bst.delete.caseTwoChildren", "bst.delete.successorStart",
                                       "bst.delete.successorFound", "bst.delete.replaceWithSuccessor",
                                       "bst.delete.successorTakesLeft", "bst.delete.remove", "bst.delete.done"});
        CHECK(response.snapshot.root == before.at(70));  // 70 became the root, still with its own ID
        CHECK(nodeWithValue(engine, 70).left == before.at(30));
    }
    SUBCASE("a deeper successor with a right child") {
        insertAll(engine, {50, 30, 70, 60, 80, 65});
        const auto before = idsByValue(engine);
        const Response response = remove(engine, 50);
        CHECK(hasStep(response, "bst.delete.successorLeft"));
        CHECK(hasStep(response, "bst.delete.detachSuccessor"));
        CHECK(hasStep(response, "bst.delete.successorTakesRight"));
        CHECK(response.frames[1].explanation ==
              "Found 50. It has two children (case 3): replace it with its in-order successor.");
        CHECK(response.snapshot.root == before.at(60));
        CHECK(nodeWithValue(engine, 70).left == before.at(65));  // 65 took 60's old place
        CHECK(inorder(engine) == std::vector<std::int64_t>{30, 60, 65, 70, 80});
    }
    SUBCASE("a deeper successor without children") {
        insertAll(engine, {50, 30, 70, 60});
        const Response response = remove(engine, 50);
        CHECK(hasStep(response, "bst.delete.detachSuccessor"));
        CHECK(nodeWithValue(engine, 70).left == kNoNode);
        CHECK(inorder(engine) == std::vector<std::int64_t>{30, 60, 70});
    }
}

TEST_CASE("bst: deleting the root in each case") {
    Engine engine;
    SUBCASE("a root that is a leaf") {
        insertAll(engine, {10});
        const Response response = remove(engine, 10);
        CHECK(response.snapshot.root == kNoNode);
        CHECK(response.snapshot.nodes.empty());
        CHECK(response.frames.back().explanation == "Deleted 10; the BST is now empty.");
    }
    SUBCASE("a root with one child") {
        insertAll(engine, {10, 5});
        const Response response = remove(engine, 10);
        CHECK(response.snapshot.root == 2);
        CHECK(nodeWithValue(engine, 5).parent == kNoNode);
    }
    SUBCASE("a root with two children") {
        insertAll(engine, {10, 5, 15});
        const Response response = remove(engine, 10);
        CHECK(response.snapshot.root == 3);
    }
}

TEST_CASE("bst: deleting an absent value is rejected and changes nothing") {
    Engine engine;
    test::checkRejected(engine, bstRequest(Operation::Delete, 5), ErrorCode::ValueNotFound);  // empty tree

    insertAll(engine, {50, 30, 70});
    const Response absent = test::run(engine, bstRequest(Operation::Delete, 45));
    CHECK(absent.status.message == "45 is not in the BST, so there is nothing to delete.");
    CHECK(events(absent) == std::vector<Event>{Event::Begin, Event::Compare, Event::Compare, Event::Reject});
    test::checkRejected(engine, bstRequest(Operation::Delete, -1), ErrorCode::ValueNotFound);
}

TEST_CASE("bst: every remaining value keeps its ID through deletes of every case") {
    Engine engine;
    insertAll(engine, {50, 30, 70, 20, 40, 60, 80, 35, 45, 65});
    std::map<Value, NodeId> expected = idsByValue(engine);
    for (const Value value : {30, 80, 50, 35}) {  // two children, leaf, root with two children, one child
        remove(engine, value);
        expected.erase(value);
        CHECK(idsByValue(engine) == expected);
    }
}

TEST_CASE("bst: traversals") {
    Engine engine;
    insertAll(engine, {50, 30, 70, 20, 40, 60, 80});
    CHECK(inorder(engine) == std::vector<std::int64_t>{20, 30, 40, 50, 60, 70, 80});
    CHECK(listData(test::run(engine, bstRequest(Operation::Preorder)), "values") ==
          std::vector<std::int64_t>{50, 30, 20, 40, 70, 60, 80});
    CHECK(listData(test::run(engine, bstRequest(Operation::Postorder)), "values") ==
          std::vector<std::int64_t>{20, 40, 30, 60, 80, 70, 50});
    CHECK(listData(test::run(engine, bstRequest(Operation::Levelorder)), "values") ==
          std::vector<std::int64_t>{50, 30, 70, 20, 40, 60, 80});
    CHECK(test::run(engine, bstRequest(Operation::Inorder)).frames[1].stepId == "bst.inorder.start");
}

TEST_CASE("bst: 200 increasing values make a 200-deep tree that still traverses and clears") {
    Engine engine;
    for (int i = 0; i < 200; ++i) {
        REQUIRE(insert(engine, i).status.ok());
    }
    test::checkRejected(engine, bstRequest(Operation::Insert, 1000), ErrorCode::CapacityExceeded);
    CHECK(inorder(engine).size() == 200);
    CHECK(integerData(remove(engine, 0), "value") == 0);  // the root, which has one child
    CHECK(integerData(test::run(engine, bstRequest(Operation::Clear)), "removedCount") == 199);
}

TEST_CASE("bst: 10,000 seeded random operations agree with std::set") {
    Engine engine;
    std::set<Value> oracle;  // allowed as a test oracle (CLAUDE.md, rule 1)
    std::mt19937 random(20261022);

    for (int step = 0; step < 10000; ++step) {
        CAPTURE(step);
        const int choice = static_cast<int>(random() % 100);
        const Value value = static_cast<Value>(random() % 101) - 50;
        if (choice < 45) {
            const Response response = insert(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 0));
            if (!response.status.ok()) REQUIRE(response.status.code == ErrorCode::DuplicateValue);
            oracle.insert(value);
        } else if (choice < 80) {
            const Response response = remove(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 1));
            if (!response.status.ok()) REQUIRE(response.status.code == ErrorCode::ValueNotFound);
            oracle.erase(value);
        } else if (choice < 99) {
            REQUIRE(booleanData(search(engine, value), "found") == (oracle.count(value) == 1));
        } else {
            test::run(engine, bstRequest(Operation::Clear));
            oracle.clear();
        }
        REQUIRE(engine.snapshot(StructureKind::Bst).size == static_cast<int>(oracle.size()));
    }
    const std::vector<std::int64_t> expected(oracle.begin(), oracle.end());
    CHECK(inorder(engine) == expected);
}
