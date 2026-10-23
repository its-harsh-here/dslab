// Operation-level undo and redo (operation-contract.md 2.8, architecture.md
// "Undo and redo"): snapshot-based and exact (IDs, values, links, heights,
// colors and the ID counter), bounded at 50 entries, and a new operation after
// an undo clears the redo branch. Every request goes through test::run(), so
// the structure and frame checkers also run after each undo and redo.
#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "dslab/Limits.h"
#include "support/Run.h"

using namespace dslab;

namespace {

struct Step {
    Operation operation;
    Value value = 0;
    std::int32_t index = 0;
    NodeId nodeId = kNoNode;
    NodeId parentId = kNoNode;
    Side side = Side::Left;
};

Request make(StructureKind structure, const Step& step) {
    Request request;
    request.structure = structure;
    request.operation = step.operation;
    request.value = step.value;
    request.index = step.index;
    request.nodeId = step.nodeId;
    request.parentId = step.parentId;
    request.side = step.side;
    return request;
}

Request make(StructureKind structure, Operation operation, Value value = 0) {
    return make(structure, Step{operation, value});
}

std::string wire(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return "stack";
        case StructureKind::Queue: return "queue";
        case StructureKind::List: return "list";
        case StructureKind::BinaryTree: return "binaryTree";
        case StructureKind::Bst: return "bst";
        case StructureKind::Avl: return "avl";
        case StructureKind::RedBlack: return "redBlack";
    }
    return "?";
}

// For every structure, a sequence of successful changes, including a clear
// and (for the trees) deletes and rotations.
const std::vector<std::pair<StructureKind, std::vector<Step>>>& sequences() {
    using O = Operation;
    static const std::vector<std::pair<StructureKind, std::vector<Step>>> all = {
        {StructureKind::Stack, {{O::Push, 10}, {O::Push, 20}, {O::Pop}, {O::Push, 30}, {O::Clear}, {O::Push, 40}}},
        {StructureKind::Queue, {{O::Enqueue, 10}, {O::Enqueue, 20}, {O::Dequeue}, {O::Enqueue, 30}, {O::Clear}, {O::Enqueue, 40}}},
        {StructureKind::List,
         {{O::InsertAt, 10, 0}, {O::InsertAt, 20, 1}, {O::InsertAt, 15, 1}, {O::DeleteNode, 0, 0, 2}, {O::Reverse}, {O::Clear},
          {O::InsertAt, 5, 0}}},
        {StructureKind::BinaryTree,
         {{O::AddRoot, 1}, {O::AddChild, 2, 0, kNoNode, 1, Side::Left}, {O::AddChild, 3, 0, kNoNode, 1, Side::Right},
          {O::AddChild, 4, 0, kNoNode, 2, Side::Left}, {O::DeleteSubtree, 0, 0, 2}, {O::Clear}, {O::AddRoot, 9}}},
        {StructureKind::Bst,
         {{O::Insert, 20}, {O::Insert, 10}, {O::Insert, 30}, {O::Insert, 25}, {O::Delete, 20}, {O::Clear}, {O::Insert, 5}}},
        {StructureKind::Avl,
         {{O::Insert, 30}, {O::Insert, 20}, {O::Insert, 10}, {O::Insert, 40}, {O::Insert, 50}, {O::Delete, 10}, {O::Clear},
          {O::Insert, 1}}},
        {StructureKind::RedBlack,
         {{O::Insert, 10}, {O::Insert, 20}, {O::Insert, 30}, {O::Insert, 15}, {O::Insert, 25}, {O::Delete, 10}, {O::Delete, 30},
          {O::Clear}, {O::Insert, 7}}},
    };
    return all;
}

}  // namespace

TEST_CASE("history: undo and redo round-trip every structure exactly") {
    for (const auto& [structure, steps] : sequences()) {
        CAPTURE(wire(structure));
        Engine engine;
        // states[i] is the structure after i steps, ID counter included.
        std::vector<Snapshot> states{engine.snapshot(structure)};
        for (const Step& step : steps) {
            REQUIRE(test::run(engine, make(structure, step)).status.ok());
            states.push_back(engine.snapshot(structure));
        }
        const int n = static_cast<int>(steps.size());

        for (int i = n; i > 0; --i) {
            CAPTURE(i);
            const Response undone = test::run(engine, make(structure, Operation::Undo));
            REQUIRE(undone.status.ok());
            CHECK(engine.snapshot(structure) == states[i - 1]);  // every field, and the ID counter
            CHECK(undone.undoCount == i - 1);
            CHECK(undone.redoCount == n - i + 1);
        }
        test::checkRejected(engine, make(structure, Operation::Undo), ErrorCode::NothingToUndo);

        for (int i = 1; i <= n; ++i) {
            CAPTURE(i);
            const Response redone = test::run(engine, make(structure, Operation::Redo));
            REQUIRE(redone.status.ok());
            CHECK(engine.snapshot(structure) == states[i]);
            CHECK(redone.undoCount == i);
            CHECK(redone.redoCount == n - i);
        }
        test::checkRejected(engine, make(structure, Operation::Redo), ErrorCode::NothingToRedo);
    }
}

TEST_CASE("history: undo restores the ID counter exactly") {
    Engine engine;
    for (const Value value : {10, 20, 30}) test::run(engine, make(StructureKind::Bst, Operation::Insert, value));
    CHECK(engine.snapshot(StructureKind::Bst).nextId == 4);

    test::run(engine, make(StructureKind::Bst, Operation::Undo));  // undoes "insert 30", which created node 3
    CHECK(engine.snapshot(StructureKind::Bst).nextId == 3);
    // So the next node gets ID 3 again (the undone node 3 no longer exists anywhere: the
    // insert below clears the redo list that held it).
    const Response insert = test::run(engine, make(StructureKind::Bst, Operation::Insert, 40));
    CHECK(test::integerData(insert, "nodeId") == 3);
    CHECK(insert.redoCount == 0);
}

TEST_CASE("history: a new operation after an undo clears the redo branch") {
    Engine engine;
    std::vector<Snapshot> states{engine.snapshot(StructureKind::Stack)};
    for (const Value value : {1, 2, 3}) {
        test::run(engine, make(StructureKind::Stack, Operation::Push, value));
        states.push_back(engine.snapshot(StructureKind::Stack));
    }
    test::run(engine, make(StructureKind::Stack, Operation::Undo));
    const Response secondUndo = test::run(engine, make(StructureKind::Stack, Operation::Undo));
    CHECK(secondUndo.redoCount == 2);

    const Response push = test::run(engine, make(StructureKind::Stack, Operation::Push, 9));
    CHECK(push.redoCount == 0);
    CHECK(push.undoCount == 2);
    test::checkRejected(engine, make(StructureKind::Stack, Operation::Redo), ErrorCode::NothingToRedo);

    test::run(engine, make(StructureKind::Stack, Operation::Undo));
    CHECK(engine.snapshot(StructureKind::Stack) == states[1]);  // back to "push 1", not to the undone pushes
}

TEST_CASE("history: at most 50 entries per structure, the oldest dropped first") {
    Engine engine;
    std::vector<Snapshot> states{engine.snapshot(StructureKind::Stack)};
    for (Value value = 1; value <= 60; ++value) {
        const Response push = test::run(engine, make(StructureKind::Stack, Operation::Push, value));
        CHECK(push.undoCount == (value < kMaxHistory ? value : kMaxHistory));
        states.push_back(engine.snapshot(StructureKind::Stack));
    }

    // The 50 newest changes can be undone; the 10 oldest were dropped.
    for (int i = 0; i < kMaxHistory; ++i) {
        REQUIRE(test::run(engine, make(StructureKind::Stack, Operation::Undo)).status.ok());
    }
    CHECK(engine.snapshot(StructureKind::Stack) == states[10]);
    test::checkRejected(engine, make(StructureKind::Stack, Operation::Undo), ErrorCode::NothingToUndo);

    // Undo and redo only move entries between the lists: never more than 50 in all.
    const Response redo = test::run(engine, make(StructureKind::Stack, Operation::Redo));
    CHECK(redo.undoCount + redo.redoCount == kMaxHistory);
    for (int i = 1; i < kMaxHistory; ++i) test::run(engine, make(StructureKind::Stack, Operation::Redo));
    CHECK(engine.snapshot(StructureKind::Stack) == states[60]);
}

TEST_CASE("history: undo and redo name the operation they reverse") {
    Engine engine;
    for (const Value value : {30, 20, 10}) test::run(engine, make(StructureKind::Avl, Operation::Insert, value));  // 10 rotates

    const Response undone = test::run(engine, make(StructureKind::Avl, Operation::Undo));
    REQUIRE(undone.frames.size() == 3);
    CHECK(test::events(undone) == std::vector<Event>{Event::Begin, Event::Restore, Event::Done});
    CHECK(undone.frames[0].stepId == "common.undo.enter");
    CHECK(undone.frames[0].explanation == "Undo insert 10 on the AVL tree.");
    CHECK(undone.frames[1].stepId == "common.undo.restore");
    CHECK(undone.frames[1].explanation == "Restore the AVL tree as it was before insert 10, with the same node IDs.");
    CHECK(undone.frames[2].explanation == "Undid insert 10; the AVL tree holds 2 nodes.");
    // The heights and balances came back too (30 with its left child 20).
    const Snapshot snapshot = engine.snapshot(StructureKind::Avl);
    CHECK(snapshot.nodes[0].height == 2);
    CHECK(snapshot.nodes[0].balance == 1);

    const Response redone = test::run(engine, make(StructureKind::Avl, Operation::Redo));
    CHECK(redone.frames[0].explanation == "Redo insert 10 on the AVL tree.");
    CHECK(redone.frames[1].explanation == "Restore the AVL tree as it was after insert 10, with the same node IDs.");
}

TEST_CASE("history: rejected and read-only operations add no entry") {
    Engine engine;
    test::run(engine, make(StructureKind::RedBlack, Operation::Insert, 10));
    for (const Operation operation : {Operation::Search, Operation::Inorder, Operation::Export, Operation::Snapshot}) {
        CHECK(test::run(engine, make(StructureKind::RedBlack, operation, 10)).undoCount == 1);
    }
    test::checkRejected(engine, make(StructureKind::RedBlack, Operation::Insert, 10), ErrorCode::DuplicateValue);
    CHECK(engine.undoCount(StructureKind::RedBlack) == 1);
}

TEST_CASE("history: an import is one undoable operation") {
    Engine source;
    for (const Value value : {5, 3, 8, 1}) test::run(source, make(StructureKind::RedBlack, Operation::Insert, value));

    Engine engine;
    for (const Value value : {40, 50}) test::run(engine, make(StructureKind::RedBlack, Operation::Insert, value));
    const Snapshot before = engine.snapshot(StructureKind::RedBlack);

    Request import = make(StructureKind::RedBlack, Operation::Import);
    import.importSnapshot = source.snapshot(StructureKind::RedBlack);
    const Response imported = test::run(engine, import);
    REQUIRE(imported.status.ok());
    CHECK(engine.snapshot(StructureKind::RedBlack) == source.snapshot(StructureKind::RedBlack));
    CHECK(test::events(imported) == std::vector<Event>{Event::Begin, Event::Restore, Event::Done});

    const Response undone = test::run(engine, make(StructureKind::RedBlack, Operation::Undo));
    CHECK(undone.frames[0].explanation == "Undo import on the red-black tree.");
    CHECK(engine.snapshot(StructureKind::RedBlack) == before);
    test::run(engine, make(StructureKind::RedBlack, Operation::Redo));
    CHECK(engine.snapshot(StructureKind::RedBlack) == source.snapshot(StructureKind::RedBlack));
}
