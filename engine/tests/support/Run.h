// Helpers shared by the structure tests.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dslab/Engine.h"

namespace dslab::test {

// Runs one request, then checks the structure's invariants and the frame
// contract (CLAUDE.md: the checker runs after every operation in tests).
Response run(Engine& engine, const Request& request);

// Shorthand for operations that take at most a value.
Response run(Engine& engine, StructureKind structure, Operation operation, Value value = 0);

// The named field of a successful result.
std::int64_t integerData(const Response& response, const std::string& name);
bool booleanData(const Response& response, const std::string& name);
bool isNullData(const Response& response, const std::string& name);
std::vector<std::int64_t> listData(const Response& response, const std::string& name);

std::vector<Event> events(const Response& response);

// Values along the chain of 'next' links starting at 'entry'.
std::vector<Value> chainValues(const Snapshot& snapshot, NodeId entry);

// Rejections: the error code is 'code', the frames are begin..reject, and the
// state and history counts are exactly as before. (Tests check separately that
// no node ID was used up, by creating a node afterwards.)
void checkRejected(Engine& engine, const Request& request, ErrorCode code);

}  // namespace dslab::test
