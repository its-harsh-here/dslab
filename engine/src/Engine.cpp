#include "dslab/Engine.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
        case StructureKind::List: return "list";
        case StructureKind::BinaryTree: return "binary tree";
        case StructureKind::Bst: return "BST";
    }
    return "structure";
}

// Turns a list of IDs or values into the 64-bit list the response holds.
template <typename T>
std::vector<std::int64_t> widen(const std::vector<T>& items) {
    return std::vector<std::int64_t>(items.begin(), items.end());
}

// The data of a traversal: values and node IDs in visit order.
std::vector<DataField> traversalData(const TraversalResult& result) {
    return {listField("values", widen(result.values)), listField("nodeIds", widen(result.nodeIds))};
}

// Which traversal a traversal operation asks for.
TraversalOrder orderOf(Operation operation) {
    switch (operation) {
        case Operation::Inorder: return TraversalOrder::In;
        case Operation::Postorder: return TraversalOrder::Post;
        case Operation::Levelorder: return TraversalOrder::Level;
        default: return TraversalOrder::Pre;
    }
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
            case StructureKind::List: outcome = runList(request, trace); break;
            case StructureKind::BinaryTree: outcome = runBinaryTree(request, trace); break;
            case StructureKind::Bst: outcome = runBst(request, trace); break;
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

Engine::Outcome Engine::runList(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::InsertAt: {
            const Result<NodeId> result = list_.insertAt(request.index, request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::DeleteNode: {
            const Result<NodeValue> result = list_.deleteNode(request.nodeId, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Search: {
            const Result<ListSearchResult> result = list_.search(request.value, trace);
            const ListSearchResult& found = result.data;
            if (!found.found) {
                return {result.status, {booleanField("found", false), nullField("nodeId"), nullField("index")}};
            }
            return {result.status,
                    {booleanField("found", true), integerField("nodeId", found.nodeId),
                     integerField("index", found.index)}};
        }
        case Operation::Reverse:
            return {list_.reverse(trace), {}};
        case Operation::Clear: {
            const Result<int> result = list_.clear(trace);
            return {result.status, {integerField("removedCount", result.data)}};
        }
        default:
            return {notAnOperation(), {}};
    }
}

Engine::Outcome Engine::runBinaryTree(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::AddRoot: {
            const Result<NodeId> result = binaryTree_.addRoot(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::AddChild: {
            const Result<NodeId> result =
                binaryTree_.addChild(request.parentId, request.side, request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::DeleteSubtree: {
            const Result<std::vector<NodeId>> result = binaryTree_.deleteSubtree(request.nodeId, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status,
                    {listField("removedIds", widen(result.data)),
                     integerField("removedCount", static_cast<std::int64_t>(result.data.size()))}};
        }
        case Operation::Preorder:
        case Operation::Inorder:
        case Operation::Postorder:
        case Operation::Levelorder: {
            const Result<TraversalResult> result = binaryTree_.traverse(orderOf(request.operation), trace);
            return {result.status, traversalData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = binaryTree_.clear(trace);
            return {result.status, {integerField("removedCount", result.data)}};
        }
        default:
            return {notAnOperation(), {}};
    }
}

Engine::Outcome Engine::runBst(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::Insert: {
            const Result<NodeId> result = bst_.insert(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::Delete: {
            const Result<NodeValue> result = bst_.remove(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Search: {
            const Result<BstSearchResult> result = bst_.search(request.value, trace);
            if (!result.data.found) {
                return {result.status, {booleanField("found", false), nullField("nodeId")}};
            }
            return {result.status, {booleanField("found", true), integerField("nodeId", result.data.nodeId)}};
        }
        case Operation::Preorder:
        case Operation::Inorder:
        case Operation::Postorder:
        case Operation::Levelorder: {
            const Result<TraversalResult> result = bst_.traverse(orderOf(request.operation), trace);
            return {result.status, traversalData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = bst_.clear(trace);
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
        case StructureKind::List: return list_.snapshot();
        case StructureKind::BinaryTree: return binaryTree_.snapshot();
        case StructureKind::Bst: return bst_.snapshot();
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
        case StructureKind::List: return listHistory_;
        case StructureKind::BinaryTree: return binaryTreeHistory_;
        case StructureKind::Bst: return bstHistory_;
    }
    return stackHistory_;
}

const History& Engine::history(StructureKind structure) const {
    switch (structure) {
        case StructureKind::Stack: return stackHistory_;
        case StructureKind::Queue: return queueHistory_;
        case StructureKind::List: return listHistory_;
        case StructureKind::BinaryTree: return binaryTreeHistory_;
        case StructureKind::Bst: return bstHistory_;
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
