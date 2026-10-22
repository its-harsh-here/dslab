// JSON request/response layer (docs/operation-contract.md, section 5).
// Algorithm classes never see JSON: this file turns text into a Request and a
// Response back into text.
#include "dslab/json/Protocol.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "dslab/Limits.h"

namespace dslab {

namespace {

// Keys stay in the order they are written, so output matches the contract.
using Json = nlohmann::ordered_json;

// ---------------------------------------------------------------------------
// The structures, their operations and the arguments each operation takes

// Arguments an operation can take, combined with |.
enum ArgFlag : unsigned {
    kNoArgs = 0,
    kValueArg = 1,
};

struct OperationInfo {
    const char* name;
    Operation operation;
    unsigned args;
};

struct StructureInfo {
    const char* name;         // on the wire, e.g. "stack"
    const char* displayName;  // in messages, e.g. "the stack"
    StructureKind kind;
    std::vector<OperationInfo> operations;
};

const std::vector<StructureInfo>& structureTable() {
    static const std::vector<StructureInfo> table = {
        {"stack", "stack", StructureKind::Stack,
         {{"push", Operation::Push, kValueArg},
          {"pop", Operation::Pop, kNoArgs},
          {"peek", Operation::Peek, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs}}},
        {"queue", "queue", StructureKind::Queue,
         {{"enqueue", Operation::Enqueue, kValueArg},
          {"dequeue", Operation::Dequeue, kNoArgs},
          {"front", Operation::Front, kNoArgs},
          {"clear", Operation::Clear, kNoArgs},
          {"snapshot", Operation::Snapshot, kNoArgs}}},
    };
    return table;
}

const StructureInfo* findStructure(const std::string& name) {
    for (const StructureInfo& info : structureTable()) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

const StructureInfo& infoOf(StructureKind kind) {
    for (const StructureInfo& info : structureTable()) {
        if (info.kind == kind) {
            return info;
        }
    }
    return structureTable().front();
}

const OperationInfo* findOperation(const StructureInfo& structure, const std::string& name) {
    for (const OperationInfo& info : structure.operations) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

// "a, b and c"
std::string joinNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            text += (i + 1 == names.size()) ? " and " : ", ";
        }
        text += names[i];
    }
    return text;
}

const char* eventName(Event event) {
    switch (event) {
        case Event::Begin: return "begin";
        case Event::Move: return "move";
        case Event::Read: return "read";
        case Event::Create: return "create";
        case Event::Link: return "link";
        case Event::Unlink: return "unlink";
        case Event::Remove: return "remove";
        case Event::Reject: return "reject";
        case Event::Done: return "done";
    }
    return "done";
}

const char* errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "OK";
        case ErrorCode::BadRequest: return "BAD_REQUEST";
        case ErrorCode::UnsupportedVersion: return "UNSUPPORTED_VERSION";
        case ErrorCode::UnknownStructure: return "UNKNOWN_STRUCTURE";
        case ErrorCode::UnknownOperation: return "UNKNOWN_OPERATION";
        case ErrorCode::ValueOutOfRange: return "VALUE_OUT_OF_RANGE";
        case ErrorCode::CapacityExceeded: return "CAPACITY_EXCEEDED";
        case ErrorCode::IdExhausted: return "ID_EXHAUSTED";
        case ErrorCode::EmptyStructure: return "EMPTY_STRUCTURE";
        case ErrorCode::InternalError: return "INTERNAL_ERROR";
    }
    return "INTERNAL_ERROR";
}

// ---------------------------------------------------------------------------
// Reading a request

// What a response repeats from its request: only the parts that were valid.
struct Echo {
    std::optional<std::int32_t> requestId;
    std::optional<StructureKind> structure;
    const char* operation = nullptr;  // nullptr until recognised
};

struct ParsedRequest {
    Status status;  // ok, or the protocol error that stopped reading
    Echo echo;
    Request request;
};

ParsedRequest fail(ParsedRequest parsed, ErrorCode code, std::string message) {
    parsed.status = errorStatus(code, std::move(message));
    return parsed;
}

// Compact JSON text. Invalid UTF-8 is replaced instead of throwing.
std::string compact(const Json& json) {
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

// The member 'name' of a JSON object, or nullptr when it is missing.
const Json* member(const Json& object, const char* name) {
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &*found;
}

// True when 'value' is a JSON integer that fits in a signed 32-bit int.
bool fitsInt32(const Json& value) {
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    }
    if (value.is_number_integer()) {
        const std::int64_t number = value.get<std::int64_t>();
        return number >= std::numeric_limits<std::int32_t>::min() &&
               number <= std::numeric_limits<std::int32_t>::max();
    }
    return false;
}

// Text from the request is repeated in a message only when it is short,
// printable ASCII.
std::string mention(const std::string& text) {
    bool printable = text.size() <= 30;
    for (const char c : text) {
        if (c < 0x20 || c > 0x7e) {
            printable = false;
        }
    }
    return printable ? "\"" + text + "\"" : std::string("(not shown)");
}

// Reads the integer argument 'name' into 'target', or returns the protocol error.
Status readInteger(const Json& args, const char* name, std::int32_t& target) {
    const Json& value = *member(args, name);
    if (!value.is_number_integer()) {
        return errorStatus(ErrorCode::BadRequest, std::string("\"") + name + "\" must be an integer.");
    }
    if (!fitsInt32(value)) {
        return errorStatus(ErrorCode::ValueOutOfRange,
                           std::string("The ") + name + " " + compact(value) +
                               " is outside the 32-bit signed integer range.");
    }
    target = value.get<std::int32_t>();
    return okStatus();
}

ParsedRequest parseRequest(const std::string& text) {
    ParsedRequest parsed;
    if (text.size() > kMaxRequestBytes) {
        return fail(parsed, ErrorCode::BadRequest, "The request is larger than 1 MiB.");
    }

    const Json json = Json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded()) {
        return fail(parsed, ErrorCode::BadRequest, "The request is not valid JSON.");
    }
    if (!json.is_object()) {
        return fail(parsed, ErrorCode::BadRequest, "The request must be a JSON object.");
    }

    // The version comes first, so a request from a newer schema is recognised
    // as such before its fields are judged.
    const Json* version = member(json, "schemaVersion");
    if (version == nullptr || !version->is_number_integer()) {
        return fail(parsed, ErrorCode::BadRequest, "The request needs \"schemaVersion\": 1.");
    }
    if (*version != 1) {
        return fail(parsed, ErrorCode::UnsupportedVersion,
                    "Schema version " + compact(*version) +
                        " is not supported; this engine speaks version 1.");
    }

    for (const auto& item : json.items()) {
        const std::string& key = item.key();
        if (key != "schemaVersion" && key != "requestId" && key != "structure" &&
            key != "operation" && key != "args") {
            return fail(parsed, ErrorCode::BadRequest,
                        "The request has an unknown field " + mention(key) + ".");
        }
    }
    for (const char* name : {"requestId", "structure", "operation", "args"}) {
        if (member(json, name) == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string("The request is missing the field \"") + name + "\".");
        }
    }

    const Json& requestId = *member(json, "requestId");
    if (!fitsInt32(requestId) || requestId.get<std::int64_t>() < 0) {
        return fail(parsed, ErrorCode::BadRequest,
                    "\"requestId\" must be an integer from 0 to 2147483647.");
    }
    parsed.request.requestId = requestId.get<std::int32_t>();
    parsed.echo.requestId = parsed.request.requestId;

    const Json& structureName = *member(json, "structure");
    if (!structureName.is_string()) {
        return fail(parsed, ErrorCode::BadRequest, "\"structure\" must be a string.");
    }
    const StructureInfo* structure = findStructure(structureName.get<std::string>());
    if (structure == nullptr) {
        std::vector<std::string> known;
        for (const StructureInfo& info : structureTable()) {
            known.push_back(info.name);
        }
        return fail(parsed, ErrorCode::UnknownStructure,
                    "Unknown structure " + mention(structureName.get<std::string>()) +
                        "; this engine has " + joinNames(known) + " so far.");
    }
    parsed.request.structure = structure->kind;
    parsed.echo.structure = structure->kind;

    const Json& operationName = *member(json, "operation");
    if (!operationName.is_string()) {
        return fail(parsed, ErrorCode::BadRequest, "\"operation\" must be a string.");
    }
    const OperationInfo* operation = findOperation(*structure, operationName.get<std::string>());
    if (operation == nullptr) {
        std::vector<std::string> known;
        for (const OperationInfo& info : structure->operations) {
            known.push_back(info.name);
        }
        return fail(parsed, ErrorCode::UnknownOperation,
                    std::string("The ") + structure->displayName + " has no operation " +
                        mention(operationName.get<std::string>()) + "; it has " +
                        joinNames(known) + ".");
    }
    parsed.request.operation = operation->operation;
    parsed.echo.operation = operation->name;

    const Json& args = *member(json, "args");
    if (!args.is_object()) {
        return fail(parsed, ErrorCode::BadRequest, "\"args\" must be an object.");
    }
    for (const auto& item : args.items()) {
        const bool allowed = (item.key() == "value" && (operation->args & kValueArg) != 0);
        if (!allowed) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " has no argument " + mention(item.key()) + ".");
        }
    }
    if ((operation->args & kValueArg) != 0) {
        if (member(args, "value") == nullptr) {
            return fail(parsed, ErrorCode::BadRequest,
                        std::string(operation->name) + " needs the argument \"value\".");
        }
        const Status status = readInteger(args, "value", parsed.request.value);
        if (!status.ok()) {
            parsed.status = status;
            return parsed;
        }
    }

    parsed.status = okStatus();
    return parsed;
}

// ---------------------------------------------------------------------------
// Writing a response

Json nodeOrNull(NodeId id) {
    return id == kNoNode ? Json(nullptr) : Json(id);
}

// Only the fields of this structure (operation-contract.md, 5.4).
Json snapshotJson(const Snapshot& snapshot) {
    Json nodes = Json::array();
    for (const NodeView& node : snapshot.nodes) {
        Json item = Json::object();
        item["id"] = node.id;
        item["value"] = node.value;
        item["next"] = nodeOrNull(node.next);
        nodes.push_back(std::move(item));
    }

    Json json = Json::object();
    json["structure"] = infoOf(snapshot.structure).name;
    json["size"] = snapshot.size;
    switch (snapshot.structure) {
        case StructureKind::Stack:
            json["top"] = nodeOrNull(snapshot.top);
            break;
        case StructureKind::Queue:
            json["front"] = nodeOrNull(snapshot.front);
            json["rear"] = nodeOrNull(snapshot.rear);
            break;
    }
    json["nodes"] = std::move(nodes);
    return json;
}

Json frameJson(const Frame& frame) {
    Json highlightNodes = Json::array();
    for (const NodeId id : frame.highlight.nodes) {
        highlightNodes.push_back(id);
    }
    Json highlightEdges = Json::array();
    for (const Edge& edge : frame.highlight.edges) {
        highlightEdges.push_back(std::to_string(edge.from) + "->" + std::to_string(edge.to));
    }
    Json highlight = Json::object();
    highlight["nodes"] = std::move(highlightNodes);
    highlight["edges"] = std::move(highlightEdges);

    Json pointers = Json::object();
    for (const NamedPointer& pointer : frame.pointers) {
        pointers[pointer.name] = nodeOrNull(pointer.node);
    }

    Json json = Json::object();
    json["index"] = frame.index;
    json["event"] = eventName(frame.event);
    json["stepId"] = frame.stepId;
    json["explanation"] = frame.explanation;
    json["highlight"] = std::move(highlight);
    json["pointers"] = std::move(pointers);
    json["output"] = frame.output;
    json["pending"] = frame.pending;
    json["snapshot"] = snapshotJson(frame.snapshot);
    return json;
}

Json dataValueJson(const DataField& field) {
    switch (field.kind) {
        case DataField::Kind::Integer: return Json(field.integer);
        case DataField::Kind::Boolean: return Json(field.boolean);
        case DataField::Kind::Null: return Json(nullptr);
        case DataField::Kind::IntegerList: return Json(field.list);
    }
    return Json(nullptr);
}

Json resultJson(const Response& response) {
    Json json = Json::object();
    if (response.status.ok()) {
        Json data = Json::object();
        for (const DataField& field : response.data) {
            data[field.name] = dataValueJson(field);
        }
        json["status"] = "ok";
        json["data"] = std::move(data);
    } else {
        json["status"] = "error";
        json["code"] = errorCodeName(response.status.code);
        json["message"] = response.status.message;
    }
    return json;
}

Json metricsJson(const Response& response) {
    int snapshotNodes = 0;
    for (const Frame& frame : response.frames) {
        snapshotNodes += static_cast<int>(frame.snapshot.nodes.size());
    }

    Json algorithm = Json::object();
    algorithm["comparisons"] = response.counters.comparisons;
    algorithm["nodesVisited"] = response.counters.nodesVisited;
    algorithm["linksChanged"] = response.counters.linksChanged;
    algorithm["rotations"] = response.counters.rotations;
    algorithm["recolors"] = response.counters.recolors;

    Json trace = Json::object();
    trace["frames"] = response.frames.size();
    trace["snapshotNodes"] = snapshotNodes;
    trace["truncated"] = response.truncated;

    Json json = Json::object();
    json["algorithm"] = std::move(algorithm);
    json["trace"] = std::move(trace);
    return json;
}

// Appends "name":value to an object that is being written by hand.
void addField(std::string& out, const char* name, const Json& value) {
    if (out.size() > 1) {
        out += ',';
    }
    out += compact(Json(name));
    out += ':';
    out += compact(value);
}

// The response is written field by field, and the frames one at a time, so a
// JSON tree of the whole response (which can be several MB) is never built.
std::string writeResponse(const Echo& echo, const Response& response) {
    const bool knownStructure = echo.structure.has_value();

    std::string out = "{";
    addField(out, "schemaVersion", Json(1));
    addField(out, "requestId", echo.requestId ? Json(*echo.requestId) : Json(nullptr));
    addField(out, "structure",
             knownStructure ? Json(infoOf(*echo.structure).name) : Json(nullptr));
    addField(out, "operation", echo.operation != nullptr ? Json(echo.operation) : Json(nullptr));
    addField(out, "result", resultJson(response));
    addField(out, "snapshot", knownStructure ? snapshotJson(response.snapshot) : Json(nullptr));

    out += ",\"frames\":[";
    for (std::size_t i = 0; i < response.frames.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += compact(frameJson(response.frames[i]));
    }
    out += ']';

    addField(out, "metrics", metricsJson(response));
    Json history = Json(nullptr);
    if (knownStructure) {
        history = Json::object();
        history["undo"] = response.undoCount;
        history["redo"] = response.redoCount;
    }
    addField(out, "history", history);
    out += '}';
    return out;
}

// Used only if something unexpected throws (operation-contract.md, 3.3).
const char* const kInternalErrorResponse =
    "{\"schemaVersion\":1,\"requestId\":null,\"structure\":null,\"operation\":null,"
    "\"result\":{\"status\":\"error\",\"code\":\"INTERNAL_ERROR\","
    "\"message\":\"The engine hit an unexpected internal error.\"},"
    "\"snapshot\":null,\"frames\":[],"
    "\"metrics\":{\"algorithm\":{\"comparisons\":0,\"nodesVisited\":0,\"linksChanged\":0,"
    "\"rotations\":0,\"recolors\":0},\"trace\":{\"frames\":0,\"snapshotNodes\":0,"
    "\"truncated\":false}},\"history\":null}";

}  // namespace

std::string handleJson(Engine& engine, const std::string& requestText) {
    try {
        const ParsedRequest parsed = parseRequest(requestText);

        Response response;
        if (parsed.status.ok()) {
            response = engine.apply(parsed.request);
        } else {
            // Protocol error: no frames; describe the structure if it was recognised.
            response.status = parsed.status;
            if (parsed.echo.structure) {
                response.snapshot = engine.snapshot(*parsed.echo.structure);
                response.undoCount = engine.undoCount(*parsed.echo.structure);
                response.redoCount = engine.redoCount(*parsed.echo.structure);
            }
        }
        return writeResponse(parsed.echo, response);
    } catch (...) {
        // Answer instead of letting the exception abort the WebAssembly module.
        return kInternalErrorResponse;
    }
}

}  // namespace dslab
