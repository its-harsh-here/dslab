// Red-black tree behaviour through the Engine (docs/red-black.md). The shared
// run() checks, after every operation, the tree shape, BST order, valid
// parent/child links, every node reachable, and all five red-black properties;
// and, for every frame, that a frame breaking property 2, 4 or 5 says
// "temporary violation" (and that no other frame does).
//
// Expected shapes and case sequences were worked out independently with a
// Python model of the CLRS algorithm, not by running this engine.
#include <algorithm>
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
using test::integerData;
using test::listData;

namespace {

Request rbRequest(Operation operation, Value value = 0) {
    Request request;
    request.structure = StructureKind::RedBlack;
    request.operation = operation;
    request.value = value;
    return request;
}

Response insert(Engine& engine, Value value) { return test::run(engine, rbRequest(Operation::Insert, value)); }
Response remove(Engine& engine, Value value) { return test::run(engine, rbRequest(Operation::Delete, value)); }

const NodeView& nodeById(const Snapshot& snapshot, NodeId id) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.id == id) return node;
    }
    FAIL("no node " << id);
    return snapshot.nodes.front();
}

// The tree as text with colors: "20B(10R,30R)"; a missing child is "-", a leaf
// is just its value and color, and the empty tree is "-".
std::string shape(const Snapshot& snapshot, NodeId id) {
    if (id == kNoNode) return "-";
    const NodeView& node = nodeById(snapshot, id);
    const std::string self = std::to_string(node.value) + (node.color == Color::Red ? "R" : "B");
    if (node.left == kNoNode && node.right == kNoNode) return self;
    return self + "(" + shape(snapshot, node.left) + "," + shape(snapshot, node.right) + ")";
}

std::string shape(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(StructureKind::RedBlack);
    return shape(snapshot, snapshot.root);
}

std::map<Value, NodeId> idsByValue(Engine& engine) {
    std::map<Value, NodeId> ids;
    for (const NodeView& node : engine.snapshot(StructureKind::RedBlack).nodes) {
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

const Frame& frameWithStep(const Response& response, const std::string& stepId) {
    for (const Frame& frame : response.frames) {
        if (frame.stepId == stepId) return frame;
    }
    FAIL("no frame with step " << stepId);
    return response.frames.front();
}

// The height of the tree in nodes (empty tree 0), from the snapshot.
int treeHeight(const Snapshot& snapshot, NodeId id) {
    if (id == kNoNode) return 0;
    const NodeView& node = nodeById(snapshot, id);
    return 1 + std::max(treeHeight(snapshot, node.left), treeHeight(snapshot, node.right));
}

// In-order values, computed from the snapshot alone.
void inorderValues(const Snapshot& snapshot, NodeId id, std::vector<std::int64_t>& out) {
    if (id == kNoNode) return;
    const NodeView& node = nodeById(snapshot, id);
    inorderValues(snapshot, node.left, out);
    out.push_back(node.value);
    inorderValues(snapshot, node.right, out);
}

// One scenario per fix-up case and side: build the tree from 'setup' (insert
// in this order), then run the last operation and expect exactly these cases.
struct Scenario {
    const char* name;
    std::vector<Value> setup;
    Operation operation;  // Insert or Delete
    Value value;
    std::string before;               // the tree after the set-up
    std::vector<std::string> cases;   // the fixupCase step IDs, in order
    std::string after;                // the tree after the last operation
};

const std::vector<Scenario>& scenarios() {
    static const std::vector<Scenario> list = {
        {"insert case 1", {30, 20, 40}, Operation::Insert, 10, "30B(20R,40R)",
         {"redBlack.insertFixup.case1"}, "30B(20B(10R,-),40B)"},
        {"insert case 2", {30, 10}, Operation::Insert, 20, "30B(10R,-)",
         {"redBlack.insertFixup.case2", "redBlack.insertFixup.case3"}, "20B(10R,30R)"},
        {"insert case 3", {30, 20}, Operation::Insert, 10, "30B(20R,-)",
         {"redBlack.insertFixup.case3"}, "20B(10R,30R)"},
        {"delete case 1", {10, 20, 30, 40, 60, 70}, Operation::Delete, 10, "20B(10B,40R(30B,60B(-,70R)))",
         {"redBlack.deleteFixup.case1", "redBlack.deleteFixup.case2"}, "40B(20B(-,30R),60B(-,70R))"},
        {"delete case 2", {10, 20, 30, 40, 70, 90, 60, 50}, Operation::Delete, 10,
         "40B(20R(10B,30B),70R(60B(50R,-),90B))", {"redBlack.deleteFixup.case2"},
         "40B(20B(-,30R),70R(60B(50R,-),90B))"},
        {"delete case 3", {10, 20, 40, 30}, Operation::Delete, 10, "20B(10B,40B(30R,-))",
         {"redBlack.deleteFixup.case3", "redBlack.deleteFixup.case4"}, "30B(20B,40B)"},
        {"delete case 4", {10, 20, 30, 40}, Operation::Delete, 10, "20B(10B,30B(-,40R))",
         {"redBlack.deleteFixup.case4"}, "30B(20B,40B)"},
        {"insert case 1 mirror", {30, 20, 40}, Operation::Insert, 50, "30B(20R,40R)",
         {"redBlack.insertFixup.case1Mirror"}, "30B(20B,40B(-,50R))"},
        {"insert case 2 mirror", {10, 30}, Operation::Insert, 20, "10B(-,30R)",
         {"redBlack.insertFixup.case2Mirror", "redBlack.insertFixup.case3Mirror"}, "20B(10R,30R)"},
        {"insert case 3 mirror", {10, 20}, Operation::Insert, 30, "10B(-,20R)",
         {"redBlack.insertFixup.case3Mirror"}, "20B(10R,30R)"},
        {"delete case 1 mirror", {10, 60, 50, 20, 40, 30}, Operation::Delete, 50, "50B(20R(10B,40B(30R,-)),60B)",
         {"redBlack.deleteFixup.case1Mirror", "redBlack.deleteFixup.case4Mirror"}, "20B(10B,40R(30B,60B))"},
        {"delete case 2 mirror", {10, 20, 30, 40, 70, 90, 60, 50}, Operation::Delete, 20,
         "40B(20R(10B,30B),70R(60B(50R,-),90B))", {"redBlack.deleteFixup.case2Mirror"},
         "40B(30B(10R,-),70R(60B(50R,-),90B))"},
        {"delete case 3 mirror", {10, 30, 40, 20}, Operation::Delete, 30, "30B(10B(-,20R),40B)",
         {"redBlack.deleteFixup.case3Mirror", "redBlack.deleteFixup.case4Mirror"}, "20B(10B,40B)"},
        {"delete case 4 mirror", {20, 30, 40, 10}, Operation::Delete, 30, "30B(20B(10R,-),40B)",
         {"redBlack.deleteFixup.case4Mirror"}, "20B(10B,40B)"},
    };
    return list;
}

// The 14 fix-up case step IDs: insert cases 1-3 and delete cases 1-4, each on both sides.
std::vector<std::string> allCaseIds() {
    std::vector<std::string> ids;
    for (const char* side : {"", "Mirror"}) {
        for (const char* name : {"case1", "case2", "case3"}) ids.push_back(std::string("redBlack.insertFixup.") + name + side);
        for (const char* name : {"case1", "case2", "case3", "case4"}) {
            ids.push_back(std::string("redBlack.deleteFixup.") + name + side);
        }
    }
    return ids;
}

}  // namespace

TEST_CASE("redBlack: every fix-up case and its mirror runs exactly as expected") {
    for (const Scenario& scenario : scenarios()) {
        CAPTURE(scenario.name);
        Engine engine;
        for (const Value value : scenario.setup) insert(engine, value);
        REQUIRE(shape(engine) == scenario.before);
        const std::map<Value, NodeId> idsBefore = idsByValue(engine);

        const Response last = test::run(engine, rbRequest(scenario.operation, scenario.value));
        REQUIRE(last.status.ok());
        CHECK(stepsWith(last, Event::FixupCase) == scenario.cases);
        CHECK(shape(engine) == scenario.after);

        // Every case frame names its case, and each rotation names the case that asked for it.
        for (const Frame& frame : last.frames) {
            if (frame.event == Event::FixupCase || frame.event == Event::Rotate) {
                const bool named = frame.explanation.rfind("Insert case ", 0) == 0 ||
                                   frame.explanation.rfind("Delete case ", 0) == 0;
                CHECK(named);
            }
        }
        // Rotations and recolors relink and repaint nodes: every surviving value keeps its ID.
        std::map<Value, NodeId> expected = idsBefore;
        if (scenario.operation == Operation::Delete) expected.erase(scenario.value);
        std::map<Value, NodeId> idsAfter = idsByValue(engine);
        if (scenario.operation == Operation::Insert) idsAfter.erase(scenario.value);
        CHECK(idsAfter == expected);
        CHECK(stepsWith(last, Event::Balanced).size() == 1);
    }
}

TEST_CASE("redBlack: all 14 fix-up case IDs are covered") {
    std::set<std::string> seen;
    for (const Scenario& scenario : scenarios()) {
        Engine engine;
        for (const Value value : scenario.setup) insert(engine, value);
        for (const std::string& step : stepsWith(test::run(engine, rbRequest(scenario.operation, scenario.value)),
                                                 Event::FixupCase)) {
            seen.insert(step);
        }
    }
    std::string list;
    for (const std::string& id : allCaseIds()) {
        CAPTURE(id);
        CHECK(seen.count(id) == 1);
        list += (list.empty() ? "" : ", ") + id.substr(id.find('.') + 1);
    }
    CHECK(seen.size() == 14);
    MESSAGE("fix-up case IDs covered (" << seen.size() << " of 14): " << list);
}

TEST_CASE("redBlack: case frames, recolors and rotations of insert case 3") {
    Engine engine;
    insert(engine, 30);
    insert(engine, 20);
    const Response last = insert(engine, 10);

    CHECK(stepsWith(last, Event::Recolor) ==
          std::vector<std::string>{"redBlack.insertFixup.case3Parent", "redBlack.insertFixup.case3Grandparent"});
    CHECK(stepsWith(last, Event::Rotate) == std::vector<std::string>{"redBlack.rotateRight.linkDown"});
    CHECK(last.counters.recolors == 2);
    CHECK(last.counters.rotations == 1);

    // The link frame and every frame up to the rotation label the red-red violation.
    CHECK(frameWithStep(last, "redBlack.insert.linkLeft").explanation ==
          "10 < 20, so attach the new node as the left child of 20; temporary violation: red 10 under red 20.");
    CHECK(frameWithStep(last, "redBlack.insertFixup.case3Parent").explanation ==
          "Color the parent 20 black; temporary violation: black heights differ under 30.");
    CHECK(frameWithStep(last, "redBlack.rotateRight.linkDown").explanation ==
          "Insert case 3: rotate right at 30, so 20 moves up.");
    CHECK(last.frames.back().explanation == "Inserted 10; the red-black tree now holds 3 nodes.");
}

TEST_CASE("redBlack: the first insert makes a red root, then colors it black") {
    Engine engine;
    const Response first = insert(engine, 7);
    CHECK(frameWithStep(first, "redBlack.insert.linkRoot").explanation ==
          "The tree was empty, so the new node becomes the root; temporary violation: red root.");
    CHECK(stepsWith(first, Event::Recolor) == std::vector<std::string>{"redBlack.insertFixup.blackRoot"});
    CHECK(shape(engine) == "7B");

    // A red node under a black parent needs no fix-up at all.
    const Response second = insert(engine, 9);
    CHECK(stepsWith(second, Event::FixupCase).empty());
    CHECK(stepsWith(second, Event::Recolor).empty());
    CHECK(frameWithStep(second, "redBlack.insertFixup.done").explanation ==
          "Nothing to fix: the parent 7 is black, so all five red-black properties hold.");
}

TEST_CASE("redBlack: deleting the root") {
    SUBCASE("a root that is a leaf: the tree becomes empty") {
        Engine engine;
        insert(engine, 5);
        const Response response = remove(engine, 5);
        CHECK(response.snapshot.root == kNoNode);
        CHECK(response.snapshot.size == 0);
        CHECK(frameWithStep(response, "redBlack.delete.extraBlack").explanation ==
              "The removed node 5 was black, so x = NIL carries an extra black (doubly black).");
        CHECK(frameWithStep(response, "redBlack.deleteFixup.blacken").explanation ==
              "x = NIL is the root, so the extra black is simply dropped; color it black.");
    }
    SUBCASE("a black root with one red child: the child takes its place and turns black") {
        Engine engine;
        insert(engine, 10);
        insert(engine, 20);
        REQUIRE(shape(engine) == "10B(-,20R)");
        const NodeId id20 = idsByValue(engine).at(20);
        const Response response = remove(engine, 10);
        CHECK(shape(engine) == "20B");
        CHECK(response.snapshot.root == id20);
        CHECK(frameWithStep(response, "redBlack.delete.extraBlack").explanation.find("(red-and-black)") !=
              std::string::npos);
        CHECK(stepsWith(response, Event::FixupCase).empty());
    }
    SUBCASE("a root with two children: its successor node moves up and takes its color") {
        Engine engine;
        for (const Value value : {20, 10, 30}) insert(engine, value);
        REQUIRE(shape(engine) == "20B(10R,30R)");
        const NodeId id30 = idsByValue(engine).at(30);
        const Response response = remove(engine, 20);
        CHECK(shape(engine) == "30B(10R,-)");
        CHECK(response.snapshot.root == id30);  // the successor node itself moved, not its value
        CHECK(frameWithStep(response, "redBlack.delete.successorTakesColor").explanation ==
              "30 takes over 20's color, black.");
        CHECK(frameWithStep(response, "redBlack.delete.extraBlack").event == Event::Balanced);  // 30 was red
    }
}

TEST_CASE("redBlack: deleting a red leaf needs no fix-up") {
    Engine engine;
    for (const Value value : {20, 10, 30}) insert(engine, value);
    const Response response = remove(engine, 10);
    CHECK(shape(engine) == "20B(-,30R)");
    CHECK(stepsWith(response, Event::FixupCase).empty());
    CHECK(stepsWith(response, Event::Recolor).empty());
    CHECK(response.counters.rotations == 0);
    CHECK(frameWithStep(response, "redBlack.delete.extraBlack").explanation ==
          "The removed node 10 was red, so no path lost a black node: nothing to fix.");
}

TEST_CASE("redBlack: deleting a black leaf runs the fix-up") {
    Engine engine;
    for (const Value value : {10, 20, 30, 40}) insert(engine, value);
    remove(engine, 40);
    REQUIRE(shape(engine) == "20B(10B,30B)");
    const Response response = remove(engine, 10);
    CHECK(shape(engine) == "20B(-,30R)");
    CHECK(stepsWith(response, Event::FixupCase) == std::vector<std::string>{"redBlack.deleteFixup.case2"});

    // Removing the black leaf breaks property 5, and the frames say so until the fix-up repairs it.
    CHECK(frameWithStep(response, "redBlack.delete.remove").explanation ==
          "Free the node that held 10; temporary violation: black heights differ under 20.");
    CHECK(frameWithStep(response, "redBlack.deleteFixup.case2Up").explanation ==
          "The extra black moves up to the parent 20, which becomes x.");
}

TEST_CASE("redBlack: deleting nodes with two children") {
    SUBCASE("the successor is deeper: it is detached, then takes z's place, links and color") {
        Engine engine;
        for (const Value value : {10, 20, 30, 40, 70, 90, 60, 50}) insert(engine, value);
        REQUIRE(shape(engine) == "40B(20R(10B,30B),70R(60B(50R,-),90B))");
        const std::map<Value, NodeId> idsBefore = idsByValue(engine);
        const Response response = remove(engine, 40);
        CHECK(shape(engine) == "50B(20R(10B,30B),70R(60B,90B))");
        CHECK(response.snapshot.root == idsBefore.at(50));
        for (const char* step : {"redBlack.delete.successorStart", "redBlack.delete.successorLeft",
                                 "redBlack.delete.successorFound", "redBlack.delete.detachSuccessor",
                                 "redBlack.delete.successorTakesRight", "redBlack.delete.replaceWithSuccessor",
                                 "redBlack.delete.successorTakesLeft", "redBlack.delete.successorTakesColor"}) {
            CAPTURE(step);
            frameWithStep(response, step);
        }
        std::map<Value, NodeId> expected = idsBefore;
        expected.erase(40);
        CHECK(idsByValue(engine) == expected);
    }
    SUBCASE("the successor is the right child and x is NIL: nil.parent leads the fix-up") {
        Engine engine;
        for (const Value value : {10, 20, 30, 40}) insert(engine, value);
        remove(engine, 40);
        REQUIRE(shape(engine) == "20B(10B,30B)");
        const Response response = remove(engine, 20);
        CHECK(shape(engine) == "30B(10R,-)");
        // x is NIL, the right child of 30: the mirror branch, found through nil.parent.
        CHECK(stepsWith(response, Event::FixupCase) == std::vector<std::string>{"redBlack.deleteFixup.case2Mirror"});
    }
}

TEST_CASE("redBlack: deleting every key until the tree is empty") {
    std::mt19937 random(5);
    std::vector<Value> values;
    for (Value value = 1; value <= 60; ++value) values.push_back(value * 3);
    std::shuffle(values.begin(), values.end(), random);
    Engine engine;
    for (const Value value : values) REQUIRE(insert(engine, value).status.ok());
    std::shuffle(values.begin(), values.end(), random);

    std::set<Value> left(values.begin(), values.end());
    for (const Value value : values) {
        CAPTURE(value);
        REQUIRE(remove(engine, value).status.ok());  // run() checks all five properties after each
        left.erase(value);
        std::set<Value> members;
        for (const NodeView& node : engine.snapshot(StructureKind::RedBlack).nodes) members.insert(node.value);
        REQUIRE(members == left);
    }
    const Snapshot empty = engine.snapshot(StructureKind::RedBlack);
    CHECK(empty.size == 0);
    CHECK(empty.root == kNoNode);
    CHECK(empty.nilColor == Color::Black);
}

TEST_CASE("redBlack: duplicates and absent keys are rejected and change nothing") {
    Engine engine;
    test::checkRejected(engine, rbRequest(Operation::Delete, 5), ErrorCode::ValueNotFound);  // empty tree
    for (const Value value : {20, 10, 30}) insert(engine, value);
    test::checkRejected(engine, rbRequest(Operation::Insert, 10), ErrorCode::DuplicateValue);
    test::checkRejected(engine, rbRequest(Operation::Delete, 25), ErrorCode::ValueNotFound);
    CHECK(integerData(insert(engine, 25), "nodeId") == 4);  // no ID was used up
}

TEST_CASE("redBlack: search, traversals and the empty tree") {
    Engine engine;
    CHECK_FALSE(booleanData(test::run(engine, rbRequest(Operation::Search, 1)), "found"));
    CHECK(listData(test::run(engine, rbRequest(Operation::Inorder)), "values").empty());

    for (const Value value : {1, 2, 3, 4, 5, 6, 7}) insert(engine, value);
    CHECK(shape(engine) == "2B(1B,4R(3B,6B(5R,7R)))");
    CHECK(listData(test::run(engine, rbRequest(Operation::Inorder)), "values") ==
          std::vector<std::int64_t>{1, 2, 3, 4, 5, 6, 7});
    CHECK(listData(test::run(engine, rbRequest(Operation::Preorder)), "values") ==
          std::vector<std::int64_t>{2, 1, 4, 3, 6, 5, 7});
    CHECK(listData(test::run(engine, rbRequest(Operation::Postorder)), "values") ==
          std::vector<std::int64_t>{1, 3, 5, 7, 6, 4, 2});
    CHECK(listData(test::run(engine, rbRequest(Operation::Levelorder)), "values") ==
          std::vector<std::int64_t>{2, 1, 4, 3, 6, 5, 7});

    const Response found = test::run(engine, rbRequest(Operation::Search, 5));
    CHECK(booleanData(found, "found"));
    CHECK(found.frames.back().highlight.nodes.size() == 4);  // 2 -> 4 -> 6 -> 5
    const Response missing = test::run(engine, rbRequest(Operation::Search, 8));
    CHECK_FALSE(booleanData(missing, "found"));
    CHECK(frameWithStep(missing, "redBlack.search.notFound").explanation ==
          "Reached NIL: 8 is not in the red-black tree.");
}

TEST_CASE("redBlack: 200 increasing values stay within the height bound") {
    Engine engine;
    for (int i = 0; i < 200; ++i) {
        REQUIRE(insert(engine, i).status.ok());
    }
    const Snapshot snapshot = engine.snapshot(StructureKind::RedBlack);
    CHECK(treeHeight(snapshot, snapshot.root) <= 2.0 * std::log2(201.0));  // a plain BST would be 200 deep
    test::checkRejected(engine, rbRequest(Operation::Insert, 1000), ErrorCode::CapacityExceeded);
    CHECK(integerData(test::run(engine, rbRequest(Operation::Clear)), "removedCount") == 200);
    CHECK(engine.snapshot(StructureKind::RedBlack).root == kNoNode);
}

TEST_CASE("redBlack: explanations stay within 140 characters with the longest values") {
    // Every value has 11 characters; run() checks the length of every frame's sentence.
    for (const Scenario& scenario : scenarios()) {
        CAPTURE(scenario.name);
        Engine engine;
        for (const Value value : scenario.setup) insert(engine, -2147483000 + value);
        const Response last = test::run(engine, rbRequest(scenario.operation, -2147483000 + scenario.value));
        CHECK(last.status.ok());
        CHECK(stepsWith(last, Event::FixupCase) == scenario.cases);
    }
}

TEST_CASE("redBlack: 5,000 seeded random operations agree with std::set") {
    Engine engine;
    std::set<Value> oracle;  // allowed as a test oracle (CLAUDE.md, rule 1)
    std::mt19937 random(20261022);
    std::map<std::string, int> casesSeen;  // how often each fix-up case ran

    for (int step = 0; step < 5000; ++step) {
        CAPTURE(step);
        const Value value = static_cast<Value>(random() % 121) - 60;
        const unsigned kind = random() % 100;
        Response response;
        if (kind < 50) {
            response = insert(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 0));
            oracle.insert(value);
        } else if (kind < 90) {
            response = remove(engine, value);
            REQUIRE(response.status.ok() == (oracle.count(value) == 1));
            oracle.erase(value);
        } else {
            response = test::run(engine, rbRequest(Operation::Search, value));
            REQUIRE(booleanData(response, "found") == (oracle.count(value) == 1));
        }
        for (const std::string& caseStep : stepsWith(response, Event::FixupCase)) {
            casesSeen[caseStep] += 1;
        }

        // Membership and in-order output match the oracle (run() has already
        // checked every red-black property), and the height is within 2 log2(n + 1).
        const Snapshot snapshot = engine.snapshot(StructureKind::RedBlack);
        std::vector<std::int64_t> inorder;
        inorderValues(snapshot, snapshot.root, inorder);
        const std::vector<std::int64_t> expected(oracle.begin(), oracle.end());
        REQUIRE(inorder == expected);
        REQUIRE(treeHeight(snapshot, snapshot.root) <= 2.0 * std::log2(static_cast<double>(oracle.size()) + 1.0));

        // The engine's own in-order traversal, every 100 operations.
        if (step % 100 == 99) {
            REQUIRE(listData(test::run(engine, rbRequest(Operation::Inorder)), "values") == expected);
        }
    }
    const std::vector<std::int64_t> expected(oracle.begin(), oracle.end());
    CHECK(listData(test::run(engine, rbRequest(Operation::Inorder)), "values") == expected);

    // The run exercised every fix-up case on both sides.
    std::string counts;
    for (const std::string& id : allCaseIds()) {
        CAPTURE(id);
        CHECK(casesSeen[id] > 0);
        counts += (counts.empty() ? "" : ", ") + id.substr(id.find('.') + 1) + " " + std::to_string(casesSeen[id]);
    }
    MESSAGE("random run, fix-up cases seen: " << counts);
}
