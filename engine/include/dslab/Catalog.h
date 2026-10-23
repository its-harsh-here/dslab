// The pseudocode catalog (docs/operation-contract.md, 5.6): for every
// structure and operation, its Big-O and the pseudocode the UI shows, with the
// step IDs that frames refer to. It is the single source of these listings
// (decision D29). Plain data: the JSON layer writes it out; tests check that
// every step ID a frame carries is in its operation's listing.
#pragma once

#include <string>
#include <vector>

#include "dslab/Request.h"
#include "dslab/Types.h"

namespace dslab {

struct PseudocodeLine {
    std::string stepId;  // "" for a line that records no frame
    std::string text;    // leading spaces are the indentation
};

struct OperationListing {
    Operation operation = Operation::Snapshot;
    std::string complexity;  // the algorithm's Big-O, without tracing cost (rule 11)
    std::vector<PseudocodeLine> lines;
};

// Every listing of one structure, one per operation (snapshot and catalog
// have none: they run no algorithm).
const std::vector<OperationListing>& catalogFor(StructureKind structure);

// The listing of one operation of one structure, or nullptr if it has none.
const OperationListing* findListing(StructureKind structure, Operation operation);

}  // namespace dslab
