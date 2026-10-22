// Independent checkers used after every operation in the tests.
//
// They read only plain data (Snapshot, Response), never the Stack class, so a
// bug in the algorithm cannot also hide itself in the check. Each returns ""
// when everything holds, otherwise a sentence naming the first broken rule.
#pragma once

#include <string>

#include "dslab/Response.h"
#include "dslab/Snapshot.h"

namespace dslab::test {

// Stack invariants: sizes agree, IDs ascend, links point at listed nodes, and
// the chain from top reaches every node exactly once and ends at NIL.
std::string checkStackSnapshot(const Snapshot& snapshot);

// Frame-list invariants of docs/operation-contract.md 6.6 for one response
// that reached the stack. 'before' is the state before the operation.
std::string checkFrames(const Response& response, const Snapshot& before);

}  // namespace dslab::test
