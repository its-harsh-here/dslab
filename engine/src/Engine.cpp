#include "dslab/Engine.h"

#include <string>
#include <utility>

namespace dslab {

namespace {

// The data of operations that return one node: its ID and value.
std::vector<DataField> nodeData(const NodeValue& node) {
    return {integerField("nodeId", node.nodeId), integerField("value", node.value)};
}

// The name used in explanations, e.g. "the stack is empty".
const char* displayName(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return "stack";
        case StructureKind::Queue: return "queue";
    }
    return "structure";
}

// The JSON layer only builds valid structure/operation pairs, so this is never returned.
Status notAnOperation() {
    return errorStatus(ErrorCode::InternalError,
                       "The operation does not belong to this structure.");
}

}  // namespace

Response Engine::apply(const Request& request) {
    Trace trace;
    const bool mutates = isMutating(request.operation);

    Snapshot before;
    if (mutates) {
        before = snapshot(request.structure);
    }

    Outcome outcome;
    if (request.operation == Operation::Snapshot) {
        readSnapshot(request.structure, trace);
        outcome.status = okStatus();
    } else {
        switch (request.structure) {
            case StructureKind::Stack: outcome = runStack(request, trace); break;
            case StructureKind::Queue: outcome = runQueue(request, trace); break;
        }
    }

    // A rejected operation changed nothing, so it gets no history entry (rule 8).
    if (mutates && outcome.status.ok()) {
        history(request.structure).record(std::move(before));
    }

    Response response;
    response.status = outcome.status;
    response.data = std::move(outcome.data);
    response.snapshot = snapshot(request.structure);
    response.frames = trace.frames();
    response.counters = trace.counters;
    response.truncated = trace.truncated();
    response.undoCount = history(request.structure).undoCount();
    response.redoCount = history(request.structure).redoCount();
    return response;
}

Engine::Outcome Engine::runStack(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::Push: {
            const Result<NodeId> result = stack_.push(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::Pop: {
            const Result<NodeValue> result = stack_.pop(trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Peek: {
            const Result<NodeValue> result = stack_.peek(trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = stack_.clear(trace);
            return {result.status, {integerField("removedCount", result.data)}};
        }
        default:
            return {notAnOperation(), {}};
    }
}

Engine::Outcome Engine::runQueue(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::Enqueue: {
            const Result<NodeId> result = queue_.enqueue(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::Dequeue: {
            const Result<NodeValue> result = queue_.dequeue(trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Front: {
            const Result<NodeValue> result = queue_.front(trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = queue_.clear(trace);
            return {result.status, {integerField("removedCount", result.data)}};
        }
        default:
            return {notAnOperation(), {}};
    }
}

Snapshot Engine::snapshot(StructureKind structure) const {
    switch (structure) {
        case StructureKind::Stack: return stack_.snapshot();
        case StructureKind::Queue: return queue_.snapshot();
    }
    return Snapshot{};
}

int Engine::undoCount(StructureKind structure) const {
    return history(structure).undoCount();
}

int Engine::redoCount(StructureKind structure) const {
    return history(structure).redoCount();
}

History& Engine::history(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return stackHistory_;
        case StructureKind::Queue: return queueHistory_;
    }
    return stackHistory_;
}

const History& Engine::history(StructureKind structure) const {
    switch (structure) {
        case StructureKind::Stack: return stackHistory_;
        case StructureKind::Queue: return queueHistory_;
    }
    return stackHistory_;
}

void Engine::readSnapshot(StructureKind structure, Trace& trace) const {
    const Snapshot current = snapshot(structure);
    const std::string name = displayName(structure);
    const int size = current.size;
    trace.frame(current, Event::Begin, "common.snapshot.enter",
                "Read the current state of the " + name + ".");
    trace.frame(current, Event::Done, "common.snapshot.done",
                size == 0 ? "The " + name + " is empty."
                          : "The " + name + " holds " + std::to_string(size) +
                                (size == 1 ? " node." : " nodes."));
}

}  // namespace dslab
