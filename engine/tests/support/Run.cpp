#include "support/Run.h"

#include <doctest/doctest.h>

#include "dslab/Catalog.h"
#include "support/Checkers.h"

namespace dslab::test {

namespace {

const DataField& findField(const Response& response, const std::string& name) {
    REQUIRE(response.status.ok());
    for (const DataField& field : response.data) {
        if (field.name == name) {
            return field;
        }
    }
    FAIL("missing result field " << name);
    return response.data.front();  // not reached: FAIL stops the test
}

// Every frame's step ID must be in the catalog listing of the operation that
// ran (operation-contract.md 6.6, item 4); "common." steps belong to no listing.
std::string checkStepIds(const Request& request, const Response& response) {
    const OperationListing* listing = findListing(request.structure, request.operation);
    for (const Frame& frame : response.frames) {
        if (frame.stepId.rfind("common.", 0) == 0) {
            continue;
        }
        if (listing == nullptr) {
            return "operation has no catalog listing, but frame " + std::to_string(frame.index) + " has step " +
                   frame.stepId;
        }
        bool listed = false;
        for (const PseudocodeLine& line : listing->lines) {
            listed = listed || line.stepId == frame.stepId;
        }
        if (!listed) {
            return "step " + frame.stepId + " is not in the catalog listing";
        }
    }
    return "";
}

}  // namespace

Response run(Engine& engine, const Request& request) {
    const Snapshot before = engine.snapshot(request.structure);
    Response response = engine.apply(request);

    CAPTURE(wireName(request.structure));
    CHECK(checkSnapshot(response.snapshot) == "");
    CHECK(checkFrames(response, before) == "");
    CHECK(checkStepIds(request, response) == "");
    // The final frame shows the response's snapshot (checkFrames), and that is
    // the engine's real state afterwards, ID counter included.
    CHECK(response.snapshot == engine.snapshot(request.structure));
    return response;
}

Response run(Engine& engine, StructureKind structure, Operation operation, Value value) {
    Request request;
    request.structure = structure;
    request.operation = operation;
    request.value = value;
    return run(engine, request);
}

std::int64_t integerData(const Response& response, const std::string& name) {
    const DataField& field = findField(response, name);
    REQUIRE(field.kind == DataField::Kind::Integer);
    return field.integer;
}

bool booleanData(const Response& response, const std::string& name) {
    const DataField& field = findField(response, name);
    REQUIRE(field.kind == DataField::Kind::Boolean);
    return field.boolean;
}

bool isNullData(const Response& response, const std::string& name) {
    return findField(response, name).kind == DataField::Kind::Null;
}

std::vector<std::int64_t> listData(const Response& response, const std::string& name) {
    const DataField& field = findField(response, name);
    REQUIRE(field.kind == DataField::Kind::IntegerList);
    return field.list;
}

std::vector<Event> events(const Response& response) {
    std::vector<Event> result;
    for (const Frame& frame : response.frames) {
        result.push_back(frame.event);
    }
    return result;
}

std::vector<Value> chainValues(const Snapshot& snapshot, NodeId entry) {
    std::vector<Value> values;
    NodeId current = entry;
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

void checkRejected(Engine& engine, const Request& request, ErrorCode code) {
    const Snapshot before = engine.snapshot(request.structure);
    const int undoBefore = engine.undoCount(request.structure);

    const Response response = run(engine, request);
    CHECK(response.status.code == code);
    CHECK(response.frames.front().event == Event::Begin);
    CHECK(response.frames.back().event == Event::Reject);
    CHECK(response.snapshot == before);
    CHECK(engine.undoCount(request.structure) == undoBefore);
}

}  // namespace dslab::test
