// The deque, doubly linked list, circular list, heaps, splay tree and hash
// table, through the Engine. The shared run() checks each structure's
// invariants (support/Checkers.cpp, independent of the structure classes), the
// frame contract and the catalog step IDs after every single operation.
// Seeded random sequences are compared with standard containers, which are
// allowed as test oracles (CLAUDE.md, rule 1).
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "dslab/Engine.h"
#include "dslab/HashTable.h"
#include "dslab/Limits.h"
#include "dslab/json/Protocol.h"
#include "support/Checkers.h"
#include "support/Run.h"

using namespace dslab;
using test::booleanData;
using test::checkRejected;
using test::events;
using test::integerData;
using test::isNullData;
using Json = nlohmann::ordered_json;

namespace {

Request make(StructureKind structure, Operation operation, Value value = 0) {
    Request r;
    r.structure = structure;
    r.operation = operation;
    r.value = value;
    return r;
}

Response run(Engine& engine, StructureKind structure, Operation operation, Value value = 0) {
    return test::run(engine, make(structure, operation, value));
}

const NodeView& nodeOf(const Snapshot& snapshot, NodeId id) {
    for (const NodeView& node : snapshot.nodes) {
        if (node.id == id) return node;
    }
    FAIL("no node " << id);
    return snapshot.nodes.front();
}

bool hasStep(const Response& response, const std::string& stepId) {
    return std::any_of(response.frames.begin(), response.frames.end(),
                       [&](const Frame& frame) { return frame.stepId == stepId; });
}

int countEvent(const Response& response, Event event) {
    const std::vector<Event> all = events(response);
    return static_cast<int>(std::count(all.begin(), all.end(), event));
}

// Fills 'structure' to the 200-node limit with 'operation' and distinct values.
void fill(Engine& engine, StructureKind structure, Operation operation) {
    for (int i = 0; i < kMaxNodes; ++i) {
        Request r = make(structure, operation, i);
        r.index = 0;
        REQUIRE(engine.apply(r).status.ok());
    }
    REQUIRE(test::checkSnapshot(engine.snapshot(structure)) == "");
}

// In-order values and the depth of every value, read from a tree snapshot.
void walkTree(const Snapshot& snapshot, NodeId node, int depth, std::vector<Value>& inorder,
              std::map<Value, int>& depthOf) {
    if (node == kNoNode) return;
    const NodeView& view = nodeOf(snapshot, node);
    walkTree(snapshot, view.left, depth + 1, inorder, depthOf);
    inorder.push_back(view.value);
    depthOf[view.value] = depth;
    walkTree(snapshot, view.right, depth + 1, inorder, depthOf);
}

}  // namespace

// ---------------------------------------------------------------------------
// Deque

namespace {
const StructureKind kDeque = StructureKind::Deque;

std::vector<Value> dequeValues(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(kDeque);
    return test::chainValues(snapshot, snapshot.front);
}
}  // namespace

TEST_CASE("deque: values go in and come out at both ends") {
    Engine engine;
    CHECK(integerData(run(engine, kDeque, Operation::PushBack, 20), "nodeId") == 1);
    run(engine, kDeque, Operation::PushBack, 30);
    run(engine, kDeque, Operation::PushFront, 10);
    CHECK(dequeValues(engine) == std::vector<Value>{10, 20, 30});

    Snapshot snapshot = engine.snapshot(kDeque);
    CHECK(snapshot.front == 3);
    CHECK(snapshot.rear == 2);
    CHECK(nodeOf(snapshot, 1).prev == 3);  // 20's prev is 10
    CHECK(nodeOf(snapshot, 1).next == 2);

    CHECK(integerData(run(engine, kDeque, Operation::PeekFront), "value") == 10);
    CHECK(integerData(run(engine, kDeque, Operation::PeekBack), "value") == 30);
    CHECK(integerData(run(engine, kDeque, Operation::PopBack), "value") == 30);
    CHECK(integerData(run(engine, kDeque, Operation::PopFront), "value") == 10);
    CHECK(dequeValues(engine) == std::vector<Value>{20});

    // The last node: both ends become NIL, whichever end it leaves from.
    const Response last = run(engine, kDeque, Operation::PopBack);
    CHECK(hasStep(last, "deque.popBack.clearOther"));
    snapshot = engine.snapshot(kDeque);
    CHECK(snapshot.front == kNoNode);
    CHECK(snapshot.rear == kNoNode);
}

TEST_CASE("deque: the frames of a push and a pop") {
    Engine engine;
    const Response first = run(engine, kDeque, Operation::PushFront, 5);
    CHECK(events(first) == std::vector<Event>{Event::Begin, Event::Create, Event::Link, Event::Link, Event::Done});
    CHECK(first.frames[2].stepId == "deque.pushFront.linkEmpty");
    const Response second = run(engine, kDeque, Operation::PushBack, 6);
    CHECK(second.frames[2].stepId == "deque.pushBack.link");
    CHECK(second.frames[2].highlight.edges.size() == 2);  // next and prev, one frame
    CHECK(second.counters.linksChanged == 3);
    const Response pop = run(engine, kDeque, Operation::PopFront);
    CHECK(events(pop) == std::vector<Event>{Event::Begin, Event::Move, Event::Link, Event::Unlink, Event::Remove, Event::Done});
    CHECK(run(engine, kDeque, Operation::PeekBack).counters.linksChanged == 0);
}

TEST_CASE("deque: rejections change nothing; clear frees every node") {
    Engine engine;
    for (const Operation operation : {Operation::PopFront, Operation::PopBack, Operation::PeekFront, Operation::PeekBack}) {
        checkRejected(engine, make(kDeque, operation), ErrorCode::EmptyStructure);
    }
    CHECK(integerData(run(engine, kDeque, Operation::PushBack, 1), "nodeId") == 1);  // no ID was used up
    run(engine, kDeque, Operation::PushBack, 1);  // duplicates are allowed
    const Response cleared = run(engine, kDeque, Operation::Clear);
    CHECK(integerData(cleared, "removedCount") == 2);
    CHECK(countEvent(cleared, Event::Remove) == 2);
    CHECK(integerData(run(engine, kDeque, Operation::Clear), "removedCount") == 0);

    fill(engine, kDeque, Operation::PushFront);
    checkRejected(engine, make(kDeque, Operation::PushFront, 1), ErrorCode::CapacityExceeded);
    checkRejected(engine, make(kDeque, Operation::PushBack, 1), ErrorCode::CapacityExceeded);
}

TEST_CASE("deque: 6,000 seeded random operations agree with std::deque") {
    std::mt19937 random(9101);
    Engine engine;
    std::deque<Value> oracle;
    for (int step = 0; step < 6000; ++step) {
        const unsigned kind = random() % 12;
        const Value value = static_cast<Value>(random() % 100) - 50;
        if (kind < 3 && oracle.size() < static_cast<std::size_t>(kMaxNodes)) {
            REQUIRE(run(engine, kDeque, Operation::PushFront, value).status.ok());
            oracle.push_front(value);
        } else if (kind < 6 && oracle.size() < static_cast<std::size_t>(kMaxNodes)) {
            REQUIRE(run(engine, kDeque, Operation::PushBack, value).status.ok());
            oracle.push_back(value);
        } else if (kind < 10) {
            const bool front = kind % 2 == 0;
            const bool peek = kind >= 8;
            const Operation operation = peek ? (front ? Operation::PeekFront : Operation::PeekBack)
                                             : (front ? Operation::PopFront : Operation::PopBack);
            const Response response = run(engine, kDeque, operation);
            if (oracle.empty()) {
                CHECK(response.status.code == ErrorCode::EmptyStructure);
            } else {
                CHECK(integerData(response, "value") == (front ? oracle.front() : oracle.back()));
                if (!peek) front ? oracle.pop_front() : oracle.pop_back();
            }
        } else if (kind == 10 && random() % 20 == 0) {
            run(engine, kDeque, Operation::Clear);
            oracle.clear();
        } else if (engine.undoCount(kDeque) > 0 && random() % 2 == 0) {
            // Undo then redo must come back to exactly the same state.
            const Snapshot now = engine.snapshot(kDeque);
            run(engine, kDeque, Operation::Undo);
            run(engine, kDeque, Operation::Redo);
            CHECK(engine.snapshot(kDeque) == now);
        }
        REQUIRE(dequeValues(engine) == std::vector<Value>(oracle.begin(), oracle.end()));
    }
}

// ---------------------------------------------------------------------------
// Doubly linked list

namespace {
const StructureKind kDoubly = StructureKind::DoublyList;

Request insertAt(StructureKind structure, int index, Value value) {
    Request r = make(structure, Operation::InsertAt, value);
    r.index = index;
    return r;
}

Request deleteNode(StructureKind structure, NodeId id) {
    Request r = make(structure, Operation::DeleteNode);
    r.nodeId = id;
    return r;
}

std::vector<NodeId> chainIds(const Snapshot& snapshot, NodeId entry) {
    std::vector<NodeId> ids;
    for (NodeId id = entry; id != kNoNode && ids.size() <= snapshot.nodes.size(); id = nodeOf(snapshot, id).next) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<Value> doublyValues(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(kDoubly);
    return test::chainValues(snapshot, snapshot.head);
}
}  // namespace

TEST_CASE("doublyList: insert at the head, the tail and the middle") {
    Engine engine;
    const Response only = test::run(engine, insertAt(kDoubly, 0, 20));
    CHECK(hasStep(only, "doublyList.insertAt.linkOnly"));
    CHECK(hasStep(test::run(engine, insertAt(kDoubly, 0, 10)), "doublyList.insertAt.moveHead"));
    CHECK(hasStep(test::run(engine, insertAt(kDoubly, 2, 40)), "doublyList.insertAt.moveTail"));
    const Response middle = test::run(engine, insertAt(kDoubly, 2, 30));
    CHECK(hasStep(middle, "doublyList.insertAt.linkNeighbours"));
    CHECK(doublyValues(engine) == std::vector<Value>{10, 20, 30, 40});

    const Snapshot snapshot = engine.snapshot(kDoubly);
    CHECK(snapshot.head == 2);
    CHECK(snapshot.tail == 3);
    CHECK(nodeOf(snapshot, 4).prev == 1);
    CHECK(nodeOf(snapshot, 4).next == 3);
    CHECK(nodeOf(snapshot, 3).prev == 4);
}

TEST_CASE("doublyList: a middle insert walks from the nearer end") {
    Engine engine;
    for (int i = 0; i < 10; ++i) test::run(engine, insertAt(kDoubly, i, i * 10));
    const Response nearHead = test::run(engine, insertAt(kDoubly, 2, 15));
    CHECK(hasStep(nearHead, "doublyList.insertAt.startHead"));
    CHECK(nearHead.counters.nodesVisited == 3);  // head, then two steps
    const Response nearTail = test::run(engine, insertAt(kDoubly, 9, 85));
    CHECK(hasStep(nearTail, "doublyList.insertAt.startTail"));
    CHECK(hasStep(nearTail, "doublyList.insertAt.walkPrev"));
    CHECK(nearTail.counters.nodesVisited == 2);  // tail, then one step back
    CHECK(doublyValues(engine) == std::vector<Value>{0, 10, 15, 20, 30, 40, 50, 60, 70, 85, 80, 90});
}

TEST_CASE("doublyList: deleting a selected node needs no walk") {
    Engine engine;
    for (int i = 0; i < 6; ++i) test::run(engine, insertAt(kDoubly, i, i));  // IDs 1..6 hold 0..5
    const Response middle = test::run(engine, deleteNode(kDoubly, 4));
    CHECK(integerData(middle, "value") == 3);
    CHECK(middle.counters.nodesVisited == 1);
    CHECK(countEvent(middle, Event::Move) == 0);
    CHECK(hasStep(test::run(engine, deleteNode(kDoubly, 1)), "doublyList.deleteNode.moveHead"));
    CHECK(hasStep(test::run(engine, deleteNode(kDoubly, 6)), "doublyList.deleteNode.moveTail"));
    CHECK(doublyValues(engine) == std::vector<Value>{1, 2, 4});
    test::run(engine, deleteNode(kDoubly, 2));
    test::run(engine, deleteNode(kDoubly, 5));
    const Response last = test::run(engine, deleteNode(kDoubly, 3));
    CHECK(hasStep(last, "doublyList.deleteNode.moveHead"));
    CHECK(hasStep(last, "doublyList.deleteNode.moveTail"));
    CHECK(engine.snapshot(kDoubly).size == 0);
}

TEST_CASE("doublyList: search, reverse and clear") {
    Engine engine;
    for (const Value value : {7, 8, 7, 9}) test::run(engine, insertAt(kDoubly, engine.snapshot(kDoubly).size, value));
    const Response found = run(engine, kDoubly, Operation::Search, 7);
    CHECK(integerData(found, "index") == 0);  // the first match
    CHECK(integerData(run(engine, kDoubly, Operation::Search, 9), "index") == 3);
    const Response missing = run(engine, kDoubly, Operation::Search, 1);
    CHECK_FALSE(booleanData(missing, "found"));
    CHECK(isNullData(missing, "nodeId"));
    CHECK(missing.counters.comparisons == 4);

    const Response reversed = run(engine, kDoubly, Operation::Reverse);
    CHECK(countEvent(reversed, Event::Link) == 5);  // four flips and the head/tail swap
    CHECK(doublyValues(engine) == std::vector<Value>{9, 7, 8, 7});
    CHECK(engine.snapshot(kDoubly).head == 4);
    CHECK(engine.snapshot(kDoubly).tail == 1);

    CHECK(integerData(run(engine, kDoubly, Operation::Clear), "removedCount") == 4);
    CHECK(run(engine, kDoubly, Operation::Reverse).status.ok());  // an empty list reverses to itself
}

TEST_CASE("doublyList: rejections change nothing") {
    Engine engine;
    checkRejected(engine, insertAt(kDoubly, 1, 5), ErrorCode::IndexOutOfRange);
    checkRejected(engine, insertAt(kDoubly, -1, 5), ErrorCode::IndexOutOfRange);
    checkRejected(engine, deleteNode(kDoubly, 1), ErrorCode::NodeNotFound);
    CHECK(integerData(test::run(engine, insertAt(kDoubly, 0, 5)), "nodeId") == 1);
    checkRejected(engine, insertAt(kDoubly, 2, 5), ErrorCode::IndexOutOfRange);
    checkRejected(engine, deleteNode(kDoubly, 2), ErrorCode::NodeNotFound);
}

TEST_CASE("doublyList: a full list rejects another insert") {
    Engine engine;
    fill(engine, kDoubly, Operation::InsertAt);
    checkRejected(engine, insertAt(kDoubly, 0, 1), ErrorCode::CapacityExceeded);
    checkRejected(engine, insertAt(kDoubly, 999, 1), ErrorCode::CapacityExceeded);  // capacity is checked first
}

TEST_CASE("doublyList: 5,000 seeded random operations agree with std::vector") {
    std::mt19937 random(9102);
    Engine engine;
    std::vector<Value> oracle;
    for (int step = 0; step < 5000; ++step) {
        const unsigned kind = random() % 12;
        const Value value = static_cast<Value>(random() % 30);
        const int size = static_cast<int>(oracle.size());
        if (kind < 5 && size < kMaxNodes) {
            const int index = static_cast<int>(random() % static_cast<unsigned>(size + 1));
            REQUIRE(test::run(engine, insertAt(kDoubly, index, value)).status.ok());
            oracle.insert(oracle.begin() + index, value);
        } else if (kind < 8 && size > 0) {
            const int index = static_cast<int>(random() % static_cast<unsigned>(size));
            const Snapshot snapshot = engine.snapshot(kDoubly);
            const NodeId id = chainIds(snapshot, snapshot.head)[static_cast<std::size_t>(index)];
            CHECK(integerData(test::run(engine, deleteNode(kDoubly, id)), "value") == oracle[static_cast<std::size_t>(index)]);
            oracle.erase(oracle.begin() + index);
        } else if (kind < 10) {
            const Response response = run(engine, kDoubly, Operation::Search, value);
            const auto at = std::find(oracle.begin(), oracle.end(), value);
            CHECK(booleanData(response, "found") == (at != oracle.end()));
            if (at != oracle.end()) CHECK(integerData(response, "index") == at - oracle.begin());
        } else if (kind == 10) {
            run(engine, kDoubly, Operation::Reverse);
            std::reverse(oracle.begin(), oracle.end());
        } else if (random() % 25 == 0) {
            run(engine, kDoubly, Operation::Clear);
            oracle.clear();
        }
        REQUIRE(doublyValues(engine) == oracle);
    }
}

// ---------------------------------------------------------------------------
// Circular list

namespace {
const StructureKind kCircular = StructureKind::CircularList;

// Values from the first node (tail.next) once round the circle.
std::vector<Value> circleValues(Engine& engine, std::vector<NodeId>* ids = nullptr) {
    const Snapshot snapshot = engine.snapshot(kCircular);
    std::vector<Value> values;
    if (snapshot.tail == kNoNode) return values;
    NodeId current = nodeOf(snapshot, snapshot.tail).next;
    for (int i = 0; i < snapshot.size; ++i) {
        values.push_back(nodeOf(snapshot, current).value);
        if (ids != nullptr) ids->push_back(current);
        current = nodeOf(snapshot, current).next;
    }
    return values;
}
}  // namespace

TEST_CASE("circularList: the last node always links back to the first") {
    Engine engine;
    const Response first = run(engine, kCircular, Operation::InsertBack, 10);
    CHECK(hasStep(first, "circularList.insertBack.linkSelf"));
    Snapshot snapshot = engine.snapshot(kCircular);
    CHECK(snapshot.tail == 1);
    CHECK(nodeOf(snapshot, 1).next == 1);  // a circle of one

    run(engine, kCircular, Operation::InsertBack, 20);
    const Response front = run(engine, kCircular, Operation::InsertFront, 5);
    CHECK_FALSE(hasStep(front, "circularList.insertFront.moveTail"));  // only insertBack moves tail
    CHECK(circleValues(engine) == std::vector<Value>{5, 10, 20});
    snapshot = engine.snapshot(kCircular);
    CHECK(snapshot.tail == 2);
    CHECK(nodeOf(snapshot, 2).next == 3);  // 20 -> 5: back to the first
    for (const NodeView& node : snapshot.nodes) CHECK(node.next != kNoNode);
}

TEST_CASE("circularList: rotate moves only tail") {
    Engine engine;
    for (const Value value : {1, 2, 3}) run(engine, kCircular, Operation::InsertBack, value);
    const Snapshot before = engine.snapshot(kCircular);
    const Response rotated = run(engine, kCircular, Operation::Rotate);
    CHECK(rotated.counters.linksChanged == 1);
    CHECK(circleValues(engine) == std::vector<Value>{2, 3, 1});
    const Snapshot after = engine.snapshot(kCircular);
    CHECK(after.nodes == before.nodes);  // no link between nodes changed
    CHECK(after.tail == 1);
    run(engine, kCircular, Operation::Rotate);
    run(engine, kCircular, Operation::Rotate);
    CHECK(engine.snapshot(kCircular).tail == before.tail);  // n rotations: back where it started

    Engine empty;
    CHECK(run(empty, kCircular, Operation::Rotate).status.ok());  // nothing to rotate is not an error
    CHECK(empty.undoCount(kCircular) == 1);
}

TEST_CASE("circularList: delete the first, a middle node, the tail and the only node") {
    Engine engine;
    for (const Value value : {1, 2, 3, 4}) run(engine, kCircular, Operation::InsertBack, value);
    CHECK(integerData(test::run(engine, deleteNode(kCircular, 2)), "value") == 2);
    const Response tail = test::run(engine, deleteNode(kCircular, 4));
    CHECK(hasStep(tail, "circularList.deleteNode.moveTail"));
    CHECK(engine.snapshot(kCircular).tail == 3);
    const Response first = test::run(engine, deleteNode(kCircular, 1));
    CHECK(first.counters.nodesVisited == 1);  // the node before the first is tail itself
    CHECK(circleValues(engine) == std::vector<Value>{3});
    const Response only = test::run(engine, deleteNode(kCircular, 3));
    CHECK(hasStep(only, "circularList.deleteNode.onlyNode"));
    CHECK(engine.snapshot(kCircular).tail == kNoNode);
}

TEST_CASE("circularList: search stops after one full circle; rejections change nothing") {
    Engine engine;
    const Response empty = run(engine, kCircular, Operation::Search, 5);
    CHECK_FALSE(booleanData(empty, "found"));
    checkRejected(engine, deleteNode(kCircular, 1), ErrorCode::NodeNotFound);
    for (const Value value : {4, 5, 6, 5}) run(engine, kCircular, Operation::InsertBack, value);
    CHECK(integerData(run(engine, kCircular, Operation::Search, 5), "index") == 1);
    const Response missing = run(engine, kCircular, Operation::Search, 9);
    CHECK_FALSE(booleanData(missing, "found"));
    CHECK(missing.counters.comparisons == 4);  // every node once, then it stops
    checkRejected(engine, deleteNode(kCircular, 99), ErrorCode::NodeNotFound);
    CHECK(integerData(run(engine, kCircular, Operation::Clear), "removedCount") == 4);

    fill(engine, kCircular, Operation::InsertFront);
    checkRejected(engine, make(kCircular, Operation::InsertFront, 1), ErrorCode::CapacityExceeded);
    checkRejected(engine, make(kCircular, Operation::InsertBack, 1), ErrorCode::CapacityExceeded);
}

TEST_CASE("circularList: 5,000 seeded random operations agree with std::deque") {
    std::mt19937 random(9103);
    Engine engine;
    std::deque<Value> oracle;
    for (int step = 0; step < 5000; ++step) {
        const unsigned kind = random() % 12;
        const Value value = static_cast<Value>(random() % 30);
        const int size = static_cast<int>(oracle.size());
        if (kind < 3 && size < kMaxNodes) {
            run(engine, kCircular, Operation::InsertFront, value);
            oracle.push_front(value);
        } else if (kind < 6 && size < kMaxNodes) {
            run(engine, kCircular, Operation::InsertBack, value);
            oracle.push_back(value);
        } else if (kind < 8 && size > 0) {
            std::vector<NodeId> ids;
            circleValues(engine, &ids);
            const std::size_t index = random() % ids.size();
            CHECK(integerData(test::run(engine, deleteNode(kCircular, ids[index])), "value") == oracle[index]);
            oracle.erase(oracle.begin() + static_cast<std::ptrdiff_t>(index));
        } else if (kind < 10) {
            const Response response = run(engine, kCircular, Operation::Search, value);
            const auto at = std::find(oracle.begin(), oracle.end(), value);
            CHECK(booleanData(response, "found") == (at != oracle.end()));
            if (at != oracle.end()) CHECK(integerData(response, "index") == at - oracle.begin());
        } else if (kind == 10) {
            run(engine, kCircular, Operation::Rotate);
            if (!oracle.empty()) {
                oracle.push_back(oracle.front());
                oracle.pop_front();
            }
        } else if (random() % 25 == 0) {
            run(engine, kCircular, Operation::Clear);
            oracle.clear();
        }
        REQUIRE(circleValues(engine) == std::vector<Value>(oracle.begin(), oracle.end()));
    }
}

// ---------------------------------------------------------------------------
// Heaps

namespace {
// The heap's array, slot 0 first.
std::vector<Value> slots(Engine& engine, StructureKind heap) {
    const Snapshot snapshot = engine.snapshot(heap);
    std::vector<Value> values(snapshot.nodes.size());
    for (const NodeView& node : snapshot.nodes) values[static_cast<std::size_t>(node.index)] = node.value;
    return values;
}
}  // namespace

TEST_CASE("minHeap: insert sifts up, extract returns values in ascending order") {
    Engine engine;
    const StructureKind heap = StructureKind::MinHeap;
    for (const Value value : {40, 30, 20}) run(engine, heap, Operation::Insert, value);
    CHECK(slots(engine, heap) == std::vector<Value>{20, 40, 30});
    const Response sifted = run(engine, heap, Operation::Insert, 10);  // slot 3 -> 1 -> 0
    CHECK(countEvent(sifted, Event::Swap) == 2);
    CHECK(sifted.counters.comparisons == 2);
    CHECK(slots(engine, heap) == std::vector<Value>{10, 20, 30, 40});
    const Response stays = run(engine, heap, Operation::Insert, 50);
    CHECK(countEvent(stays, Event::Swap) == 0);
    CHECK(countEvent(stays, Event::Compare) == 1);

    // A node keeps its ID while its value moves between slots (rule 3).
    const Snapshot snapshot = engine.snapshot(heap);
    CHECK(snapshot.root == 4);
    CHECK(nodeOf(snapshot, 4).value == 10);
    CHECK(nodeOf(snapshot, 4).index == 0);
    CHECK(nodeOf(snapshot, 1).value == 40);

    CHECK(integerData(run(engine, heap, Operation::Peek), "value") == 10);
    std::vector<Value> out;
    while (engine.snapshot(heap).size > 0) out.push_back(static_cast<Value>(integerData(run(engine, heap, Operation::Extract), "value")));
    CHECK(out == std::vector<Value>{10, 20, 30, 40, 50});
}

TEST_CASE("maxHeap: the largest value is at the root; duplicates are allowed") {
    Engine engine;
    const StructureKind heap = StructureKind::MaxHeap;
    for (const Value value : {5, 9, 9, 1, 7}) run(engine, heap, Operation::Insert, value);
    CHECK(integerData(run(engine, heap, Operation::Peek), "value") == 9);
    const Response extracted = run(engine, heap, Operation::Extract);
    CHECK(integerData(extracted, "value") == 9);
    CHECK(extracted.frames[2].stepId == "maxHeap.extract.moveLast");
    CHECK(integerData(run(engine, heap, Operation::Extract), "value") == 9);
    CHECK(integerData(run(engine, heap, Operation::Extract), "value") == 7);
    CHECK(slots(engine, heap) == std::vector<Value>{5, 1});

    // The two heaps are independent structures.
    CHECK(engine.snapshot(StructureKind::MinHeap).size == 0);
}

TEST_CASE("heaps: rejections change nothing; clear removes leaf by leaf") {
    for (const StructureKind heap : {StructureKind::MinHeap, StructureKind::MaxHeap}) {
        Engine engine;
        checkRejected(engine, make(heap, Operation::Extract), ErrorCode::EmptyStructure);
        checkRejected(engine, make(heap, Operation::Peek), ErrorCode::EmptyStructure);
        CHECK(integerData(run(engine, heap, Operation::Insert, 3), "nodeId") == 1);
        const Response single = run(engine, heap, Operation::Extract);  // one node: no swap, no sifting
        CHECK(countEvent(single, Event::Swap) == 0);
        fill(engine, heap, Operation::Insert);
        checkRejected(engine, make(heap, Operation::Insert, 1), ErrorCode::CapacityExceeded);
        const Response cleared = run(engine, heap, Operation::Clear);
        CHECK(integerData(cleared, "removedCount") == kMaxNodes);
        CHECK(countEvent(cleared, Event::Remove) == kMaxNodes);
    }
}

TEST_CASE("heaps: 4,000 seeded random operations each agree with std::priority_queue") {
    for (const StructureKind heap : {StructureKind::MinHeap, StructureKind::MaxHeap}) {
        CAPTURE(test::wireName(heap));
        const bool isMin = heap == StructureKind::MinHeap;
        std::mt19937 random(isMin ? 9104u : 9105u);
        Engine engine;
        // The oracle orders by "comes out last" (std::priority_queue pops the greatest).
        std::priority_queue<Value, std::vector<Value>, std::function<bool(Value, Value)>> oracle(
            [isMin](Value a, Value b) { return isMin ? a > b : a < b; });
        for (int step = 0; step < 4000; ++step) {
            const unsigned kind = random() % 10;
            if (kind < 5 && oracle.size() < static_cast<std::size_t>(kMaxNodes)) {
                const Value value = static_cast<Value>(random() % 60) - 30;
                const Response response = run(engine, heap, Operation::Insert, value);
                REQUIRE(response.status.ok());
                oracle.push(value);
                // O(log n): at most one comparison per level.
                CHECK(response.counters.comparisons <= 8);
            } else if (kind < 9) {
                const Response response = run(engine, heap, kind < 8 ? Operation::Extract : Operation::Peek);
                if (oracle.empty()) {
                    CHECK(response.status.code == ErrorCode::EmptyStructure);
                } else {
                    CHECK(integerData(response, "value") == oracle.top());
                    CHECK(response.counters.comparisons <= 16);  // two per level on the way down
                    if (kind < 8) oracle.pop();
                }
            } else if (random() % 30 == 0) {
                run(engine, heap, Operation::Clear);
                while (!oracle.empty()) oracle.pop();
            }
            REQUIRE(engine.snapshot(heap).size == static_cast<int>(oracle.size()));
        }
    }
}

// ---------------------------------------------------------------------------
// Splay tree

namespace {
const StructureKind kSplay = StructureKind::Splay;

Value rootValue(Engine& engine) {
    const Snapshot snapshot = engine.snapshot(kSplay);
    REQUIRE(snapshot.root != kNoNode);
    return nodeOf(snapshot, snapshot.root).value;
}

std::vector<Value> splayInorder(Engine& engine, std::map<Value, int>* depths = nullptr) {
    const Snapshot snapshot = engine.snapshot(kSplay);
    std::vector<Value> inorder;
    std::map<Value, int> depthOf;
    walkTree(snapshot, snapshot.root, 0, inorder, depthOf);
    if (depths != nullptr) *depths = depthOf;
    return inorder;
}
}  // namespace

TEST_CASE("splay: an inserted value becomes the root") {
    Engine engine;
    for (const Value value : {50, 30, 70, 20, 40}) {
        REQUIRE(run(engine, kSplay, Operation::Insert, value).status.ok());
        CHECK(rootValue(engine) == value);
    }
    CHECK(splayInorder(engine) == std::vector<Value>{20, 30, 40, 50, 70});
}

TEST_CASE("splay: zig, zig-zig and zig-zag each run, on both sides") {
    // Sorted inserts make a left spine: 3 at the root, then 2, then 1.
    Engine engine;
    for (const Value value : {1, 2, 3}) run(engine, kSplay, Operation::Insert, value);
    const Response zigZig = run(engine, kSplay, Operation::Search, 1);  // 1 is a left-left grandchild
    CHECK(hasStep(zigZig, "splay.search.zigZig"));
    CHECK(zigZig.counters.rotations == 2);
    CHECK(rootValue(engine) == 1);
    // Now 1 -> right 2 -> right 3 (a right spine): 2 is the root's right child.
    const Response zig = run(engine, kSplay, Operation::Search, 2);
    CHECK(hasStep(zig, "splay.search.zig"));
    CHECK(zig.counters.rotations == 1);
    CHECK(rootValue(engine) == 2);

    Engine other;
    for (const Value value : {10, 30}) run(other, kSplay, Operation::Insert, value);  // 30 root, 10 its left child
    const Response zigZag = run(other, kSplay, Operation::Insert, 20);  // right child of a left child
    CHECK(hasStep(zigZag, "splay.insert.zigZag"));
    CHECK(rootValue(other) == 20);
    Engine mirror;
    for (const Value value : {30, 10}) run(mirror, kSplay, Operation::Insert, value);  // 10 root, 30 its right child
    CHECK(hasStep(run(mirror, kSplay, Operation::Insert, 20), "splay.insert.zigZag"));
    for (const Value value : {40, 50}) run(mirror, kSplay, Operation::Insert, value);
    const Response right = run(mirror, kSplay, Operation::Search, 10);
    CHECK(rootValue(mirror) == 10);
    CHECK(test::checkSnapshot(mirror.snapshot(kSplay)) == "");
    CHECK(right.counters.rotations >= 2);
}

TEST_CASE("splay: a search changes the tree, so it is undoable; a miss splays the last node") {
    Engine engine;
    for (const Value value : {10, 20, 30, 40}) run(engine, kSplay, Operation::Insert, value);
    const Snapshot before = engine.snapshot(kSplay);
    const int undoBefore = engine.undoCount(kSplay);

    const Response found = run(engine, kSplay, Operation::Search, 10);
    CHECK(booleanData(found, "found"));
    CHECK(rootValue(engine) == 10);
    CHECK(engine.undoCount(kSplay) == undoBefore + 1);
    const Response undone = run(engine, kSplay, Operation::Undo);
    CHECK(undone.frames.front().explanation == "Undo search 10 on the splay tree.");
    CHECK(engine.snapshot(kSplay) == before);

    const Response missing = run(engine, kSplay, Operation::Search, 5);  // would be left of 10
    CHECK_FALSE(booleanData(missing, "found"));
    CHECK(isNullData(missing, "nodeId"));
    CHECK(rootValue(engine) == 10);  // the last node on the path
    CHECK(run(engine, kSplay, Operation::Search, 10).counters.comparisons == 1);  // now one step away

    Engine empty;
    CHECK_FALSE(booleanData(run(empty, kSplay, Operation::Search, 1), "found"));
}

TEST_CASE("splay: delete relinks the successor and splays the parent") {
    Engine engine;
    for (const Value value : {50, 30, 70, 20, 40, 60, 80}) run(engine, kSplay, Operation::Insert, value);
    run(engine, kSplay, Operation::Search, 50);
    const Snapshot before = engine.snapshot(kSplay);
    const NodeView& root = nodeOf(before, before.root);
    REQUIRE(root.left != kNoNode);
    REQUIRE(root.right != kNoNode);
    // Find a non-root node with two children if there is one; otherwise delete the root.
    const Response removed = run(engine, kSplay, Operation::Delete, 50);
    CHECK(integerData(removed, "value") == 50);
    CHECK(hasStep(removed, "splay.delete.caseTwoChildren"));
    CHECK(splayInorder(engine) == std::vector<Value>{20, 30, 40, 60, 70, 80});
    // Every surviving value kept its node ID.
    const Snapshot after = engine.snapshot(kSplay);
    for (const NodeView& node : after.nodes) CHECK(nodeOf(before, node.id).value == node.value);

    // Deleting a deep leaf splays its parent to the root.
    Engine chain;
    for (const Value value : {5, 4, 3, 2, 1}) run(chain, kSplay, Operation::Insert, value);  // 1 root, right spine 2..5
    std::map<Value, int> depths;
    splayInorder(chain, &depths);
    Value deepest = 1;
    for (const auto& [value, depth] : depths) if (depth > depths[deepest]) deepest = value;
    const Snapshot chainBefore = chain.snapshot(kSplay);
    NodeId deepestId = kNoNode;
    for (const NodeView& node : chainBefore.nodes) if (node.value == deepest) deepestId = node.id;
    const Value parentValue = nodeOf(chainBefore, nodeOf(chainBefore, deepestId).parent).value;
    const Response leaf = run(chain, kSplay, Operation::Delete, deepest);
    CHECK(leaf.counters.rotations > 0);
    CHECK(rootValue(chain) == parentValue);
}

TEST_CASE("splay: rejections change nothing, not even the shape") {
    Engine engine;
    checkRejected(engine, make(kSplay, Operation::Delete, 1), ErrorCode::ValueNotFound);
    for (const Value value : {10, 20, 30}) run(engine, kSplay, Operation::Insert, value);
    checkRejected(engine, make(kSplay, Operation::Insert, 10), ErrorCode::DuplicateValue);  // no splay on a rejection
    checkRejected(engine, make(kSplay, Operation::Delete, 15), ErrorCode::ValueNotFound);
    CHECK(integerData(run(engine, kSplay, Operation::Insert, 40), "nodeId") == 4);
    CHECK(integerData(run(engine, kSplay, Operation::Clear), "removedCount") == 4);
    fill(engine, kSplay, Operation::Insert);
    checkRejected(engine, make(kSplay, Operation::Insert, 1000), ErrorCode::CapacityExceeded);
}

TEST_CASE("splay: sorted inserts then searches stay within the frame budget (200 nodes)") {
    Engine engine;
    for (int value = 0; value < kMaxNodes; ++value) REQUIRE(run(engine, kSplay, Operation::Insert, value).status.ok());
    // The tree is now a 200-node left spine: the deepest search is the worst case.
    const Response deepest = run(engine, kSplay, Operation::Search, 0);
    CHECK(booleanData(deepest, "found"));
    CHECK(deepest.counters.rotations == kMaxNodes - 1);
    CHECK(rootValue(engine) == 0);
    for (const Value value : {199, 100, 0, 57}) run(engine, kSplay, Operation::Delete, value);
    for (const Operation order : {Operation::Preorder, Operation::Inorder, Operation::Postorder, Operation::Levelorder}) {
        CHECK(test::listData(run(engine, kSplay, order), "values").size() == 196);
    }
}

TEST_CASE("splay: 6,000 seeded random operations agree with std::set") {
    std::mt19937 random(9106);
    Engine engine;
    std::set<Value> oracle;
    for (int step = 0; step < 6000; ++step) {
        const unsigned kind = random() % 10;
        const Value value = static_cast<Value>(random() % 120);
        if (kind < 4) {
            const Response response = run(engine, kSplay, Operation::Insert, value);
            if (oracle.count(value) != 0) {
                CHECK(response.status.code == ErrorCode::DuplicateValue);
            } else if (oracle.size() == static_cast<std::size_t>(kMaxNodes)) {
                CHECK(response.status.code == ErrorCode::CapacityExceeded);
            } else {
                REQUIRE(response.status.ok());
                oracle.insert(value);
                CHECK(rootValue(engine) == value);
            }
        } else if (kind < 7) {
            const Response response = run(engine, kSplay, Operation::Delete, value);
            CHECK(response.status.ok() == (oracle.erase(value) == 1));
        } else if (kind < 9) {
            const Response response = run(engine, kSplay, Operation::Search, value);
            CHECK(booleanData(response, "found") == (oracle.count(value) == 1));
            if (oracle.count(value) == 1) CHECK(rootValue(engine) == value);
        } else if (engine.undoCount(kSplay) > 0) {
            const Snapshot now = engine.snapshot(kSplay);
            run(engine, kSplay, Operation::Undo);
            run(engine, kSplay, Operation::Redo);
            CHECK(engine.snapshot(kSplay) == now);
        }
        REQUIRE(splayInorder(engine) == std::vector<Value>(oracle.begin(), oracle.end()));
    }
}

// ---------------------------------------------------------------------------
// Hash table

namespace {
const StructureKind kHash = StructureKind::HashTable;

std::vector<Value> bucketValues(Engine& engine, int bucket) {
    const Snapshot snapshot = engine.snapshot(kHash);
    return test::chainValues(snapshot, snapshot.buckets[static_cast<std::size_t>(bucket)]);
}
}  // namespace

TEST_CASE("hashTable: a value goes to bucket value mod 7, at the head of its chain") {
    Engine engine;
    CHECK(engine.snapshot(kHash).buckets == std::vector<NodeId>(kHashBuckets, kNoNode));
    for (const Value value : {1, 8, 15, 9, 13}) REQUIRE(run(engine, kHash, Operation::Insert, value).status.ok());
    CHECK(bucketValues(engine, 1) == std::vector<Value>{15, 8, 1});  // newest first
    CHECK(bucketValues(engine, 2) == std::vector<Value>{9});
    CHECK(bucketValues(engine, 6) == std::vector<Value>{13});
    CHECK(bucketValues(engine, 0).empty());

    // Negative values land in 0..6 too: -1 mod 7 is 6, -7 mod 7 is 0.
    run(engine, kHash, Operation::Insert, -1);
    run(engine, kHash, Operation::Insert, -7);
    run(engine, kHash, Operation::Insert, -2147483647 - 1);
    CHECK(bucketValues(engine, 6) == std::vector<Value>{-1, 13});
    CHECK(bucketValues(engine, 0) == std::vector<Value>{-7});
    CHECK(HashTable::bucketOf(-2147483647 - 1) == 5);  // -2147483648 = 7 * (-306783379) + 5
    CHECK(bucketValues(engine, 5) == std::vector<Value>{-2147483647 - 1});
}

TEST_CASE("hashTable: frames carry the bucket being worked in") {
    Engine engine;
    run(engine, kHash, Operation::Insert, 3);
    const Response insert = run(engine, kHash, Operation::Insert, 10);
    CHECK(events(insert) == std::vector<Event>{Event::Begin, Event::Hash, Event::Compare, Event::Create, Event::Link,
                                               Event::Link, Event::Done});
    CHECK(insert.frames[0].bucket == -1);  // not computed yet
    for (std::size_t i = 1; i < insert.frames.size(); ++i) CHECK(insert.frames[i].bucket == 3);
    CHECK(insert.frames[1].explanation == "bucket = 10 mod 7 = 3; its chain starts with 3.");

    // On the wire: "bucket" is in every hash table frame and in no other structure's.
    const Json wire = Json::parse(handleJson(
        engine, R"({"schemaVersion":1,"requestId":1,"structure":"hashTable","operation":"search","args":{"value":3}})"));
    CHECK(wire["frames"][0]["bucket"].is_null());
    CHECK(wire["frames"][1]["bucket"] == 3);
    CHECK(wire["result"]["data"] == Json{{"found", true}, {"nodeId", 1}, {"bucket", 3}});
    CHECK(wire["snapshot"]["buckets"] == Json::array({nullptr, nullptr, nullptr, 2, nullptr, nullptr, nullptr}));
    const Json other = Json::parse(handleJson(
        engine, R"({"schemaVersion":1,"requestId":1,"structure":"stack","operation":"snapshot","args":{}})"));
    CHECK_FALSE(other["frames"][0].contains("bucket"));
}

TEST_CASE("hashTable: search and delete look at one chain only") {
    Engine engine;
    for (Value value = 0; value < 70; ++value) run(engine, kHash, Operation::Insert, value);  // ten per bucket
    const Response found = run(engine, kHash, Operation::Search, 3);  // the oldest in bucket 3: last in its chain
    CHECK(booleanData(found, "found"));
    CHECK(found.counters.comparisons == 10);
    CHECK(run(engine, kHash, Operation::Search, 66).counters.comparisons == 1);  // the newest: the head
    const Response missing = run(engine, kHash, Operation::Search, 700);
    CHECK_FALSE(booleanData(missing, "found"));
    CHECK(isNullData(missing, "nodeId"));
    CHECK(integerData(missing, "bucket") == 0);
    CHECK(missing.counters.comparisons == 10);  // never the other 60 values

    const Response head = run(engine, kHash, Operation::Delete, 66);
    CHECK(hasStep(head, "hashTable.delete.unlinkHead"));
    const Response middle = run(engine, kHash, Operation::Delete, 31);
    CHECK(hasStep(middle, "hashTable.delete.bypass"));
    const Response last = run(engine, kHash, Operation::Delete, 3);
    CHECK(hasStep(last, "hashTable.delete.bypass"));
    CHECK(bucketValues(engine, 3) == std::vector<Value>{59, 52, 45, 38, 24, 17, 10});
}

TEST_CASE("hashTable: rejections change nothing; clear empties every bucket") {
    Engine engine;
    checkRejected(engine, make(kHash, Operation::Delete, 4), ErrorCode::ValueNotFound);
    run(engine, kHash, Operation::Insert, 4);
    run(engine, kHash, Operation::Insert, 11);
    checkRejected(engine, make(kHash, Operation::Insert, 4), ErrorCode::DuplicateValue);  // found at the end of the chain
    checkRejected(engine, make(kHash, Operation::Delete, 18), ErrorCode::ValueNotFound);  // same bucket, absent
    CHECK(integerData(run(engine, kHash, Operation::Insert, 18), "nodeId") == 3);
    const Response cleared = run(engine, kHash, Operation::Clear);
    CHECK(integerData(cleared, "removedCount") == 3);
    CHECK(engine.snapshot(kHash).buckets == std::vector<NodeId>(kHashBuckets, kNoNode));
    fill(engine, kHash, Operation::Insert);
    checkRejected(engine, make(kHash, Operation::Insert, 5000), ErrorCode::CapacityExceeded);
    // The worst case: 200 values in one bucket still fits the frame budget.
    run(engine, kHash, Operation::Clear);
    for (int i = 0; i < kMaxNodes; ++i) REQUIRE(engine.apply(make(kHash, Operation::Insert, i * 7)).status.ok());
    CHECK(run(engine, kHash, Operation::Search, 0).counters.comparisons == kMaxNodes);
    checkRejected(engine, make(kHash, Operation::Delete, 7 * 5000), ErrorCode::ValueNotFound);
}

TEST_CASE("hashTable: 6,000 seeded random operations agree with std::set") {
    std::mt19937 random(9107);
    Engine engine;
    std::set<Value> oracle;
    for (int step = 0; step < 6000; ++step) {
        const unsigned kind = random() % 10;
        const Value value = static_cast<Value>(random() % 260) - 130;
        if (kind < 4) {
            const Response response = run(engine, kHash, Operation::Insert, value);
            if (oracle.size() == static_cast<std::size_t>(kMaxNodes)) {
                CHECK(response.status.code == ErrorCode::CapacityExceeded);
            } else if (oracle.count(value) != 0) {
                CHECK(response.status.code == ErrorCode::DuplicateValue);
            } else {
                REQUIRE(response.status.ok());
                oracle.insert(value);
            }
        } else if (kind < 7) {
            CHECK(run(engine, kHash, Operation::Delete, value).status.ok() == (oracle.erase(value) == 1));
        } else if (kind < 9) {
            CHECK(booleanData(run(engine, kHash, Operation::Search, value), "found") == (oracle.count(value) == 1));
        } else if (random() % 40 == 0) {
            run(engine, kHash, Operation::Clear);
            oracle.clear();
        } else if (engine.undoCount(kHash) > 0) {
            const Snapshot now = engine.snapshot(kHash);
            run(engine, kHash, Operation::Undo);
            run(engine, kHash, Operation::Redo);
            CHECK(engine.snapshot(kHash) == now);
        }
        std::set<Value> stored;
        for (const NodeView& node : engine.snapshot(kHash).nodes) stored.insert(node.value);
        REQUIRE(stored == oracle);
    }
}

// ---------------------------------------------------------------------------
// All seven: export/import through JSON, and rejected files

TEST_CASE("new structures: an export imports back exactly, and broken files are rejected whole") {
    const std::vector<std::pair<std::string, std::vector<std::string>>> setups = {
        {"deque", {R"("operation":"pushBack","args":{"value":1})", R"("operation":"pushFront","args":{"value":2})",
                   R"("operation":"pushBack","args":{"value":3})"}},
        {"doublyList", {R"("operation":"insertAt","args":{"index":0,"value":1})", R"("operation":"insertAt","args":{"index":1,"value":2})",
                        R"("operation":"insertAt","args":{"index":1,"value":3})"}},
        {"circularList", {R"("operation":"insertBack","args":{"value":1})", R"("operation":"insertBack","args":{"value":2})",
                          R"("operation":"insertFront","args":{"value":3})"}},
        {"minHeap", {R"("operation":"insert","args":{"value":5})", R"("operation":"insert","args":{"value":3})",
                     R"("operation":"insert","args":{"value":4})"}},
        {"maxHeap", {R"("operation":"insert","args":{"value":5})", R"("operation":"insert","args":{"value":8})",
                     R"("operation":"insert","args":{"value":4})"}},
        {"splay", {R"("operation":"insert","args":{"value":5})", R"("operation":"insert","args":{"value":8})",
                   R"("operation":"insert","args":{"value":4})"}},
        {"hashTable", {R"("operation":"insert","args":{"value":5})", R"("operation":"insert","args":{"value":12})",
                       R"("operation":"insert","args":{"value":4})"}},
    };
    // One edit per structure that keeps the JSON well formed but breaks an invariant.
    const std::map<std::string, std::pair<std::string, std::string>> breaks = {
        {"deque", {"rear", "prev"}},           {"doublyList", {"tail", "prev"}}, {"circularList", {"tail", "next"}},
        {"minHeap", {"root", "index"}},        {"maxHeap", {"root", "value"}},   {"splay", {"root", "value"}},
        {"hashTable", {"buckets", "value"}},
    };
    for (const auto& [structure, bodies] : setups) {
        CAPTURE(structure);
        const auto send = [&](Engine& engine, const std::string& body) {
            return Json::parse(handleJson(engine, R"({"schemaVersion":1,"requestId":1,"structure":")" + structure + "\"," + body + "}"));
        };
        Engine engine;
        for (const std::string& body : bodies) REQUIRE(send(engine, body)["result"]["status"] == "ok");
        const std::string text = send(engine, R"("operation":"export","args":{})")["result"]["data"]["text"];
        const Json document = Json::parse(text);

        Engine other;
        Json import = {{"schemaVersion", 1}, {"requestId", 2}, {"structure", structure}, {"operation", "import"},
                       {"args", {{"text", text}}}};
        const Json imported = Json::parse(handleJson(other, import.dump()));
        REQUIRE(imported["result"]["status"] == "ok");
        CHECK(imported["result"]["data"]["nodeCount"] == 3);
        CHECK(send(other, R"("operation":"export","args":{})")["result"]["data"]["text"] == text);
        CHECK(imported["history"]["undo"] == 1);  // an import is one undoable operation

        // Broken files: each must be rejected with IMPORT_INVALID and change nothing.
        std::vector<Json> broken;
        Json wrongEntry = document;
        const std::string entry = breaks.at(structure).first;
        if (entry == "buckets") {
            wrongEntry["snapshot"]["buckets"][0] = wrongEntry["snapshot"]["nodes"][0]["id"];  // 5 does not hash to bucket 0
        } else {
            wrongEntry["snapshot"][entry] = nullptr;
        }
        broken.push_back(wrongEntry);
        Json wrongField = document;
        const std::string field = breaks.at(structure).second;
        if (field == "value") {
            // A leaf above its parent's value (heap), out of order (splay), in the wrong bucket (hash table).
            wrongField["snapshot"]["nodes"][0]["value"] = structure == "maxHeap" ? 100 : structure == "splay" ? 9 : 6;
        } else if (field == "index") {
            wrongField["snapshot"]["nodes"][0]["index"] = 7;
        } else if (field == "prev") {
            wrongField["snapshot"]["nodes"][0]["prev"] = wrongField["snapshot"]["nodes"][0]["id"];  // points at itself
        } else {
            wrongField["snapshot"]["nodes"][0][field] = nullptr;
        }
        broken.push_back(wrongField);
        Json missing = document;
        missing["snapshot"]["nodes"][1].erase(structure == "minHeap" || structure == "maxHeap" ? "index" : "value");
        broken.push_back(missing);
        Json extra = document;
        extra["snapshot"]["nodes"][2]["color"] = "red";
        broken.push_back(extra);
        Json wrongSize = document;
        wrongSize["snapshot"]["size"] = 2;
        broken.push_back(wrongSize);

        for (const Json& file : broken) {
            CAPTURE(file.dump());
            import["args"]["text"] = file.dump();
            const Json rejected = Json::parse(handleJson(other, import.dump()));
            CHECK(rejected["result"].value("code", "accepted") == "IMPORT_INVALID");
            CHECK(rejected["frames"].size() == 2);
            CHECK(send(other, R"("operation":"export","args":{})")["result"]["data"]["text"] == text);
            CHECK(rejected["history"]["undo"] == 1);
        }
    }
}

TEST_CASE("new structures: every explanation fits, even with the longest values") {
    // The largest magnitudes make the longest sentences (140 characters is the limit run() checks).
    const Value low = -2147483647 - 1;
    const Value high = 2147483647;
    Engine engine;
    for (const Value value : {low, high, low + 1, high - 1, low + 2}) {
        run(engine, kDeque, Operation::PushFront, value);
        run(engine, kDeque, Operation::PushBack, value);
        test::run(engine, insertAt(kDoubly, engine.snapshot(kDoubly).size / 2, value));
        run(engine, kCircular, Operation::InsertFront, value);
        run(engine, kCircular, Operation::InsertBack, value);
        run(engine, StructureKind::MinHeap, Operation::Insert, value);
        run(engine, StructureKind::MaxHeap, Operation::Insert, value);
        run(engine, kSplay, Operation::Insert, value);
        run(engine, kHash, Operation::Insert, value);
        run(engine, kHash, Operation::Insert, value);  // rejected: a duplicate
    }
    for (const Value value : {low, high, low + 5}) {
        run(engine, kDoubly, Operation::Search, value);
        run(engine, kCircular, Operation::Search, value);
        run(engine, kSplay, Operation::Search, value);
        run(engine, kHash, Operation::Search, value);
    }
    run(engine, kDoubly, Operation::Reverse);
    run(engine, kCircular, Operation::Rotate);
    test::run(engine, deleteNode(kDoubly, 3));
    test::run(engine, deleteNode(kCircular, 3));
    for (const StructureKind structure : {kDeque, kDeque}) {
        run(engine, structure, Operation::PopFront);
        run(engine, structure, Operation::PopBack);
        run(engine, structure, Operation::PeekFront);
        run(engine, structure, Operation::PeekBack);
    }
    for (const StructureKind heap : {StructureKind::MinHeap, StructureKind::MaxHeap}) {
        run(engine, heap, Operation::Peek);
        while (engine.snapshot(heap).size > 0) run(engine, heap, Operation::Extract);
    }
    for (const Value value : {low, high, high - 1, low + 1, low + 2}) {
        run(engine, kSplay, Operation::Delete, value);
        run(engine, kHash, Operation::Delete, value);
    }
    for (const StructureKind structure : {kDeque, kDoubly, kCircular, kSplay, kHash}) run(engine, structure, Operation::Clear);
}
