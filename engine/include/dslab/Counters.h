// Work done by the algorithm itself (docs/operation-contract.md, 6.5). The
// counts would be the same if no frame were recorded (rule 11).
#pragma once

namespace dslab {

struct Counters {
    int comparisons = 0;   // key comparisons
    int nodesVisited = 0;  // times a cursor variable was set to a node
    int linksChanged = 0;  // writes to links and entry pointers
    int rotations = 0;
    int recolors = 0;
};

}  // namespace dslab
