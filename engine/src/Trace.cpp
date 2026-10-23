#include "dslab/Trace.h"

#include <utility>

#include "dslab/Limits.h"

namespace dslab {

void Trace::frame(Snapshot snapshot, Event event, std::string stepId, std::string explanation,
                  Highlight highlight, std::vector<NamedPointer> pointers,
                  std::vector<Value> output, std::vector<NodeId> pending) {
    const bool isLast = (event == Event::Done || event == Event::Reject);

    // Keep one place free for the final frame, which is always kept.
    if (!isLast && static_cast<int>(frames_.size()) >= kMaxFramesPerOperation - 1) {
        truncated_ = true;
        return;
    }

    Frame next;
    next.index = static_cast<int>(frames_.size());
    next.event = event;
    next.stepId = std::move(stepId);
    next.explanation = std::move(explanation);
    next.highlight = std::move(highlight);
    next.pointers = std::move(pointers);
    next.output = std::move(output);
    next.pending = std::move(pending);
    next.counters = counters;
    next.snapshot = std::move(snapshot);
    frames_.push_back(std::move(next));
}

}  // namespace dslab
