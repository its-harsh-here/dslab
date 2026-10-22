#include "support/Checkers.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

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

// True when 'from' is listed and one of its links points at 'to'.
bool hasLink(const Snapshot& snapshot, NodeId from, NodeId to) {
    return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(), [from, to](const NodeView& view) {
        return view.id == from && view.next == to;
    });
}

// Sizes agree, at most 200 nodes, IDs positive and strictly ascending.
std::string checkCommon(const Snapshot& snapshot) {
    if (snapshot.size != static_cast<int>(snapshot.nodes.size())) {
        return "size " + std::to_string(snapshot.size) + " does not match " +
               std::to_string(snapshot.nodes.size()) + " listed nodes";
    }
    if (snapshot.size > kMaxNodes) {
        return "more than 200 nodes";
    }
    NodeId previous = kNoNode;
    for (const NodeView& node : snapshot.nodes) {
        if (node.id <= previous) {
            return "IDs are not positive and strictly ascending at node " + id(node.id);
        }
        previous = node.id;
    }
    return "";
}

// The chain from 'entry' must reach every node once and end at NIL.
// On success 'chain' holds the IDs in order.
std::string checkChain(const Snapshot& snapshot, NodeId entry, const char* entryName,
                       std::vector<NodeId>& chain) {
    std::map<NodeId, NodeId> nextOf;
    for (const NodeView& node : snapshot.nodes) {
        nextOf[node.id] = node.next;
    }
    for (const NodeView& node : snapshot.nodes) {
        if (node.next != kNoNode && nextOf.count(node.next) == 0) {
            return "node " + id(node.id) + " links to missing node " + id(node.next);
        }
    }
    if ((entry == kNoNode) != snapshot.nodes.empty()) {
        return std::string(entryName) + " must be NIL exactly when the structure is empty";
    }
    if (entry != kNoNode && nextOf.count(entry) == 0) {
        return std::string(entryName) + " points to missing node " + id(entry);
    }

    std::set<NodeId> seen;
    for (NodeId node = entry; node != kNoNode; node = nextOf[node]) {
        if (!seen.insert(node).second) {
            return "the chain from " + std::string(entryName) + " loops back to node " + id(node);
        }
        chain.push_back(node);
    }
    if (seen.size() != snapshot.nodes.size()) {
        return std::string("some nodes cannot be reached from ") + entryName;
    }
    return "";
}

std::string checkLinear(const Snapshot& snapshot) {
    std::vector<NodeId> chain;
    switch (snapshot.structure) {
        case StructureKind::Stack:
            if (snapshot.front != kNoNode || snapshot.rear != kNoNode) {
                return "a stack must not use front or rear";
            }
            return checkChain(snapshot, snapshot.top, "top", chain);
        case StructureKind::Queue: {
            if (snapshot.top != kNoNode) {
                return "a queue must not use top";
            }
            const std::string problem = checkChain(snapshot, snapshot.front, "front", chain);
            if (!problem.empty()) {
                return problem;
            }
            const NodeId last = chain.empty() ? kNoNode : chain.back();
            if (snapshot.rear != last) {
                return "rear is " + id(snapshot.rear) + " but the last node is " + id(last);
            }
            return "";
        }
    }
    return "unknown structure";
}

}  // namespace

std::string wireName(StructureKind structure) {
    switch (structure) {
        case StructureKind::Stack: return "stack";
        case StructureKind::Queue: return "queue";
    }
    return "unknown";
}

std::string checkSnapshot(const Snapshot& snapshot) {
    const std::string common = checkCommon(snapshot);
    if (!common.empty()) {
        return common;
    }
    return checkLinear(snapshot);
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

    const std::string prefix = wireName(response.snapshot.structure) + ".";
    std::set<NodeId> seenInThisOperation;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const Frame& frame = frames[i];
        const std::string where = "frame " + std::to_string(i) + ": ";

        if (frame.index != static_cast<int>(i)) {
            return where + "index is " + std::to_string(frame.index);
        }
        if (frame.snapshot.structure != response.snapshot.structure) {
            return where + "shows a different structure";
        }
        if (!response.status.ok() && frame.snapshot != before) {
            return where + "a rejected operation changed the state";
        }
        if (frame.stepId.rfind(prefix, 0) != 0 && frame.stepId.rfind("common.", 0) != 0) {
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
