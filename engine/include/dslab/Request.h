// A request as plain C++ data, after the JSON layer has checked it.
#pragma once

#include <cstdint>

#include "dslab/Types.h"

namespace dslab {

enum class Operation { Push, Pop, Peek, Clear, Snapshot };

struct Request {
    std::int32_t requestId = 0;
    StructureKind structure = StructureKind::Stack;
    Operation operation = Operation::Snapshot;
    Value value = 0;  // used by push only
};

}  // namespace dslab
