// Bounded undo history for one structure (docs/operation-contract.md 2.8 and 8).
// This milestone only records entries so that the "history" counts in every
// response are correct; the undo and redo operations arrive in milestone 7.
#pragma once

#include <vector>

#include "dslab/Snapshot.h"

namespace dslab {

class History {
public:
    // Called after every successful mutating operation with the state from
    // before it. Keeps at most kMaxHistory entries (the oldest is dropped) and
    // empties the redo list.
    void record(Snapshot before);

    int undoCount() const { return static_cast<int>(undo_.size()); }
    int redoCount() const { return static_cast<int>(redo_.size()); }

private:
    std::vector<Snapshot> undo_;  // oldest first
    std::vector<Snapshot> redo_;  // filled by undo in milestone 7
};

}  // namespace dslab
