// A response as plain C++ data, before the JSON layer writes it out
// (docs/operation-contract.md, 5.2).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "dslab/Frame.h"
#include "dslab/Result.h"
#include "dslab/Snapshot.h"
#include "dslab/Trace.h"

namespace dslab {

// One named item of result data, such as "nodeId": 3 or "found": false.
// 'kind' says which member holds the value.
struct DataField {
    enum class Kind { Integer, Boolean, Null, IntegerList };

    std::string name;
    Kind kind = Kind::Integer;
    std::int64_t integer = 0;
    bool boolean = false;
    std::vector<std::int64_t> list;
};

inline DataField integerField(std::string name, std::int64_t value) {
    DataField field;
    field.name = std::move(name);
    field.kind = DataField::Kind::Integer;
    field.integer = value;
    return field;
}

inline DataField booleanField(std::string name, bool value) {
    DataField field;
    field.name = std::move(name);
    field.kind = DataField::Kind::Boolean;
    field.boolean = value;
    return field;
}

inline DataField nullField(std::string name) {
    DataField field;
    field.name = std::move(name);
    field.kind = DataField::Kind::Null;
    return field;
}

inline DataField listField(std::string name, std::vector<std::int64_t> values) {
    DataField field;
    field.name = std::move(name);
    field.kind = DataField::Kind::IntegerList;
    field.list = std::move(values);
    return field;
}

struct Response {
    Status status;
    std::vector<DataField> data;  // the result data when status is ok, in order
    Snapshot snapshot;            // the structure after the operation
    std::vector<Frame> frames;
    Counters counters;
    bool truncated = false;
    int undoCount = 0;
    int redoCount = 0;
};

}  // namespace dslab
