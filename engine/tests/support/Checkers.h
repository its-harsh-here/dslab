// Independent checkers used after every operation in the tests.
//
// They read only plain data (Snapshot, Response), never the structure classes,
// so a bug in an algorithm cannot also hide itself in the check. Each returns
// "" when everything holds, otherwise a sentence naming the first broken rule.
#pragma once

#include <string>

#include "dslab/Response.h"
#include "dslab/Snapshot.h"

namespace dslab::test {

// The structure's wire name, e.g. "queue" (step IDs start with it).
std::string wireName(StructureKind structure);

// Invariants of a finished operation's snapshot, chosen by its structure.
// Always: sizes agree, IDs ascend, links point at listed nodes, unused entry
// pointers are NIL. Linear: the chain from the entry pointer reaches every
// node exactly once and ends at NIL; a queue's rear is its last node. Trees:
// one root, one parent link per other node, parent fields correct, every node
// reachable; a BST's in-order values strictly increase.
std::string checkSnapshot(const Snapshot& snapshot);

// Frame-list invariants of docs/operation-contract.md 6.6 for one response
// that reached a structure. 'before' is the state before the operation.
std::string checkFrames(const Response& response, const Snapshot& before);

}  // namespace dslab::test
