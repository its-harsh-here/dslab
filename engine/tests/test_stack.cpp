// Stack behaviour through the Engine, with both checkers run after every
// operation (CLAUDE.md, testing rules).
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Checkers.h"

using namespace dslab;

namespace {

// Runs one operation, then checks the stack invariants and the frame contract.
Response run(Engine& engine, Operation operation, Value value = 0) {
    const Snapshot before = engine.snapshot(StructureKind::Stack);

    Request request;
    request.operation = operation;
    request.value = value;
    Response response = engine.apply(request);

    CHECK(test::checkStackSnapshot(response.snapshot) == "");
    CHECK(test::checkFrames(response, before) == "");
    return response;
}

Response push(Engine& engine, Value value) { return run(engine, Operation::Push, value); }
Response pop(Engine& engine) { return run(engine, Operation::Pop); }
Response peek(Engine& engine) { return run(engine, Operation::Peek); }
Response clear(Engine& engine) { return run(engine, Operation::Clear); }

// The named field of a successful result, e.g. field(response, "value").
std::int64_t field(const Response& response, const std::string& name) {
    REQUIRE(response.status.ok());
    for (const DataField& data : response.data) {
        if (data.name == name) {
            return data.value;
        }
    }
    FAIL("missing result field " << name);
    return 0;
}

std::vector<Event> events(const Response& response) {
    std::vector<Event> result;
    for (const Frame& frame : response.frames) {
        result.push_back(frame.event);
    }
    return result;
}

// Values from top to bottom, following the links in a snapshot.
std::vector<Value> valuesFromTop(const Snapshot& snapshot) {
    std::vector<Value> values;
    NodeId current = snapshot.top;
    while (current != kNoNode && values.size() <= snapshot.nodes.size()) {
        NodeId next = kNoNode;
        for (const NodeView& node : snapshot.nodes) {
            if (node.id == current) {
                values.push_back(node.value);
                next = node.next;
            }
        }
        current = next;
    }
    return values;
}

}  // namespace

TEST_CASE("pop returns values in LIFO order") {
    Engine engine;
    for (const Value value : {10, 20, 30}) {
        REQUIRE(push(engine, value).status.ok());
    }
    CHECK(valuesFromTop(engine.snapshot(StructureKind::Stack)) == std::vector<Value>{30, 20, 10});

    CHECK(field(pop(engine), "value") == 30);
    CHECK(field(pop(engine), "value") == 20);
    CHECK(field(push(engine, 40), "nodeId") == 4);
    CHECK(field(pop(engine), "value") == 40);
    CHECK(field(pop(engine), "value") == 10);
    CHECK(engine.snapshot(StructureKind::Stack).size == 0);
}

TEST_CASE("peek returns the top value and changes nothing") {
    Engine engine;
    push(engine, 10);
    push(engine, 20);
    const Snapshot before = engine.snapshot(StructureKind::Stack);
    const int undoBefore = engine.undoCount(StructureKind::Stack);

    const Response response = peek(engine);
    CHECK(field(response, "value") == 20);
    CHECK(field(response, "nodeId") == 2);
    CHECK(response.snapshot == before);
    CHECK(engine.undoCount(StructureKind::Stack) == undoBefore);  // reads make no history
}

TEST_CASE("repeated values are allowed and keep their own IDs") {
    Engine engine;
    for (int i = 0; i < 3; ++i) {
        REQUIRE(push(engine, 7).status.ok());
    }

    const Snapshot snapshot = engine.snapshot(StructureKind::Stack);
    REQUIRE(snapshot.nodes.size() == 3);
    CHECK(snapshot.nodes[0].id == 1);
    CHECK(snapshot.nodes[1].id == 2);
    CHECK(snapshot.nodes[2].id == 3);
    CHECK(snapshot.top == 3);
    CHECK(valuesFromTop(snapshot) == std::vector<Value>{7, 7, 7});

    // LIFO by node, even though the values are equal.
    CHECK(field(pop(engine), "nodeId") == 3);
    CHECK(field(pop(engine), "nodeId") == 2);
    CHECK(field(pop(engine), "nodeId") == 1);
}

TEST_CASE("pop and peek on an empty stack are typed errors that change nothing") {
    Engine engine;
    std::int64_t nextId = 1;

    SUBCASE("on a stack that was never used") {}
    SUBCASE("on a stack that became empty again") {
        push(engine, 5);
        pop(engine);
        nextId = 2;
    }

    const Snapshot before = engine.snapshot(StructureKind::Stack);
    const int undoBefore = engine.undoCount(StructureKind::Stack);

    const Response popped = pop(engine);
    CHECK(popped.status.code == ErrorCode::EmptyStructure);
    CHECK(popped.status.message == "Cannot pop: the stack is empty.");
    CHECK(events(popped) == std::vector<Event>{Event::Begin, Event::Reject});
    CHECK(popped.snapshot == before);

    const Response peeked = peek(engine);
    CHECK(peeked.status.code == ErrorCode::EmptyStructure);
    CHECK(peeked.status.message == "Cannot peek: the stack is empty.");
    CHECK(events(peeked) == std::vector<Event>{Event::Begin, Event::Reject});
    CHECK(peeked.snapshot == before);

    // No history entry, and no node ID was used up by the rejections.
    CHECK(engine.undoCount(StructureKind::Stack) == undoBefore);
    CHECK(field(push(engine, 9), "nodeId") == nextId);
}

TEST_CASE("1,000 create/destroy cycles leave an empty stack and fresh IDs") {
    Engine engine;
    for (int i = 1; i <= 1000; ++i) {
        const Response pushed = push(engine, i);
        REQUIRE(field(pushed, "nodeId") == i);  // IDs are never reused
        const Response popped = pop(engine);
        REQUIRE(field(popped, "value") == i);
        REQUIRE(field(popped, "nodeId") == i);
        REQUIRE(popped.snapshot.size == 0);
    }

    const Snapshot after = engine.snapshot(StructureKind::Stack);
    CHECK(after.nodes.empty());
    CHECK(after.top == kNoNode);
    CHECK(engine.undoCount(StructureKind::Stack) == 50);  // history stays bounded
    CHECK(field(push(engine, 1), "nodeId") == 1001);
}

TEST_CASE("the 201st push is rejected and changes nothing") {
    Engine engine;
    for (int i = 1; i <= 200; ++i) {
        REQUIRE(push(engine, i).status.ok());
    }
    const Snapshot full = engine.snapshot(StructureKind::Stack);
    const int undoBefore = engine.undoCount(StructureKind::Stack);

    const Response rejected = push(engine, 201);
    CHECK(rejected.status.code == ErrorCode::CapacityExceeded);
    CHECK(events(rejected) == std::vector<Event>{Event::Begin, Event::Reject});
    CHECK(rejected.snapshot == full);
    CHECK(engine.undoCount(StructureKind::Stack) == undoBefore);

    // The rejected push used no ID: after one pop, the next node gets ID 201.
    CHECK(field(pop(engine), "value") == 200);
    CHECK(field(push(engine, 201), "nodeId") == 201);
}

TEST_CASE("clear frees every node with one frame per node") {
    Engine engine;
    push(engine, 1);
    push(engine, 2);
    push(engine, 3);

    const Response cleared = clear(engine);
    CHECK(field(cleared, "removedCount") == 3);
    CHECK(events(cleared) == std::vector<Event>{Event::Begin, Event::Remove, Event::Remove,
                                                Event::Remove, Event::Done});
    CHECK(cleared.snapshot.nodes.empty());
    CHECK(cleared.counters.nodesVisited == 3);
    CHECK(cleared.counters.linksChanged == 3);

    const Response again = clear(engine);
    CHECK(field(again, "removedCount") == 0);
    CHECK(events(again) == std::vector<Event>{Event::Begin, Event::Done});
}

TEST_CASE("push and pop follow the documented steps and count their own work") {
    Engine engine;
    push(engine, 10);

    const Response pushed = push(engine, 20);
    CHECK(events(pushed) ==
          std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Link, Event::Done});
    CHECK(pushed.frames[1].stepId == "stack.push.create");
    CHECK(pushed.frames[2].stepId == "stack.push.linkNext");
    CHECK(pushed.frames[3].stepId == "stack.push.moveTop");
    CHECK(pushed.counters.linksChanged == 2);
    CHECK(pushed.counters.nodesVisited == 0);
    CHECK(pushed.counters.comparisons == 0);

    // In the create frame the new node exists but is not linked yet.
    const Snapshot& created = pushed.frames[1].snapshot;
    CHECK(created.size == 2);
    CHECK(created.top == 1);

    const Response popped = pop(engine);
    CHECK(events(popped) ==
          std::vector<Event>{Event::Begin, Event::Move, Event::Link, Event::Remove, Event::Done});
    CHECK(popped.counters.nodesVisited == 1);
    CHECK(popped.counters.linksChanged == 1);

    // After "top <- node.next" the old top is detached but still owned.
    const Snapshot& detached = popped.frames[2].snapshot;
    CHECK(detached.size == 2);
    CHECK(detached.top == 1);
}

TEST_CASE("the snapshot operation is read-only") {
    Engine engine;
    push(engine, 3);

    Request request;
    request.operation = Operation::Snapshot;
    const Response response = engine.apply(request);
    CHECK(response.status.ok());
    CHECK(events(response) == std::vector<Event>{Event::Begin, Event::Done});
    CHECK(response.frames[1].explanation == "The stack holds 1 node.");
    CHECK(engine.undoCount(StructureKind::Stack) == 1);
}

TEST_CASE("values at both ends of the 32-bit range round-trip") {
    Engine engine;
    const Value lowest = std::numeric_limits<Value>::min();
    const Value highest = std::numeric_limits<Value>::max();
    push(engine, lowest);
    push(engine, highest);
    CHECK(field(pop(engine), "value") == highest);
    CHECK(field(pop(engine), "value") == lowest);
}
