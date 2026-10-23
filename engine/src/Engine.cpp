#include "dslab/Engine.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "dslab/Catalog.h"
#include "dslab/Validate.h"

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
        case StructureKind::Avl: return "AVL tree";
        case StructureKind::RedBlack: return "red-black tree";
    }
    return "structure";
}

// "1 node" or "3 nodes".
std::string nodeCount(int count) {
    return std::to_string(count) + (count == 1 ? " node" : " nodes");
}

// How undo and redo name an operation: "insert 25", "pop", "delete node #3".
std::string operationText(const Request& request) {
    const std::string value = std::to_string(request.value);
    switch (request.operation) {
        case Operation::Push: return "push " + value;
        case Operation::Pop: return "pop";
        case Operation::Enqueue: return "enqueue " + value;
        case Operation::Dequeue: return "dequeue";
        case Operation::InsertAt: return "insert " + value + " at index " + std::to_string(request.index);
        case Operation::DeleteNode: return "delete node #" + std::to_string(request.nodeId);
        case Operation::Reverse: return "reverse";
        case Operation::AddRoot: return "add root " + value;
        case Operation::AddChild:
            return "add " + value + " as the " + (request.side == Side::Left ? "left" : "right") + " child of #" +
                   std::to_string(request.parentId);
        case Operation::DeleteSubtree: return "delete the subtree of #" + std::to_string(request.nodeId);
        case Operation::Insert: return "insert " + value;
        case Operation::Delete: return "delete " + value;
        case Operation::Clear: return "clear";
        case Operation::Import: return "import";
        default: return "the last operation";
    }
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
    switch (request.operation) {
        case Operation::Snapshot:
            readSnapshot(request.structure, trace);
            outcome.status = okStatus();
            break;
        case Operation::Catalog:
            readCatalog(request.structure, trace);
            outcome.status = okStatus();
            break;
        case Operation::Undo: outcome = undo(request.structure, trace); break;
        case Operation::Redo: outcome = redo(request.structure, trace); break;
        case Operation::Export: outcome = exportState(request.structure, trace); break;
        case Operation::Import: outcome = importState(request, trace); break;
        default:
            switch (request.structure) {
                case StructureKind::Stack: outcome = runStack(request, trace); break;
                case StructureKind::Queue: outcome = runQueue(request, trace); break;
                case StructureKind::List: outcome = runList(request, trace); break;
                case StructureKind::BinaryTree: outcome = runBinaryTree(request, trace); break;
                case StructureKind::Bst: outcome = runBst(request, trace); break;
                case StructureKind::Avl: outcome = runAvl(request, trace); break;
                case StructureKind::RedBlack: outcome = runRedBlack(request, trace); break;
            }
    }

    // A rejected operation changed nothing, so it gets no history entry (rule 8).
    if (mutates && outcome.status.ok()) {
        history(request.structure).record(std::move(before), operationText(request));
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

Engine::Outcome Engine::runAvl(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::Insert: {
            const Result<NodeId> result = avl_.insert(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::Delete: {
            const Result<NodeValue> result = avl_.remove(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Search: {
            const Result<BstSearchResult> result = avl_.search(request.value, trace);
            if (!result.data.found) {
                return {result.status, {booleanField("found", false), nullField("nodeId")}};
            }
            return {result.status, {booleanField("found", true), integerField("nodeId", result.data.nodeId)}};
        }
        case Operation::Preorder:
        case Operation::Inorder:
        case Operation::Postorder:
        case Operation::Levelorder: {
            const Result<TraversalResult> result = avl_.traverse(orderOf(request.operation), trace);
            return {result.status, traversalData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = avl_.clear(trace);
            return {result.status, {integerField("removedCount", result.data)}};
        }
        default:
            return {notAnOperation(), {}};
    }
}

Engine::Outcome Engine::runRedBlack(const Request& request, Trace& trace) {
    switch (request.operation) {
        case Operation::Insert: {
            const Result<NodeId> result = redBlack_.insert(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, {integerField("nodeId", result.data)}};
        }
        case Operation::Delete: {
            const Result<NodeValue> result = redBlack_.remove(request.value, trace);
            if (!result.status.ok()) return {result.status, {}};
            return {result.status, nodeData(result.data)};
        }
        case Operation::Search: {
            const Result<BstSearchResult> result = redBlack_.search(request.value, trace);
            if (!result.data.found) {
                return {result.status, {booleanField("found", false), nullField("nodeId")}};
            }
            return {result.status, {booleanField("found", true), integerField("nodeId", result.data.nodeId)}};
        }
        case Operation::Preorder:
        case Operation::Inorder:
        case Operation::Postorder:
        case Operation::Levelorder: {
            const Result<TraversalResult> result = redBlack_.traverse(orderOf(request.operation), trace);
            return {result.status, traversalData(result.data)};
        }
        case Operation::Clear: {
            const Result<int> result = redBlack_.clear(trace);
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
        case StructureKind::Avl: return avl_.snapshot();
        case StructureKind::RedBlack: return redBlack_.snapshot();
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
        case StructureKind::Avl: return avlHistory_;
        case StructureKind::RedBlack: return redBlackHistory_;
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
        case StructureKind::Avl: return avlHistory_;
        case StructureKind::RedBlack: return redBlackHistory_;
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

void Engine::readCatalog(StructureKind structure, Trace& trace) const {
    const Snapshot current = snapshot(structure);
    const std::string name = displayName(structure);
    const int count = static_cast<int>(catalogFor(structure).size());
    trace.frame(current, Event::Begin, "common.catalog.enter",
                "Read the pseudocode catalog of the " + name + ".");
    trace.frame(current, Event::Done, "common.catalog.done",
                "The catalog lists " + std::to_string(count) + " operations of the " + name + ".");
}

// undo:  the state before the newest change comes back; the current one goes onto the redo list.
Engine::Outcome Engine::undo(StructureKind structure, Trace& trace) {
    const Snapshot current = snapshot(structure);
    const std::string name = displayName(structure);
    History& list = history(structure);
    if (list.undoCount() == 0) {
        trace.frame(current, Event::Begin, "common.undo.enter", "Undo the last operation on the " + name + ".");
        const std::string message = "There is nothing to undo.";
        trace.frame(current, Event::Reject, "common.undo.empty", message);
        return {errorStatus(ErrorCode::NothingToUndo, message), {}};
    }
    const HistoryEntry entry = list.undo(current);
    trace.frame(current, Event::Begin, "common.undo.enter", "Undo " + entry.label + " on the " + name + ".");
    restore(structure, entry.snapshot);
    const Snapshot restored = snapshot(structure);
    trace.frame(restored, Event::Restore, "common.undo.restore",
                "Restore the " + name + " as it was before " + entry.label + ", with the same node IDs.");
    trace.frame(restored, Event::Done, "common.undo.done",
                "Undid " + entry.label + "; the " + name + " holds " + nodeCount(restored.size) + ".");
    return {okStatus(), {}};
}

// redo:  the mirror image of undo.
Engine::Outcome Engine::redo(StructureKind structure, Trace& trace) {
    const Snapshot current = snapshot(structure);
    const std::string name = displayName(structure);
    History& list = history(structure);
    if (list.redoCount() == 0) {
        trace.frame(current, Event::Begin, "common.redo.enter", "Redo the last undone operation on the " + name + ".");
        const std::string message = "There is nothing to redo.";
        trace.frame(current, Event::Reject, "common.redo.empty", message);
        return {errorStatus(ErrorCode::NothingToRedo, message), {}};
    }
    const HistoryEntry entry = list.redo(current);
    trace.frame(current, Event::Begin, "common.redo.enter", "Redo " + entry.label + " on the " + name + ".");
    restore(structure, entry.snapshot);
    const Snapshot restored = snapshot(structure);
    trace.frame(restored, Event::Restore, "common.redo.restore",
                "Restore the " + name + " as it was after " + entry.label + ", with the same node IDs.");
    trace.frame(restored, Event::Done, "common.redo.done",
                "Redid " + entry.label + "; the " + name + " holds " + nodeCount(restored.size) + ".");
    return {okStatus(), {}};
}

Engine::Outcome Engine::exportState(StructureKind structure, Trace& trace) const {
    const Snapshot current = snapshot(structure);
    const std::string name = displayName(structure);
    trace.frame(current, Event::Begin, "common.export.enter", "Export the " + name + " as a file.");
    trace.frame(current, Event::Done, "common.export.done",
                "Exported " + nodeCount(current.size) + " and the ID counter; the " + name + " is unchanged.");
    return {okStatus(), {}};
}

// import:  the JSON layer decoded the document (rules 1 and 2); every other
// rule is checked here before anything changes, so a bad file changes nothing.
Engine::Outcome Engine::importState(const Request& request, Trace& trace) {
    const Snapshot current = snapshot(request.structure);
    const std::string name = displayName(request.structure);
    trace.frame(current, Event::Begin, "common.import.enter", "Import a file into the " + name + ".");
    const Status status = request.importProblem.empty() ? validateSnapshot(request.importSnapshot)
                                                         : errorStatus(ErrorCode::ImportInvalid, request.importProblem);
    if (!status.ok()) {
        trace.frame(current, Event::Reject, "common.import.invalid", status.message);
        return {status, {}};
    }
    restore(request.structure, request.importSnapshot);
    const Snapshot imported = snapshot(request.structure);
    trace.frame(imported, Event::Restore, "common.import.restore",
                "Replace the " + name + " with the file's " + nodeCount(imported.size) + ", keeping their IDs.");
    trace.frame(imported, Event::Done, "common.import.done",
                "Imported " + nodeCount(imported.size) + " into the " + name + ".");
    return {okStatus(), {integerField("nodeCount", imported.size)}};
}

void Engine::restore(StructureKind structure, const Snapshot& snapshot) {
    switch (structure) {
        case StructureKind::Stack: stack_.restore(snapshot); break;
        case StructureKind::Queue: queue_.restore(snapshot); break;
        case StructureKind::List: list_.restore(snapshot); break;
        case StructureKind::BinaryTree: binaryTree_.restore(snapshot); break;
        case StructureKind::Bst: bst_.restore(snapshot); break;
        case StructureKind::Avl: avl_.restore(snapshot); break;
        case StructureKind::RedBlack: redBlack_.restore(snapshot); break;
    }
}

}  // namespace dslab
