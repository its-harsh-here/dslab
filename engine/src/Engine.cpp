#include "dslab/Engine.h"

#include <string>
#include <utility>

namespace dslab {

namespace {

// Operations that change the structure get an undo-history entry when they succeed.
bool isMutating(Operation operation) {
    return operation == Operation::Push || operation == Operation::Pop ||
           operation == Operation::Clear;
}

}  // namespace

Response Engine::apply(const Request& request) {
    // Only the stack exists so far, so 'request.structure' is always Stack.
    Response response;
    Trace trace;

    Snapshot before;
    if (isMutating(request.operation)) {
        before = stack_.snapshot();
    }

    switch (request.operation) {
        case Operation::Push: {
            const Result<NodeId> result = stack_.push(request.value, trace);
            response.status = result.status;
            if (result.status.ok()) {
                response.data = {{"nodeId", result.data}};
            }
            break;
        }
        case Operation::Pop: {
            const Result<NodeValue> result = stack_.pop(trace);
            response.status = result.status;
            if (result.status.ok()) {
                response.data = {{"nodeId", result.data.nodeId}, {"value", result.data.value}};
            }
            break;
        }
        case Operation::Peek: {
            const Result<NodeValue> result = stack_.peek(trace);
            response.status = result.status;
            if (result.status.ok()) {
                response.data = {{"nodeId", result.data.nodeId}, {"value", result.data.value}};
            }
            break;
        }
        case Operation::Clear: {
            const Result<int> result = stack_.clear(trace);
            response.status = result.status;
            if (result.status.ok()) {
                response.data = {{"removedCount", result.data}};
            }
            break;
        }
        case Operation::Snapshot: {
            readSnapshot(trace);
            response.status = okStatus();
            break;
        }
    }

    // A rejected operation changed nothing, so it gets no history entry (rule 8).
    if (isMutating(request.operation) && response.status.ok()) {
        stackHistory_.record(std::move(before));
    }

    response.snapshot = stack_.snapshot();
    response.frames = trace.frames();
    response.counters = trace.counters;
    response.truncated = trace.truncated();
    response.undoCount = stackHistory_.undoCount();
    response.redoCount = stackHistory_.redoCount();
    return response;
}

Snapshot Engine::snapshot(StructureKind /*structure*/) const {
    return stack_.snapshot();
}

int Engine::undoCount(StructureKind /*structure*/) const {
    return stackHistory_.undoCount();
}

int Engine::redoCount(StructureKind /*structure*/) const {
    return stackHistory_.redoCount();
}

void Engine::readSnapshot(Trace& trace) const {
    const Snapshot current = stack_.snapshot();
    const int size = current.size;
    trace.frame(current, Event::Begin, "common.snapshot.enter", "Read the current state of the stack.");
    trace.frame(current, Event::Done, "common.snapshot.done",
                size == 0 ? std::string("The stack is empty.")
                          : "The stack holds " + std::to_string(size) +
                                (size == 1 ? " node." : " nodes."));
}

}  // namespace dslab
