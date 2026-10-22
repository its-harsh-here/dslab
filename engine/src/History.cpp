#include "dslab/History.h"

#include <utility>

#include "dslab/Limits.h"

namespace dslab {

void History::record(Snapshot before) {
    undo_.push_back(std::move(before));
    if (static_cast<int>(undo_.size()) > kMaxHistory) {
        undo_.erase(undo_.begin());  // drop the oldest entry
    }
    redo_.clear();  // after a new change, the old redo entries no longer apply
}

}  // namespace dslab
