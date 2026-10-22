// A response as plain C++ data, before the JSON layer writes it out
// (docs/operation-contract.md, 5.2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dslab/Frame.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"

namespace dslab {

// One named number in the result data, such as {"nodeId", 3}.
struct DataField {
    std::string name;
    std::int64_t value = 0;
};

struct Response {
    Status status;
    std::vector<DataField> data;  // the result data when status is ok, in order
    Snapshot snapshot;            // the structure after the operation
    std::vector<Frame> frames;
    Counters counters;
    bool truncated = false;
    int undoCount = 0;
    int redoCount = 0;
};

}  // namespace dslab
