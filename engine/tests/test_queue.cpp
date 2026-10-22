// Queue behaviour through the Engine; the shared run() checks the invariants
// and the frame contract after every operation.
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Run.h"

using namespace dslab;
using test::events;
using test::integerData;

namespace {

Response run(Engine& engine, Operation operation, Value value = 0) {
    return test::run(engine, StructureKind::Queue, operation, value);
}

Response enqueue(Engine& engine, Value value) { return run(engine, Operation::Enqueue, value); }
Response dequeue(Engine& engine) { return run(engine, Operation::Dequeue); }
Response front(Engine& engine) { return run(engine, Operation::Front); }
Response clear(Engine& engine) { return run(engine, Operation::Clear); }

Request queueRequest(Operation operation, Value value = 0) {
    Request request;
    request.structure = StructureKind::Queue;
    request.operation = operation;
    request.value = value;
    return request;
}

std::vector<Value> valuesFromFront(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(StructureKind::Queue);
    return test::chainValues(snapshot, snapshot.front);
}

}  // namespace

TEST_CASE("queue: dequeue returns values in FIFO order") {
    Engine engine;
    for (const Value value : {10, 20, 30}) {
        REQUIRE(enqueue(engine, value).status.ok());
    }
    CHECK(valuesFromFront(engine) == std::vector<Value>{10, 20, 30});

    CHECK(integerData(dequeue(engine), "value") == 10);
    CHECK(integerData(enqueue(engine, 40), "nodeId") == 4);
    CHECK(integerData(dequeue(engine), "value") == 20);
    CHECK(valuesFromFront(engine) == std::vector<Value>{30, 40});

    const Snapshot snapshot = engine.snapshot(StructureKind::Queue);
    CHECK(snapshot.front == 3);
    CHECK(snapshot.rear == 4);
}

TEST_CASE("queue: a single node is both front and rear") {
    Engine engine;
    const Response added = enqueue(engine, 5);
    CHECK(events(added) ==
          std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Link, Event::Done});
    CHECK(added.frames[2].stepId == "queue.enqueue.linkFront");
    CHECK(added.snapshot.front == 1);
    CHECK(added.snapshot.rear == 1);
    CHECK(added.counters.linksChanged == 2);

    // Removing the only node empties front and rear, each with its own frame.
    const Response removed = dequeue(engine);
    CHECK(events(removed) == std::vector<Event>{Event::Begin, Event::Move, Event::Unlink,
                                                Event::Unlink, Event::Remove, Event::Done});
    CHECK(removed.frames[3].stepId == "queue.dequeue.clearRear");
    CHECK(removed.snapshot.front == kNoNode);
    CHECK(removed.snapshot.rear == kNoNode);
    CHECK(removed.counters.linksChanged == 2);
}

TEST_CASE("queue: enqueue and dequeue on a longer queue follow the documented steps") {
    Engine engine;
    enqueue(engine, 1);
    const Response added = enqueue(engine, 2);
    CHECK(added.frames[2].stepId == "queue.enqueue.linkRear");
    CHECK(added.frames[3].stepId == "queue.enqueue.moveRear");
    CHECK(added.counters.linksChanged == 2);

    // In the create frame the new node exists but nothing points to it yet.
    const Snapshot& created = added.frames[1].snapshot;
    CHECK(created.size == 2);
    CHECK(created.rear == 1);

    const Response removed = dequeue(engine);
    CHECK(events(removed) ==
          std::vector<Event>{Event::Begin, Event::Move, Event::Link, Event::Remove, Event::Done});
    CHECK(removed.counters.nodesVisited == 1);
    CHECK(removed.counters.linksChanged == 1);
    // After "front <- node.next" the old front is detached but still owned.
    CHECK(removed.frames[2].snapshot.size == 2);
    CHECK(removed.frames[2].snapshot.front == 2);
}

TEST_CASE("queue: dequeue and front on an empty queue are rejected and change nothing") {
    Engine engine;
    std::int64_t nextId = 1;

    SUBCASE("never used") {}
    SUBCASE("emptied again") {
        enqueue(engine, 3);
        dequeue(engine);
        nextId = 2;
    }

    test::checkRejected(engine, queueRequest(Operation::Dequeue), ErrorCode::EmptyStructure);
    test::checkRejected(engine, queueRequest(Operation::Front), ErrorCode::EmptyStructure);
    CHECK(integerData(enqueue(engine, 9), "nodeId") == nextId);  // no ID was used up
}

TEST_CASE("queue: front reads without changing anything") {
    Engine engine;
    enqueue(engine, 7);
    enqueue(engine, 8);
    const Snapshot before = engine.snapshot(StructureKind::Queue);
    const int undoBefore = engine.undoCount(StructureKind::Queue);

    const Response response = front(engine);
    CHECK(integerData(response, "value") == 7);
    CHECK(integerData(response, "nodeId") == 1);
    CHECK(response.snapshot == before);
    CHECK(engine.undoCount(StructureKind::Queue) == undoBefore);
}

TEST_CASE("queue: negative and repeated values are allowed") {
    Engine engine;
    enqueue(engine, -5);
    enqueue(engine, -5);
    enqueue(engine, std::numeric_limits<Value>::min());
    CHECK(valuesFromFront(engine) == std::vector<Value>{-5, -5, std::numeric_limits<Value>::min()});
    CHECK(integerData(dequeue(engine), "nodeId") == 1);
    CHECK(integerData(dequeue(engine), "nodeId") == 2);
    CHECK(integerData(dequeue(engine), "value") == std::numeric_limits<Value>::min());
}

TEST_CASE("queue: the 201st enqueue is rejected and changes nothing") {
    Engine engine;
    for (int i = 1; i <= 200; ++i) {
        REQUIRE(enqueue(engine, i).status.ok());
    }
    test::checkRejected(engine, queueRequest(Operation::Enqueue, 201), ErrorCode::CapacityExceeded);
    CHECK(integerData(dequeue(engine), "value") == 1);
    CHECK(integerData(enqueue(engine, 201), "nodeId") == 201);
}

TEST_CASE("queue: clear frees every node and resets rear") {
    Engine engine;
    enqueue(engine, 1);
    enqueue(engine, 2);
    const Response cleared = clear(engine);
    CHECK(integerData(cleared, "removedCount") == 2);
    CHECK(events(cleared) ==
          std::vector<Event>{Event::Begin, Event::Remove, Event::Remove, Event::Done});
    CHECK(cleared.snapshot.front == kNoNode);
    CHECK(cleared.snapshot.rear == kNoNode);

    // A queue that was cleared works normally again.
    enqueue(engine, 3);
    CHECK(valuesFromFront(engine) == std::vector<Value>{3});

    const Response again = clear(engine);
    CHECK(integerData(again, "removedCount") == 1);
    CHECK(integerData(clear(engine), "removedCount") == 0);
}

TEST_CASE("queue: 10,000 seeded random operations agree with std::vector") {
    Engine engine;
    std::vector<Value> oracle;  // allowed as a test oracle (CLAUDE.md, rule 1)
    std::mt19937 random(20261022);

    for (int step = 0; step < 10000; ++step) {
        CAPTURE(step);
        const int choice = static_cast<int>(random() % 100);
        if (choice < 50) {
            const Value value = static_cast<Value>(random() % 2001) - 1000;
            const Response response = enqueue(engine, value);
            if (oracle.size() < 200) {
                REQUIRE(response.status.ok());
                oracle.push_back(value);
            } else {
                REQUIRE(response.status.code == ErrorCode::CapacityExceeded);
            }
        } else if (choice < 85) {
            const Response response = dequeue(engine);
            if (oracle.empty()) {
                REQUIRE(response.status.code == ErrorCode::EmptyStructure);
            } else {
                REQUIRE(integerData(response, "value") == oracle.front());
                oracle.erase(oracle.begin());
            }
        } else if (choice < 99) {
            const Response response = front(engine);
            if (oracle.empty()) {
                REQUIRE(response.status.code == ErrorCode::EmptyStructure);
            } else {
                REQUIRE(integerData(response, "value") == oracle.front());
            }
        } else {
            REQUIRE(integerData(clear(engine), "removedCount") == static_cast<std::int64_t>(oracle.size()));
            oracle.clear();
        }
        REQUIRE(valuesFromFront(engine) == oracle);
    }
}
