// Typed results: every operation answers with a Status, never by throwing
// (docs/operation-contract.md, section 3).
#pragma once

#include <string>
#include <utility>

#include "dslab/Types.h"

namespace dslab {

enum class ErrorCode {
    Ok,
    // Protocol errors: found before any structure is touched.
    BadRequest,
    UnsupportedVersion,
    UnknownStructure,
    UnknownOperation,
    ValueOutOfRange,
    // Operation errors: the operation started and was rejected.
    CapacityExceeded,
    IdExhausted,
    EmptyStructure,
    IndexOutOfRange,
    NodeNotFound,
    RootExists,
    ChildExists,
    DuplicateValue,
    ValueNotFound,
    // Should never happen.
    InternalError
};

struct Status {
    ErrorCode code = ErrorCode::Ok;
    std::string message;  // one English sentence; empty when ok

    bool ok() const { return code == ErrorCode::Ok; }
};

inline Status okStatus() {
    return Status{};
}

inline Status errorStatus(ErrorCode code, std::string message) {
    return Status{code, std::move(message)};
}

// A status plus the data an operation returns when it succeeds.
template <typename T>
struct Result {
    Status status;
    T data{};  // meaningful only when status.ok()
};

// The data of pop and peek: which node, and the value it holds.
struct NodeValue {
    NodeId nodeId = kNoNode;
    Value value = 0;
};

}  // namespace dslab
