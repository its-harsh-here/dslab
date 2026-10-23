// Checks an imported snapshot before it may replace a structure
// (docs/operation-contract.md 5.7, rules 3 to 10). The JSON layer has already
// checked rules 1 and 2: the document's format, version and structure, and
// that every field is present with the right type. This file checks that the
// snapshot describes a structure the engine itself could have produced.
#pragma once

#include "dslab/Result.h"
#include "dslab/Snapshot.h"

namespace dslab {

// Ok, or IMPORT_INVALID with one sentence naming the first broken rule (and
// the node, where there is one). It never changes anything.
Status validateSnapshot(const Snapshot& snapshot);

}  // namespace dslab
