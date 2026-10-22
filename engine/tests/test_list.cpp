// Singly linked list behaviour through the Engine; the shared run() checks the
// invariants and the frame contract after every operation.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "dslab/Engine.h"
#include "support/Run.h"

using namespace dslab;
using test::booleanData;
using test::events;
using test::integerData;
using test::isNullData;

namespace {

Request listRequest(Operation operation) {
    Request request;
    request.structure = StructureKind::List;
    request.operation = operation;
    return request;
}

Response insertAt(Engine& engine, int index, Value value) {
    Request request = listRequest(Operation::InsertAt);
    request.index = index;
    request.value = value;
    return test::run(engine, request);
}

Response deleteNode(Engine& engine, NodeId id) {
    Request request = listRequest(Operation::DeleteNode);
    request.nodeId = id;
    return test::run(engine, request);
}

Response search(Engine& engine, Value value) {
    Request request = listRequest(Operation::Search);
    request.value = value;
    return test::run(engine, request);
}

Response reverse(Engine& engine) { return test::run(engine, listRequest(Operation::Reverse)); }
Response clear(Engine& engine) { return test::run(engine, listRequest(Operation::Clear)); }

std::vector<Value> values(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(StructureKind::List);
    return test::chainValues(snapshot, snapshot.head);
}

// Builds the list 10 -> 20 -> 30 (IDs 1, 2, 3) by appending.
void buildTenTwentyThirty(Engine& engine) {
    insertAt(engine, 0, 10);
    insertAt(engine, 1, 20);
    insertAt(engine, 2, 30);
    REQUIRE(values(engine) == std::vector<Value>{10, 20, 30});
}

}  // namespace

TEST_CASE("list: insert at the front, in the middle and at the end") {
    Engine engine;
    const Response first = insertAt(engine, 0, 20);  // into an empty list
    CHECK(events(first) ==
          std::vector<Event>{Event::Begin, Event::Create, Event::Unlink, Event::Link, Event::Done});
    CHECK(first.snapshot.head == 1);

    insertAt(engine, 0, 10);  // new head
    insertAt(engine, 2, 40);  // append: index = size
    const Response middle = insertAt(engine, 2, 30);
    CHECK(values(engine) == std::vector<Value>{10, 20, 30, 40});

    // index 2: start at the head (index 0), walk once (index 1), then link in.
    CHECK(events(middle) == std::vector<Event>{Event::Begin, Event::Move, Event::Move, Event::Create,
                                               Event::Link, Event::Link, Event::Done});
    CHECK(middle.frames[1].stepId == "list.insertAt.start");
    CHECK(middle.frames[2].stepId == "list.insertAt.walk");
    CHECK(middle.frames[5].stepId == "list.insertAt.linkPrev");
    CHECK(middle.counters.nodesVisited == 2);
    CHECK(middle.counters.linksChanged == 2);
    // While the new node is being linked in, it exists but is not reachable yet.
    CHECK(middle.frames[4].snapshot.size == 4);
}

TEST_CASE("list: invalid indices are rejected and change nothing") {
    Engine engine;
    SUBCASE("on an empty list") {
        Request request = listRequest(Operation::InsertAt);
        request.index = 1;
        test::checkRejected(engine, request, ErrorCode::IndexOutOfRange);
        CHECK(test::run(engine, request).status.message == "Index 1 is outside 0 to 0 for an empty list.");
    }
    SUBCASE("on a list of three") {
        buildTenTwentyThirty(engine);
        for (const int index : {-1, 4, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            CAPTURE(index);
            Request request = listRequest(Operation::InsertAt);
            request.index = index;
            request.value = 99;
            test::checkRejected(engine, request, ErrorCode::IndexOutOfRange);
        }
        CHECK(integerData(insertAt(engine, 3, 40), "nodeId") == 4);  // no ID was used up
    }
}

TEST_CASE("list: delete the head, a middle node, the tail and the only node") {
    Engine engine;
    buildTenTwentyThirty(engine);

    const Response middle = deleteNode(engine, 2);
    CHECK(integerData(middle, "value") == 20);
    CHECK(events(middle) == std::vector<Event>{Event::Begin, Event::Move, Event::Found, Event::Link,
                                               Event::Remove, Event::Done});
    CHECK(values(engine) == std::vector<Value>{10, 30});

    const Response tail = deleteNode(engine, 3);
    CHECK(tail.frames[3].event == Event::Unlink);  // prev.next becomes NIL
    CHECK(values(engine) == std::vector<Value>{10});

    const Response only = deleteNode(engine, 1);  // the head and the only node
    CHECK(only.frames[1].stepId == "list.deleteNode.atHead");
    CHECK(only.snapshot.head == kNoNode);
    CHECK(values(engine).empty());

    buildTenTwentyThirty(engine);  // IDs 4, 5, 6
    const Response head = deleteNode(engine, 4);
    CHECK(head.frames[2].stepId == "list.deleteNode.moveHead");
    CHECK(values(engine) == std::vector<Value>{20, 30});
}

TEST_CASE("list: deleting an unknown or already-freed ID is rejected and changes nothing") {
    Engine engine;
    Request request = listRequest(Operation::DeleteNode);
    request.nodeId = 1;
    test::checkRejected(engine, request, ErrorCode::NodeNotFound);  // empty list

    buildTenTwentyThirty(engine);
    deleteNode(engine, 2);
    for (const NodeId id : {2, 0, -5, 99}) {  // 2 existed but was freed
        CAPTURE(id);
        request.nodeId = id;
        test::checkRejected(engine, request, ErrorCode::NodeNotFound);
    }
}

TEST_CASE("list: duplicates keep their own IDs, and deleting by ID removes the selected one") {
    Engine engine;
    insertAt(engine, 0, 7);
    insertAt(engine, 1, 7);
    insertAt(engine, 2, 7);
    deleteNode(engine, 2);  // the middle 7

    const Snapshot snapshot = engine.snapshot(StructureKind::List);
    REQUIRE(snapshot.nodes.size() == 2);
    CHECK(snapshot.nodes[0].id == 1);
    CHECK(snapshot.nodes[1].id == 3);
    CHECK(snapshot.nodes[0].next == 3);
}

TEST_CASE("list: search finds the first match, or reports an absent value") {
    Engine engine;
    const Response empty = search(engine, 5);
    CHECK_FALSE(booleanData(empty, "found"));
    CHECK(isNullData(empty, "nodeId"));
    CHECK(isNullData(empty, "index"));

    buildTenTwentyThirty(engine);
    insertAt(engine, 3, 20);  // a second 20 at index 3

    const Response found = search(engine, 20);
    CHECK(booleanData(found, "found"));
    CHECK(integerData(found, "nodeId") == 2);  // the first 20 from head
    CHECK(integerData(found, "index") == 1);
    CHECK(events(found) == std::vector<Event>{Event::Begin, Event::Compare, Event::Found, Event::Done});
    CHECK(found.counters.comparisons == 2);

    const Response absent = search(engine, -1);
    CHECK_FALSE(booleanData(absent, "found"));
    CHECK(absent.frames[absent.frames.size() - 2].event == Event::NotFound);
    CHECK(absent.counters.comparisons == 4);
}

TEST_CASE("list: reverse an empty, a single-node and a longer list") {
    Engine engine;
    CHECK(events(reverse(engine)) == std::vector<Event>{Event::Begin, Event::Unlink, Event::Done});

    insertAt(engine, 0, -1);
    reverse(engine);
    CHECK(values(engine) == std::vector<Value>{-1});

    insertAt(engine, 1, -2);
    insertAt(engine, 2, -3);
    const Response reversed = reverse(engine);
    CHECK(values(engine) == std::vector<Value>{-3, -2, -1});
    CHECK(reversed.counters.linksChanged == 4);  // three next links and head
    // In the middle the old head is not reachable from the new links yet: the
    // frame after the second flip shows 2 -> 1 while head still points at 1.
    const Snapshot& middle = reversed.frames[2].snapshot;
    CHECK(middle.head == 1);
    CHECK(middle.nodes[1].next == 1);
}

TEST_CASE("list: the 201st node is rejected and clear frees everything") {
    Engine engine;
    for (int i = 0; i < 200; ++i) {
        REQUIRE(insertAt(engine, 0, i).status.ok());
    }
    Request request = listRequest(Operation::InsertAt);
    request.index = 0;
    test::checkRejected(engine, request, ErrorCode::CapacityExceeded);

    const Response cleared = clear(engine);
    CHECK(integerData(cleared, "removedCount") == 200);
    CHECK(cleared.snapshot.head == kNoNode);
    CHECK(integerData(clear(engine), "removedCount") == 0);
}

TEST_CASE("list: 10,000 seeded random operations agree with std::vector") {
    Engine engine;
    std::vector<std::pair<NodeId, Value>> oracle;  // (id, value) from head; a test oracle
    std::mt19937 random(20261022);

    for (int step = 0; step < 10000; ++step) {
        CAPTURE(step);
        const int choice = static_cast<int>(random() % 100);
        const int size = static_cast<int>(oracle.size());
        if (choice < 40 && size < 40) {
            const int index = static_cast<int>(random() % static_cast<unsigned>(size + 1));
            const Value value = static_cast<Value>(random() % 21) - 10;  // small range: many duplicates
            const Response response = insertAt(engine, index, value);
            oracle.insert(oracle.begin() + index, {static_cast<NodeId>(integerData(response, "nodeId")), value});
        } else if (choice < 65 && size > 0) {
            const int position = static_cast<int>(random() % static_cast<unsigned>(size));
            const Response response = deleteNode(engine, oracle[position].first);
            REQUIRE(integerData(response, "value") == oracle[position].second);
            oracle.erase(oracle.begin() + position);
        } else if (choice < 90) {
            const Value value = static_cast<Value>(random() % 21) - 10;
            const Response response = search(engine, value);
            const auto match = std::find_if(oracle.begin(), oracle.end(),
                                            [value](const auto& item) { return item.second == value; });
            REQUIRE(booleanData(response, "found") == (match != oracle.end()));
            if (match != oracle.end()) {
                REQUIRE(integerData(response, "nodeId") == match->first);
                REQUIRE(integerData(response, "index") == match - oracle.begin());
            }
        } else if (choice < 99) {
            reverse(engine);
            std::reverse(oracle.begin(), oracle.end());
        } else {
            clear(engine);
            oracle.clear();
        }

        std::vector<Value> expected;
        for (const auto& item : oracle) {
            expected.push_back(item.second);
        }
        REQUIRE(values(engine) == expected);
    }
}
