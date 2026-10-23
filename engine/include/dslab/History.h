// Operation-level undo and redo for one structure (docs/operation-contract.md
// 2.8 and 8), snapshot-based: before every successful change the engine keeps
// a full copy of the structure (with its ID counter). Undo puts that copy back;
// the state it replaces goes onto the redo list (docs/architecture.md,
// "Undo and redo").
#pragma once

#include <string>
#include <vector>

#include "dslab/Snapshot.h"

namespace dslab {

struct HistoryEntry {
    Snapshot snapshot;  // the state undo (or redo) returns to
    std::string label;  // the operation between it and its neighbour, e.g. "insert 25"
};

class History {
public:
    // After a successful change: keeps 'before' (the state before it) on the
    // undo list and empties the redo list, whose states no longer follow from
    // the current one. At most kMaxHistory entries; the oldest is dropped.
    void record(Snapshot before, std::string label);

    // Precondition: undoCount() > 0. Returns the newest undo entry; 'current'
    // goes onto the redo list with the same label.
    HistoryEntry undo(Snapshot current);

    // Precondition: redoCount() > 0. The mirror image of undo().
    HistoryEntry redo(Snapshot current);

    int undoCount() const { return static_cast<int>(undo_.size()); }
    int redoCount() const { return static_cast<int>(redo_.size()); }

private:
    // Oldest first. Undo and redo only move entries between the two lists, so
    // together they never hold more than kMaxHistory entries.
    std::vector<HistoryEntry> undo_;
    std::vector<HistoryEntry> redo_;
};

}  // namespace dslab
