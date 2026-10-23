// NodePool, Trace and History on their own.
#include <cstdint>
#include <limits>

#include <doctest/doctest.h>

#include "dslab/History.h"
#include "dslab/Limits.h"
#include "dslab/NodePool.h"
#include "dslab/Trace.h"

using namespace dslab;

namespace {

struct TestNode {
    NodeId id = kNoNode;
    Value value = 0;
};

}  // namespace

TEST_CASE("NodePool gives ascending IDs and keeps ID order after a destroy") {
    NodePool<TestNode> pool;
    TestNode* a = pool.create(10);
    TestNode* b = pool.create(20);
    TestNode* c = pool.create(30);
    CHECK(a->id == 1);
    CHECK(b->id == 2);
    CHECK(c->id == 3);

    pool.destroy(b);
    REQUIRE(pool.size() == 2);
    CHECK(pool.nodes()[0]->id == 1);
    CHECK(pool.nodes()[1]->id == 3);
    CHECK(pool.create(40)->id == 4);  // ID 2 is never reused
}

TEST_CASE("NodePool reports when IDs run out") {
    const std::int64_t largest = std::numeric_limits<NodeId>::max();
    NodePool<TestNode> pool(largest);
    CHECK(pool.hasFreeId());
    CHECK(pool.create(1)->id == largest);
    CHECK_FALSE(pool.hasFreeId());
}

TEST_CASE("Trace keeps the final frame even when the cap is reached") {
    Trace trace;
    for (int i = 0; i < kMaxFramesPerOperation + 100; ++i) {
        trace.frame(Snapshot{}, Event::Move, "stack.test.step", "A step.");
    }
    trace.frame(Snapshot{}, Event::Done, "stack.test.done", "Done.");

    CHECK(trace.truncated());
    CHECK(trace.frames().size() == static_cast<std::size_t>(kMaxFramesPerOperation));
    CHECK(trace.frames().back().event == Event::Done);
    CHECK(trace.frames().back().index == kMaxFramesPerOperation - 1);
}

TEST_CASE("History keeps at most 50 undo entries") {
    History history;
    for (int i = 0; i < kMaxHistory + 10; ++i) {
        history.record(Snapshot{}, "push");
    }
    CHECK(history.undoCount() == kMaxHistory);
    CHECK(history.redoCount() == 0);
}
