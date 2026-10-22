// Owns one instance of every structure and its history, and runs requests
// (docs/architecture.md, 2.1). This milestone has only the stack.
#pragma once

#include "dslab/History.h"
#include "dslab/Request.h"
#include "dslab/Response.h"
#include "dslab/Snapshot.h"
#include "dslab/Stack.h"

namespace dslab {

class Engine {
public:
    // Runs one checked request and returns its result, final snapshot and frames.
    Response apply(const Request& request);

    // The current state and history counts, used to answer protocol errors.
    Snapshot snapshot(StructureKind structure) const;
    int undoCount(StructureKind structure) const;
    int redoCount(StructureKind structure) const;

private:
    // The shared read-only "snapshot" operation: a begin and a done frame.
    void readSnapshot(Trace& trace) const;

    Stack stack_;
    History stackHistory_;
};

}  // namespace dslab
