// Owns one instance of every structure and its history, and runs requests
// (docs/architecture.md, 2.1).
#pragma once

#include <vector>

#include "dslab/BinaryTree.h"
#include "dslab/Bst.h"
#include "dslab/History.h"
#include "dslab/Queue.h"
#include "dslab/Request.h"
#include "dslab/Response.h"
#include "dslab/SinglyLinkedList.h"
#include "dslab/Snapshot.h"
#include "dslab/Stack.h"

namespace dslab {

class Engine {
public:
    // Runs one checked request and returns its result, final snapshot and frames.
    Response apply(const Request& request);

    // The current state and history counts, also used to answer protocol errors.
    Snapshot snapshot(StructureKind structure) const;
    int undoCount(StructureKind structure) const;
    int redoCount(StructureKind structure) const;

private:
    // What one structure operation returns before the shared parts are added.
    struct Outcome {
        Status status;
        std::vector<DataField> data;
    };

    Outcome runStack(const Request& request, Trace& trace);
    Outcome runQueue(const Request& request, Trace& trace);
    Outcome runList(const Request& request, Trace& trace);
    Outcome runBinaryTree(const Request& request, Trace& trace);
    Outcome runBst(const Request& request, Trace& trace);

    // The shared read-only "snapshot" operation: a begin and a done frame.
    void readSnapshot(StructureKind structure, Trace& trace) const;

    History& history(StructureKind structure);
    const History& history(StructureKind structure) const;

    Stack stack_;
    Queue queue_;
    SinglyLinkedList list_;
    BinaryTree binaryTree_;
    Bst bst_;

    History stackHistory_;
    History queueHistory_;
    History listHistory_;
    History binaryTreeHistory_;
    History bstHistory_;
};

}  // namespace dslab
