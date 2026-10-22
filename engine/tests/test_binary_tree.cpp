// Ordinary binary tree behaviour through the Engine; the shared run() checks
// the tree invariants and the frame contract after every operation.
#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Run.h"

using namespace dslab;
using test::events;
using test::integerData;
using test::listData;

namespace {

Request treeRequest(Operation operation) {
    Request request;
    request.structure = StructureKind::BinaryTree;
    request.operation = operation;
    return request;
}

Response addRoot(Engine& engine, Value value) {
    Request request = treeRequest(Operation::AddRoot);
    request.value = value;
    return test::run(engine, request);
}

Request addChildRequest(NodeId parent, Side side, Value value) {
    Request request = treeRequest(Operation::AddChild);
    request.parentId = parent;
    request.side = side;
    request.value = value;
    return request;
}

Response addChild(Engine& engine, NodeId parent, Side side, Value value) {
    return test::run(engine, addChildRequest(parent, side, value));
}

Response deleteSubtree(Engine& engine, NodeId id) {
    Request request = treeRequest(Operation::DeleteSubtree);
    request.nodeId = id;
    return test::run(engine, request);
}

Response traverse(Engine& engine, Operation order) { return test::run(engine, treeRequest(order)); }

std::vector<std::int64_t> values(Engine& engine, Operation order) {
    return listData(traverse(engine, order), "values");
}

std::int64_t count(const std::vector<Event>& events, Event event) {
    std::int64_t total = 0;
    for (const Event e : events) {
        total += (e == event) ? 1 : 0;
    }
    return total;
}

// Builds this tree (node IDs after #):
//   10 #1
//   +-- left:  20 #2
//   |          +-- left:  40 #4
//   |          +-- right: 50 #5
//   +-- right: 30 #3
//              +-- right: 60 #6
void buildSample(Engine& engine) {
    addRoot(engine, 10);
    addChild(engine, 1, Side::Left, 20);
    addChild(engine, 1, Side::Right, 30);
    addChild(engine, 2, Side::Left, 40);
    addChild(engine, 2, Side::Right, 50);
    addChild(engine, 3, Side::Right, 60);
}

}  // namespace

TEST_CASE("binary tree: the user places every node") {
    Engine engine;
    const Response root = addRoot(engine, 10);
    CHECK(events(root) == std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Done});
    CHECK(root.snapshot.root == 1);

    const Response child = addChild(engine, 1, Side::Right, 30);
    CHECK(events(child) == std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Done});
    CHECK(child.counters.linksChanged == 2);  // node.parent and parent.right
    // In the create frame the node knows its parent, but the parent's link is still NIL.
    const Snapshot& created = child.frames[1].snapshot;
    CHECK(created.nodes[1].parent == 1);
    CHECK(created.nodes[0].right == kNoNode);

    // A node with one child keeps that child on the side the user chose.
    const Snapshot after = engine.snapshot(StructureKind::BinaryTree);
    CHECK(after.nodes[0].left == kNoNode);
    CHECK(after.nodes[0].right == 2);
}

TEST_CASE("binary tree: duplicates and negative values are allowed") {
    Engine engine;
    addRoot(engine, -5);
    addChild(engine, 1, Side::Left, -5);
    addChild(engine, 1, Side::Right, -5);
    CHECK(values(engine, Operation::Preorder) == std::vector<std::int64_t>{-5, -5, -5});
    CHECK(listData(traverse(engine, Operation::Preorder), "nodeIds") == std::vector<std::int64_t>{1, 2, 3});
}

TEST_CASE("binary tree: rejected placements change nothing") {
    Engine engine;
    test::checkRejected(engine, addChildRequest(1, Side::Left, 5), ErrorCode::NodeNotFound);  // empty tree

    addRoot(engine, 10);
    addChild(engine, 1, Side::Left, 20);

    Request secondRoot = treeRequest(Operation::AddRoot);
    secondRoot.value = 99;
    test::checkRejected(engine, secondRoot, ErrorCode::RootExists);
    test::checkRejected(engine, addChildRequest(1, Side::Left, 99), ErrorCode::ChildExists);
    test::checkRejected(engine, addChildRequest(42, Side::Right, 99), ErrorCode::NodeNotFound);

    const Response occupied = test::run(engine, addChildRequest(1, Side::Left, 99));
    CHECK(occupied.status.message == "#1 (10) already has a left child, #2 (20).");
    CHECK(integerData(addChild(engine, 1, Side::Right, 30), "nodeId") == 3);  // no ID was used up
}

TEST_CASE("binary tree: the four traversals") {
    Engine engine;
    buildSample(engine);
    CHECK(values(engine, Operation::Preorder) == std::vector<std::int64_t>{10, 20, 40, 50, 30, 60});
    CHECK(values(engine, Operation::Inorder) == std::vector<std::int64_t>{40, 20, 50, 10, 30, 60});
    CHECK(values(engine, Operation::Postorder) == std::vector<std::int64_t>{40, 50, 20, 60, 30, 10});
    CHECK(values(engine, Operation::Levelorder) == std::vector<std::int64_t>{10, 20, 30, 40, 50, 60});

    // Depth-first: one arrival and one visit frame per node, plus begin and done.
    const Response inorder = traverse(engine, Operation::Inorder);
    CHECK(inorder.frames.size() == 2 * 6 + 2);
    CHECK(count(events(inorder), Event::Visit) == 6);
    CHECK(count(events(inorder), Event::Move) == 6);
    CHECK(inorder.frames[1].stepId == "binaryTree.inorder.start");
    // The output grows one value per visit; the last frame shows all of it.
    CHECK(inorder.frames.back().output == std::vector<Value>{40, 20, 50, 10, 30, 60});

    // Level order: one visit frame per node; "pending" is the queue after the visit.
    const Response level = traverse(engine, Operation::Levelorder);
    CHECK(level.frames.size() == 6 + 2);
    CHECK(level.frames[1].pending == std::vector<NodeId>{2, 3});
    CHECK(level.frames[2].pending == std::vector<NodeId>{3, 4, 5});
    CHECK(level.frames[6].pending.empty());
}

TEST_CASE("binary tree: traversals of an empty and a single-node tree") {
    Engine engine;
    const Response empty = traverse(engine, Operation::Postorder);
    CHECK(listData(empty, "values").empty());
    CHECK(events(empty) == std::vector<Event>{Event::Begin, Event::Done});

    addRoot(engine, 7);
    for (const Operation order : {Operation::Preorder, Operation::Inorder, Operation::Postorder, Operation::Levelorder}) {
        CHECK(values(engine, order) == std::vector<std::int64_t>{7});
    }
}

TEST_CASE("binary tree: deleting a leaf, an inner subtree and the root") {
    Engine engine;
    buildSample(engine);

    const Response leaf = deleteSubtree(engine, 6);
    CHECK(listData(leaf, "removedIds") == std::vector<std::int64_t>{6});
    CHECK(engine.snapshot(StructureKind::BinaryTree).nodes[2].right == kNoNode);  // 30 lost its child

    // Post-order: children are freed before their parent.
    const Response inner = deleteSubtree(engine, 2);
    CHECK(listData(inner, "removedIds") == std::vector<std::int64_t>{4, 5, 2});
    CHECK(integerData(inner, "removedCount") == 3);
    CHECK(events(inner) == std::vector<Event>{Event::Begin, Event::Unlink, Event::Remove, Event::Remove,
                                              Event::Remove, Event::Done});
    CHECK(values(engine, Operation::Preorder) == std::vector<std::int64_t>{10, 30});

    const Response root = deleteSubtree(engine, 1);
    CHECK(listData(root, "removedIds") == std::vector<std::int64_t>{3, 1});
    CHECK(root.snapshot.root == kNoNode);
    CHECK(root.snapshot.nodes.empty());

    // Freed and unknown IDs are rejected.
    for (const NodeId id : {2, 99}) {
        Request request = treeRequest(Operation::DeleteSubtree);
        request.nodeId = id;
        test::checkRejected(engine, request, ErrorCode::NodeNotFound);
    }
}

TEST_CASE("binary tree: a 200-node chain fills the tree, traverses and clears") {
    Engine engine;
    addRoot(engine, 0);
    for (int i = 1; i < 200; ++i) {
        REQUIRE(addChild(engine, i, Side::Left, i).status.ok());  // every node hangs left: depth 200
    }
    test::checkRejected(engine, addChildRequest(200, Side::Left, 200), ErrorCode::CapacityExceeded);

    CHECK(values(engine, Operation::Inorder).front() == 199);  // the deepest node comes first
    CHECK(values(engine, Operation::Postorder).back() == 0);

    const Response cleared = test::run(engine, treeRequest(Operation::Clear));
    CHECK(integerData(cleared, "removedCount") == 200);
    CHECK(cleared.snapshot.root == kNoNode);
    CHECK(integerData(test::run(engine, treeRequest(Operation::Clear)), "removedCount") == 0);
}
