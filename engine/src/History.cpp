#include "dslab/History.h"

#include <utility>

#include "dslab/Limits.h"

namespace dslab {

void History::record(Snapshot before, std::string label) {
    undo_.push_back(HistoryEntry{std::move(before), std::move(label)});
    if (static_cast<int>(undo_.size()) > kMaxHistory) {
        undo_.erase(undo_.begin());  // drop the oldest entry
    }
    redo_.clear();  // after a new change, the old redo entries no longer apply
}

HistoryEntry History::undo(Snapshot current) {
    HistoryEntry entry = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back(HistoryEntry{std::move(current), entry.label});
    return entry;
}

HistoryEntry History::redo(Snapshot current) {
    HistoryEntry entry = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back(HistoryEntry{std::move(current), entry.label});
    return entry;
}

}  // namespace dslab
