#include "support/Checkers.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "dslab/Limits.h"

namespace dslab::test {

namespace {

std::string id(NodeId node) {
    return std::to_string(node);
}

// True when 'node' is listed in 'snapshot'.
bool contains(const Snapshot& snapshot, NodeId node) {
    return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(),
                       [node](const NodeView& view) { return view.id == node; });
}

// True when 'from' is listed and its next link is 'to'.
bool hasLink(const Snapshot& snapshot, NodeId from, NodeId to) {
    return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(),
                       [from, to](const NodeView& view) { return view.id == from && view.next == to; });
}

}  // namespace

std::string checkStackSnapshot(const Snapshot& snapshot) {
    if (snapshot.structure != StructureKind::Stack) {
        return "the snapshot is not a stack";
    }
    if (snapshot.size != static_cast<int>(snapshot.nodes.size())) {
        return "size " + std::to_string(snapshot.size) + " does not match " +
               std::to_string(snapshot.nodes.size()) + " listed nodes";
    }
    if (snapshot.size > kMaxNodes) {
        return "more than 200 nodes";
    }

    std::map<NodeId, NodeId> nextOf;
    NodeId previous = kNoNode;
    for (const NodeView& node : snapshot.nodes) {
        if (node.id <= previous) {
            return "IDs are not positive and strictly ascending at node " + id(node.id);
        }
        previous = node.id;
        nextOf[node.id] = node.next;
    }
    for (const NodeView& node : snapshot.nodes) {
        if (node.next != kNoNode && nextOf.count(node.next) == 0) {
            return "node " + id(node.id) + " links to missing node " + id(node.next);
        }
    }

    if ((snapshot.top == kNoNode) != snapshot.nodes.empty()) {
        return "top must be NIL exactly when the stack is empty";
    }
    if (snapshot.top != kNoNode && nextOf.count(snapshot.top) == 0) {
        return "top points to missing node " + id(snapshot.top);
    }

    std::set<NodeId> seen;
    for (NodeId node = snapshot.top; node != kNoNode; node = nextOf[node]) {
        if (!seen.insert(node).second) {
            return "the chain from top loops back to node " + id(node);
        }
    }
    if (seen.size() != snapshot.nodes.size()) {
        return "some nodes cannot be reached from top";
    }
    return "";
}

std::string checkFrames(const Response& response, const Snapshot& before) {
    const std::vector<Frame>& frames = response.frames;
    if (frames.size() < 2) {
        return "fewer than 2 frames";
    }
    if (response.truncated) {
        return "the trace was truncated";
    }

    const int largest = std::max(before.size, response.snapshot.size);
    if (static_cast<int>(frames.size()) > 2 * largest + 16) {
        return "more than 2n + 16 frames";
    }

    if (frames.front().event != Event::Begin) {
        return "frame 0 is not begin";
    }
    if (frames.front().snapshot != before) {
        return "frame 0 does not show the state before the operation";
    }
    const Event expectedLast = response.status.ok() ? Event::Done : Event::Reject;
    if (frames.back().event != expectedLast) {
        return "the last frame is not done (success) or reject (error)";
    }
    if (frames.back().snapshot != response.snapshot) {
        return "the last frame's snapshot differs from the response snapshot";
    }
    if (!response.status.ok() && frames.back().explanation != response.status.message) {
        return "the reject explanation differs from the result message";
    }

    std::set<NodeId> seenInThisOperation;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const Frame& frame = frames[i];
        const std::string where = "frame " + std::to_string(i) + ": ";

        if (frame.index != static_cast<int>(i)) {
            return where + "index is " + std::to_string(frame.index);
        }
        if (!response.status.ok() && frame.snapshot != before) {
            return where + "a rejected operation changed the state";
        }
        if (frame.stepId.rfind("stack.", 0) != 0 && frame.stepId.rfind("common.", 0) != 0) {
            return where + "step ID '" + frame.stepId + "' has the wrong prefix";
        }
        const std::string& sentence = frame.explanation;
        if (sentence.empty() || sentence.size() > 140 || sentence.back() != '.') {
            return where + "explanation is not one sentence of at most 140 characters";
        }

        for (const NodeView& node : frame.snapshot.nodes) {
            seenInThisOperation.insert(node.id);
        }
        for (const NodeId node : frame.highlight.nodes) {
            if (!contains(frame.snapshot, node) && seenInThisOperation.count(node) == 0) {
                return where + "highlights unknown node " + id(node);
            }
        }
        for (const Edge& edge : frame.highlight.edges) {
            if (!hasLink(frame.snapshot, edge.from, edge.to)) {
                return where + "highlights edge " + id(edge.from) + "->" + id(edge.to) +
                       ", which is not a link";
            }
        }
        for (const NamedPointer& pointer : frame.pointers) {
            if (pointer.node != kNoNode && !contains(frame.snapshot, pointer.node)) {
                return where + "pointer '" + pointer.name + "' names a node that is not listed";
            }
        }
    }
    return "";
}

}  // namespace dslab::test
