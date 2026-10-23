// Collects the frames of one operation while the algorithm runs (rule 7),
// and counts the algorithm's own work separately from tracing cost (rule 11).
#pragma once

#include <string>
#include <vector>

#include "dslab/Counters.h"
#include "dslab/Frame.h"
#include "dslab/Snapshot.h"

namespace dslab {

class Trace {
public:
    // Records one frame showing 'snapshot', the state right after the step,
    // with a copy of the counters so far.
    // 'output' and 'pending' are used by traversals only (values visited so
    // far; the level-order queue). Done and Reject frames are always kept;
    // other frames stop being kept once only one place is left under
    // kMaxFramesPerOperation.
    void frame(Snapshot snapshot, Event event, std::string stepId, std::string explanation,
               Highlight highlight = {}, std::vector<NamedPointer> pointers = {},
               std::vector<Value> output = {}, std::vector<NodeId> pending = {});

    const std::vector<Frame>& frames() const { return frames_; }

    // True if a frame was dropped because of the cap (tests treat that as a bug).
    bool truncated() const { return truncated_; }

    Counters counters;

private:
    std::vector<Frame> frames_;
    bool truncated_ = false;
};

}  // namespace dslab
